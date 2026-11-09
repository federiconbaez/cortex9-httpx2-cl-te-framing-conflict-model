/* CLI: regenerate results/ or inspect one request.
 *   hx2 results fixtures/cases.tsv <dir>
 *   hx2 case <METHOD> <user_te|-> <user_cl|-> <body>
 * Pure bookkeeping: no socket is opened and httpx2 is never imported. */
#include "cx9_util.h"
#include "hx_model.h"
#include <stdio.h>
#include <string.h>

static void names(FILE *f, const hx_result *r) {
  int i;
  for (i = 0; i < r->h.count; ++i) fprintf(f, "%s%s", i ? "|" : "", r->h.name[i]);
}

static void framing_values(FILE *f, const hx_result *r) {
  int i;
  const char *cl = "", *te = "";
  for (i = 0; i < r->h.count; ++i) {
    if (strcmp(r->h.name[i], "Content-Length") == 0) cl = r->h.value[i];
    if (strcmp(r->h.name[i], "Transfer-Encoding") == 0) te = r->h.value[i];
  }
  cx9_csv_field(f, cl);
  fputc(',', f);
  cx9_csv_field(f, te);
}

static int results(const char *cases, const char *dir) {
  static char line[3 * CX9_FIELD_MAX];
  static const char *methods[] = {"GET", "POST", "PUT", "QUERY", "DELETE"};
  static const char *tes[] = {"-", "chunked", "gzip"}, *cls[] = {"-", "8", "3"};
  static const char *bodies[] = {"none", "bytes:0", "bytes:8", "iter_known:8", "iter_unknown", "aiter",
                                 "json:8", "form:8", "multipart_known:8", "multipart_unknown", "stream"};
  char path[1024];
  FILE *in = fopen(cases, "r"), *out, *sw;
  size_t m, t, c, b;
  int v;
  if (!in) return 1;
  snprintf(path, sizeof path, "%s/cases.csv", dir);
  out = fopen(path, "w");
  if (!out) { fclose(in); return 1; }
  fputs("id,kind,version,framing,headers,content_length,transfer_encoding,cl_mismatch\n", out);
  while (fgets(line, sizeof line, in)) {
    char *f[CX9_MAX_FIELDS];
    hx_input x;
    if (line[0] == '#' || line[0] == '\n') continue;
    if (cx9_split_tsv(line, f, CX9_MAX_FIELDS) != 8) { fclose(in); fclose(out); return 1; }
    if (hx_parse(f[2], f[3], f[4], f[5], &x) != 0) {
      for (v = 0; v < 2; ++v) fprintf(out, "%s,%s,%s,BAD_INPUT,,,,\n", f[0], f[1], hx_version_name((hx_version)v));
      continue;
    }
    for (v = 0; v < 2; ++v) {
      hx_result r = hx_build((hx_version)v, &x);
      fprintf(out, "%s,%s,%s,%s,", f[0], f[1], hx_version_name((hx_version)v), hx_framing_name(r.framing));
      names(out, &r);
      fputc(',', out);
      framing_values(out, &r);
      fprintf(out, ",%d\n", r.cl_mismatch);
    }
  }
  fclose(in);
  fclose(out);
  snprintf(path, sizeof path, "%s/sweep.csv", dir);
  sw = fopen(path, "w");
  if (!sw) return 1;
  fputs("method,user_te,user_cl,body,pre,post,pre_headers,post_headers\n", sw);
  for (m = 0; m < 5; ++m)
    for (t = 0; t < 3; ++t)
      for (c = 0; c < 3; ++c)
        for (b = 0; b < sizeof bodies / sizeof *bodies; ++b) {
          hx_input x;
          hx_result r[2];
          if (hx_parse(methods[m], tes[t], cls[c], bodies[b], &x) != 0) { fclose(sw); return 1; }
          r[0] = hx_build(HX_PRE, &x);
          r[1] = hx_build(HX_POST, &x);
          fprintf(sw, "%s,%s,%s,%s,%s,%s,", methods[m], tes[t], cls[c], bodies[b], hx_framing_name(r[0].framing),
                  hx_framing_name(r[1].framing));
          names(sw, &r[0]);
          fputc(',', sw);
          names(sw, &r[1]);
          fputc('\n', sw);
        }
  fclose(sw);
  return 0;
}

int main(int argc, char **argv) {
  if (argc == 4 && strcmp(argv[1], "results") == 0) return results(argv[2], argv[3]);
  if (argc == 6 && strcmp(argv[1], "case") == 0) {
    hx_input x;
    int v, i;
    if (hx_parse(argv[2], argv[3], argv[4], argv[5], &x) != 0) {
      puts("BAD_INPUT");
      return 0;
    }
    for (v = 0; v < 2; ++v) {
      hx_result r = hx_build((hx_version)v, &x);
      printf("%-4s %-8s cl_mismatch=%d\n", hx_version_name((hx_version)v), hx_framing_name(r.framing), r.cl_mismatch);
      for (i = 0; i < r.h.count; ++i) printf("     %s: %s\n", r.h.name[i], r.h.value[i]);
    }
    return 0;
  }
  fprintf(stderr, "usage: %s results <cases.tsv> <dir> | case <METHOD> <user_te|-> <user_cl|-> <body>\n", argv[0]);
  return 2;
}
