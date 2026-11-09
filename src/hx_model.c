#include "hx_model.h"
#include <stdio.h>
#include <string.h>

static int lower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

static int eq_ic(const char *a, const char *b) {
  for (; *a && *b; ++a, ++b)
    if (lower((unsigned char)*a) != lower((unsigned char)*b)) return 0;
  return *a == *b;
}

/* Headers.__contains__: case-insensitive name lookup (_models.py 360-362). */
static int has(const hx_headers *h, const char *name) {
  int i;
  for (i = 0; i < h->count; ++i)
    if (eq_ic(h->name[i], name)) return 1;
  return 0;
}

static const char *get(const hx_headers *h, const char *name) {
  int i;
  for (i = 0; i < h->count; ++i)
    if (eq_ic(h->name[i], name)) return h->value[i];
  return NULL;
}

static void append(hx_headers *h, const char *name, const char *value) {
  if (h->count >= HX_MAXH) return; /* cannot happen: at most 6 headers */
  snprintf(h->name[h->count], sizeof h->name[0], "%s", name);
  snprintf(h->value[h->count], sizeof h->value[0], "%s", value);
  h->count++;
}

static void prepend(hx_headers *h, const char *name, const char *value) {
  int i;
  if (h->count >= HX_MAXH) return;
  for (i = h->count; i > 0; --i) {
    memcpy(h->name[i], h->name[i - 1], sizeof h->name[0]);
    memcpy(h->value[i], h->value[i - 1], sizeof h->value[0]);
  }
  h->count++;
  snprintf(h->name[0], sizeof h->name[0], "%s", name);
  snprintf(h->value[0], sizeof h->value[0], "%s", value);
}

static const char *KINDS[] = {"none", "bytes", "iter_known", "iter_unknown", "aiter",
                              "json", "form", "multipart_known", "multipart_unknown", "stream"};

static int needs_n(hx_body b) {
  return b == HX_B_BYTES || b == HX_B_ITER_KNOWN || b == HX_B_JSON || b == HX_B_FORM ||
         b == HX_B_MULTI_KNOWN;
}

static int value_ok(const char *s) {
  size_t n = strlen(s), i;
  if (n == 0 || n >= HX_MAXV || s[0] == ' ' || s[n - 1] == ' ') return 0;
  for (i = 0; i < n; ++i)
    if ((unsigned char)s[i] < 0x20 || (unsigned char)s[i] > 0x7e) return 0;
  return 1;
}

int hx_parse(const char *method, const char *te, const char *cl, const char *body, hx_input *in) {
  static const char *methods[] = {"GET", "HEAD", "POST", "PUT", "PATCH", "DELETE", "OPTIONS", "QUERY"};
  size_t i, klen;
  const char *colon;
  int found = 0;
  memset(in, 0, sizeof *in);
  for (i = 0; i < sizeof methods / sizeof *methods; ++i)
    if (strcmp(method, methods[i]) == 0) found = 1;
  if (!found) return -1;
  strcpy(in->method, method);
  if (strcmp(te, "-") != 0) {
    if (!value_ok(te)) return -1;
    in->has_te = 1;
    strcpy(in->te, te);
  }
  if (strcmp(cl, "-") != 0) {
    if (!value_ok(cl)) return -1;
    in->has_cl = 1;
    strcpy(in->cl, cl);
  }
  colon = strchr(body, ':');
  klen = colon ? (size_t)(colon - body) : strlen(body);
  for (i = 0; i < sizeof KINDS / sizeof *KINDS; ++i)
    if (strlen(KINDS[i]) == klen && strncmp(body, KINDS[i], klen) == 0) break;
  if (i == sizeof KINDS / sizeof *KINDS) return -1;
  in->body = (hx_body)i;
  if (needs_n(in->body)) {
    const char *p;
    unsigned long v = 0;
    if (!colon || !colon[1]) return -1;
    for (p = colon + 1; *p; ++p) {
      if (*p < '0' || *p > '9' || v > 100000000UL) return -1;
      v = v * 10 + (unsigned long)(*p - '0');
    }
    if (v > 100000000UL) return -1;
    /* json and form bodies are never empty here; form data={} sends nothing */
    if ((in->body == HX_B_JSON || in->body == HX_B_FORM || in->body == HX_B_MULTI_KNOWN) && v == 0) return -1;
    in->n = v;
  } else if (colon) {
    return -1;
  }
  return 0;
}

/* The default headers encode_request() returns, in dict order. */
static void defaults(const hx_input *in, hx_headers *d) {
  char n[24];
  memset(d, 0, sizeof *d);
  snprintf(n, sizeof n, "%lu", in->n);
  switch (in->body) {
  case HX_B_NONE: case HX_B_STREAM: break;
  case HX_B_BYTES: if (in->n > 0) append(d, "Content-Length", n); break; /* 105-109 */
  case HX_B_ITER_KNOWN: append(d, "Content-Length", n); break;          /* 120-121 */
  case HX_B_ITER_UNKNOWN: case HX_B_AITER: append(d, "Transfer-Encoding", "chunked"); break;
  case HX_B_JSON:
    append(d, "Content-Length", n);
    append(d, "Content-Type", "application/json");
    break;
  case HX_B_FORM:
    append(d, "Content-Length", n);
    append(d, "Content-Type", "application/x-www-form-urlencoded");
    break;
  case HX_B_MULTI_KNOWN:
    append(d, "Content-Length", n);
    append(d, "Content-Type", "multipart/form-data; boundary=B");
    break;
  case HX_B_MULTI_UNKNOWN:
    append(d, "Transfer-Encoding", "chunked");
    append(d, "Content-Type", "multipart/form-data; boundary=B");
    break;
  }
}

hx_result hx_build(hx_version v, const hx_input *in) {
  hx_result r;
  hx_headers d;
  int i;
  const char *cl;
  memset(&r, 0, sizeof r);
  /* Headers(headers): the caller's headers, in this order */
  if (in->has_te) append(&r.h, "Transfer-Encoding", in->te);
  if (in->has_cl) append(&r.h, "Content-Length", in->cl);
  if (in->body != HX_B_STREAM) {
    defaults(in, &d);
    for (i = 0; i < d.count; ++i) {
      /* post 455-457: ignore Content-Length if Transfer-Encoding is set */
      if (v == HX_POST && eq_ic(d.name[i], "content-length") && has(&r.h, "Transfer-Encoding")) continue;
      /* pre 455-457 / post 458-460: ignore Transfer-Encoding if Content-Length is set */
      if (eq_ic(d.name[i], "transfer-encoding") && has(&r.h, "Content-Length")) continue;
      if (!has(&r.h, d.name[i])) append(&r.h, d.name[i], d.value[i]); /* setdefault */
    }
    {
      int has_host = has(&r.h, "Host");
      int has_len = has(&r.h, "Content-Length") || has(&r.h, "Transfer-Encoding");
      int body_method = strcmp(in->method, "POST") == 0 || strcmp(in->method, "PUT") == 0 ||
                        strcmp(in->method, "PATCH") == 0 || strcmp(in->method, "QUERY") == 0;
      /* auto_headers + self.headers.raw: Host first, then Content-Length: 0 */
      if (!has_len && body_method) prepend(&r.h, "Content-Length", "0");
      if (!has_host) prepend(&r.h, "Host", "example.org");
    }
  }
  r.framing = has(&r.h, "Content-Length")
                  ? (has(&r.h, "Transfer-Encoding") ? HX_CONFLICT : HX_CL_ONLY)
                  : (has(&r.h, "Transfer-Encoding") ? HX_TE_ONLY : HX_NONE);
  cl = get(&r.h, "Content-Length");
  if (cl && (needs_n(in->body) || in->body == HX_B_NONE)) {
    char n[24];
    snprintf(n, sizeof n, "%lu", in->body == HX_B_NONE ? 0UL : in->n);
    r.cl_mismatch = strcmp(cl, n) != 0;
  }
  return r;
}

const char *hx_framing_name(hx_framing f) {
  static const char *n[] = {"NONE", "CL_ONLY", "TE_ONLY", "CONFLICT"};
  return n[f];
}
const char *hx_version_name(hx_version v) { return v == HX_PRE ? "pre" : "post"; }
