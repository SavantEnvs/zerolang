// Text ProgramGraph dump format: parse and validate.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "graph_sentinel.h"
#include "program_graph_format.h"

static int fuzz_one(const uint8_t *data, size_t size) {
  char *text = malloc(size + 1);
  if (!text) return -1;
  memcpy(text, data, size);
  text[size] = 0;

  ZProgramGraph graph = {0};
  ZDiag diag = {0};
  if (!z_program_graph_parse_dump(text, &graph, &diag)) {
    free(text);
    return -1;
  }
  ZProgramGraphValidation validation = {0};
  z_program_graph_validate(&graph, &validation);
  z_program_graph_free(&graph);
  free(text);
  return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  int result = fuzz_one(data, size);
  graph_sentinel_reprime(false);
  return result;
}
