/* Tests for the HTTPX2 request-framing model. Run under ASan + UBSan with
 * -fno-sanitize-recover (scripts/build.sh test). */
#include "cx9_util.h"
#include "hx_model.h"
#include <stdio.h>
#include <string.h>

static int checks, failures;
#define CHECK(c, what)                                                    \
  do {                                                                    \
    checks++;                                                             \
    if (!(c)) {                                                           \
      failures++;                                                         \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, what);               \
    }                                                                     \
  } while (0)

#define MAXCASES 64
typedef struct {
  char id[64], kind[8], hand[2][16];
  int ok;
  hx_input in;
} tcase;
static tcase cases[MAXCASES];
static int ncases;

static void names(const hx_result *r, char *out, size_t n) {
  int i;
  size_t used = 0;
  out[0] = 0;
  for (i = 0; i < r->h.count && used < n; ++i)
    used += (size_t)snprintf(out + used, n - used, "%s%s", i ? "|" : "", r->h.name[i]);
}

static int count_name(const hx_result *r, const char *name) {
  int i, k = 0;
  for (i = 0; i < r->h.count; ++i) k += strcmp(r->h.name[i], name) == 0;
  return k;
}

static const char *value_of(const hx_result *r, const char *name) {
  int i;
  for (i = 0; i < r->h.count; ++i)
    if (strcmp(r->h.name[i], name) == 0) return r->h.value[i];
  return NULL;
}

static void load_cases(const char *dir) {
  static char line[3 * CX9_FIELD_MAX];
  char path[512];
  FILE *f;
  snprintf(path, sizeof path, "%s/cases.tsv", dir);
  f = fopen(path, "r");
  CHECK(f != NULL, "open cases.tsv");
  if (!f) return;
  while (fgets(line, sizeof line, f) && ncases < MAXCASES) {
    char *fl[CX9_MAX_FIELDS];
    tcase *c = &cases[ncases];
    if (line[0] == '#' || line[0] == '\n') continue;
    if (cx9_split_tsv(line, fl, CX9_MAX_FIELDS) != 8 || strlen(fl[0]) >= 64 || strlen(fl[1]) >= 8 ||
        strlen(fl[6]) >= 16 || strlen(fl[7]) >= 16) {
      CHECK(0, "cases.tsv row");
      continue;
    }
    strcpy(c->id, fl[0]);
    strcpy(c->kind, fl[1]);
    strcpy(c->hand[0], fl[6]);
    strcpy(c->hand[1], fl[7]);
    c->ok = hx_parse(fl[2], fl[3], fl[4], fl[5], &c->in) == 0;
    ncases++;
  }
  fclose(f);
  CHECK(ncases >= 30, "at least 30 fixture cases");
}

static void test_hand_labels(void) {
  int i, v, pos = 0, neg = 0, adv = 0, bad = 0;
  for (i = 0; i < ncases; ++i) {
    hx_result r[2];
    if (!cases[i].ok) {
      CHECK(strcmp(cases[i].hand[0], "BAD_INPUT") == 0 && strcmp(cases[i].hand[1], "BAD_INPUT") == 0, cases[i].id);
      CHECK(strcmp(cases[i].kind, "bad") == 0, "only bad rows are outside the domain");
      bad++;
      continue;
    }
    for (v = 0; v < 2; ++v) {
      r[v] = hx_build((hx_version)v, &cases[i].in);
      CHECK(strcmp(hx_framing_name(r[v].framing), cases[i].hand[v]) == 0, cases[i].id);
    }
    if (strcmp(cases[i].kind, "pos") == 0) {
      CHECK(r[0].framing == HX_CONFLICT && r[1].framing == HX_TE_ONLY, "pos: conflict fixed");
      CHECK(cases[i].in.has_te && !cases[i].in.has_cl, "pos: caller set only Transfer-Encoding");
      CHECK(value_of(&r[1], "Transfer-Encoding") && strcmp(value_of(&r[1], "Transfer-Encoding"), cases[i].in.te) == 0,
            "pos: the caller's Transfer-Encoding survives");
      pos++;
    }
    if (strcmp(cases[i].kind, "neg") == 0) {
      char a[256], b[256];
      names(&r[0], a, sizeof a);
      names(&r[1], b, sizeof b);
      CHECK(r[0].framing == r[1].framing && strcmp(a, b) == 0 && r[0].framing != HX_CONFLICT,
            "neg: identical headers, no conflict");
      neg++;
    }
    if (strcmp(cases[i].kind, "adv") == 0) adv++;
  }
  CHECK(pos >= 8 && neg >= 8 && adv >= 8 && bad >= 5, "enough positive, negative, adversarial and bad fixtures");
}

static void test_expected(const char *dir) {
  static char line[3 * CX9_FIELD_MAX];
  char path[512];
  FILE *f;
  int rows = 0, i;
  snprintf(path, sizeof path, "%s/expected.tsv", dir);
  f = fopen(path, "r");
  CHECK(f != NULL, "open expected.tsv");
  if (!f) return;
  while (fgets(line, sizeof line, f)) {
    char *fl[CX9_MAX_FIELDS];
    int n, v;
    if (strncmp(line, "id\t", 3) == 0) continue;
    n = cx9_split_tsv(line, fl, CX9_MAX_FIELDS);
    CHECK(n == 5, "expected.tsv row");
    if (n != 5) continue;
    v = strcmp(fl[1], "pre") == 0 ? 0 : strcmp(fl[1], "post") == 0 ? 1 : -1;
    CHECK(v >= 0, "known version");
    if (v < 0) continue;
    for (i = 0; i < ncases; ++i) {
      hx_result r;
      char hn[256], mm[4];
      if (strcmp(cases[i].id, fl[0]) != 0) continue;
      rows++;
      if (!cases[i].ok) {
        CHECK(strcmp(fl[2], "BAD_INPUT") == 0, fl[0]);
        continue;
      }
      r = hx_build((hx_version)v, &cases[i].in);
      names(&r, hn, sizeof hn);
      snprintf(mm, sizeof mm, "%d", r.cl_mismatch);
      CHECK(strcmp(fl[2], hx_framing_name(r.framing)) == 0 && strcmp(fl[3], hn) == 0 && strcmp(fl[4], mm) == 0, fl[0]);
    }
  }
  fclose(f);
  CHECK(rows == 2 * ncases, "every case has two rows");
}

/* Exhaustive over methods, caller headers and body kinds. */
static void test_grid(void) {
  static const char *methods[] = {"GET", "HEAD", "POST", "PUT", "PATCH", "DELETE", "OPTIONS", "QUERY"};
  static const char *tes[] = {"-", "chunked", "gzip", "gzip, chunked", "Chunked"};
  static const char *cls[] = {"-", "0", "1", "8", "100000000"};
  static const char *bodies[] = {"none", "stream", "iter_unknown", "aiter", "multipart_unknown", "bytes:0",
                                 "bytes:1", "bytes:8", "iter_known:0", "iter_known:8", "json:2", "form:7",
                                 "multipart_known:120"};
  size_t m, t, c, b;
  int conflicts_pre = 0, conflicts_post = 0, fixed = 0;
  for (m = 0; m < 8; ++m)
    for (t = 0; t < 5; ++t)
      for (c = 0; c < 5; ++c)
        for (b = 0; b < sizeof bodies / sizeof *bodies; ++b) {
          hx_input x;
          hx_result r0, r1;
          int k, default_cl, same;
          char a[256], d[256];
          CHECK(hx_parse(methods[m], tes[t], cls[c], bodies[b], &x) == 0, "grid input in domain");
          r0 = hx_build(HX_PRE, &x);
          r1 = hx_build(HX_POST, &x);
          default_cl = (x.body == HX_B_BYTES && x.n > 0) || x.body == HX_B_ITER_KNOWN || x.body == HX_B_JSON ||
                       x.body == HX_B_FORM || x.body == HX_B_MULTI_KNOWN;
          conflicts_pre += r0.framing == HX_CONFLICT;
          conflicts_post += r1.framing == HX_CONFLICT;
          CHECK((r0.framing == HX_CONFLICT) == ((x.has_te && x.has_cl) || (x.has_te && default_cl)),
                "pre conflicts exactly when TE is set and a Content-Length exists");
          CHECK((r1.framing == HX_CONFLICT) == (x.has_te && x.has_cl), "post conflicts only if the caller set both");
          names(&r0, a, sizeof a);
          names(&r1, d, sizeof d);
          same = strcmp(a, d) == 0 && r0.framing == r1.framing;
          if (x.has_te && !x.has_cl && default_cl) {
            fixed++;
            CHECK(!same && r1.framing == HX_TE_ONLY && count_name(&r1, "Content-Length") == 0, "the fix drops CL");
          } else {
            CHECK(same, "outside the fixed case both versions are identical");
          }
          for (k = 0; k < 2; ++k) {
            const hx_result *r = k ? &r1 : &r0;
            CHECK(count_name(r, "Content-Length") <= 1 && count_name(r, "Transfer-Encoding") <= 1 &&
                      count_name(r, "Host") <= 1,
                  "setdefault never duplicates a header");
            if (x.has_te) CHECK(strcmp(value_of(r, "Transfer-Encoding"), x.te) == 0, "caller TE kept verbatim");
            if (x.has_cl) CHECK(strcmp(value_of(r, "Content-Length"), x.cl) == 0, "caller CL kept verbatim");
            if (x.body == HX_B_STREAM)
              CHECK(count_name(r, "Host") == 0 && r->h.count == x.has_te + x.has_cl, "stream=: no automatic headers");
            else
              CHECK(r->h.count > 0 && strcmp(r->h.name[0], "Host") == 0, "Host comes first");
          }
        }
  CHECK(fixed == 8 * 4 * 1 * 7, "fixed: 8 methods x 4 TE values x no CL x 7 sized bodies");
  CHECK(conflicts_pre == fixed + 8 * 4 * 4 * 13 && conflicts_post == 8 * 4 * 4 * 13, "conflict counts");
}

static void test_adversarial(void) {
  hx_input x;
  hx_result r;
  CHECK(hx_parse("POST", "chunked", "-", "bytes:100000001", &x) != 0, "length cap");
  CHECK(hx_parse("POST", "chunked", "-", "bytes:", &x) != 0, "missing length");
  CHECK(hx_parse("POST", "chunked", "-", "bytes:1x", &x) != 0, "junk length");
  CHECK(hx_parse("POST", "chunked", "-", "aiter:3", &x) != 0, "length on unsized body");
  CHECK(hx_parse("POST", "", "-", "none", &x) != 0, "empty header value");
  CHECK(hx_parse("POST", "chunked\r", "-", "none", &x) != 0, "control byte");
  CHECK(hx_parse("POST", "-", "8 ", "none", &x) != 0, "trailing space");
  CHECK(hx_parse("POST", "a234567890123456789012345678901234567890123456789012345678901234", "-", "none", &x) != 0,
        "value too long");
  CHECK(hx_parse("POST", "a23456789012345678901234567890123456789012345678901234567890123", "-", "bytes:2", &x) == 0,
        "63-byte value fits");
  r = hx_build(HX_PRE, &x);
  CHECK(r.framing == HX_CONFLICT && strlen(value_of(&r, "Transfer-Encoding")) == 63, "long value kept");
  CHECK(hx_parse("POST", "chunked", "-", "multipart_known:100000000", &x) == 0, "large multipart");
  r = hx_build(HX_PRE, &x);
  CHECK(strcmp(value_of(&r, "Content-Length"), "100000000") == 0 && r.h.count == 4, "pre: four headers");
  r = hx_build(HX_POST, &x);
  CHECK(value_of(&r, "Content-Length") == NULL && r.h.count == 3, "post: three headers");
}

/* PRACTICE.md levels. */
static void test_practice(void) {
  hx_input x;
  CHECK(hx_parse("POST", "-", "-", "bytes:8", &x) == 0 && hx_build(HX_PRE, &x).framing == HX_CL_ONLY,
        "practice_level_1");
  CHECK(hx_parse("POST", "chunked", "-", "bytes:8", &x) == 0 && hx_build(HX_PRE, &x).framing == HX_CONFLICT,
        "practice_level_2");
  CHECK(hx_build(HX_POST, &x).framing == HX_TE_ONLY, "practice_level_3");
  CHECK(hx_parse("POST", "-", "5", "iter_unknown", &x) == 0 && hx_build(HX_PRE, &x).framing == HX_CL_ONLY,
        "practice_level_4");
  CHECK(hx_parse("POST", "chunked", "8", "bytes:8", &x) == 0 && hx_build(HX_POST, &x).framing == HX_CONFLICT,
        "practice_level_5");
  CHECK(hx_parse("POST", "chunked", "-", "stream", &x) == 0 && hx_build(HX_POST, &x).h.count == 1,
        "practice_level_6");
}

int main(int argc, char **argv) {
  const char *dir = argc > 1 ? argv[1] : "fixtures";
  load_cases(dir);
  test_hand_labels();
  test_expected(dir);
  test_grid();
  test_adversarial();
  test_practice();
  printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
