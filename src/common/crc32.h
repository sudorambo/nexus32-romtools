#ifndef NEXUS32_CRC32_H
#define NEXUS32_CRC32_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* CRC32 (IEEE polynomial). Initial value 0; pass previous crc as initial for incremental. */
uint32_t crc32_update(uint32_t crc, const void *data, unsigned long len);

/* Convenience: CRC32 of a single buffer (initial 0). */
uint32_t crc32(const void *data, unsigned long len);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS32_CRC32_H */
