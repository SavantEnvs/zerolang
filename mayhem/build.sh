#!/usr/bin/env bash
# Builds the zero compiler sources three ways: sanitized+SanCov objects linked into the libFuzzer and
# -standalone harness binaries, and an unsanitized `zero` CLI plus the upstream C smoke test for test.sh.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

SRC="${SRC:-/mayhem}"
cd "$SRC"

# Node (>= 22.6, for --experimental-strip-types) is baked into the image by the Dockerfile.
export PATH="/opt/toolchains/deps/node-22.23.4/bin:$PATH"

Z=native/zero-c
O=mayhem/obj
rm -rf "$O"
mkdir -p "$O/san" "$O/plain"

# The stdlib graph embed is generated (gitignored); the skills/runtime embeds are committed.
node --experimental-strip-types --disable-warning=ExperimentalWarning scripts/embed-stdlib-graphs.mts

SRCS=$(cd "$Z/src" && ls *.c | grep -vx main.c)
CSTD="-std=c11 -DZERO_BUILD_HASH=\"mayhem\" -I$Z/include -I$Z/src"

# 1) sanitized + SanCov-instrumented compiler objects
# program_graph_mir.c qsort()s a NULL, zero-length array for any graph with no entry function (UB per the C
# standard, harmless in practice); with it halting, every library graph in the seeds dies at startup.
export Z O CSTD CC SANITIZER_FLAGS DEBUG_FLAGS
printf '%s\n' $SRCS | xargs -P "$MAYHEM_JOBS" -I{} bash -c '
  extra=""; [ "$1" = program_graph_mir.c ] && extra="-fno-sanitize=nonnull-attribute"
  $CC $SANITIZER_FLAGS $extra $DEBUG_FLAGS -fsanitize=fuzzer-no-link -O1 $CSTD -c "$Z/src/$1" -o "$O/san/$1.o"' _ {}

$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c mayhem/lsan_off.cc -o "$O/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$O/standalone_main.o"

# 2) harnesses: libFuzzer binary + standalone reproducer
for h in fuzz_canonical_text fuzz_graph_binary fuzz_graph_dump fuzz_manifest_toml; do
  $CC $SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link -O1 $CSTD -c "mayhem/harnesses/$h.c" -o "$O/$h.o"
  $CC $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$O/$h.o" "$O"/san/*.o "$O/lsan_off.o" -lm -o "/mayhem/$h"
  $CC $SANITIZER_FLAGS $DEBUG_FLAGS "$O/$h.o" "$O/standalone_main.o" "$O"/san/*.o "$O/lsan_off.o" -lm -o "/mayhem/$h-standalone"
done

# 3) oracle: the normal-flags CLI (upstream Makefile flags) and upstream's C smoke test
printf '%s\n' $SRCS | xargs -P "$MAYHEM_JOBS" -I{} \
  $CC -std=c11 -Os $COVERAGE_FLAGS -DZERO_BUILD_HASH=\"mayhem\" -I$Z/include -I$Z/src -c "$Z/src/{}" -o "$O/plain/{}.o"
$CC -std=c11 -Os $COVERAGE_FLAGS -DZERO_BUILD_HASH=\"mayhem\" -I$Z/include -I$Z/src -c "$Z/src/main.c" -o "$O/plain/main.c.o"
$CC $COVERAGE_FLAGS "$O"/plain/*.o -lm -o /mayhem/zero
$CC -std=c11 -Os $COVERAGE_FLAGS -I$Z/include -I$Z/src -c "$Z/tests/canonical_text_smoke.c" -o "$O/canonical_text_smoke.o"
$CC $COVERAGE_FLAGS "$O/canonical_text_smoke.o" $(ls "$O"/plain/*.o | grep -v '/main.c.o$') -lm -o /mayhem/canonical_text_smoke
