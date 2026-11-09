# Limitations

- **A model, not a finding.** httpx2 was not installed, imported or run;
  no request was sent and no proxy or server was tested. The C model and
  the Python derivation were both written by reading the files listed in
  SOURCES.md.
- **Header list only.** The output is the list `Request.headers` would hold.
  How httpcore/h11 then serializes the request on HTTP/1.1 (the GHSA says
  the body goes out chunked while both headers are sent) and how a proxy or
  server resolves the conflict (RFC 9112 6.3 says `Transfer-Encoding`
  overrides `Content-Length`; some intermediaries disagree) are **not
  modelled**. The impact claim is the GHSA's, not this repo's.
- **The `wsgi.py` part of 829b93a is not modelled.** The fix also makes
  `WSGITransport` set `CONTENT_LENGTH` from `len(request.content)` when the
  request has content and no `Content-Length` header (new lines 110-111),
  so a WSGI app can still read a body that now arrives with only
  `Transfer-Encoding`. It is a consequence of the fix, not part of the
  framing decision.
- **Caller-set conflicts stay.** If the caller sets both headers, both
  versions send both (220 of 495 sweep rows). The fix only stops httpx2
  from *adding* the conflicting header; this is by design, not a gap
  found here.
- **`peek_filelike_length()` is an input.** Its code (`_utils.py`) was not
  fetched; the model takes "known length n" or "unknown" as given.
- **Simplified domain.** Methods must be one of 8 upper-case names
  (`Request` upper-cases any string at `_models.py` 412); header names are
  canonical (lookups are case-insensitive, so spelling does not change the
  result); values are printable ASCII up to 63 bytes without outer spaces;
  `encode_text`/`encode_html` (`_content.py` 153-166) are not called by
  `encode_request` and are not modelled; when several of `content=`,
  `files=`, `data=`, `json=` are given, `encode_request` uses the first in
  that order (200-207) and the model takes only that one; non-mapping
  `data=` (deprecated, routed to `encode_content`, 188-198) is treated as
  `content=`; `Content-Type` values are placeholders. Cookies, `Host` set by
  the caller and URLs without a host are out of scope (`Host` is always
  added with a placeholder value).
- **Releases.** Only the `_prepare` function was compared at the tags
  v2.10.0 and v2.11.0 (body hashes equal to 4fa6c8e and 829b93a); other
  files at those tags were not compared. Releases on PyPI were not
  checked.
- **Known-length file-like iterables** also reach the bug in the model
  (`iter_known`); the GHSA lists only bytes, JSON, form and known-length
  multipart. This follows from `_content.py` 117-121; it is an addition,
  not a contradiction.
