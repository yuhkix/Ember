# Prophecy — Dragon's Prophet IDX Unpacker/Packer

**Date:** 2026-03-29
**Status:** Approved

## Purpose

Modern C++ reimplementation of the Dragon's Prophet IDX/P00x archive tool. Supports both unpacking existing archives and packing folders into new archives. Replaces the legacy 2013 Delphi tool with a clean ImGui interface.

## Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Build system | xmake | User preference |
| Rendering backend | DirectX 11 | Native Windows, rock-solid ImGui backend |
| UI layout | Tabbed (Unpack / Pack) | Clean separation of concerns |
| Packing compression | User-selectable (Raw / LZMA / CRN) | Maximum flexibility |
| Dependencies | All vendored in repo | Self-contained, no package manager needed |

## Architecture

### Core Library (UI-independent)

**`idx_reader.h/cpp`** — IDX archive parser and extractor.
- Parses 24-byte IDX header (file count at offset 20)
- Reads per-entry metadata (33 bytes fixed + null-terminated filename)
- Resolves data files (`.p001`, `.p002`, etc.) from entry's `data_file_index` byte
- Reads 28-byte data chunk header from data files
- Detects compression: sizes equal = raw, first byte `0x5D` = LZMA, else = CRN
- Extracts files: raw copy, LZMA decompress, or CRN→DDS transcode

**`idx_writer.h/cpp`** — IDX archive creator.
- Scans source directory recursively for files
- Writes IDX header + entries with correct offsets
- Writes data files (`.p00x`) with 28-byte data chunk headers
- Supports compression modes: raw (store), LZMA compress, DDS→CRN compress
- Configurable data file size threshold for splitting across multiple `.p00x` files

**`lzma_codec.h/cpp`** — LZMA compression and decompression.
- Wraps LZMA SDK's `LzmaEncode` and `LzmaDecode`
- Buffer-based API: `compress(input, output)` / `decompress(input, output, decompressed_size)`
- Uses standard LZMA1 format (properties byte 0x5D default)

**`crn_codec.h/cpp`** — CRN texture transcoding.
- Wraps `crn_decomp.h` for CRN→DDS decompression
- Writes proper DDS header with correct DXT format (DXT1/DXT3/DXT5/etc.)
- For packing: DDS→CRN compression (if crunch encoder is available, otherwise store raw)

### UI Layer

**`main.cpp`** — Entry point.
- Win32 window creation + DirectX 11 device/swapchain init
- ImGui context setup with custom font
- Main render loop: poll events → new frame → render app → present

**`app.h/cpp`** — Application UI.
- Two-tab interface: "Unpack" and "Pack"
- **Unpack tab:** IDX file picker, file list table, extract button, progress bar
- **Pack tab:** Folder picker, compression selector, file preview, output picker, pack button, progress bar
- Background worker thread for extraction/packing (keeps UI responsive)
- Status messages and error display

**`theme.h/cpp`** — Dark + red glow ImGui style.
- Background: `#0D0D0D`, panels: `#1A1A1A`, borders: `#2A2A2A`
- Red accent: `#FF2222`, hover: `#FF4444`, active: `#CC1111`
- Red glow on active buttons, selected tabs, progress bar fill
- Window rounding, frame rounding for modern feel
- Custom `ImGui::StyleColorsDark()` override

**`anim.h/cpp`** — Smooth transitions and fade animations.
- Lerp-based animation system driven by `ImGui::GetIO().DeltaTime`
- Tab switch: content fades out/in with alpha lerp (~200ms)
- Progress bar: smooth fill interpolation (no jumpy updates)
- Button hover: color transitions via lerp (instant → smooth ~150ms)
- File list population: staggered row fade-in (rows appear sequentially)
- Status messages: fade-in on appear, fade-out after timeout
- Window open: initial fade-in from transparent (~300ms)
- All animations use ease-out curve for natural feel

### Dependencies (vendored)

| Library | Purpose | License |
|---------|---------|---------|
| ImGui (docking) | GUI framework | MIT |
| ImGui DX11 backend | Rendering | MIT |
| LZMA SDK | LZMA compression | Public domain |
| crn_decomp.h | CRN→DDS transcoding | MIT |
| nfd (Native File Dialog) | OS file/folder pickers | Zlib |

## Project Structure

```
Prophecy/
├── xmake.lua
├── docs/
│   └── 2026-03-29-prophecy-design.md
├── src/
│   ├── main.cpp
│   ├── app.h
│   ├── app.cpp
│   ├── theme.h
│   ├── theme.cpp
│   ├── anim.h
│   ├── anim.cpp
│   ├── idx_reader.h
│   ├── idx_reader.cpp
│   ├── idx_writer.h
│   ├── idx_writer.cpp
│   ├── lzma_codec.h
│   ├── lzma_codec.cpp
│   ├── crn_codec.h
│   └── crn_codec.cpp
└── vendor/
    ├── imgui/          (ImGui source + DX11 backend)
    ├── lzma/           (LZMA SDK C source)
    ├── crunch/         (crn_decomp.h)
    └── nfd/            (Native File Dialog)
```

## IDX Format Reference

See `../IDX_FORMAT_SPEC.md` for the complete reverse-engineered format specification.

### Key structures:
- **IDX Header:** 24 bytes, file count at offset 0x14
- **IDX Entry:** 33 bytes fixed (data_offset at +0x0C, data_file_index byte at +0x18) + null-terminated filename
- **Data chunk header:** 28 bytes (compressed_total at +0x00, decompressed_size at +0x14)
- **Compression detection:** sizes equal = raw, first data byte `0x5D` = LZMA, else = CRN

## Error Handling

- File I/O errors: display in status bar, skip file, continue extraction/packing
- Decompression failures: log error, skip file, report count at end
- Invalid IDX format: show error dialog, refuse to process
- Out of memory: graceful message (large archives may need streaming approach)

## Threading Model

- UI runs on main thread (ImGui render loop)
- Extraction/packing runs on a background `std::thread`
- Shared state via `std::atomic<float>` progress and `std::atomic<bool>` cancel flag
- Results collected in thread-safe queue, displayed after completion
