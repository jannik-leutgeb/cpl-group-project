# GWBasic → C Compiler (JavaCC)

GWBasic-Compiler auf Basis von JavaCC: übersetzt BASIC nach C (Stack-Maschine),
der C-Code wird mit gcc zu einem ausführbaren Programm kompiliert.

## Bauen und ausführen

    mvn clean compile
    java -cp target/classes:src/main/resources de.hft_stuttgart.cpl.GWBasic TEST.BAS > out.c
    gcc -w -o test out.c -lm
    ./test

Oder alles auf einmal (baut + läuft TEST.BAS und alle Tests):

    ./run_tests.sh
    ./run_tests.sh tests/types/t01_float_vs_double.bas   # einzelne Datei

Wichtig: `-lm` beim gcc (Runtime nutzt math.h). Das pom.xml verlangt
Java 24 — bei älterem JDK `maven.compiler.source/target` im pom anpassen.

## Typsystem

Der Typ einer Variable folgt aus ihrem Namens-Suffix:

| Suffix | Typ | Verhalten beim Speichern |
|--------|--------|--------------------------|
| (keins) | float | auf single precision (~7 signifikante Stellen) reduziert — der DEFAULT |
| `#` | double | volle Präzision (~16 Stellen) |
| `%` | int | kaufmännisch gerundet (3.5 → 4, 4.5 → 5) |
| `$` | string | Zeichenkette |

Typprüfung erfolgt zur Laufzeit: String in numerische Variable (oder
umgekehrt) gibt eine runtime warning und einen Default-Wert, niemals
einen C-Compiler-Fehler.

## Sprachumfang

PRINT, LET (skalare und Array-Ziele), WHILE/WEND, FOR/NEXT (mit
optionalem STEP, auch negativ und float), GOTO, GOSUB/RETURN
(Adress-Stack, GCC computed goto), DIM (eindimensionale Arrays,
Indizes 0..n inklusiv), Vergleichsoperatoren `= <> < <= > >=`,
String-Konkatenation mit `+`, Float-Literale (`3.14`, `.5`).

## Fehlerbehandlung (Vorgabe der Aufgabe)

GOTO/GOSUB auf nicht existierende Zeilennummer → Compiler-Fehler mit
Exit-Code 1 (gcc wird nie aufgerufen). Lesen einer nie zugewiesenen
Variable → runtime warning, weiter mit 0 bzw. "". Typ-Mismatch →
runtime warning. Division durch null, RETURN ohne GOSUB,
Array-Index außerhalb → runtime error.

## Tests

`tests/` enthält Feature- und Fehlerfall-Tests, `tests/types/` die
Typsystem-Tests. Erwartete Ausgaben:

| Test | Ausgabe |
|------|---------|
| TEST.BAS | 51 50 … 1 |
| tests/array.bas | 30 |
| tests/array_loop.bas | 0 1 4 9 16 |
| tests/for_up.bas | 1 2 3 4 5 |
| tests/for_down.bas | 10 8 6 4 2 |
| tests/floatstep.bas | 1 1.5 2 |
| tests/nested.bas | 11 12 21 22 |
| tests/goto_ok.bas | 1 2 |
| tests/gosub.bas | 5 9 0 |
| tests/strcat.bas | FooBar |
| tests/strsuffix.bas | Hi there |
| tests/goto_bad.bas | Compiler-Fehler: GOTO 999 — target line number does not exist |
| tests/gosub_bad.bas | Compiler-Fehler: GOSUB 999 — target line number does not exist |
| tests/ret_bad.bas | runtime error: RETURN without GOSUB |
| tests/uninit.bas | 0 + warning (used before assigned) |
| tests/typemix.bas | 0 + warning (string into numeric) |
| tests/types/t01_float_vs_double.bas | 0.3333333 / 0.3333333333333333 |
| tests/types/t02_int_rounding.bas | 3 4 4 5 |
| tests/types/t03_precision_loss.bas | 0.6666667 / 0.6666666666666666 |
| tests/types/t04_pi_three_types.bas | 3.141593 / 3.14159265358979 / 3 |
| tests/types/t05_whole_numbers.bas | 42 42 42 |
| tests/types/t06_type_mismatch.bas | 0 + drei warnings (A, B#, C%) |
| tests/types/t07_num_into_string.bas | Leerzeile + warning |
| tests/types/t08_default_is_float.bas | 1.428571 |
| tests/types/t09_float_loop.bas | 0 0.25 0.5 0.75 1 |
| tests/types/t10_mixed_arith.bas | 1 |

## Bekannte Einschränkungen

Kein unäres Minus in Ausdrücken (`LET A=-2.5` schlägt fehl; Workaround
`LET A=0-2.5`; STEP -2 funktioniert, da gesondert geparst). DIM nur
eindimensional. LET-Keyword ist Pflicht. GOSUB/RETURN nutzt eine
GCC-Erweiterung (labels as values).
