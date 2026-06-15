# GWBasic → C Compiler (JavaCC)

A GWBasic compiler built on JavaCC: it translates BASIC into C (stack machine),
and the C code is compiled into an executable program with gcc.

## Building and running

    mvn clean compile
    java -cp target/classes:src/main/resources de.hft_stuttgart.cpl.GWBasic TEST.BAS > out.c
    gcc -w -o test out.c -lm
    ./test

Or all at once (builds + runs TEST.BAS and all tests):

    ./run_tests.sh
    ./run_tests.sh tests/types/t01_float_vs_double.bas   # single file

Important: `-lm` for gcc (the runtime uses math.h). The pom.xml requires
Java 24 — for an older JDK, adjust `maven.compiler.source/target` in the pom.

## macOS notes

The project is written for Linux, but it runs on a Mac with two small
adjustments:

1. **`timeout` for `run_tests.sh`**: macOS does not ship GNU `timeout`.
   Install it via Homebrew:

       brew install coreutils

   The script then automatically detects `gtimeout` (it looks for
   `timeout` first, then `gtimeout`; without either it runs without a
   time limit). No further editing of `run_tests.sh` is needed.

2. **JDK ≥ 24**: `mvn` compiles to Java 24 bytecode (see pom.xml). If
   `java` points to an older JDK, execution fails with
   `UnsupportedClassVersionError`. Install a suitable JDK
   (e.g. `brew install openjdk`) and set `JAVA_HOME` to it so that
   `mvn` and `java` use the same JDK:

       export JAVA_HOME=$(/usr/libexec/java_home -v 24)

   Putting that line in `~/.zshrc` makes it permanent.

## Type system

A variable's type follows from its name suffix:

| Suffix  | Type   | Behavior when storing                                              |
|---------|--------|-------------------------------------------------------------------|
| (none)  | float  | reduced to single precision (~7 significant digits) — the DEFAULT  |
| `#`     | double | full precision (~16 digits)                                       |
| `%`     | int    | rounded half-up (3.5 → 4, 4.5 → 5)                                |
| `$`     | string | character string                                                  |

Type checking happens at runtime: a string into a numeric variable (or
vice versa) produces a runtime warning and a default value, never a
C compiler error.

## Language scope

PRINT, LET (scalar and array targets), WHILE/WEND, FOR/NEXT (with
optional STEP, also negative and float), GOTO, GOSUB/RETURN
(address stack, GCC computed goto), DIM (one-dimensional arrays,
indices 0..n inclusive), comparison operators `= <> < <= > >=`,
string concatenation with `+`, float literals (`3.14`, `.5`).

## Error handling (as required by the assignment)

GOTO/GOSUB to a non-existent line number → compiler error with
exit code 1 (gcc is never invoked). Reading a never-assigned
variable → runtime warning, continues with 0 or "". Type mismatch →
runtime warning. Division by zero, RETURN without GOSUB,
array index out of bounds → runtime error.

## Tests

`tests/` contains feature and error-case tests, `tests/types/` the
type-system tests. Expected outputs:

| Test                                 | Output                                                         |
|--------------------------------------|----------------------------------------------------------------|
| TEST.BAS                             | 51 50 … 1                                                      |
| tests/array.bas                      | 30                                                             |
| tests/array_loop.bas                 | 0 1 4 9 16                                                     |
| tests/for_up.bas                     | 1 2 3 4 5                                                      |
| tests/for_down.bas                   | 10 8 6 4 2                                                     |
| tests/floatstep.bas                  | 1 1.5 2                                                        |
| tests/nested.bas                     | 11 12 21 22                                                    |
| tests/goto_ok.bas                    | 1 2                                                            |
| tests/gosub.bas                      | 5 9 0                                                          |
| tests/strcat.bas                     | FooBar                                                         |
| tests/strsuffix.bas                  | Hi there                                                       |
| tests/goto_bad.bas                   | compiler error: GOTO 999 — target line number does not exist   |
| tests/gosub_bad.bas                  | compiler error: GOSUB 999 — target line number does not exist  |
| tests/ret_bad.bas                    | runtime error: RETURN without GOSUB                            |
| tests/uninit.bas                     | 0 + warning (used before assigned)                             |
| tests/typemix.bas                    | 0 + warning (string into numeric)                              |
| tests/types/t01_float_vs_double.bas  | 0.3333333 / 0.3333333333333333                                 |
| tests/types/t02_int_rounding.bas     | 3 4 4 5                                                        |
| tests/types/t03_precision_loss.bas   | 0.6666667 / 0.6666666666666666                                 |
| tests/types/t04_pi_three_types.bas   | 3.141593 / 3.14159265358979 / 3                                |
| tests/types/t05_whole_numbers.bas    | 42 42 42                                                       |
| tests/types/t06_type_mismatch.bas    | 0 + three warnings (A, B#, C%)                                 |
| tests/types/t07_num_into_string.bas  | blank line + warning                                           |
| tests/types/t08_default_is_float.bas | 1.428571                                                       |
| tests/types/t09_float_loop.bas       | 0 0.25 0.5 0.75 1                                              |
| tests/types/t10_mixed_arith.bas      | 1                                                              |

## Known limitations

No unary minus in expressions (`LET A=-2.5` fails; workaround
`LET A=0-2.5`; STEP -2 works because it is parsed separately). DIM is
one-dimensional only. The LET keyword is mandatory. GOSUB/RETURN uses a
GCC extension (labels as values).
