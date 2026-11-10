// zero.toml package manifest parser.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "manifest_toml.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  char *text = malloc(size + 1);
  if (!text) return -1;
  memcpy(text, data, size);
  text[size] = 0;

  ZManifest manifest = {0};
  ZDiag diag = {0};
  bool ok = z_parse_manifest_toml(text, &manifest, &diag);
  z_free_manifest(&manifest);
  free(text);
  return ok ? 0 : -1;
}
