# nexus32-romtools

ROM packaging, validation, and inspection tools for the NEXUS-32 fantasy game console. Conforms to the [NEXUS-32 specification](https://github.com/nexus32/nexus32-spec) §9 (ROM format) and §11 (toolchain), and to the [ROM format contract](../nexus32-spec/specs/001-nexus32-spec-baseline/contracts/rom-format.md).

## Tools

- **romcheck** — Validate a `.nxrom` file (magic, format version, checksums, segment layout).
- **rompack** — Pack code, data, and assets into a `.nxrom` (manifest mode via `pack.toml`, or binary mode from a `.nxbin`).
- **rominspect** — Inspect ROM header, assets, and disassemble code at entry point (full instruction mnemonics).

## Build

Requirements: C17 compiler (GCC or Clang), CMake 3.14+.

```bash
mkdir build && cd build
cmake ..
make
```

Executables: `romcheck`, `rompack`, `rominspect` (in `build/` or install path).

## Usage

- **Validate a ROM:** `romcheck game.nxrom [--verbose]`  
  Exit 0 = valid, 1 = errors. Rejects invalid or unsupported-format ROMs with clear messages.

- **Pack a ROM (manifest mode):** `rompack -o game.nxrom -c pack.toml [--no-validate]`  
  Use a `pack.toml` manifest with keys: `code` (path to code.bin), `data` (optional), `entry_point` (e.g. 0x400), `title`, `author`. Optional: `screen_width`, `screen_height`, `cycle_budget`.

- **Pack a ROM (binary mode):** `rompack -o game.nxrom -b game.nxbin [-c pack.toml] [--no-validate]`  
  Reads code and data from the SDK linker output (`.nxbin`). Entry point comes from the .nxbin header. Optional `-c pack.toml` supplies title, author, screen_*, cycle_budget; if omitted, defaults are used.

- **Inspect a ROM:** `rominspect game.nxrom [--header] [--assets] [--disasm N]`  
  Default: header summary and memory usage. `--header`: full header dump; `--assets`: asset list; `--disasm N`: disassemble N instructions at entry point.

## Conformance

Implementation follows [nexus32-spec/docs/implementation-checklist.md](../nexus32-spec/docs/implementation-checklist.md) for romtools: ROM format contract, accept/reject rules, no silent undefined behavior on unsupported format version.
