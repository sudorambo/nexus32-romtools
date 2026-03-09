/*
 * Minimal LZ4 block API for nexus32-romtools.
 * Compatible with LZ4 block format. For full LZ4, replace with upstream lz4.h/lz4.c.
 */
#ifndef NEXUS32_LZ4_H
#define NEXUS32_LZ4_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Compress srcSize bytes from src into dst. Returns compressed size, or 0 on error.
 * dstCapacity must be at least LZ4_compressBound(srcSize). */
int LZ4_compress_default(const char *src, char *dst, int srcSize, int dstCapacity);

/* Maximum compressed size in worst case (no compression). */
int LZ4_compressBound(int inputSize);

/* Decompress compressedSize bytes from src into dst (max dstCapacity).
 * Returns decompressed size, or negative on error. */
int LZ4_decompress_safe(const char *src, char *dst, int compressedSize, int dstCapacity);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS32_LZ4_H */
