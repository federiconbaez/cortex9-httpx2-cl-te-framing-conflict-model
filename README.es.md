# cortex9-httpx2-cl-te-framing-conflict-model

Modelo offline de **CVE-2026-84380** (GHSA-pf96-p4fj-6566, CWE-444,
CVSS 3.1 5.6): HTTPX2 `< 2.11.0` podía agregar un `Content-Length`
automático a una request en la que quien llama ya había puesto
`Transfer-Encoding`, y la request salía con los dos encabezados de
framing. `Request._prepare()` aplicaba cada encabezado por defecto con
`setdefault()`, que solo mira el mismo nombre. El fix, commit 829b93a de
pydantic/httpx2 ("Respect explicit Transfer-Encoding headers (#1137)",
primero en v2.11.0), omite el `Content-Length` por defecto si ya hay
`Transfer-Encoding`.
Educativo, determinista, C11 revisado con ASan/UBSan. **No se abre ningún
socket y httpx2 nunca se importa ni se ejecuta.**

English: [README.md](README.md). Ejercicios: [PRACTICE.md](PRACTICE.md).

## Qué se modela (atado al código)

| Versión | Código | Regla por encabezado por defecto |
|---|---|---|
| `pre` 4fa6c8e (= `_prepare` de v2.10.0) | `_models.py` 453-470 | omite `Transfer-Encoding` si hay `Content-Length` (455-457); si no, `setdefault` (458) |
| `post` 829b93a (= `_prepare` de v2.11.0) | `_models.py` 453-473 | **nuevas 455-457**: omite `Content-Length` si hay `Transfer-Encoding`; después igual que antes (458-461) |

Los encabezados por defecto salen de `encode_request()` (`_content.py`
177-209): bytes/str dan `Content-Length` solo si no están vacíos
(105-109); un iterable tipo archivo da `Content-Length` si
`peek_filelike_length()` conoce el largo, si no `chunked` (111-122); los
iterables async dan `chunked` (124-126); form (131-142) y JSON (169-174)
dan `Content-Length` y `Content-Type`; multipart (`_multipart.py` 266-271)
da `Content-Length` o `chunked`, más `Content-Type`. Después se anteponen
`Host` y, para POST/PUT/PATCH/QUERY sin ningún encabezado de framing,
`Content-Length: 0` (`_models.py` 460-470 en `pre`, 463-473 en `post`). `Request(stream=...)` saltea
todo esto (436-450). Los nombres no distinguen mayúsculas.

Salida por request: los nombres en orden, el framing (`NONE`, `CL_ONLY`,
`TE_ONLY`, `CONFLICT` si están los dos) y si `Content-Length` difiere de un
largo de cuerpo conocido.

## Evidencia diferencial (`results/`)

`results/sweep.csv`: 495 requests (5 métodos x 3 TE x 3 CL de quien llama x
11 cuerpos). `CONFLICT`: `pre` 270, `post` 220. Las 50 que cambian son
exactamente "TE puesto por quien llama, sin CL propio, cuerpo de largo
conocido distinto de cero"; ahí `post` quita `Content-Length`. Las 220 que
quedan son requests donde quien llama puso ambos encabezados: el fix no las
toca (y el GHSA no dice que lo haga). Todo lo demás es idéntico. La grilla
de `tests/test_model.c` (2600 requests, 8 métodos, 5 grafías de TE)
comprueba los mismos invariantes.

## Compilar y comprobar

```sh
sh scripts/build.sh test
python3 scripts/derive_expected.py | diff - fixtures/expected.tsv
sh scripts/build.sh results /tmp/r && diff -r results /tmp/r
./build/hx2 case POST chunked - bytes:8
```

Fuentes: [SOURCES.md](SOURCES.md). Límites: [LIMITATIONS.md](LIMITATIONS.md).
Licencia: Apache-2.0.
