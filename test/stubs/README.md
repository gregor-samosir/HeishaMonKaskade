# Arduino-Ersatzheader fuer die Hosttests

Damit `src/decode.cpp` und `src/Topics.cpp` auf dem Mac (bzw. in der CI)
uebersetzt werden koennen, ohne die Arduino-Toolchain zu installieren. Es steht
nur so viel darin, wie der Dekodierpfad wirklich anfasst.

Benutzt von:

* `test/byte110_test.cpp` (`-I test/stubs`)
* `test/decode_vergleich.py` (kopiert die Dateien in sein Arbeitsverzeichnis)

Beide teilen sich diese eine Fassung, damit die Nachbildung nicht in zwei
Varianten auseinanderlaeuft. Bis 3.6.1 standen die Header als Zeichenketten in
`decode_vergleich.py`.
