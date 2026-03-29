# Dragon's Prophet IDX / P00x Archive Format Specification

Reverse-engineered from `IDXUnpacker.exe` (2013, Delphi/Borland, 32-bit).

## Overview

The archive system uses two file types:
- **`.idx`** — Index file containing the table of contents (header + per-file entries)
- **`.p00x`** — Data files containing the actual file data (e.g., `.p001`, `.p002`, `.p003`)

Each entry in the IDX references a specific data file and offset within it.

---

## IDX File Layout

### IDX Header (24 bytes)

| Offset | Size | Type   | Field        | Description                     |
|--------|------|--------|--------------|---------------------------------|
| 0x00   | 4    | uint32 | unknown_0    | Unknown (possibly magic/version)|
| 0x04   | 4    | uint32 | unknown_1    | Unknown                         |
| 0x08   | 4    | uint32 | unknown_2    | Unknown                         |
| 0x0C   | 4    | uint32 | unknown_3    | Unknown                         |
| 0x10   | 4    | uint32 | unknown_4    | Unknown                         |
| 0x14   | 4    | uint32 | file_count   | Total number of file entries     |

### IDX Entry (variable length, repeated `file_count` times)

Each entry consists of a **33-byte fixed structure** followed by a **null-terminated filename string**.

#### Fixed Fields (33 bytes)

| Offset | Size | Type   | Field           | Description                                      |
|--------|------|--------|-----------------|--------------------------------------------------|
| 0x00   | 4    | uint32 | field_0         | Unknown                                          |
| 0x04   | 4    | uint32 | field_1         | Unknown                                          |
| 0x08   | 4    | uint32 | field_2         | Unknown                                          |
| 0x0C   | 4    | uint32 | data_offset     | Byte offset into the data (.p00x) file           |
| 0x10   | 4    | uint32 | field_4         | Unknown                                          |
| 0x14   | 4    | uint32 | field_5         | Unknown (adjusted +28 at runtime for display)    |
| 0x18   | 1    | uint8  | data_file_index | Data file index (formats as `.p%03u`, e.g., 1 → `.p001`) |
| 0x19   | 4    | uint32 | field_7         | Unknown                                          |
| 0x1D   | 4    | uint32 | field_8         | Unknown                                          |

**Total fixed size: 33 bytes** (4+4+4+4+4+4+1+4+4)

#### Variable-Length Filename

Immediately after the 33 fixed bytes, a **null-terminated ASCII string** contains the relative file path for extraction (e.g., `data\textures\grass.dds`).

---

## Data File (.p00x) Layout

Data files are named by taking the IDX filename (without extension) and appending `.p001`, `.p002`, etc., based on the `data_file_index` byte.

Example: `archive.idx` → `archive.p001`, `archive.p002`

### Per-File Data Chunk

At the byte offset specified by `data_offset` in the IDX entry, the data file contains:

#### Data Chunk Header (28 bytes)

| Offset | Size | Type   | Field             | Description                                    |
|--------|------|--------|-------------------|------------------------------------------------|
| 0x00   | 4    | uint32 | total_size        | Decompressed size + 28 (total chunk including header) |
| 0x04   | 4    | uint32 | field_1           | Unknown                                        |
| 0x08   | 4    | uint32 | field_2           | Unknown                                        |
| 0x0C   | 4    | uint32 | field_3           | Unknown                                        |
| 0x10   | 4    | uint32 | field_4           | Unknown                                        |
| 0x14   | 4    | uint32 | compressed_size   | Compressed data size (excluding this 28-byte header) |
| 0x18   | 4    | uint32 | field_6           | Unknown                                        |

#### Data Payload

Immediately after the 28-byte header: the actual file data (compressed or raw).

---

## Compression Detection

The tool determines compression by comparing sizes and checking a magic byte:

```
adjusted_compressed = compressed_size + 28
if (total_size == adjusted_compressed):
    # File is UNCOMPRESSED — raw copy
    copy (compressed_size) bytes from (data_offset + 28)
else:
    # File is COMPRESSED — detect algorithm
    byte = read_byte_at(data_offset + 28)
    if (byte == 0x5D):
        # LZMA compression (0x5D is the standard LZMA properties byte)
        dp_lzma_uncompress(src_path, dest_path, data_offset + 28, total_size - 28, adjusted_compressed)
    else:
        # Crunch compression (custom DP engine algorithm)
        dp_crunch_uncompress(src_path, dest_path, data_offset + 28, total_size - 28)
```

### DPLib.dll Exports

| Function               | Params | Signature (cdecl)                                              |
|------------------------|--------|----------------------------------------------------------------|
| `dp_lzma_uncompress`   | 5      | `(src_path, dest_path, data_offset, decomp_size, comp_chunk_size)` |
| `dp_crunch_uncompress` | 4      | `(src_path, dest_path, data_offset, decomp_size)`              |

Both functions take file paths (not buffers), handle file I/O internally, and write the decompressed output to `dest_path`.

---

## Output Directory Structure

The unpacker constructs output paths as follows:

```
output_dir = IDX_path_without_extension + "\"
full_path  = IDX_parent_dir + output_dir + entry_filename_path
```

Example:
- IDX file: `C:\Games\data.idx`
- Entry filename: `textures\grass.dds`
- Output: `C:\Games\data\textures\grass.dds`

Subdirectories are created automatically via `ForceDirectories()`.

---

## UI Display (ListView columns)

| Column | Content                          | Format     |
|--------|----------------------------------|------------|
| Name   | Entry filename                   | string     |
| Offset | `data_offset` from IDX entry     | `%0.8x`    |
| Size 1 | `total_size` from data header    | decimal    |
| Size 2 | `compressed_size + 28` (adjusted)| decimal    |
| File   | Data file path (`.p00x`)         | string     |

---

## DPLib.dll Analysis

Both decompression routines are wrappers around well-known public libraries:

### dp_lzma_uncompress (LZMA)

Standard **LZMA SDK** (Igor Pavlov, public domain) decompression.

```c
// Signature (cdecl, 5 params)
int dp_lzma_uncompress(
    const char* src_path,       // Source data file path
    const char* dest_path,      // Output file path
    int         offset,         // Byte offset in source file
    size_t      compressed_sz,  // Compressed data size
    size_t      decompressed_sz // Decompressed output size
);
```

- Reads compressed data from `src_path` at `offset`
- Data uses standard LZMA1 format: 1-byte properties + 4-byte dict size + 8-byte uncompressed size + compressed stream
- The `0x5D` detection byte is the default LZMA properties byte (lc=3, lp=0, pb=2)
- Writes decompressed output to `dest_path`

### dp_crunch_uncompress (CRN → DDS Transcoder)

Uses **crn_decomp.h** from Rich Geldreich's open-source [Crunch library](https://github.com/BinomialLLC/crunch) (MIT license).

```c
// Signature (cdecl, 4 params)
FILE* dp_crunch_uncompress(
    const char* src_path,       // Source data file path
    const char* dest_path,      // Output DDS file path
    int         offset,         // Byte offset in source file
    size_t      crn_data_size   // CRN compressed data size
);
```

- This is NOT general-purpose decompression — it's a **texture format transcoder**
- Reads CRN (Crunch) compressed texture data
- Transcodes to DDS format with proper header (DXT1/DXT3/DXT5/etc.)
- Output is a valid `.dds` file with magic `DDS ` and full DDS_HEADER
- Supported DDS formats:
  - Case 0: DXT1 (0x31545844)
  - Case 1: DXT3 (0x33545844)
  - Case 2: DXT5 (0x35545844)
  - Case 7: ETC1 (0x59524455, "UDRY" hack)
  - Case 8: A1R5G5B5 (0x32384942)
  - Case 9: R5G6B5 (0x31384942)
  - Cases 3-6: Various BC formats (BC1A, RG_H, RG_L, RA_H)

### Reimplementation Strategy

Both algorithms can be replaced with open-source libraries:
- **LZMA**: Use `lzma.h` from LZMA SDK or `liblzma` (xz-utils)
- **Crunch**: Use `crn_decomp.h` single-header library (header-only, MIT license)

---

## Notes

- IDXUnpacker.exe: **Delphi 6/7** (Borland), VCL framework, 32-bit x86
- DPLib.dll: **MSVC 2010** (Visual Studio 10.0), 32-bit x86
- The IDX header's first 20 bytes (5 DWORDs) are read but appear unused in extraction logic
- The entry struct stores the `data_file_index` as a single byte (max 255 data files)
- Progress is tracked via a `TProgressBar` with `Application.ProcessMessages` for UI responsiveness
