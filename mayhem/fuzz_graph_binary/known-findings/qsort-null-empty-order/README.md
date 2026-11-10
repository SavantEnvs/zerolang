# `qsort(NULL, 0, ...)` when a graph has no entry function

`native/zero-c/src/program_graph_mir.c:6773` (`ir_lower_program_graph`) calls `qsort(order, ordered_len, ...)` with
`order == NULL` and `ordered_len == 0` whenever the graph has no exported or hosted `main` function. Passing a null
pointer to `qsort` is undefined behavior per C11 7.24.1p2 (UBSan `nonnull-attribute`), though harmless with glibc.

- Impact: none in practice; every library-only graph (for example a std module graph) trips it.
- Handling: `mayhem/build.sh` compiles only `program_graph_mir.c` with `-fno-sanitize=nonnull-attribute`; all other
  files keep ASan and every UBSan check halting. Without it every library graph in the seed corpus aborts the target
  at startup.
- Fix: `if (ordered_len) qsort(...)`.
