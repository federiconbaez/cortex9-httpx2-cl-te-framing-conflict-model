# cortex9-httpx2-cl-te-framing-conflict-model

Offline model of **CVE-2026-84380** (GHSA-pf96-p4fj-6566, CWE-444,
CVSS 3.1 5.6): HTTPX2 `< 2.11.0` could add an automatic `Content-Length` to
a request whose caller already set `Transfer-Encoding`, so the request
carried both framing headers. `Request._prepare()` applied each default
header with `setdefault()`, which only looks at the same header name. The
fix, pydantic/httpx2 commit 829b93a ("Respect explicit Transfer-Encoding
headers (#1137)", first in v2.11.0), skips the default `Content-Length`
when `Transfer-Encoding` is present.
Educational, deterministic, C11 checked with ASan/UBSan. **No socket is
opened and httpx2 is never imported or run.**

Spanish: [README.es.md](README.es.md). Exercises: [PRACTICE.md](PRACTICE.md).

## What is modelled (tied to the code)

| Version | Code | Rule for each default header |
|---|---|---|
| `pre` 4fa6c8e (= v2.10.0 `_prepare`) | `_models.py` 453-470 | skip `Transfer-Encoding` if `Content-Length` is set (455-457), else `setdefault` (458) |
| `post` 829b93a (= v2.11.0 `_prepare`) | `_models.py` 453-473 | **new 455-457**: skip `Content-Length` if `Transfer-Encoding` is set; then as before (458-461) |

Default headers come from `encode_request()` (`_content.py` 177-209):
bytes/str give `Content-Length` only when non-empty (105-109); a file-like
iterable gives `Content-Length` if `peek_filelike_length()` knows it, else
`Transfer-Encoding: chunked` (111-122); async iterables give chunked
(124-126); form (131-142) and JSON (169-174) give `Content-Length` plus
`Content-Type`; multipart (`_multipart.py` 266-271) gives
`Content-Length` or chunked, plus `Content-Type`. Then `Host` and, for
POST/PUT/PATCH/QUERY with neither framing header, `Content-Length: 0` are
put first (`_models.py` 460-470 in `pre`, 463-473 in `post`). `Request(stream=...)` skips all of this
(436-450). Header names are case-insensitive (`Headers.__contains__`).

Output per request: the ordered header names, the framing (`NONE`,
`CL_ONLY`, `TE_ONLY`, `CONFLICT` when both headers are present) and whether
`Content-Length` differs from a known body length.

## Differential evidence (`results/`)

`results/sweep.csv`: 495 requests (5 methods x 3 caller TE x 3 caller CL x
11 bodies). `CONFLICT`: `pre` 270, `post` 220. The 50 requests that change
are exactly "caller set `Transfer-Encoding`, no caller `Content-Length`,
body of known non-zero length"; there `post` drops `Content-Length`. The
220 left are requests where the caller set both headers: the fix does not
touch them (and the GHSA does not claim it does). Every other request is
identical in both versions. The wider grid in `tests/test_model.c` (2,600
requests, 8 methods, 5 TE spellings) checks the same invariants.

## Build and check

```sh
sh scripts/build.sh test
python3 scripts/derive_expected.py | diff - fixtures/expected.tsv
sh scripts/build.sh results /tmp/r && diff -r results /tmp/r
./build/hx2 case POST chunked - bytes:8
```

Sources: [SOURCES.md](SOURCES.md). Limits: [LIMITATIONS.md](LIMITATIONS.md).
License: Apache-2.0.
