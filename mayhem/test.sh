#!/usr/bin/env bash
#
# mayhem/test.sh — known-answer checks of the zero compiler CLI plus upstream's C smoke test, all prebuilt by
# mayhem/build.sh with the project's normal flags. Every assertion compares an exact computed value.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
cd "$SRC"

emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

passed=0
failed=0
# expect <name> <expected> <actual>
expect() {
  if [ "$2" = "$3" ]; then
    passed=$((passed + 1))
  else
    failed=$((failed + 1))
    printf 'FAIL %s\n  expected: %s\n  actual:   %s\n' "$1" "$2" "$3" >&2
  fi
}

for bin in /mayhem/zero /mayhem/canonical_text_smoke; do
  [ -x "$bin" ] || { echo "missing $bin: mayhem/build.sh did not build it" >&2; emit_ctrf zero-kat 0 1; exit 1; }
done

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
cp examples/*.graph examples/add.0 "$work"/
zero=/mayhem/zero

# Programs that print a fixed line: compiled to a native executable by the compiler, then run.
while IFS='|' read -r name want; do
  expect "run $name" "$want" "$("$zero" run "$work/$name.graph" 2>/dev/null)"
done <<'LIST'
add|math works
allocator-collections|allocator collections ok
branch|branch yes
codec-varint|codec primitives ok
config-shape|config shape parsed
const-arithmetic|const arithmetic ok
countdown|countdown done
fallibility|fallibility ok
functions|function call works
generic-pair|generic pair ok
grep-scan|grep scan ok
hello|hello from zero
hello-let|hello from a binding
memory-primitives|memory type forms runnable
parse-cursor|parse primitives ok
primitive-language-gaps|arrays and assignment ok
std-json-bytes|std json bytes ok
std-math|std math ok
std-str|std str ok
std-text-format-parse|text format parse ok
type-alias|type alias ok
LIST

expect "check add.graph" "ok" "$("$zero" check "$work/add.graph" 2>&1)"
expect "check rejects .0 text input" "BLD002" "$("$zero" check "$work/add.0" 2>&1 | head -1 | grep -o 'BLD002')"
expect "fmt is canonical" "$(cat examples/add.0)" "$("$zero" fmt "$work/add.0" 2>&1)"
expect "dump graph hash" 'hash "graph:21883cd42a46fce8"' "$("$zero" dump "$work/add.graph" 2>&1 | sed -n 4p)"
expect "dump node count" "36" "$("$zero" dump "$work/add.graph" 2>&1 | grep -c '^node ')"
expect "query function signature" "answer() -> i32 #decl_d618bd63 body:#block_e0b70e82" \
  "$("$zero" query --fn answer "$work/add.graph" 2>&1 | grep 'answer()' | sed 's/^ *//')"
expect "tokens first word" '{"kind": "word", "text": "fn", "line": 1, "column": 1, "offset": 0, "length": 2},' \
  "$("$zero" tokens --json "$work/add.0" 2>&1 | grep -m1 '"text": "fn"' | sed 's/^ *//')"
trap_out="$("$zero" run "$work/direct-array-bounds-trap.graph" 2>&1)"
expect "array bounds trap" "trap: index out of bounds" "$(printf '%s\n' "$trap_out" | grep -o 'trap: index out of bounds' | head -1)"

expect "upstream canonical text smoke" "canonical text smoke ok" "$(/mayhem/canonical_text_smoke 2>&1 | tail -1)"

emit_ctrf "zero-kat" "$passed" "$failed"
