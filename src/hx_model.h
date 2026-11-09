/* Model of the request headers HTTPX2 builds (CVE-2026-84380).
 *   src/httpx2/httpx2/_content.py (same blob c75b6121 at both commits):
 *     encode_content 102-128, encode_urlencoded_data 131-142, encode_json
 *     169-174, encode_request 177-209; _multipart.py get_headers 266-271
 *   src/httpx2/httpx2/_models.py Request.__init__ 412-450 (stream= skips _prepare) and _prepare():
 *     pre  4fa6c8e (= v2.10.0) lines 453-470: setdefault() per default
 *          header; Transfer-Encoding skipped only if Content-Length is set
 *     post 829b93a (= v2.11.0) lines 453-473: also skip the default
 *          Content-Length when Transfer-Encoding is set (new 455-457)
 * Nothing is serialized or sent: the output is the header list. */
#ifndef HX_MODEL_H
#define HX_MODEL_H

typedef enum { HX_PRE, HX_POST } hx_version;
typedef enum { HX_NONE, HX_CL_ONLY, HX_TE_ONLY, HX_CONFLICT } hx_framing;

typedef enum {
  HX_B_NONE,          /* no content/data/files/json */
  HX_B_BYTES,         /* content=bytes or str of n bytes */
  HX_B_ITER_KNOWN,    /* content=file-like iterable, peek_filelike_length() = n */
  HX_B_ITER_UNKNOWN,  /* content=iterable of unknown length */
  HX_B_AITER,         /* content=async iterable */
  HX_B_JSON,          /* json=..., n bytes once encoded */
  HX_B_FORM,          /* data={...} urlencoded, n bytes */
  HX_B_MULTI_KNOWN,   /* files=..., known length n */
  HX_B_MULTI_UNKNOWN, /* files=..., unknown length */
  HX_B_STREAM         /* stream=...: no automatic headers at all */
} hx_body;

#define HX_MAXV 64
#define HX_MAXH 8

typedef struct {
  char method[8];
  int has_te, has_cl;           /* caller-supplied headers */
  char te[HX_MAXV], cl[HX_MAXV];
  hx_body body;
  unsigned long n;              /* body length where it applies */
} hx_input;

typedef struct {
  int count;
  char name[HX_MAXH][24];
  char value[HX_MAXH][HX_MAXV];
} hx_headers;

typedef struct {
  hx_framing framing;
  hx_headers h;
  int cl_mismatch;              /* Content-Length differs from a known body length */
} hx_result;

/* Parse method, "-"/value for TE and CL, and "kind" or "kind:n". */
int hx_parse(const char *method, const char *te, const char *cl, const char *body, hx_input *in);
hx_result hx_build(hx_version v, const hx_input *in);

const char *hx_framing_name(hx_framing f);
const char *hx_version_name(hx_version v);
#endif
