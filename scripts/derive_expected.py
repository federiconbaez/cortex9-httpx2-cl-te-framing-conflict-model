#!/usr/bin/env python3
"""Independent derivation of fixtures/expected.tsv.

Written from httpx2's Request.__init__ and Request._prepare()
(src/httpx2/httpx2/_models.py at 4fa6c8e and 829b93a) and from
encode_request()/encode_content() in _content.py and get_headers() in
_multipart.py, not from the C model. It mirrors the Python code: a
case-insensitive header list, setdefault(), and auto_headers + raw.
httpx2 itself is not imported.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from cx9_util import read_cases  # noqa: E402

METHODS = ("GET", "HEAD", "POST", "PUT", "PATCH", "DELETE", "OPTIONS", "QUERY")
SIZED = ("bytes", "iter_known", "json", "form", "multipart_known")
UNSIZED = ("none", "stream", "iter_unknown", "aiter", "multipart_unknown")


class Headers:
    def __init__(self, items=()):
        self.raw = list(items)

    def __contains__(self, key):
        return any(k.lower() == key.lower() for k, _ in self.raw)

    def get(self, key):
        for k, v in self.raw:
            if k.lower() == key.lower():
                return v
        return None

    def setdefault(self, key, value):
        if key not in self:
            self.raw.append((key, value))


def encode_request(kind, n):
    if kind == "bytes":
        return {"Content-Length": str(n)} if n else {}
    if kind == "iter_known":
        return {"Content-Length": str(n)}
    if kind in ("iter_unknown", "aiter"):
        return {"Transfer-Encoding": "chunked"}
    if kind == "json":
        return {"Content-Length": str(n), "Content-Type": "application/json"}
    if kind == "form":
        return {"Content-Length": str(n), "Content-Type": "application/x-www-form-urlencoded"}
    if kind == "multipart_known":
        return {"Content-Length": str(n), "Content-Type": "multipart/form-data; boundary=B"}
    if kind == "multipart_unknown":
        return {"Transfer-Encoding": "chunked", "Content-Type": "multipart/form-data; boundary=B"}
    return {}


def prepare(version, method, headers, default_headers):
    for key, value in default_headers.items():
        if version == "post" and key.lower() == "content-length" and "Transfer-Encoding" in headers:
            continue
        if key.lower() == "transfer-encoding" and "Content-Length" in headers:
            continue
        headers.setdefault(key, value)
    auto = []
    if "Host" not in headers:
        auto.append(("Host", "example.org"))
    if not ("Content-Length" in headers or "Transfer-Encoding" in headers) and method in ("POST", "PUT", "PATCH", "QUERY"):
        auto.append(("Content-Length", "0"))
    return Headers(auto + headers.raw)


def clean(v):
    return v and v == v.strip() and all(0x20 <= ord(c) <= 0x7e for c in v) and len(v) < 64


def parse(row):
    method, te, cl, body = row[2], row[3], row[4], row[5]
    if method not in METHODS:
        return None
    for v in (te, cl):
        if v != "-" and not clean(v):
            return None
    kind, sep, num = body.partition(":")
    if kind in UNSIZED and not sep:
        n = 0
    elif kind in SIZED and sep and num.isdigit() and num.isascii():
        n = int(num)
        if n > 100000000 or (n == 0 and kind in ("json", "form", "multipart_known")):
            return None
    else:
        return None
    return method, te, cl, kind, n


def build(version, sc):
    method, te, cl, kind, n = sc
    user = []
    if te != "-":
        user.append(("Transfer-Encoding", te))
    if cl != "-":
        user.append(("Content-Length", cl))
    headers = Headers(user)
    if kind != "stream":
        headers = prepare(version, method, headers, encode_request(kind, n))
    has_cl, has_te = "Content-Length" in headers, "Transfer-Encoding" in headers
    framing = {(1, 1): "CONFLICT", (1, 0): "CL_ONLY", (0, 1): "TE_ONLY", (0, 0): "NONE"}[(has_cl, has_te)]
    mismatch = 0
    if has_cl and (kind in SIZED or kind == "none"):
        mismatch = int(headers.get("Content-Length") != str(n))
    return framing, "|".join(k for k, _ in headers.raw), mismatch


def main():
    root = Path(__file__).resolve().parent.parent
    print("id\tversion\tframing\theaders\tcl_mismatch")
    for row in read_cases(root / "fixtures" / "cases.tsv"):
        sc = parse(row)
        for v in ("pre", "post"):
            if sc is None:
                print(f"{row[0]}\t{v}\tBAD_INPUT\t-\t-")
                continue
            print("\t".join([row[0], v] + [str(x) for x in build(v, sc)]))


if __name__ == "__main__":
    main()
