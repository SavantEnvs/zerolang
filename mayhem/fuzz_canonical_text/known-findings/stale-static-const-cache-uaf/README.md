# Stale static-const cache in the checker (heap-use-after-free on reuse)

`native/zero-c/src/checker.c` memoizes per-const type and canonical-argument strings in process-global arrays
(`static_const_type_cache_*`, lines ~3200-3263). `static_const_type_cache_prime()` treats the cache as valid when
`program == static_const_type_cache_program && program->consts.len == static_const_type_cache_len`, i.e. it is keyed by
the address of the last-checked `Program` and its const count. The cached `const char *` values point into
that Program's memory.

When a Program is freed and a new one is later built at the same address with the same const count, the cache is
reused and `strcmp(classified, type)` (checker.c:3254, `canonical_static_arg_for_type_cached`) reads freed strings.

- Impact: heap-use-after-free read in `z_check_program` for any embedder that checks more than one program per
  process (the `zero` CLI checks one program per process, so it does not trigger there).
- Reproducer: `crash-b78ce38e…` is the input that exposed it in a libFuzzer run, but it only crashes when a previous
  input left a stale cache entry, so replaying it alone passes. Build the harness without the sentinel re-prime
  (`kSentinelSource` in `mayhem/harnesses/fuzz_canonical_text.c`) and fuzz with the seeds to reproduce.
- Fix: invalidate the cache in `z_free_program`/at the start of `z_check_program`, or key it by a per-Program
  generation counter instead of an address.

The harness re-primes the cache with a never-freed sentinel Program after every input so each input's outcome depends
only on that input.
