/*
 * Minimal LZ4 block compressor/decompressor for nexus32-romtools.
 * LZ4 block format only. No frame. BSD 2-Clause compatible with upstream LZ4.
 */
#include "lz4.h"
#include <string.h>

#define LZ4_MEMORY_USAGE 14
#define HASH_SIZE (1 << (LZ4_MEMORY_USAGE - 2))
#define MIN_MATCH 4
#define COPY_LENGTH 8
#define LAST_LITERALS 5
#define MFLIMIT (COPY_LENGTH + MIN_MATCH)
#define LZ4_minLength (MIN_MATCH + 4)

static inline unsigned LZ4_hash4(unsigned val)
{
	return (val * 2654435761u) >> (32 - (LZ4_MEMORY_USAGE - 2));
}

int LZ4_compressBound(int inputSize)
{
	return inputSize + (inputSize / 255) + 16;
}

/* Emit length (possibly extended) and return updated pointer. */
static unsigned char *emit_len(unsigned char *p, unsigned len)
{
	while (len >= 255) {
		*p++ = 255;
		len -= 255;
	}
	*p++ = (unsigned char)len;
	return p;
}

int LZ4_compress_default(const char *src, char *dst, int srcSize, int dstCapacity)
{
	const unsigned char *const src_end = (const unsigned char *)src + srcSize;
	const unsigned char *src_p = (const unsigned char *)src;
	unsigned char *d = (unsigned char *)dst;
	const unsigned char *hash_table[HASH_SIZE];
	memset(hash_table, 0, sizeof(hash_table));
#define HASH_POS(p) LZ4_hash4(*(const unsigned *)(p))

	if (srcSize == 0)
		return 0;
	if (srcSize < MIN_MATCH)
		goto last_literals;

	{
		const unsigned char *anchor = src_p;
		const unsigned char *const dst_end = (unsigned char *)dst + dstCapacity - 1;

		while (src_p + MIN_MATCH <= src_end) {
			unsigned h = HASH_POS(src_p);
			const unsigned char *ref = hash_table[h];
			hash_table[h] = src_p;

			if (ref >= (const unsigned char *)src && ref < src_p && src_p - ref < 65536) {
				unsigned match_len = 0;
				while (src_p + match_len < src_end && ref + match_len < src_p &&
				       src_p[match_len] == ref[match_len])
					match_len++;
				if (match_len >= MIN_MATCH) {
					unsigned lit_len = (unsigned)(src_p - anchor);
					unsigned token = (lit_len < 15 ? lit_len : 15) | ((match_len - MIN_MATCH) < 15 ? (match_len - MIN_MATCH) << 4 : 15 << 4);
					if (d + 1 + (lit_len >= 15 ? 1 + lit_len / 255 : 0) + lit_len + 2 + (match_len - MIN_MATCH >= 15 ? 1 + (match_len - MIN_MATCH) / 255 : 0) > dst_end)
						return 0;
					*d++ = (unsigned char)token;
					if (lit_len >= 15) {
						d = emit_len(d, lit_len - 15);
					}
					memcpy(d, anchor, lit_len);
					d += lit_len;
					*(unsigned short *)d = (unsigned short)(src_p - ref);
					d += 2;
					if (match_len - MIN_MATCH >= 15) {
						d = emit_len(d, match_len - MIN_MATCH - 15);
					}
					src_p += match_len;
					anchor = src_p;
					continue;
				}
			}
			src_p++;
		}
		src_p = anchor;
	}
last_literals:
	{
		unsigned lit_len = (unsigned)(src_end - src_p);
		if (d + 1 + (lit_len >= 15 ? 1 + lit_len / 255 : 0) + lit_len > (unsigned char *)dst + dstCapacity)
			return 0;
		unsigned token = lit_len < 15 ? lit_len : 15;
		*d++ = (unsigned char)token;
		if (lit_len >= 15)
			d = emit_len(d, lit_len - 15);
		memcpy(d, src_p, lit_len);
		d += lit_len;
	}
	return (int)(d - (unsigned char *)dst);
}

int LZ4_decompress_safe(const char *src, char *dst, int compressedSize, int dstCapacity)
{
	const unsigned char *s = (const unsigned char *)src;
	const unsigned char *const s_end = s + compressedSize;
	unsigned char *d = (unsigned char *)dst;
	unsigned char *const d_end = d + dstCapacity;

	while (s < s_end) {
		unsigned token = *s++;
		unsigned lit_len = token >> 4;
		unsigned match_len = (token & 15) + MIN_MATCH;

		if (lit_len == 15) {
			unsigned b;
			do {
				if (s >= s_end) return -1;
				b = *s++;
				lit_len += b;
			} while (b == 255);
		}
		if (d + lit_len > d_end || s + lit_len > s_end)
			return -1;
		memcpy(d, s, lit_len);
		d += lit_len;
		s += lit_len;
		if (s >= s_end)
			break;
		if (match_len != MIN_MATCH) {
			if (s + 2 > s_end)
				return -1;
			unsigned offset = (unsigned)s[0] | ((unsigned)s[1] << 8);
			s += 2;
			if (match_len == 15 + MIN_MATCH) {
				unsigned b;
				do {
					if (s >= s_end) return -1;
					b = *s++;
					match_len += b;
				} while (b == 255);
			}
			if (offset == 0 || d - offset < (unsigned char *)dst || d + match_len > d_end)
				return -1;
			unsigned char *ref = d - offset;
			do {
				*d++ = *ref++;
			} while (--match_len);
		} else {
			/* End marker: last sequence has no match */
			break;
		}
	}
	return (int)(d - (unsigned char *)dst);
}
