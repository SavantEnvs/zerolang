// The compiler keeps single-slot, process-global memos keyed by ProgramGraph address plus table pointers, lengths
// and the graph hash: `adjacency_memo` (program_graph_adjacency.c, filled by z_program_graph_validate) and
// `ir_graph_index_cache` (program_graph_mir.c, filled by lowering). They are never freed or invalidated when the
// graph dies, so a later input's graph that lands on the same addresses with the same lengths and hash could
// replay the previous input's orderings. After every input the harness runs a never-freed sentinel graph through
// the same entry points, so each memo points at it and an input's result depends only on that input.
#ifndef MAYHEM_GRAPH_SENTINEL_H
#define MAYHEM_GRAPH_SENTINEL_H

#include <stdbool.h>

#include "program_graph_build.h"
#include "program_graph_format.h"

static const char kSentinelDump[] =
    "zero-graph v1\n"
    "origin source-text\n"
    "module \"unsupported-target-feature\"\n"
    "hash \"graph:f23dc4890223c71f\"\n"
    "\n"
    "node #mod_bbcd80a8 Module name:\"unsupported-target-feature\" path:\"unsupported-target-feature.0\" line:1 column:1\n"
    "node #decl_19769c04 Function name:\"main\" type:\"Void\" path:\"unsupported-target-feature.0\" line:1 column:1 public:true\n"
    "node #param_a3156520 Param name:\"net\" type:\"Net\" path:\"unsupported-target-feature.0\" line:1 column:13\n"
    "node #type_97f40969 TypeRef type:\"Net\" path:\"unsupported-target-feature.0\" line:1 column:13\n"
    "node #type_0d0ac663 TypeRef type:\"Void\" path:\"unsupported-target-feature.0\" line:1 column:1\n"
    "node #block_2bcf5c5e Block name:\"body\" path:\"unsupported-target-feature.0\" line:1 column:1\n"
    "edge #mod_bbcd80a8 function #decl_19769c04 order:0\n"
    "edge #decl_19769c04 param #param_a3156520 order:0\n"
    "edge #param_a3156520 type #type_97f40969\n"
    "edge #decl_19769c04 returnType #type_0d0ac663\n"
    "edge #decl_19769c04 body #block_2bcf5c5e\n";

static ZProgramGraph graph_sentinel;
static bool graph_sentinel_ready;

// Call at the end of every input, on every return path. `lower` also repoints the MIR index memo.
static void graph_sentinel_reprime(bool lower) {
  if (!graph_sentinel_ready) {
    ZDiag diag = {0};
    graph_sentinel_ready = z_program_graph_parse_dump(kSentinelDump, &graph_sentinel, &diag);
    if (!graph_sentinel_ready) return;
  }
  if (lower) {
    SourceInput input = {0};
    IrProgram ir = z_lower_program_graph_with_source(&graph_sentinel, &input, z_find_target(z_host_target()));
    z_free_ir_program(&ir);
    z_free_source(&input);
  }
  ZProgramGraphValidation validation = {0};
  z_program_graph_validate(&graph_sentinel, &validation);
}

#endif
