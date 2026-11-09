# Sources

Local copies under `security-research-45/sources/cve2026/` (not in this
repo), fetched read-only (GitHub API GET, raw.githubusercontent.com); the
SHA-256 of each copy is listed in that directory's `SHA256SUMS` and was
re-checked on 2026-11-09.

| Source | URL | Used for | SHA-256 of copy |
|---|---|---|---|
| Commit 829b93a2393212996f613e635261f777d9ec6eab diff ("Respect explicit Transfer-Encoding headers (#1137)", 2026-08-13 12:21 ART, 4 files +31/-1, parent 4fa6c8e) | https://github.com/pydantic/httpx2/commit/829b93a2393212996f613e635261f777d9ec6eab | `_models.py` new 455-457, `wsgi.py` new 110-111 (not modelled), upstream tests | 86985e84d8718fee589adfd34d4ac97623787ad08739522d033ee7bc19d1a045 |
| `src/httpx2/httpx2/_models.py` at 4fa6c8ee96fb79035c6820e4f44a10b6262d9c9f (blob 98bf0a2c) | https://github.com/pydantic/httpx2/blob/4fa6c8ee96fb79035c6820e4f44a10b6262d9c9f/src/httpx2/httpx2/_models.py | `pre`: `Request.__init__` 412-450, `_prepare` 453-470, `Headers.__contains__` 360-362 | 4ac1815220d01c87b5e2f8cbd5e0d25bb1cb0b282ce9533fa5e3fda5ff71e993 |
| same file at 829b93a (blob 9fb02337) | https://github.com/pydantic/httpx2/blob/829b93a2393212996f613e635261f777d9ec6eab/src/httpx2/httpx2/_models.py | `post`: `_prepare` 453-473 | 08dc029807c3cbdccd410608d023a6ddfd2e1d9959e46db2347e6906ad9265de |
| `src/httpx2/httpx2/_content.py` at 4fa6c8e (blob c75b6121, same at 829b93a) | https://github.com/pydantic/httpx2/blob/4fa6c8ee96fb79035c6820e4f44a10b6262d9c9f/src/httpx2/httpx2/_content.py | `encode_content` 102-128, `encode_urlencoded_data` 131-142, `encode_json` 169-174, `encode_request` 177-209 | 83c9948d4aba162daa236b749b87f71f2a7e0f32344b8156ecd8d6425967d7bc |
| `src/httpx2/httpx2/_multipart.py` at 4fa6c8e (blob 1a9beffb, same at 829b93a), fetched for this repo | https://github.com/pydantic/httpx2/blob/4fa6c8ee96fb79035c6820e4f44a10b6262d9c9f/src/httpx2/httpx2/_multipart.py | `MultipartStream.get_headers` 266-271 | c62a8146cf90531d9d76f4c035d54e316327b92066784b28fe43bea6e3f9e499 |
| GHSA-pf96-p4fj-6566 (published 2026-09-08, `< 2.11.0`, patched 2.11.0, CWE-444, CVSS 5.6) | https://github.com/pydantic/httpx2/security/advisories/GHSA-pf96-p4fj-6566 | summary, example request, affected body kinds, "streaming bodies with an explicit Content-Length are not affected" | b2e60909dc8c5783c40909591b69d241409b42527da9a41bf9d311c791096df7 |
| NVD CVE-2026-84380 (Awaiting Analysis) | https://nvd.nist.gov/vuln/detail/CVE-2026-84380 | references 829b93a, PR 1137, v2.11.0 | 0551953070b7a8e361d00444af962818950af7064fcebc022146e828ba1b526c |
| cve.org CVE-2026-84380 (PUBLISHED 2026-09-02, CNA GitHub_M) | https://www.cve.org/CVERecord?id=CVE-2026-84380 | affected `< 2.11.0`, same references | d498ca3074f1044c322557d650b04dbbc37617b59aa187101e8747ac3b651817 |
| Tag containment (compare API) and `_prepare` body hashes | `tags_CVE-2026-84380.txt` | 829b93a in v2.11.0, not in v2.10.0; `_prepare` at v2.10.0 = 4fa6c8e, at v2.11.0 = 829b93a | 6b9e238e9c12fd8fa90ca86b9833222bdba3031dabae474bae4b96842a543273 |

No source contained instructions addressed to a tool or agent. People named
in sources were not copied.
