# Guía práctica (6 niveles)

Ejercicios **offline y locales** sobre cómo un cliente HTTP arma los
encabezados de framing. No se abre ningún socket ni se usa httpx2. Cada
respuesta está comprobada en `tests/test_model.c` (`practice_level_N`),
compilado con ASan/UBSan. Si querés ver bytes reales en la red, hacelo solo
en tu propio lab: un servidor de juguete tuyo en `127.0.0.1` o en una VM
descartable, nunca contra servicios o proxies de terceros.

Comando: `./build/hx2 case <MÉTODO> <te|-> <cl|-> <cuerpo>` (después de
`sh scripts/build.sh results /tmp/r`).

**Básico**

1. `POST - - bytes:8`: ¿qué framing sale en `pre`? (respuesta: `CL_ONLY`,
   `Content-Length: 8` viene de `encode_content`)
2. `POST chunked - bytes:8`: ¿y en `pre`? (respuesta: `CONFLICT`;
   `setdefault` agrega `Content-Length` porque solo mira ese nombre; es el
   ejemplo del GHSA)

**Intermedio**

3. Mismo caso en `post`. (respuesta: `TE_ONLY`; las líneas nuevas 455-457
   de `_models.py` omiten el `Content-Length` por defecto)
4. `POST - 5 iter_unknown`: ¿había conflicto en `pre`? (respuesta: no,
   `CL_ONLY`; la dirección inversa ya estaba cubierta por 455-457 de `pre`,
   como dice el GHSA)

**Avanzado**

5. `POST chunked 8 bytes:8` en `post`. (respuesta: sigue `CONFLICT`; si
   quien llama pone los dos encabezados, httpx2 no los toca)
6. `POST chunked - stream`: ¿cuántos encabezados quedan en `post`?
   (respuesta: 1; con `stream=` no se llama a `_prepare` y ni siquiera se
   agrega `Host`)

Más allá: agregá un caso a `fixtures/cases.tsv` con su etiqueta a mano,
regenerá `fixtures/expected.tsv` con `scripts/derive_expected.py` y
comprobá que el modelo coincide. En tu propio código: cuando completás
encabezados por defecto, ¿mirás solo el mismo nombre o también los que son
mutuamente excluyentes? ¿Tus tests cubren el caso en que quien llama ya
eligió el framing?
