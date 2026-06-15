#!/bin/bash

set -u

CLASSES="target/classes"
RES="src/main/resources"
MAIN="de.hft_stuttgart.cpl.GWBasic"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

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

build() {
  echo ">> mvn clean compile"
  mvn -q clean compile || { echo "Maven build failed"; exit 1; }
}

one() {
  local bas="$1"
  local base; base="$(basename "$bas" .bas)"
  echo "=================================================="
  echo " $bas"
  echo "--------------------------------------------------"

  if ! java -cp "$CLASSES:$RES" "$MAIN" "$bas" > "$TMP/$base.c" 2>"$TMP/$base.cerr"; then
      echo "[compiler rejected it -- correct for the *_bad tests]"
      cat "$TMP/$base.cerr"
      return
  fi

  if ! gcc -w -o "$TMP/$base.bin" "$TMP/$base.c" -lm 2>"$TMP/$base.gccerr"; then
      echo "[GCC ERROR]"
      cat "$TMP/$base.gccerr"
      return
  fi

  echo "[output]"
  $TIMEOUT "$TMP/$base.bin"
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
