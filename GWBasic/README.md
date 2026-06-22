# GWBasic → C Compiler (JavaCC)

A GWBasic compiler built on JavaCC: it translates BASIC into C (stack machine),
and the C code is compiled into an executable program with gcc.

## Building and running

Build, transpile `TEST.BAS`, compile the C-code, and run it:

1. **Compile the transpiler:**

```shell
mvn clean compile
```

2. **Transpile `TEST.BAS` to C with JavaCC:**

```shell
java -cp target/classes:src/main/resources de.hft_stuttgart.cpl.GWBasic TEST.BAS > out.c
```

3. **Compile the generated C-code with gcc:**

```shell
gcc -w -o test out.c -lm
```

4. **Run the program:**

```shell
./test
```


5. **Run all tests at once (builds + runs TEST.BAS and all tests):**

```shell
./run_tests.sh
```

**Optional: Run a single test file (e.g. `tests/array.bas`):**

```shell
./run_tests.sh tests/array.bas
```

**Important:**

`-lm` for gcc (the runtime uses math.h). The build targets **Java 21**
(`maven.compiler.release=21`) and the maven-enforcer-plugin fails
directly on any JDK older than 21. Any JDK >= 21 works; the produced
bytecode is always Java 21.

If `java`/`mvn` point to an older JDK the build stops with a clear
message. Install a suitable JDK (e.g. `apt install openjdk@21` on Linux
or `brew install openjdk@21` on MacOS) and set `JAVA_HOME` to it so that
`mvn` and `java` use the same JDK:

- Linux:

```shell
export JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64
```

- MacOS:

```shell
export JAVA_HOME=$(/usr/libexec/java_home -v 21)
```

> Putting that line in `~/.bashrc` or `~/.zshrc` (depending on your
> default terminal) makes it permanent.

## MacOS notes

The project is written for Linux, but it runs on MacOS with one small
adjustment:

**`timeout` for `run_tests.sh`**: macOS does not ship GNU `timeout`.

**Install it via Homebrew:**

```shell
brew install coreutils
```

> The script now automatically detects `gtimeout`. No further editing
> of `run_tests.sh` is needed.


## Type system

A variable's type follows from its name suffix:

| **Suffix** | **Type** | **Behavior when storing**                                         |
|------------|----------|-------------------------------------------------------------------|
| (none)     | float    | reduced to single precision (~7 significant digits) — the DEFAULT |
| `#`        | double   | full precision (~16 digits)                                       |
| `%`        | int      | rounded half-up (3.5 → 4, 4.5 → 5)                                |
| `$`        | string   | character string                                                  |

> Type checking happens at runtime: a string into a numeric variable
> (or vice versa) produces a runtime warning and a default value, never
> a C compiler error.

## Language scope

PRINT, LET (scalar and array targets), WHILE/WEND, FOR/NEXT (with
optional STEP, also negative and float), GOTO, GOSUB/RETURN
(address stack, GCC computed goto), DIM (one-dimensional arrays,
indices 0..n inclusive), comparison operators `= <> < <= > >=`,
string concatenation with `+`, float literals (`3.14`, `.5`).

## Error handling (as required by the assignment)

GOTO/GOSUB to a non-existent line number → compiler error with
exit code 1 (gcc is never invoked). On such a BASIC source error the
incomplete C is written to `target/<name>.cfail` (note: NOT a `.c`
extension), so it is easy to see that this code was never compiled,
and stdout stays empty. Reading a never-assigned variable → runtime
warning, continues with 0 or "". Type mismatch → runtime warning.
Division by zero, RETURN without GOSUB, array index out of bounds →
runtime error (clear message, exit 1).

## Tests

`tests/` contains feature and error-case tests, `tests/types/` the
type-system tests. Expected outputs:

| **Test**                             | **Output**                                                    |
|--------------------------------------|---------------------------------------------------------------|
| TEST.BAS                             | 51 50 … 1                                                     |
| tests/array.bas                      | 30                                                            |
| tests/array_loop.bas                 | 0 1 4 9 16                                                    |
| tests/for_up.bas                     | 1 2 3 4 5                                                     |
| tests/for_down.bas                   | 10 8 6 4 2                                                    |
| tests/floatstep.bas                  | 1 1.5 2                                                       |
| tests/nested.bas                     | 11 12 21 22                                                   |
| tests/goto_ok.bas                    | 1 2                                                           |
| tests/gosub.bas                      | 5 9 0                                                         |
| tests/strcat.bas                     | FooBar                                                        |
| tests/strsuffix.bas                  | Hi there                                                      |
| tests/goto_bad.bas                   | compiler error: GOTO 999 — target line number does not exist  |
| tests/gosub_bad.bas                  | compiler error: GOSUB 999 — target line number does not exist |
| tests/ret_bad.bas                    | runtime error: RETURN without GOSUB                           |
| tests/uninit.bas                     | 0 + warning (used before assigned)                            |
| tests/typemix.bas                    | 0 + warning (string into numeric)                             |
| tests/types/t01_float_vs_double.bas  | 0.3333333 / 0.3333333333333333                                |
| tests/types/t02_int_rounding.bas     | 3 4 4 5                                                       |
| tests/types/t03_precision_loss.bas   | 0.6666667 / 0.6666666666666666                                |
| tests/types/t04_pi_three_types.bas   | 3.141593 / 3.14159265358979 / 3                               |
| tests/types/t05_whole_numbers.bas    | 42 42 42                                                      |
| tests/types/t06_type_mismatch.bas    | 0 + three warnings (A, B#, C%)                                |
| tests/types/t07_num_into_string.bas  | blank line + warning                                          |
| tests/types/t08_default_is_float.bas | 1.428571                                                      |
| tests/types/t09_float_loop.bas       | 0 0.25 0.5 0.75 1                                             |
| tests/types/t10_mixed_arith.bas      | 1                                                             |

## Known limitations

No unary minus in expressions (`LET A=-2.5` fails; workaround
`LET A=0-2.5`; STEP -2 works because it is parsed separately). DIM is
one-dimensional only. The LET keyword is mandatory. GOSUB/RETURN uses a
GCC extension (labels as values).
