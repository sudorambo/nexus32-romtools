/*
 * rominspect — NEXUS-32 ROM inspector.
 * Usage: rominspect game.nxrom [--header] [--assets] [--disasm N]
 * Default: header summary and memory usage.
 */

#include "asset_dir.h"
#include "rom_header.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROMINSPECT_MAX_ASSETS 65536

static const char *asset_type_name(uint32_t t)
{
	switch (t) {
	case ASSET_TYPE_TEXTURE: return "texture";
	case ASSET_TYPE_MESH:    return "mesh";
	case ASSET_TYPE_AUDIO:  return "audio";
	case ASSET_TYPE_TILEMAP: return "tilemap";
	case ASSET_TYPE_SHADER: return "shader";
	case ASSET_TYPE_GENERIC: return "generic";
	default: return "?";
	}
}

static const char *target_region_name(uint32_t r)
{
	switch (r) {
	case TARGET_MAIN_RAM:  return "Main RAM";
	case TARGET_VRAM:      return "VRAM";
	case TARGET_AUDIO_RAM: return "Audio RAM";
	default: return "?";
	}
}

static void print_header_dump(const nxrom_header_t *h)
{
	printf("ROM Header Dump\n");
	printf("  magic:            %02X %02X %02X %02X (\"%c%c%c%c\")\n",
		h->magic[0], h->magic[1], h->magic[2], h->magic[3],
		h->magic[0], h->magic[1], h->magic[2], h->magic[3]);
	printf("  format_version:   0x%04X\n", (unsigned)h->format_version);
	printf("  flags:            0x%04X\n", (unsigned)h->flags);
	printf("  entry_point:      0x%08X\n", (unsigned)h->entry_point);
	printf("  code_offset:      0x%08X (%u)\n", (unsigned)h->code_offset, (unsigned)h->code_offset);
	printf("  code_size:        %u\n", (unsigned)h->code_size);
	printf("  data_offset:      0x%08X (%u)\n", (unsigned)h->data_offset, (unsigned)h->data_offset);
	printf("  data_size:        %u\n", (unsigned)h->data_size);
	printf("  asset_table_off:  0x%08X\n", (unsigned)h->asset_table_offset);
	printf("  asset_table_size: %u\n", (unsigned)h->asset_table_size);
	printf("  total_rom_size:   %u\n", (unsigned)h->total_rom_size);
	printf("  cycle_budget:     %u\n", (unsigned)h->cycle_budget);
	printf("  screen_width:     %u\n", (unsigned)h->screen_width);
	printf("  screen_height:    %u\n", (unsigned)h->screen_height);
	printf("  title:            \"%.31s\"\n", h->title);
	printf("  author:           \"%.31s\"\n", h->author);
	printf("  checksum:         0x%08X\n", (unsigned)h->checksum);
	printf("  header_checksum:  0x%08X\n", (unsigned)h->header_checksum);
}

static void print_summary(const nxrom_header_t *h, size_t file_size, int checksum_ok)
{
	uint32_t asset_bytes = 0;
	if (h->asset_table_size >= 4) {
		/* We don't have the ROM buffer here to sum asset sizes; use 0 or leave for --assets */
		(void)asset_bytes;
	}
	printf("NEXUS-32 ROM Inspector\n");
	printf("══════════════════════\n");
	printf("Title:       %.31s\n", h->title[0] ? h->title : "(none)");
	printf("Author:      %.31s\n", h->author[0] ? h->author : "(none)");
	printf("Format:      v1.0 (0x0100)\n");
	printf("Total Size:  %zu bytes (%.1f MB)\n", file_size, file_size / (1024.0 * 1024.0));
	printf("Checksum:    %s\n", checksum_ok ? "OK" : "MISMATCH");
	printf("\nMemory Usage:\n");
	printf("  Code:      %u bytes (%u KB)\n", (unsigned)h->code_size, (unsigned)(h->code_size / 1024));
	printf("  Data:      %u bytes (%u KB)\n", (unsigned)h->data_size, (unsigned)(h->data_size / 1024));
	printf("  Assets:    (see --assets for list)\n");
}

static void print_assets(const asset_entry_t *entries, int n)
{
	printf("Assets (%d):\n", n);
	for (int i = 0; i < n; i++) {
		char name[33];
		memcpy(name, entries[i].name, 32);
		name[32] = '\0';
		printf("  %d: %-32s type=%-8s format=0x%08X rom_off=0x%08X comp=%u uncomp=%u target=%s\n",
			i, name, asset_type_name(entries[i].asset_type), (unsigned)entries[i].format,
			(unsigned)entries[i].rom_offset, (unsigned)entries[i].compressed_size,
			(unsigned)entries[i].uncompressed_size, target_region_name(entries[i].target_region));
	}
}

static void disasm_one(uint32_t insn, uint32_t pc, char *buf, size_t bufsize)
{
	uint32_t op = insn >> 26, rs = (insn >> 21) & 31u, rt = (insn >> 16) & 31u;
	uint32_t rd = (insn >> 11) & 31u, shamt = (insn >> 6) & 31u, func = insn & 63u;
	uint32_t imm = insn & 0xFFFFu;
	int32_t simm = (int32_t)(int16_t)(uint16_t)imm;

	if (insn == 0) { snprintf(buf, bufsize, "nop"); return; }
	if (op == 0x00u) {
		const char *mn = NULL;
		switch (func) {
		case 0x00: snprintf(buf, bufsize, "sll r%u, r%u, %u", rd, rt, shamt); return;
		case 0x02: snprintf(buf, bufsize, "srl r%u, r%u, %u", rd, rt, shamt); return;
		case 0x03: snprintf(buf, bufsize, "sra r%u, r%u, %u", rd, rt, shamt); return;
		case 0x04: mn = "sllv"; break; case 0x06: mn = "srlv"; break; case 0x07: mn = "srav"; break;
		case 0x08: snprintf(buf, bufsize, "jr r%u", rs); return;
		case 0x09: snprintf(buf, bufsize, "jalr r%u, r%u", rd, rs); return;
		case 0x18: mn = "mul"; break; case 0x19: mn = "mulh"; break;
		case 0x1a: mn = "div"; break; case 0x1b: mn = "divu"; break; case 0x1c: mn = "mod"; break;
		case 0x20: mn = "add"; break; case 0x21: mn = "addu"; break;
		case 0x22: mn = "sub"; break; case 0x23: mn = "subu"; break;
		case 0x24: mn = "and"; break; case 0x25: mn = "or"; break;
		case 0x26: mn = "xor"; break; case 0x27: mn = "nor"; break;
		case 0x2a: mn = "slt"; break; case 0x2b: mn = "sltu"; break;
		default: break;
		}
		if (mn) { snprintf(buf, bufsize, "%s r%u, r%u, r%u", mn, rd, rs, rt); return; }
	}
	switch (op) {
	case 0x02u: snprintf(buf, bufsize, "j 0x%08X", (pc & 0xF0000000u) | ((insn & 0x3FFFFFFu) << 2)); return;
	case 0x03u: snprintf(buf, bufsize, "jal 0x%08X", (pc & 0xF0000000u) | ((insn & 0x3FFFFFFu) << 2)); return;
	case 0x04u: case 0x05u: case 0x14u: case 0x15u: case 0x16u: case 0x17u: {
		const char *br[] = { [0x04]=  "beq", [0x05] = "bne", [0x14] = "blt", [0x15] = "bgt", [0x16] = "ble", [0x17] = "bge" };
		snprintf(buf, bufsize, "%s r%u, r%u, 0x%08X", br[op], rs, rt, pc + 4 + (uint32_t)(simm << 2)); return;
	}
	case 0x08u: snprintf(buf, bufsize, "addi r%u, r%u, %d", rt, rs, (int)simm); return;
	case 0x09u: snprintf(buf, bufsize, "addiu r%u, r%u, %d", rt, rs, (int)simm); return;
	case 0x0au: snprintf(buf, bufsize, "slti r%u, r%u, %d", rt, rs, (int)simm); return;
	case 0x0bu: snprintf(buf, bufsize, "sltiu r%u, r%u, %d", rt, rs, (int)simm); return;
	case 0x0cu: snprintf(buf, bufsize, "andi r%u, r%u, 0x%04x", rt, rs, imm); return;
	case 0x0du: snprintf(buf, bufsize, "ori r%u, r%u, 0x%04x", rt, rs, imm); return;
	case 0x0eu: snprintf(buf, bufsize, "xori r%u, r%u, 0x%04x", rt, rs, imm); return;
	case 0x0fu: snprintf(buf, bufsize, "lui r%u, 0x%04x", rt, imm); return;
	case 0x20u: snprintf(buf, bufsize, "lb r%u, %d(r%u)", rt, (int)simm, rs); return;
	case 0x21u: snprintf(buf, bufsize, "lh r%u, %d(r%u)", rt, (int)simm, rs); return;
	case 0x23u: snprintf(buf, bufsize, "lw r%u, %d(r%u)", rt, (int)simm, rs); return;
	case 0x24u: snprintf(buf, bufsize, "lbu r%u, %d(r%u)", rt, (int)simm, rs); return;
	case 0x25u: snprintf(buf, bufsize, "lhu r%u, %d(r%u)", rt, (int)simm, rs); return;
	case 0x28u: snprintf(buf, bufsize, "sb r%u, %d(r%u)", rt, (int)simm, rs); return;
	case 0x29u: snprintf(buf, bufsize, "sh r%u, %d(r%u)", rt, (int)simm, rs); return;
	case 0x2bu: snprintf(buf, bufsize, "sw r%u, %d(r%u)", rt, (int)simm, rs); return;
	case 0x3fu:
		switch (func) {
		case 0x00: snprintf(buf, bufsize, "syscall"); return;
		case 0x01: snprintf(buf, bufsize, "break"); return;
		case 0x02: snprintf(buf, bufsize, "nop"); return;
		case 0x03: snprintf(buf, bufsize, "halt"); return;
		case 0x04: snprintf(buf, bufsize, "eret"); return;
		case 0x10: snprintf(buf, bufsize, "mfc0 r%u, r%u", rt, rd); return;
		case 0x11: snprintf(buf, bufsize, "mtc0 r%u, r%u", rt, rd); return;
		default: break;
		}
		break;
	default: break;
	}
	snprintf(buf, bufsize, ".word 0x%08X", insn);
}

static void print_disasm(const uint8_t *rom, size_t rom_size, const nxrom_header_t *h, int n)
{
	uint32_t off = h->code_offset;
	uint32_t size = h->code_size;
	if (off + size > rom_size || n <= 0)
		return;
	printf("Disassembly at entry 0x%08X (%d instructions):\n", (unsigned)h->entry_point, n);
	const uint8_t *p = rom + off;
	for (int i = 0; i < n && (uint32_t)(i * 4) < size; i++) {
		uint32_t word = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
		char line[128];
		disasm_one(word, h->entry_point + (uint32_t)(i * 4), line, sizeof(line));
		printf("  %08X:  %08X  %s\n", (unsigned)(h->entry_point + i * 4), (unsigned)word, line);
		p += 4;
	}
}

int main(int argc, char **argv)
{
	const char *path = NULL;
	int do_header = 0, do_assets = 0, disasm_n = -1;
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--header") == 0 || strcmp(argv[i], "-h") == 0)
			do_header = 1;
		else if (strcmp(argv[i], "--assets") == 0 || strcmp(argv[i], "-a") == 0)
			do_assets = 1;
		else if (strcmp(argv[i], "--disasm") == 0 || strcmp(argv[i], "-d") == 0) {
			if (i + 1 < argc) {
				disasm_n = atoi(argv[++i]);
				if (disasm_n <= 0) disasm_n = 16;
			} else
				disasm_n = 16;
		} else if (!path)
			path = argv[i];
		else {
			fprintf(stderr, "rominspect: usage: rominspect <file.nxrom> [--header] [--assets] [--disasm N]\n");
			return 1;
		}
	}
	if (!path) {
		fprintf(stderr, "rominspect: usage: rominspect <file.nxrom> [--header] [--assets] [--disasm N]\n");
		return 1;
	}

	FILE *f = fopen(path, "rb");
	if (!f) {
		fprintf(stderr, "rominspect: cannot open %s\n", path);
		return 1;
	}
	fseek(f, 0, SEEK_END);
	long flen = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (flen < (long)(int)NXROM_HEADER_SIZE) {
		fprintf(stderr, "rominspect: file too small\n");
		fclose(f);
		return 1;
	}
	size_t size = (size_t)flen;
	uint8_t *buf = malloc(size);
	if (!buf) {
		fprintf(stderr, "rominspect: out of memory\n");
		fclose(f);
		return 1;
	}
	if (fread(buf, 1, size, f) != size) {
		fprintf(stderr, "rominspect: read failed\n");
		free(buf);
		fclose(f);
		return 1;
	}
	fclose(f);

	nxrom_header_t h;
	if (nxrom_header_read(buf, &h) != 0) {
		fprintf(stderr, "rominspect: invalid header\n");
		free(buf);
		return 1;
	}

	int checksum_ok = 0;
	if (nxrom_header_magic_ok(&h) && h.total_rom_size == size) {
		uint32_t hc = nxrom_header_checksum_compute(buf);
		uint32_t rc = nxrom_rom_checksum_compute(buf, size);
		checksum_ok = (hc == h.header_checksum && rc == h.checksum);
	}

	if (do_header)
		print_header_dump(&h);
	else
		print_summary(&h, size, checksum_ok);

	if (do_assets) {
		asset_entry_t entries[ROMINSPECT_MAX_ASSETS];
		int n = asset_dir_parse(buf, size, h.asset_table_offset, h.asset_table_size, entries, ROMINSPECT_MAX_ASSETS);
		if (n >= 0)
			print_assets(entries, n);
		else
			printf("(asset table invalid or empty)\n");
	}

	if (disasm_n > 0)
		print_disasm(buf, size, &h, disasm_n);

	free(buf);
	return 0;
}
