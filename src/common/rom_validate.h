#ifndef NEXUS32_ROM_VALIDATE_H
#define NEXUS32_ROM_VALIDATE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Validate ROM buffer. Returns 0 if valid, -1 on error (prints to stderr).
 * prefix: e.g. "romcheck" or "rompack" for "romcheck: error: ..."; NULL = "error: ".
 * verbose: if non-zero, print passed checks to stdout. */
int rom_validate(const uint8_t *buf, size_t size, const char *prefix, int verbose);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS32_ROM_VALIDATE_H */
