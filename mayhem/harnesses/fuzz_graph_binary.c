// Binary ProgramGraph store (ZRGBIN1): parse, validate, then the build pipeline behind `zero build`:
// embedded std merge, MIR lowering and direct ELF emission.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "graph_sentinel.h"
#include "program_graph_build.h"
#include "program_graph_store_binary.h"

static int fuzz_one(const uint8_t *data, size_t size) {
  ZProgramGraphStore store;
  z_program_graph_store_init(&store);
  ZDiag diag = {0};
  if (!z_program_graph_store_parse_binary("input.graph", data, size, &store, &diag)) {
    z_program_graph_store_free(&store);
    return -1;
  }

  ZProgramGraphValidation validation = {0};
  if (z_program_graph_validate(&store.graph, &validation)) {
    const ZTargetInfo *target = z_find_target(z_host_target());
    SourceInput input = {0};
    ZDiag merge_diag = {0};
    if (z_program_graph_merge_embedded_std_graph_modules(&store.graph, &input, &merge_diag)) {
      IrProgram ir = z_lower_program_graph_with_source(&store.graph, &input, target);
      if (ir.mir_valid) {
        ZBuf out;
        zbuf_init(&out);
        ZDiag emit_diag = {0};
        z_emit_elf64_exe_from_ir(&ir, &out, &emit_diag);
        zbuf_free(&out);
      }
      z_free_ir_program(&ir);
    }
    z_free_source(&input);
  }
  z_program_graph_store_free(&store);
  return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  int result = fuzz_one(data, size);
  graph_sentinel_reprime(true);
  return result;
}
