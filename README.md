<p align="center">
  <h1 align="center">PROPHECY</h1>
  <p align="center">
    A modern IDX / P00x archive tool for Dragon's Prophet
    <br />
    <strong>Unpack & Repack game archives with a sleek interface</strong>
  </p>
</p>

---

## About

**Prophecy** is a modern reimplementation of the Dragon's Prophet IDX archive unpacker/packer, built from scratch in C++ with a custom dark-themed UI. It replaces the legacy 2013 Delphi tool with a fast, clean, and fully open-source alternative.

### Features

- **Unpack** IDX/P00x archives with live progress and per-file status
- **Pack** directories into IDX/P00x archives with selectable compression
- **Auto compression** — intelligently picks Raw or LZMA per file type
- **LZMA decompression** — standard LZMA1 format support
- **CRN to DDS transcoding** — Crunch-compressed textures extracted as DDS
- **Full format support** — handles `.ros`, `.ras`, and all other DP engine file types
- **Custom UI framework** — pixel-perfect file lists, smooth animations, red glow effects
- **Borderless window** — custom title bar with native resize/snap support

### Supported Formats

| Format | Description | Support |
|--------|-------------|---------|
| `.idx` | Archive index file | Read / Write |
| `.p00x` | Archive data files (.p001, .p002, ...) | Read / Write |
| `.ros` | Scene/object graph | Extract / Pack |
| `.ras` | Animation/skeleton data | Extract / Pack |
| `.dds` | DirectDraw Surface textures | Extract (from CRN) |
| LZMA | Compressed entries | Decompress / Compress |
| CRN | Crunch-compressed textures | Decompress to DDS |

## Screenshots

> *Coming soon*

## Building

### Prerequisites

- [xmake](https://xmake.io/) build system
- MSVC 2022 (Visual Studio 2022 with C++ workload)
- Git (for cloning dependencies)

### Setup

```bash
git clone https://github.com/yuhkix/Prophecy.git
cd Prophecy
setup.bat
xmake f -p windows
xmake build
xmake run
```

`setup.bat` clones the required vendored dependencies (ImGui, LZMA SDK, Native File Dialog).

### Dependencies

All dependencies are vendored — no package manager required.

| Library | Purpose | License |
|---------|---------|---------|
| [Dear ImGui](https://github.com/ocornut/imgui) (docking) | GUI framework + DX11 backend | MIT |
| [LZMA SDK](https://github.com/jljusten/LZMA-SDK) | LZMA compression/decompression | Public Domain |
| [crn_decomp.h](https://github.com/BinomialLLC/crunch) | CRN texture transcoding | MIT |
| [NFD Extended](https://github.com/btzy/nativefiledialog-extended) | Native file/folder dialogs | Zlib |

## Architecture

```
src/
├── main.cpp                 Entry point, DX11 + ImGui setup
├── core/                    Format logic (no UI dependency)
│   ├── idx_reader.h/cpp     IDX parser + file extractor
│   ├── idx_writer.h/cpp     IDX packer (directory -> archive)
│   ├── lzma_codec.h/cpp     LZMA compress / decompress
│   └── crn_codec.h/cpp      CRN -> DDS transcoder
└── ui/                      ImGui interface layer
    ├── app.h/cpp            Application UI (tabs, controls)
    ├── theme.h/cpp          Dark + red glow theme
    ├── anim.h/cpp           Smooth transitions & animations
    └── widgets.h/cpp        Custom widget framework
```

## Format Documentation

The IDX/P00x archive format was fully reverse-engineered from the original binaries using IDA Pro. See [`IDX_FORMAT_SPEC.md`](./IDX_FORMAT_SPEC.md) for the complete specification.

## License

This project is provided as-is for educational and modding purposes.

---

<p align="center">
  <sub>Built with C++26 | ImGui | DirectX 11</sub>
</p>
