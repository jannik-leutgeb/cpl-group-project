#!/bin/bash

set -u

CLASSES="target/classes"
RES="src/main/resources"
MAIN="de.hft_stuttgart.cpl.GWBasic"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

# Colors, but only when writing to a terminal.
if [ -t 1 ]; then
  C_GREEN=$'\033[32m'; C_RED=$'\033[31m'; C_BOLD=$'\033[1m'; C_OFF=$'\033[0m'
else
  C_GREEN=""; C_RED=""; C_BOLD=""; C_OFF=""
fi

# Pick a timeout command if one is available (GNU coreutils ships `timeout` on
# Linux; on macOS it's `gtimeout` from `brew install coreutils`). Fall back to
# running the binary directly if neither exists.
if command -v timeout >/dev/null 2>&1; then
  TIMEOUT="timeout 5"
elif command -v gtimeout >/dev/null 2>&1; then
  TIMEOUT="gtimeout 5"
else
  TIMEOUT=""
fi

PASS_COUNT=0
FAIL_COUNT=0
FAILED_TESTS=""

build() {
  echo ">> mvn clean compile"
  mvn -q clean compile || { echo "Maven build failed"; exit 1; }
}

# Run one .bas through the whole pipeline and decide pass/fail.
# Convention: a test whose name ends in "_bad" is expected to fail in a
# controlled way (rejected by the transpiler, or a runtime error with a clear
# message and non-zero exit). Every other test must run cleanly and exit 0.
one() {
  local bas="$1"
  local base; base="$(basename "$bas" .bas)"

  local expect_fail=0
  case "$base" in *_bad) expect_fail=1;; esac

  echo "=================================================="
  echo " $bas"
  echo "--------------------------------------------------"

  local status
  if ! java -cp "$CLASSES:$RES" "$MAIN" "$bas" > "$TMP/$base.c" 2>"$TMP/$base.cerr"; then
      echo "[transpiler rejected it]"
      cat "$TMP/$base.cerr"
      status="compile-rejected"
  elif ! gcc -w -o "$TMP/$base.bin" "$TMP/$base.c" -lm 2>"$TMP/$base.gccerr"; then
      echo "[GCC ERROR]"
      cat "$TMP/$base.gccerr"
      status="gcc-error"
  else
      echo "[output]"
      $TIMEOUT "$TMP/$base.bin"
      local rc=$?
      if [ "$rc" -eq 124 ]; then
          echo "[timed out]"
          status="timeout"
      elif [ "$rc" -ne 0 ]; then
          echo "[runtime error, exit $rc]"
          status="runtime-error"
      else
          status="ok"
      fi
  fi

  local pass=0
  if [ "$expect_fail" -eq 1 ]; then
      # expected to fail: ok at compile time or at runtime, but not a hang or a clean run
      case "$status" in compile-rejected|runtime-error) pass=1;; esac
  else
      case "$status" in ok) pass=1;; esac
  fi

  if [ "$pass" -eq 1 ]; then
      PASS_COUNT=$((PASS_COUNT + 1))
      echo "${C_GREEN}[PASS]${C_OFF} $bas ($status)"
  else
      FAIL_COUNT=$((FAIL_COUNT + 1))
      FAILED_TESTS="${FAILED_TESTS}  ${bas} (${status})"$'\n'
      echo "${C_RED}[FAIL]${C_OFF} $bas ($status)"
  fi
}

build
if [ $# -ge 1 ]; then
  one "$1"
else
  [ -f TEST.BAS ] && one TEST.BAS
  for f in tests/*.bas tests/types/*.bas; do
    [ -f "$f" ] && one "$f"
  done
fi

TOTAL=$((PASS_COUNT + FAIL_COUNT))
echo ""
echo "=================================================="
if [ "$FAIL_COUNT" -eq 0 ]; then
  echo " ${C_BOLD}${C_GREEN}ALL PASSED${C_OFF} -- $TOTAL/$TOTAL tests succeeded"
else
  echo " ${C_BOLD}SUMMARY${C_OFF}: ${C_GREEN}${PASS_COUNT} passed${C_OFF}, ${C_RED}${FAIL_COUNT} failed${C_OFF} (of $TOTAL)"
  echo "--------------------------------------------------"
  echo " Failed tests:"
  printf '%s' "$FAILED_TESTS"
fi
echo "=================================================="

# Non-zero exit if anything failed (useful for CI / IntelliJ).
[ "$FAIL_COUNT" -eq 0 ]
