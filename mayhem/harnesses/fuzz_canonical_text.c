// Zerolang source text (.0): tokenizer, structural parser, formatter, Program parser and checker.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "canonical_text.h"

// checker.c keeps a process-global cache keyed by the address of the last-checked Program. A later Program at the
// same address with the same const count reuses the cache and dereferences type strings freed with the old one.
// After every input the harness therefore re-primes that cache with a Program that is never freed, so each
// input's result depends only on that input (see known-findings/stale-static-const-cache-uaf).
static const char kSentinelSource[] =
    "const cap = 3\n"
    "\n"
    "const enabled = true\n"
    "\n"
    "const selected = Mode.fast\n"
    "\n"
    "type Gate<static enabledFlag: Bool, static selectedMode: Mode> {\n"
    "    value: i32,\n"
    "}\n"
    "\n"
    "enum Mode: u8 {\n"
    "    fast,\n"
    "    tiny,\n"
    "}\n"
    "\n"
    "fn head<T: Type, static N: usize>(items: ref<[N]T>) -> T {\n"
    "    return items[0]\n"
    "}\n"
    "\n"
    "fn readGate<static enabledFlag: Bool, static selectedMode: Mode>(gate: ref<Gate<enabledFlag, selectedMode>>) -> i32 {\n"
    "    if enabledFlag {\n"
    "        return gate.value\n"
    "    }\n"
    "    return 0\n"
    "}\n"
    "\n"
    "pub fn main() -> Void {\n"
    "    let values: [cap]i32 = [3, 4, 5]\n"
    "    let first: i32 = head(&values)\n"
    "    let gate: Gate<enabled, selected> = Gate { value: 9 }\n"
    "    let gated: i32 = readGate(&gate)\n"
    "    expect (first == 3 && gated == 9)\n"
    "}\n";

static Program sentinel;
static bool sentinel_ready;

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  char *source = malloc(size + 1);
  if (!source) return -1;
  memcpy(source, data, size);
  source[size] = 0;

  ZDiag diag = {0};
  ZCanonicalTokenVec tokens = z_canonical_text_tokenize(source, &diag);
  if (diag.code != 0) {
    z_free_canonical_text_tokens(&tokens);
    free(source);
    return -1;
  }
  ZCanonicalTree tree = {0};
  ZCanonicalFacts facts = {0};
  z_canonical_text_parse(&tokens, &tree, &facts, &diag);
  z_free_canonical_text_tree(&tree);
  z_free_canonical_text_tokens(&tokens);

  ZDiag format_diag = {0};
  ZBuf formatted;
  zbuf_init(&formatted);
  z_canonical_text_format_source(source, &formatted, &format_diag);
  zbuf_free(&formatted);

  ZDiag program_diag = {0};
  Program program = {0};
  if (z_parse_canonical_text_program_source(source, &program, &program_diag)) {
    ZDiag check_diag = {0};
    z_check_program(&program, &check_diag);
  }
  z_free_program(&program);

  if (!sentinel_ready) {
    ZDiag sentinel_diag = {0};
    sentinel_ready = z_parse_canonical_text_program_source(kSentinelSource, &sentinel, &sentinel_diag);
  }
  if (sentinel_ready) {
    ZDiag sentinel_diag = {0};
    z_check_program(&sentinel, &sentinel_diag);
  }

  free(source);
  return 0;
}
