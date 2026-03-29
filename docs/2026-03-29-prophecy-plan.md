# Prophecy Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a modern C++ IDX archive unpacker/packer with dark-themed red-accent ImGui UI.

**Architecture:** Layered design — core library (codecs, reader, writer) with no UI dependency, thin UI layer on top using ImGui+DX11. Background threading for long operations. All dependencies vendored.

**Tech Stack:** C++26, xmake, ImGui (docking, DX11), LZMA SDK, crn_decomp.h, Native File Dialog (nfd)

---

### Task 1: Project scaffolding and vendored dependencies

**Files:**
- Create: `Prophecy/xmake.lua`
- Create: `Prophecy/src/main.cpp` (minimal placeholder)
- Download: `Prophecy/vendor/imgui/` (ImGui v1.91.x docking branch)
- Download: `Prophecy/vendor/lzma/` (LZMA SDK 24.09)
- Download: `Prophecy/vendor/crunch/crn_decomp.h`
- Download: `Prophecy/vendor/nfd/` (nativefiledialog-extended)

- [ ] **Step 1: Create directory structure**

```bash
cd D:/Backups/IDXUnpacker/Prophecy
mkdir -p src vendor/imgui vendor/lzma vendor/crunch vendor/nfd
```

- [ ] **Step 2: Download ImGui docking branch**

```bash
cd D:/Backups/IDXUnpacker/Prophecy/vendor
git clone --depth 1 --branch docking https://github.com/ocornut/imgui.git imgui
```

We need these files from imgui: `imgui.cpp`, `imgui.h`, `imgui_draw.cpp`, `imgui_tables.cpp`, `imgui_widgets.cpp`, `imgui_demo.cpp`, `imgui_internal.h`, `imconfig.h`, `imstb_rectpack.h`, `imstb_textedit.h`, `imstb_truetype.h`, and from `backends/`: `imgui_impl_dx11.cpp`, `imgui_impl_dx11.h`, `imgui_impl_win32.cpp`, `imgui_impl_win32.h`.

- [ ] **Step 3: Download LZMA SDK**

```bash
cd D:/Backups/IDXUnpacker/Prophecy/vendor
git clone --depth 1 https://github.com/jljusten/LZMA-SDK.git lzma
```

We need from `C/`: `LzmaDec.c`, `LzmaDec.h`, `LzmaEnc.c`, `LzmaEnc.h`, `LzFind.c`, `LzFind.h`, `LzFindMt.c`, `LzFindMt.h`, `LzFindOpt.c`, `Threads.c`, `Threads.h`, `LzmaLib.c`, `LzmaLib.h`, `7zTypes.h`, `Compiler.h`, `Precomp.h`, `CpuArch.h`, `CpuArch.c`.

- [ ] **Step 4: Download crn_decomp.h**

```bash
cd D:/Backups/IDXUnpacker/Prophecy/vendor/crunch
curl -L -o crn_decomp.h https://raw.githubusercontent.com/BinomialLLC/crunch/master/inc/crn_decomp.h
```

- [ ] **Step 5: Download nativefiledialog-extended**

```bash
cd D:/Backups/IDXUnpacker/Prophecy/vendor
git clone --depth 1 https://github.com/btzy/nativefiledialog-extended.git nfd
```

- [ ] **Step 6: Write xmake.lua**

```lua
-- Prophecy/xmake.lua
set_project("Prophecy")
set_version("1.0.0")
set_languages("c++26", "c23") -- C++26

add_rules("mode.debug", "mode.release")

-- Windows-specific
if is_plat("windows") then
    add_cxflags("/utf-8")
    add_defines("_CRT_SECURE_NO_WARNINGS", "UNICODE", "_UNICODE")
end

target("Prophecy")
    set_kind("binary")
    set_targetdir("$(buildir)/bin")

    -- Source files
    add_files("src/*.cpp")

    -- ImGui core
    add_files(
        "vendor/imgui/imgui.cpp",
        "vendor/imgui/imgui_draw.cpp",
        "vendor/imgui/imgui_tables.cpp",
        "vendor/imgui/imgui_widgets.cpp",
        "vendor/imgui/imgui_demo.cpp",
        "vendor/imgui/backends/imgui_impl_dx11.cpp",
        "vendor/imgui/backends/imgui_impl_win32.cpp"
    )
    add_includedirs("vendor/imgui", "vendor/imgui/backends")

    -- LZMA SDK
    add_files(
        "vendor/lzma/C/LzmaDec.c",
        "vendor/lzma/C/LzmaEnc.c",
        "vendor/lzma/C/LzFind.c",
        "vendor/lzma/C/LzFindMt.c",
        "vendor/lzma/C/LzFindOpt.c",
        "vendor/lzma/C/Threads.c",
        "vendor/lzma/C/LzmaLib.c",
        "vendor/lzma/C/CpuArch.c"
    )
    add_includedirs("vendor/lzma/C")
    add_defines("_7ZIP_ST") -- single-threaded LZMA for simplicity

    -- Crunch (header-only, included from crn_codec.cpp)
    add_includedirs("vendor/crunch")

    -- Native File Dialog Extended
    add_files("vendor/nfd/src/nfd_win.cpp")
    add_includedirs("vendor/nfd/src/include")

    -- System libraries (DirectX 11, Windows)
    add_syslinks("d3d11", "d3dcompiler", "dxgi", "ole32", "shell32", "user32", "gdi32")

    -- Windows subsystem (no console window)
    if is_plat("windows") then
        add_ldflags("/SUBSYSTEM:WINDOWS", "/ENTRY:mainCRTStartup", {force = true})
    end
```

- [ ] **Step 7: Write minimal main.cpp placeholder**

```cpp
// src/main.cpp
#include <cstdio>

int main() {
    printf("Prophecy - Dragon's Prophet IDX Tool\n");
    return 0;
}
```

- [ ] **Step 8: Verify build**

```bash
cd D:/Backups/IDXUnpacker/Prophecy
xmake f -m debug
xmake build
```

Expected: Build succeeds (or warns about missing vendor files — we'll fix paths after download).

- [ ] **Step 9: Commit**

```bash
git init
git add -A
git commit -m "feat: project scaffolding with xmake and vendored deps"
```

---

### Task 2: LZMA codec

**Files:**
- Create: `src/lzma_codec.h`
- Create: `src/lzma_codec.cpp`

- [ ] **Step 1: Write lzma_codec.h**

```cpp
// src/lzma_codec.h
#pragma once
#include <cstdint>
#include <vector>

namespace lzma {

// Decompress LZMA1 data. Input must include the 13-byte LZMA header
// (1 byte props + 4 bytes dict size + 8 bytes uncompressed size).
// Returns decompressed data, or empty vector on failure.
std::vector<uint8_t> decompress(const uint8_t* data, size_t size);

// Compress data using LZMA1. Returns compressed data WITH 13-byte header prepended.
// Returns empty vector on failure.
std::vector<uint8_t> compress(const uint8_t* data, size_t size);

} // namespace lzma
```

- [ ] **Step 2: Write lzma_codec.cpp**

```cpp
// src/lzma_codec.cpp
#include "lzma_codec.h"
#include <LzmaDec.h>
#include <LzmaEnc.h>
#include <cstring>

namespace lzma {

// --- LZMA SDK memory allocator callbacks ---
static void* LzmaAlloc(ISzAllocPtr, size_t size) { return malloc(size); }
static void LzmaFree(ISzAllocPtr, void* ptr) { free(ptr); }
static ISzAlloc g_alloc = { LzmaAlloc, LzmaFree };

std::vector<uint8_t> decompress(const uint8_t* data, size_t size) {
    if (size < 13) return {};

    // Parse LZMA header: 1 byte props, 4 bytes dict, 8 bytes uncompressed size
    uint8_t props = data[0];
    uint64_t uncompressed_size = 0;
    for (int i = 0; i < 8; i++)
        uncompressed_size |= (uint64_t)data[5 + i] << (8 * i);

    // Cap at 512MB to avoid insane allocations
    if (uncompressed_size > 512 * 1024 * 1024) return {};

    std::vector<uint8_t> output(static_cast<size_t>(uncompressed_size));
    size_t dest_len = output.size();
    size_t src_len = size - 13; // skip header

    ELzmaStatus status;
    int res = LzmaDecode(
        output.data(), &dest_len,
        data + 13, &src_len,
        data, 5, // props (first 5 bytes = props + dict size)
        LZMA_FINISH_END, &status, &g_alloc
    );

    if (res != SZ_OK) return {};
    output.resize(dest_len);
    return output;
}

// --- Encoder stream callbacks ---
struct OutStream {
    ISeqOutStream iface;
    std::vector<uint8_t>* buf;
};

static size_t WriteOut(const ISeqOutStream* p, const void* data, size_t size) {
    auto* s = reinterpret_cast<const OutStream*>(p);
    auto* bytes = static_cast<const uint8_t*>(data);
    s->buf->insert(s->buf->end(), bytes, bytes + size);
    return size;
}

struct InStream {
    ISeqInStream iface;
    const uint8_t* data;
    size_t size;
    size_t pos;
};

static SRes ReadIn(const ISeqInStream* p, void* buf, size_t* size) {
    auto* s = const_cast<InStream*>(reinterpret_cast<const InStream*>(p));
    size_t remaining = s->size - s->pos;
    if (*size > remaining) *size = remaining;
    memcpy(buf, s->data + s->pos, *size);
    s->pos += *size;
    return SZ_OK;
}

std::vector<uint8_t> compress(const uint8_t* data, size_t size) {
    CLzmaEncHandle enc = LzmaEnc_Create(&g_alloc);
    if (!enc) return {};

    CLzmaEncProps props;
    LzmaEncProps_Init(&props);
    props.level = 5;
    props.dictSize = 1 << 20; // 1MB dictionary
    LzmaEnc_SetProps(enc, &props);

    // Write props header (5 bytes)
    uint8_t propsEncoded[5];
    size_t propsSize = 5;
    LzmaEnc_WriteProperties(enc, propsEncoded, &propsSize);

    // Build header: 5 bytes props + 8 bytes uncompressed size (LE)
    std::vector<uint8_t> result;
    result.insert(result.end(), propsEncoded, propsEncoded + 5);
    for (int i = 0; i < 8; i++)
        result.push_back(static_cast<uint8_t>((uint64_t)size >> (8 * i)));

    // Compress
    OutStream out;
    out.iface.Write = WriteOut;
    out.buf = &result;

    InStream in;
    in.iface.Read = ReadIn;
    in.data = data;
    in.size = size;
    in.pos = 0;

    SRes res = LzmaEnc_Encode(enc, &out.iface, &in.iface, nullptr, &g_alloc, &g_alloc);
    LzmaEnc_Destroy(enc, &g_alloc, &g_alloc);

    if (res != SZ_OK) return {};
    return result;
}

} // namespace lzma
```

- [ ] **Step 3: Verify it compiles**

```bash
xmake build
```

- [ ] **Step 4: Commit**

```bash
git add src/lzma_codec.h src/lzma_codec.cpp
git commit -m "feat: LZMA codec (compress + decompress)"
```

---

### Task 3: CRN codec

**Files:**
- Create: `src/crn_codec.h`
- Create: `src/crn_codec.cpp`

- [ ] **Step 1: Write crn_codec.h**

```cpp
// src/crn_codec.h
#pragma once
#include <cstdint>
#include <vector>

namespace crn {

// Transcode CRN data to DDS file format.
// Returns a complete DDS file (magic + header + surface data), or empty on failure.
std::vector<uint8_t> crn_to_dds(const uint8_t* crn_data, size_t crn_size);

} // namespace crn
```

- [ ] **Step 2: Write crn_codec.cpp**

```cpp
// src/crn_codec.cpp
#define CRN_DECOMP_IMPLEMENTATION
#include "crn_codec.h"
#include <crn_decomp.h>
#include <cstring>

namespace crn {

struct DDS_PIXELFORMAT {
    uint32_t size, flags, fourCC, rgbBitCount;
    uint32_t rMask, gMask, bMask, aMask;
};

struct DDS_HEADER {
    uint32_t size, flags, height, width;
    uint32_t pitchOrLinearSize, depth, mipMapCount;
    uint32_t reserved1[11];
    DDS_PIXELFORMAT ddspf;
    uint32_t caps, caps2, caps3, caps4, reserved2;
};

static uint32_t format_to_fourcc(crn_format fmt) {
    switch (fmt) {
        case cCRNFmtDXT1:  return 0x31545844; // "DXT1"
        case cCRNFmtDXT3:  return 0x33545844; // "DXT3"
        case cCRNFmtDXT5:  return 0x35545844; // "DXT5"
        case cCRNFmtDXT5A: return 0x31495441; // "ATI1"
        case cCRNFmtDXN_XY: return 0x32495441; // "ATI2"
        default: return 0;
    }
}

static uint32_t bytes_per_block(crn_format fmt) {
    switch (fmt) {
        case cCRNFmtDXT1: case cCRNFmtDXT5A: return 8;
        default: return 16;
    }
}

std::vector<uint8_t> crn_to_dds(const uint8_t* crn_data, size_t crn_size) {
    crnd::crn_texture_info info;
    if (!crnd::crnd_get_texture_info(crn_data, (uint32_t)crn_size, &info))
        return {};

    crnd::crnd_unpack_context ctx = crnd::crnd_unpack_begin(crn_data, (uint32_t)crn_size);
    if (!ctx) return {};

    // Build DDS header
    DDS_HEADER hdr = {};
    hdr.size = 124;
    hdr.flags = 0x81007; // CAPS|HEIGHT|WIDTH|PIXELFORMAT|MIPMAPCOUNT|LINEARSIZE
    hdr.width = info.m_width;
    hdr.height = info.m_height;
    hdr.mipMapCount = info.m_levels;
    hdr.ddspf.size = 32;
    hdr.ddspf.flags = 4; // DDPF_FOURCC
    hdr.ddspf.fourCC = format_to_fourcc(static_cast<crn_format>(info.m_format));
    hdr.caps = 0x1000; // DDSCAPS_TEXTURE
    if (info.m_levels > 1)
        hdr.caps |= 0x400008; // COMPLEX|MIPMAP
        hdr.flags |= 0x20000; // MIPMAPCOUNT

    uint32_t bpb = bytes_per_block(static_cast<crn_format>(info.m_format));
    uint32_t blocks_x = (info.m_width + 3) / 4;
    uint32_t blocks_y = (info.m_height + 3) / 4;
    hdr.pitchOrLinearSize = blocks_x * blocks_y * bpb;

    // Assemble output: magic + header + mip data
    std::vector<uint8_t> result;
    uint32_t magic = 0x20534444; // "DDS "
    result.insert(result.end(), (uint8_t*)&magic, (uint8_t*)&magic + 4);
    result.insert(result.end(), (uint8_t*)&hdr, (uint8_t*)&hdr + sizeof(hdr));

    for (uint32_t level = 0; level < info.m_levels; level++) {
        uint32_t w = (info.m_width >> level)  | 1;
        uint32_t h = (info.m_height >> level) | 1;
        if (w < 1) w = 1;
        if (h < 1) h = 1;

        uint32_t bx = (w + 3) / 4;
        uint32_t by = (h + 3) / 4;
        uint32_t face_size = bx * by * bpb;
        uint32_t row_pitch = bx * bpb;

        std::vector<uint8_t> level_data(face_size);
        void* pDst = level_data.data();

        if (!crnd::crnd_unpack_level(ctx, &pDst, face_size, row_pitch, level)) {
            crnd::crnd_unpack_end(ctx);
            return {};
        }

        result.insert(result.end(), level_data.begin(), level_data.end());
    }

    crnd::crnd_unpack_end(ctx);
    return result;
}

} // namespace crn
```

- [ ] **Step 3: Verify it compiles**

```bash
xmake build
```

- [ ] **Step 4: Commit**

```bash
git add src/crn_codec.h src/crn_codec.cpp
git commit -m "feat: CRN codec (CRN -> DDS transcoder)"
```

---

### Task 4: IDX reader (parser + extractor)

**Files:**
- Create: `src/idx_reader.h`
- Create: `src/idx_reader.cpp`

- [ ] **Step 1: Write idx_reader.h**

```cpp
// src/idx_reader.h
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <functional>

struct IdxHeader {
    uint32_t unknown[5];
    uint32_t file_count;
};

struct IdxEntryFixed {
    uint32_t field_0;
    uint32_t field_1;
    uint32_t field_2;
    uint32_t data_offset;
    uint32_t field_4;
    uint32_t field_5;
    uint8_t  data_file_index;
    uint32_t field_7;
    uint32_t field_8;
};

struct DataChunkHeader {
    uint32_t total_size;       // compressed_total + 28
    uint32_t field_1;
    uint32_t field_2;
    uint32_t field_3;
    uint32_t field_4;
    uint32_t compressed_size;  // data size without 28-byte header
    uint32_t field_6;
};

enum class CompressionType { Raw, Lzma, Crunch };

struct IdxEntry {
    IdxEntryFixed fixed;
    std::string   filename;

    // Populated after reading data chunk header
    DataChunkHeader data_header;
    CompressionType compression;
    std::string     data_file_path;
};

// Progress callback: (current_file_index, total_files) -> return false to cancel
using ProgressCallback = std::function<bool(int current, int total)>;

class IdxReader {
public:
    // Parse an IDX file. Returns false on error.
    bool open(const std::string& idx_path);

    const IdxHeader& header() const { return m_header; }
    const std::vector<IdxEntry>& entries() const { return m_entries; }
    const std::string& error() const { return m_error; }

    // Extract all files to output_dir. Returns number of files extracted.
    int extract_all(const std::string& output_dir, ProgressCallback progress = nullptr);

    // Extract a single entry to output_dir.
    bool extract_entry(const IdxEntry& entry, const std::string& output_dir);

private:
    IdxHeader              m_header = {};
    std::vector<IdxEntry>  m_entries;
    std::string            m_idx_path;
    std::string            m_base_dir;  // directory containing the IDX
    std::string            m_base_name; // IDX filename without extension
    std::string            m_error;

    std::string resolve_data_file(uint8_t index) const;
    CompressionType detect_compression(const DataChunkHeader& hdr,
                                        const std::string& data_file,
                                        uint32_t data_offset) const;
    bool create_directories(const std::string& path) const;
};
```

- [ ] **Step 2: Write idx_reader.cpp**

```cpp
// src/idx_reader.cpp
#include "idx_reader.h"
#include "lzma_codec.h"
#include "crn_codec.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

// Read helpers
static bool read_bytes(FILE* f, void* buf, size_t n) {
    return fread(buf, 1, n, f) == n;
}

static std::string read_null_string(FILE* f) {
    std::string s;
    char c;
    while (fread(&c, 1, 1, f) == 1 && c != '\0')
        s += c;
    return s;
}

std::string IdxReader::resolve_data_file(uint8_t index) const {
    char ext[16];
    snprintf(ext, sizeof(ext), ".p%03u", index);
    return m_base_dir + m_base_name + ext;
}

CompressionType IdxReader::detect_compression(const DataChunkHeader& hdr,
                                               const std::string& data_file,
                                               uint32_t data_offset) const {
    uint32_t adjusted = hdr.compressed_size + 28;
    if (hdr.total_size == adjusted)
        return CompressionType::Raw;

    // Read first byte of data payload
    FILE* f = fopen(data_file.c_str(), "rb");
    if (!f) return CompressionType::Raw;
    fseek(f, data_offset + 28, SEEK_SET);
    uint8_t marker = 0;
    fread(&marker, 1, 1, f);
    fclose(f);

    return (marker == 0x5D) ? CompressionType::Lzma : CompressionType::Crunch;
}

bool IdxReader::open(const std::string& idx_path) {
    m_entries.clear();
    m_error.clear();
    m_idx_path = idx_path;

    // Derive base directory and name
    fs::path p(idx_path);
    m_base_dir = p.parent_path().string();
    if (!m_base_dir.empty() && m_base_dir.back() != '\\' && m_base_dir.back() != '/')
        m_base_dir += '\\';
    m_base_name = p.stem().string();

    FILE* f = fopen(idx_path.c_str(), "rb");
    if (!f) { m_error = "Cannot open IDX file"; return false; }

    // Read 24-byte header
    if (!read_bytes(f, &m_header, 24)) {
        m_error = "Failed to read IDX header";
        fclose(f); return false;
    }

    m_entries.resize(m_header.file_count);

    for (uint32_t i = 0; i < m_header.file_count; i++) {
        auto& e = m_entries[i];

        // Read 33-byte fixed entry (field by field to handle 1-byte alignment)
        read_bytes(f, &e.fixed.field_0, 4);
        read_bytes(f, &e.fixed.field_1, 4);
        read_bytes(f, &e.fixed.field_2, 4);
        read_bytes(f, &e.fixed.data_offset, 4);
        read_bytes(f, &e.fixed.field_4, 4);
        read_bytes(f, &e.fixed.field_5, 4);
        read_bytes(f, &e.fixed.data_file_index, 1);
        read_bytes(f, &e.fixed.field_7, 4);
        read_bytes(f, &e.fixed.field_8, 4);

        // Read null-terminated filename
        e.filename = read_null_string(f);
        e.data_file_path = resolve_data_file(e.fixed.data_file_index);

        // Read data chunk header from data file
        FILE* df = fopen(e.data_file_path.c_str(), "rb");
        if (df) {
            fseek(df, e.fixed.data_offset, SEEK_SET);
            read_bytes(df, &e.data_header, 28);
            fclose(df);
        }

        e.compression = detect_compression(e.data_header, e.data_file_path, e.fixed.data_offset);
    }

    fclose(f);
    return true;
}

bool IdxReader::create_directories(const std::string& path) const {
    fs::path p(path);
    fs::path dir = p.parent_path();
    if (!dir.empty())
        fs::create_directories(dir);
    return true;
}

bool IdxReader::extract_entry(const IdxEntry& entry, const std::string& output_dir) {
    std::string out_path = output_dir;
    if (!out_path.empty() && out_path.back() != '\\' && out_path.back() != '/')
        out_path += '\\';
    out_path += entry.filename;

    // Normalize separators
    std::replace(out_path.begin(), out_path.end(), '/', '\\');
    create_directories(out_path);

    uint32_t data_start = entry.fixed.data_offset + 28;

    if (entry.compression == CompressionType::Raw) {
        // Raw copy
        uint32_t raw_size = entry.data_header.compressed_size;
        FILE* src = fopen(entry.data_file_path.c_str(), "rb");
        if (!src) return false;
        fseek(src, data_start, SEEK_SET);

        std::vector<uint8_t> buf(raw_size);
        fread(buf.data(), 1, raw_size, src);
        fclose(src);

        FILE* dst = fopen(out_path.c_str(), "wb");
        if (!dst) return false;
        fwrite(buf.data(), 1, raw_size, dst);
        fclose(dst);
        return true;
    }

    if (entry.compression == CompressionType::Lzma) {
        // Read compressed data (includes LZMA header)
        uint32_t comp_size = entry.data_header.total_size - 28;
        FILE* src = fopen(entry.data_file_path.c_str(), "rb");
        if (!src) return false;
        fseek(src, data_start, SEEK_SET);

        std::vector<uint8_t> comp(comp_size);
        fread(comp.data(), 1, comp_size, src);
        fclose(src);

        auto decompressed = lzma::decompress(comp.data(), comp.size());
        if (decompressed.empty()) return false;

        FILE* dst = fopen(out_path.c_str(), "wb");
        if (!dst) return false;
        fwrite(decompressed.data(), 1, decompressed.size(), dst);
        fclose(dst);
        return true;
    }

    if (entry.compression == CompressionType::Crunch) {
        // Read CRN data
        uint32_t crn_size = entry.data_header.total_size - 28;
        FILE* src = fopen(entry.data_file_path.c_str(), "rb");
        if (!src) return false;
        fseek(src, data_start, SEEK_SET);

        std::vector<uint8_t> crn_data(crn_size);
        fread(crn_data.data(), 1, crn_size, src);
        fclose(src);

        auto dds = crn::crn_to_dds(crn_data.data(), crn_data.size());
        if (dds.empty()) return false;

        // Output gets .dds extension if not already
        FILE* dst = fopen(out_path.c_str(), "wb");
        if (!dst) return false;
        fwrite(dds.data(), 1, dds.size(), dst);
        fclose(dst);
        return true;
    }

    return false;
}

int IdxReader::extract_all(const std::string& output_dir, ProgressCallback progress) {
    int extracted = 0;
    int total = static_cast<int>(m_entries.size());
    for (int i = 0; i < total; i++) {
        if (progress && !progress(i, total))
            break; // cancelled
        if (extract_entry(m_entries[i], output_dir))
            extracted++;
    }
    if (progress) progress(total, total);
    return extracted;
}
```

- [ ] **Step 3: Verify it compiles**

```bash
xmake build
```

- [ ] **Step 4: Commit**

```bash
git add src/idx_reader.h src/idx_reader.cpp
git commit -m "feat: IDX reader (parse + extract with LZMA/CRN/raw)"
```

---

### Task 5: IDX writer (packer)

**Files:**
- Create: `src/idx_writer.h`
- Create: `src/idx_writer.cpp`

- [ ] **Step 1: Write idx_writer.h**

```cpp
// src/idx_writer.h
#pragma once
#include "idx_reader.h" // reuse IdxHeader, IdxEntryFixed, DataChunkHeader structs
#include <string>
#include <vector>
#include <functional>

enum class PackCompression { Raw, Lzma, Crunch };

struct PackOptions {
    PackCompression compression = PackCompression::Lzma;
    uint32_t max_data_file_size = 0; // 0 = single file, else split threshold in bytes
};

class IdxWriter {
public:
    // Pack a directory into an IDX archive.
    // idx_output_path: path for the .idx file (data files auto-named .p001 etc.)
    // Returns true on success.
    bool pack(const std::string& source_dir,
              const std::string& idx_output_path,
              const PackOptions& options,
              ProgressCallback progress = nullptr);

    const std::string& error() const { return m_error; }

private:
    std::string m_error;
};
```

- [ ] **Step 2: Write idx_writer.cpp**

```cpp
// src/idx_writer.cpp
#include "idx_writer.h"
#include "lzma_codec.h"
#include <cstdio>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

bool IdxWriter::pack(const std::string& source_dir,
                     const std::string& idx_output_path,
                     const PackOptions& options,
                     ProgressCallback progress) {
    m_error.clear();

    // Collect all files recursively
    std::vector<std::string> rel_paths;
    fs::path src_root(source_dir);
    for (auto& entry : fs::recursive_directory_iterator(src_root)) {
        if (entry.is_regular_file()) {
            auto rel = fs::relative(entry.path(), src_root).string();
            rel_paths.push_back(rel);
        }
    }

    if (rel_paths.empty()) {
        m_error = "No files found in source directory";
        return false;
    }

    int total = static_cast<int>(rel_paths.size());

    // Derive base name for data files
    fs::path idx_p(idx_output_path);
    std::string base_dir = idx_p.parent_path().string();
    if (!base_dir.empty() && base_dir.back() != '\\') base_dir += '\\';
    std::string base_name = idx_p.stem().string();

    // Open first data file
    uint8_t current_data_index = 1;
    char data_ext[16];
    snprintf(data_ext, sizeof(data_ext), ".p%03u", current_data_index);
    std::string data_path = base_dir + base_name + data_ext;
    FILE* data_file = fopen(data_path.c_str(), "wb");
    if (!data_file) { m_error = "Cannot create data file"; return false; }

    uint32_t data_offset = 0;

    // Build entries
    struct WriteEntry {
        IdxEntryFixed fixed;
        std::string filename;
    };
    std::vector<WriteEntry> entries;

    for (int i = 0; i < total; i++) {
        if (progress && !progress(i, total)) {
            fclose(data_file);
            return false;
        }

        std::string full_path = source_dir;
        if (full_path.back() != '\\' && full_path.back() != '/')
            full_path += '\\';
        full_path += rel_paths[i];

        // Read source file
        FILE* src = fopen(full_path.c_str(), "rb");
        if (!src) continue;
        fseek(src, 0, SEEK_END);
        size_t file_size = ftell(src);
        fseek(src, 0, SEEK_SET);
        std::vector<uint8_t> file_data(file_size);
        fread(file_data.data(), 1, file_size, src);
        fclose(src);

        // Compress if requested
        std::vector<uint8_t> payload;
        bool is_compressed = false;
        if (options.compression == PackCompression::Lzma && file_size > 0) {
            payload = lzma::compress(file_data.data(), file_data.size());
            if (!payload.empty()) {
                is_compressed = true;
            }
        }

        if (!is_compressed) {
            payload = std::move(file_data);
        }

        // Check if we need to split to a new data file
        if (options.max_data_file_size > 0 &&
            data_offset > 0 &&
            data_offset + 28 + payload.size() > options.max_data_file_size) {
            fclose(data_file);
            current_data_index++;
            snprintf(data_ext, sizeof(data_ext), ".p%03u", current_data_index);
            data_path = base_dir + base_name + data_ext;
            data_file = fopen(data_path.c_str(), "wb");
            if (!data_file) { m_error = "Cannot create data file"; return false; }
            data_offset = 0;
        }

        // Write 28-byte data chunk header
        DataChunkHeader dhdr = {};
        if (is_compressed) {
            dhdr.total_size = static_cast<uint32_t>(payload.size()) + 28;
            dhdr.compressed_size = static_cast<uint32_t>(file_size); // original size
        } else {
            dhdr.total_size = static_cast<uint32_t>(payload.size()) + 28;
            dhdr.compressed_size = static_cast<uint32_t>(payload.size()); // same = raw
        }
        fwrite(&dhdr, 1, 28, data_file);

        // Write payload
        fwrite(payload.data(), 1, payload.size(), data_file);

        // Build IDX entry
        WriteEntry we;
        memset(&we.fixed, 0, sizeof(we.fixed));
        we.fixed.data_offset = data_offset;
        we.fixed.data_file_index = current_data_index;
        we.filename = rel_paths[i];
        // Normalize to backslashes
        std::replace(we.filename.begin(), we.filename.end(), '/', '\\');
        entries.push_back(we);

        data_offset += 28 + static_cast<uint32_t>(payload.size());
    }

    fclose(data_file);

    // Write IDX file
    FILE* idx = fopen(idx_output_path.c_str(), "wb");
    if (!idx) { m_error = "Cannot create IDX file"; return false; }

    // 24-byte header
    IdxHeader hdr = {};
    hdr.file_count = static_cast<uint32_t>(entries.size());
    fwrite(&hdr, 1, 24, idx);

    // Write entries
    for (auto& e : entries) {
        fwrite(&e.fixed.field_0, 4, 1, idx);
        fwrite(&e.fixed.field_1, 4, 1, idx);
        fwrite(&e.fixed.field_2, 4, 1, idx);
        fwrite(&e.fixed.data_offset, 4, 1, idx);
        fwrite(&e.fixed.field_4, 4, 1, idx);
        fwrite(&e.fixed.field_5, 4, 1, idx);
        fwrite(&e.fixed.data_file_index, 1, 1, idx);
        fwrite(&e.fixed.field_7, 4, 1, idx);
        fwrite(&e.fixed.field_8, 4, 1, idx);
        // Null-terminated filename
        fwrite(e.filename.c_str(), 1, e.filename.size() + 1, idx);
    }

    fclose(idx);
    if (progress) progress(total, total);
    return true;
}
```

- [ ] **Step 3: Verify it compiles**

```bash
xmake build
```

- [ ] **Step 4: Commit**

```bash
git add src/idx_writer.h src/idx_writer.cpp
git commit -m "feat: IDX writer (pack directory into IDX/P00x archive)"
```

---

### Task 6: ImGui theme (dark + red glow)

**Files:**
- Create: `src/theme.h`
- Create: `src/theme.cpp`

- [ ] **Step 1: Write theme.h**

```cpp
// src/theme.h
#pragma once
#include <imgui.h>

namespace theme {

void apply_dark_red();

// Call each frame to render glow effects behind active elements
void render_glow_rect(ImVec2 min, ImVec2 max, float intensity = 1.0f);
void render_glow_progress(ImVec2 min, ImVec2 max, float fraction);

} // namespace theme
```

- [ ] **Step 2: Write theme.cpp**

```cpp
// src/theme.cpp
#include "theme.h"
#include <imgui_internal.h>
#include <cmath>

namespace theme {

static constexpr ImU32 COL_BG         = IM_COL32(13, 13, 13, 255);
static constexpr ImU32 COL_PANEL      = IM_COL32(26, 26, 26, 255);
static constexpr ImU32 COL_BORDER     = IM_COL32(42, 42, 42, 255);
static constexpr ImU32 COL_TEXT       = IM_COL32(220, 220, 220, 255);
static constexpr ImU32 COL_TEXT_DIM   = IM_COL32(120, 120, 120, 255);
static constexpr ImU32 COL_RED        = IM_COL32(255, 34, 34, 255);
static constexpr ImU32 COL_RED_HOVER  = IM_COL32(255, 68, 68, 255);
static constexpr ImU32 COL_RED_ACTIVE = IM_COL32(204, 17, 17, 255);
static constexpr ImU32 COL_RED_DIM    = IM_COL32(80, 10, 10, 255);
static constexpr ImU32 COL_GLOW       = IM_COL32(255, 34, 34, 60);

static ImVec4 u32_to_vec4(ImU32 c) {
    return ImVec4(
        ((c >> 0)  & 0xFF) / 255.0f,
        ((c >> 8)  & 0xFF) / 255.0f,
        ((c >> 16) & 0xFF) / 255.0f,
        ((c >> 24) & 0xFF) / 255.0f
    );
}

void apply_dark_red() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding    = 6.0f;
    s.FrameRounding     = 4.0f;
    s.GrabRounding      = 4.0f;
    s.TabRounding       = 4.0f;
    s.PopupRounding     = 4.0f;
    s.ScrollbarRounding = 4.0f;
    s.ChildRounding     = 4.0f;
    s.WindowBorderSize  = 1.0f;
    s.FrameBorderSize   = 0.0f;
    s.FramePadding      = ImVec2(8, 5);
    s.ItemSpacing       = ImVec2(8, 6);
    s.WindowPadding     = ImVec2(12, 12);

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]           = u32_to_vec4(COL_BG);
    c[ImGuiCol_ChildBg]            = u32_to_vec4(COL_PANEL);
    c[ImGuiCol_PopupBg]            = u32_to_vec4(IM_COL32(20, 20, 20, 240));
    c[ImGuiCol_Border]             = u32_to_vec4(COL_BORDER);
    c[ImGuiCol_FrameBg]            = u32_to_vec4(IM_COL32(35, 35, 35, 255));
    c[ImGuiCol_FrameBgHovered]     = u32_to_vec4(IM_COL32(50, 50, 50, 255));
    c[ImGuiCol_FrameBgActive]      = u32_to_vec4(IM_COL32(60, 30, 30, 255));
    c[ImGuiCol_TitleBg]            = u32_to_vec4(IM_COL32(15, 15, 15, 255));
    c[ImGuiCol_TitleBgActive]      = u32_to_vec4(IM_COL32(30, 10, 10, 255));
    c[ImGuiCol_Tab]                = u32_to_vec4(IM_COL32(30, 30, 30, 255));
    c[ImGuiCol_TabHovered]         = u32_to_vec4(IM_COL32(80, 20, 20, 255));
    c[ImGuiCol_TabSelected]        = u32_to_vec4(IM_COL32(60, 15, 15, 255));
    c[ImGuiCol_Button]             = u32_to_vec4(COL_RED_DIM);
    c[ImGuiCol_ButtonHovered]      = u32_to_vec4(COL_RED);
    c[ImGuiCol_ButtonActive]       = u32_to_vec4(COL_RED_ACTIVE);
    c[ImGuiCol_Header]             = u32_to_vec4(IM_COL32(50, 15, 15, 255));
    c[ImGuiCol_HeaderHovered]      = u32_to_vec4(IM_COL32(80, 20, 20, 255));
    c[ImGuiCol_HeaderActive]       = u32_to_vec4(IM_COL32(100, 25, 25, 255));
    c[ImGuiCol_Text]               = u32_to_vec4(COL_TEXT);
    c[ImGuiCol_TextDisabled]       = u32_to_vec4(COL_TEXT_DIM);
    c[ImGuiCol_ScrollbarBg]        = u32_to_vec4(IM_COL32(15, 15, 15, 255));
    c[ImGuiCol_ScrollbarGrab]      = u32_to_vec4(IM_COL32(50, 50, 50, 255));
    c[ImGuiCol_ScrollbarGrabHovered] = u32_to_vec4(COL_RED_DIM);
    c[ImGuiCol_ScrollbarGrabActive]  = u32_to_vec4(COL_RED);
    c[ImGuiCol_CheckMark]          = u32_to_vec4(COL_RED);
    c[ImGuiCol_SliderGrab]         = u32_to_vec4(COL_RED);
    c[ImGuiCol_SliderGrabActive]   = u32_to_vec4(COL_RED_HOVER);
    c[ImGuiCol_Separator]          = u32_to_vec4(COL_BORDER);
    c[ImGuiCol_TableHeaderBg]      = u32_to_vec4(IM_COL32(25, 25, 25, 255));
    c[ImGuiCol_TableBorderStrong]  = u32_to_vec4(COL_BORDER);
    c[ImGuiCol_TableBorderLight]   = u32_to_vec4(IM_COL32(35, 35, 35, 255));
    c[ImGuiCol_PlotHistogram]      = u32_to_vec4(COL_RED);
    c[ImGuiCol_ProgressBarActive]  = u32_to_vec4(COL_RED);
}

void render_glow_rect(ImVec2 min, ImVec2 max, float intensity) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float expand = 6.0f * intensity;
    ImU32 glow = IM_COL32(255, 34, 34, (int)(40 * intensity));
    for (int i = 3; i >= 1; i--) {
        float e = expand * (float)i / 3.0f;
        dl->AddRectFilled(
            ImVec2(min.x - e, min.y - e),
            ImVec2(max.x + e, max.y + e),
            IM_COL32(255, 34, 34, (int)(15 * intensity * i / 3.0f)),
            8.0f
        );
    }
}

void render_glow_progress(ImVec2 min, ImVec2 max, float fraction) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float fill_w = (max.x - min.x) * fraction;
    ImVec2 fill_max = ImVec2(min.x + fill_w, max.y);

    // Glow behind filled portion
    if (fraction > 0.01f) {
        for (int i = 3; i >= 1; i--) {
            float e = 4.0f * (float)i / 3.0f;
            dl->AddRectFilled(
                ImVec2(min.x - e, min.y - e),
                ImVec2(fill_max.x + e, fill_max.y + e),
                IM_COL32(255, 34, 34, 12 * i),
                4.0f
            );
        }
    }

    // Background
    dl->AddRectFilled(min, max, IM_COL32(35, 35, 35, 255), 4.0f);
    // Fill
    if (fraction > 0.01f)
        dl->AddRectFilled(min, fill_max, IM_COL32(255, 34, 34, 255), 4.0f);
    // Border
    dl->AddRect(min, max, IM_COL32(60, 60, 60, 255), 4.0f);
}

} // namespace theme
```

- [ ] **Step 3: Commit**

```bash
git add src/theme.h src/theme.cpp
git commit -m "feat: dark + red glow ImGui theme"
```

---

### Task 7: Animation system

**Files:**
- Create: `src/anim.h`
- Create: `src/anim.cpp`

- [ ] **Step 1: Write anim.h**

```cpp
// src/anim.h
#pragma once
#include <imgui.h>
#include <unordered_map>
#include <string>

namespace anim {

// Initialize animation state. Call once at startup.
void init();

// Call once per frame before any animation queries.
void update();

// Ease-out lerp: approaches target smoothly.
// id: unique string key for this animation.
// target: desired value.
// speed: transition speed (higher = faster). 6.0 ~ 150ms, 4.0 ~ 200ms, 3.0 ~ 300ms.
float lerp(const char* id, float target, float speed = 6.0f);

// Fade alpha for tab switching. Returns 0..1 alpha.
float tab_fade(int active_tab);

// Staggered fade for list rows. Returns alpha for row i.
// Call reset_stagger() when list content changes.
float row_fade(int row_index);
void  reset_stagger();

// Status message with auto-fade. Returns current alpha (0 when expired).
void  set_status(const std::string& msg, float duration = 3.0f);
float status_alpha();
const std::string& status_text();

// Window fade-in. Returns alpha 0..1.
float window_fade();

} // namespace anim
```

- [ ] **Step 2: Write anim.cpp**

```cpp
// src/anim.cpp
#include "anim.h"
#include <algorithm>
#include <cmath>

namespace anim {

static float s_dt = 0.016f;
static std::unordered_map<std::string, float> s_values;

// Tab fade
static int   s_prev_tab = -1;
static float s_tab_alpha = 1.0f;
static bool  s_tab_fading_out = false;
static int   s_tab_target = 0;

// Stagger
static float s_stagger_start = 0.0f;
static bool  s_stagger_reset = false;

// Status
static std::string s_status_msg;
static float s_status_timer = 0.0f;
static float s_status_duration = 3.0f;
static float s_status_alpha = 0.0f;

// Window
static float s_window_alpha = 0.0f;
static float s_total_time = 0.0f;

static float ease_out(float t) {
    return 1.0f - powf(1.0f - t, 3.0f);
}

void init() {
    s_values.clear();
    s_window_alpha = 0.0f;
    s_total_time = 0.0f;
    s_prev_tab = -1;
    s_tab_alpha = 0.0f;
}

void update() {
    s_dt = ImGui::GetIO().DeltaTime;
    s_total_time += s_dt;

    // Window fade-in
    if (s_window_alpha < 1.0f) {
        s_window_alpha += s_dt * 3.3f; // ~300ms
        if (s_window_alpha > 1.0f) s_window_alpha = 1.0f;
    }

    // Status fade
    if (s_status_timer > 0.0f) {
        s_status_timer -= s_dt;
        if (s_status_timer > 0.5f)
            s_status_alpha = std::min(s_status_alpha + s_dt * 6.0f, 1.0f);
        else
            s_status_alpha = std::max(s_status_alpha - s_dt * 2.0f, 0.0f);
    } else {
        s_status_alpha = std::max(s_status_alpha - s_dt * 4.0f, 0.0f);
    }

    // Stagger timer
    if (s_stagger_reset) {
        s_stagger_start = s_total_time;
        s_stagger_reset = false;
    }
}

float lerp(const char* id, float target, float speed) {
    auto it = s_values.find(id);
    if (it == s_values.end()) {
        s_values[id] = target;
        return target;
    }
    float& val = it->second;
    float diff = target - val;
    val += diff * std::min(s_dt * speed, 1.0f);
    // Snap when close
    if (fabsf(diff) < 0.001f) val = target;
    return val;
}

float tab_fade(int active_tab) {
    if (s_prev_tab == -1) {
        s_prev_tab = active_tab;
        s_tab_alpha = 1.0f;
        return 1.0f;
    }
    if (active_tab != s_prev_tab) {
        s_prev_tab = active_tab;
        s_tab_alpha = 0.0f;
    }
    s_tab_alpha += s_dt * 5.0f; // ~200ms
    if (s_tab_alpha > 1.0f) s_tab_alpha = 1.0f;
    return ease_out(s_tab_alpha);
}

float row_fade(int row_index) {
    float elapsed = s_total_time - s_stagger_start;
    float row_delay = row_index * 0.015f; // 15ms per row
    float row_elapsed = elapsed - row_delay;
    if (row_elapsed < 0.0f) return 0.0f;
    float t = std::min(row_elapsed * 5.0f, 1.0f); // 200ms fade
    return ease_out(t);
}

void reset_stagger() {
    s_stagger_reset = true;
}

void set_status(const std::string& msg, float duration) {
    s_status_msg = msg;
    s_status_timer = duration;
    s_status_duration = duration;
    s_status_alpha = 0.0f;
}

float status_alpha() { return s_status_alpha; }
const std::string& status_text() { return s_status_msg; }

float window_fade() {
    return ease_out(s_window_alpha);
}

} // namespace anim
```

- [ ] **Step 3: Commit**

```bash
git add src/anim.h src/anim.cpp
git commit -m "feat: animation system (lerp, tab fade, stagger, status)"
```

---

### Task 8: DX11 + ImGui main loop

**Files:**
- Modify: `src/main.cpp` (full rewrite)

- [ ] **Step 1: Write main.cpp with DX11 + ImGui setup**

```cpp
// src/main.cpp
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <d3d11.h>
#include <tchar.h>

#include "app.h"
#include "theme.h"
#include "anim.h"

// DX11 globals
static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;

static void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

static void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

static bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT flags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };
    HRESULT res = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        levels, 1, D3D11_SDK_VERSION, &sd,
        &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK) return false;
    CreateRenderTarget();
    return true;
}

static void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;
    switch (msg) {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED) {
            g_ResizeWidth = (UINT)LOWORD(lParam);
            g_ResizeHeight = (UINT)HIWORD(lParam);
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int main() {
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0, 0,
                       GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr,
                       L"ProphecyClass", nullptr };
    RegisterClassExW(&wc);
    HWND hwnd = CreateWindowW(wc.lpszClassName, L"Prophecy - Dragon's Prophet IDX Tool",
                              WS_OVERLAPPEDWINDOW, 100, 100, 1100, 700,
                              nullptr, nullptr, wc.hInstance, nullptr);

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    theme::apply_dark_red();
    anim::init();
    App app;

    bool done = false;
    while (!done) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;

        if (g_ResizeWidth != 0 && g_ResizeHeight != 0) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        anim::update();

        // Window fade
        float alpha = anim::window_fade();
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
        app.render();
        ImGui::PopStyleVar();

        ImGui::Render();
        const float clear[4] = { 0.05f, 0.05f, 0.05f, 1.0f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0); // vsync
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}
```

- [ ] **Step 2: Commit**

```bash
git add src/main.cpp
git commit -m "feat: DX11 + ImGui main loop with theme and animations"
```

---

### Task 9: App UI — Unpack tab

**Files:**
- Create: `src/app.h`
- Create: `src/app.cpp`

- [ ] **Step 1: Write app.h**

```cpp
// src/app.h
#pragma once
#include "idx_reader.h"
#include "idx_writer.h"
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>

class App {
public:
    App();
    ~App();
    void render();

private:
    // Tab state
    int m_active_tab = 0; // 0=Unpack, 1=Pack

    // --- Unpack state ---
    std::string m_idx_path;
    IdxReader   m_reader;
    bool        m_idx_loaded = false;
    std::string m_unpack_output_dir;

    // --- Pack state ---
    std::string    m_pack_source_dir;
    std::string    m_pack_output_path;
    int            m_pack_compression = 1; // 0=Raw, 1=LZMA, 2=CRN
    std::vector<std::string> m_pack_files_preview;

    // --- Worker thread ---
    std::thread       m_worker;
    std::atomic<bool> m_working{false};
    std::atomic<bool> m_cancel{false};
    std::atomic<float> m_progress{0.0f};
    std::mutex        m_status_mutex;
    std::string       m_status_msg;

    void render_unpack_tab();
    void render_pack_tab();
    void render_status_bar();

    void start_unpack();
    void start_pack();
    void set_status(const std::string& msg);
};
```

- [ ] **Step 2: Write app.cpp**

```cpp
// src/app.cpp
#include "app.h"
#include "theme.h"
#include "anim.h"
#include <imgui.h>
#include <nfd.h>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

App::App() { NFD_Init(); }
App::~App() {
    m_cancel = true;
    if (m_worker.joinable()) m_worker.join();
    NFD_Quit();
}

void App::set_status(const std::string& msg) {
    std::lock_guard<std::mutex> lock(m_status_mutex);
    m_status_msg = msg;
    anim::set_status(msg);
}

void App::render() {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::Begin("##Main", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    // Title
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.13f, 0.13f, 1.0f));
    ImGui::Text("PROPHECY");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextDisabled("Dragon's Prophet IDX Tool");
    ImGui::Separator();

    // Tabs
    if (ImGui::BeginTabBar("##Tabs")) {
        if (ImGui::BeginTabItem("Unpack")) { m_active_tab = 0; ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Pack"))   { m_active_tab = 1; ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }

    // Animated tab content
    float tab_alpha = anim::tab_fade(m_active_tab);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * tab_alpha);

    if (m_active_tab == 0) render_unpack_tab();
    else                    render_pack_tab();

    ImGui::PopStyleVar();

    render_status_bar();
    ImGui::End();
}

void App::render_unpack_tab() {
    // File picker row
    ImGui::Text("IDX File:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 90);
    ImGui::InputText("##idx_path", &m_idx_path[0], m_idx_path.capacity() + 1,
                     ImGuiInputTextFlags_ReadOnly);
    ImGui::SameLine();
    if (ImGui::Button("Browse##idx", ImVec2(80, 0))) {
        nfdu8char_t* outPath = nullptr;
        nfdu8filteritem_t filters[] = { { "IDX Files", "idx" } };
        nfdopendialogu8args_t args = {};
        args.filterList = filters;
        args.filterCount = 1;
        if (NFD_OpenDialogU8_With(&outPath, &args) == NFD_OKAY) {
            m_idx_path = outPath;
            NFD_FreePathU8(outPath);
            m_idx_loaded = m_reader.open(m_idx_path);
            if (m_idx_loaded) {
                // Default output dir = IDX path without extension
                fs::path p(m_idx_path);
                m_unpack_output_dir = (p.parent_path() / p.stem()).string();
                anim::reset_stagger();
                set_status("Loaded " + std::to_string(m_reader.entries().size()) + " entries");
            } else {
                set_status("Error: " + m_reader.error());
            }
        }
    }

    // Output directory
    ImGui::Text("Output:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 90);
    char out_buf[512] = {};
    strncpy(out_buf, m_unpack_output_dir.c_str(), sizeof(out_buf) - 1);
    if (ImGui::InputText("##out_dir", out_buf, sizeof(out_buf)))
        m_unpack_output_dir = out_buf;
    ImGui::SameLine();
    if (ImGui::Button("Browse##out", ImVec2(80, 0))) {
        nfdu8char_t* outPath = nullptr;
        if (NFD_PickFolderU8(&outPath, nullptr) == NFD_OKAY) {
            m_unpack_output_dir = outPath;
            NFD_FreePathU8(outPath);
        }
    }

    ImGui::Spacing();

    // Extract button + progress
    bool can_extract = m_idx_loaded && !m_working;
    if (!can_extract) ImGui::BeginDisabled();
    if (ImGui::Button("Extract All", ImVec2(120, 30))) {
        start_unpack();
    }
    if (!can_extract) ImGui::EndDisabled();

    if (m_working) {
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(80, 30))) {
            m_cancel = true;
        }
    }

    // Progress bar with glow
    if (m_working || m_progress > 0.01f) {
        float smooth_progress = anim::lerp("unpack_progress", m_progress.load(), 8.0f);
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        ImVec2 bar_min = cursor;
        ImVec2 bar_max = ImVec2(cursor.x + ImGui::GetContentRegionAvail().x, cursor.y + 20);
        theme::render_glow_progress(bar_min, bar_max, smooth_progress);
        ImGui::Dummy(ImVec2(0, 24));

        ImGui::Text("%.0f%%", smooth_progress * 100.0f);
    }

    ImGui::Spacing();

    // File list table
    if (m_idx_loaded && ImGui::BeginTable("##files", 5,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
            ImVec2(0, ImGui::GetContentRegionAvail().y - 30))) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_None, 300);
        ImGui::TableSetupColumn("Offset", ImGuiTableColumnFlags_None, 80);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_None, 80);
        ImGui::TableSetupColumn("Compressed", ImGuiTableColumnFlags_None, 80);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_None, 60);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        auto& entries = m_reader.entries();
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(entries.size()));
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                auto& e = entries[i];
                float row_alpha = anim::row_fade(i);
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * row_alpha);

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextUnformatted(e.filename.c_str());
                ImGui::TableNextColumn(); ImGui::Text("%08X", e.fixed.data_offset);
                ImGui::TableNextColumn(); ImGui::Text("%u", e.data_header.total_size);
                ImGui::TableNextColumn(); ImGui::Text("%u", e.data_header.compressed_size + 28);
                ImGui::TableNextColumn();
                const char* type_str = "Raw";
                if (e.compression == CompressionType::Lzma) type_str = "LZMA";
                else if (e.compression == CompressionType::Crunch) type_str = "CRN";
                ImGui::TextUnformatted(type_str);

                ImGui::PopStyleVar();
            }
        }
        ImGui::EndTable();
    }
}

void App::render_pack_tab() {
    // Source folder
    ImGui::Text("Source Folder:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 90);
    char src_buf[512] = {};
    strncpy(src_buf, m_pack_source_dir.c_str(), sizeof(src_buf) - 1);
    if (ImGui::InputText("##pack_src", src_buf, sizeof(src_buf)))
        m_pack_source_dir = src_buf;
    ImGui::SameLine();
    if (ImGui::Button("Browse##psrc", ImVec2(80, 0))) {
        nfdu8char_t* outPath = nullptr;
        if (NFD_PickFolderU8(&outPath, nullptr) == NFD_OKAY) {
            m_pack_source_dir = outPath;
            NFD_FreePathU8(outPath);
            // Preview files
            m_pack_files_preview.clear();
            for (auto& entry : fs::recursive_directory_iterator(m_pack_source_dir)) {
                if (entry.is_regular_file())
                    m_pack_files_preview.push_back(
                        fs::relative(entry.path(), m_pack_source_dir).string());
            }
            anim::reset_stagger();
            set_status(std::to_string(m_pack_files_preview.size()) + " files found");
        }
    }

    // Output IDX
    ImGui::Text("Output IDX:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 90);
    char out_buf[512] = {};
    strncpy(out_buf, m_pack_output_path.c_str(), sizeof(out_buf) - 1);
    if (ImGui::InputText("##pack_out", out_buf, sizeof(out_buf)))
        m_pack_output_path = out_buf;
    ImGui::SameLine();
    if (ImGui::Button("Browse##pout", ImVec2(80, 0))) {
        nfdu8char_t* outPath = nullptr;
        nfdu8filteritem_t filters[] = { { "IDX Files", "idx" } };
        nfdsavedialogu8args_t args = {};
        args.filterList = filters;
        args.filterCount = 1;
        if (NFD_SaveDialogU8_With(&outPath, &args) == NFD_OKAY) {
            m_pack_output_path = outPath;
            NFD_FreePathU8(outPath);
        }
    }

    // Compression selector
    ImGui::Text("Compression:");
    ImGui::SameLine();
    ImGui::RadioButton("Raw", &m_pack_compression, 0); ImGui::SameLine();
    ImGui::RadioButton("LZMA", &m_pack_compression, 1); ImGui::SameLine();
    ImGui::RadioButton("CRN (textures)", &m_pack_compression, 2);

    ImGui::Spacing();

    // Pack button
    bool can_pack = !m_pack_source_dir.empty() && !m_pack_output_path.empty() && !m_working;
    if (!can_pack) ImGui::BeginDisabled();
    if (ImGui::Button("Pack", ImVec2(120, 30))) {
        start_pack();
    }
    if (!can_pack) ImGui::EndDisabled();

    if (m_working) {
        ImGui::SameLine();
        if (ImGui::Button("Cancel##pack", ImVec2(80, 30))) {
            m_cancel = true;
        }
    }

    // Progress
    if (m_working || m_progress > 0.01f) {
        float smooth_progress = anim::lerp("pack_progress", m_progress.load(), 8.0f);
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        ImVec2 bar_min = cursor;
        ImVec2 bar_max = ImVec2(cursor.x + ImGui::GetContentRegionAvail().x, cursor.y + 20);
        theme::render_glow_progress(bar_min, bar_max, smooth_progress);
        ImGui::Dummy(ImVec2(0, 24));
        ImGui::Text("%.0f%%", smooth_progress * 100.0f);
    }

    ImGui::Spacing();

    // File preview list
    if (!m_pack_files_preview.empty() && ImGui::BeginTable("##pack_files", 1,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_ScrollY,
            ImVec2(0, ImGui::GetContentRegionAvail().y - 30))) {
        ImGui::TableSetupColumn("Files to pack");
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(m_pack_files_preview.size()));
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                float row_alpha = anim::row_fade(i);
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * row_alpha);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(m_pack_files_preview[i].c_str());
                ImGui::PopStyleVar();
            }
        }
        ImGui::EndTable();
    }
}

void App::render_status_bar() {
    float alpha = anim::status_alpha();
    if (alpha > 0.01f) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
        ImGui::TextDisabled("%s", anim::status_text().c_str());
        ImGui::PopStyleVar();
    }
}

void App::start_unpack() {
    if (m_worker.joinable()) m_worker.join();
    m_working = true;
    m_cancel = false;
    m_progress = 0.0f;

    std::string out_dir = m_unpack_output_dir;
    m_worker = std::thread([this, out_dir]() {
        int count = m_reader.extract_all(out_dir, [this](int cur, int total) -> bool {
            m_progress = total > 0 ? (float)cur / (float)total : 0.0f;
            return !m_cancel.load();
        });
        m_progress = 1.0f;
        m_working = false;
        set_status("Extracted " + std::to_string(count) + " files");
    });
}

void App::start_pack() {
    if (m_worker.joinable()) m_worker.join();
    m_working = true;
    m_cancel = false;
    m_progress = 0.0f;

    std::string src = m_pack_source_dir;
    std::string out = m_pack_output_path;
    int comp = m_pack_compression;

    m_worker = std::thread([this, src, out, comp]() {
        PackOptions opts;
        if (comp == 0) opts.compression = PackCompression::Raw;
        else if (comp == 1) opts.compression = PackCompression::Lzma;
        else opts.compression = PackCompression::Crunch;

        IdxWriter writer;
        bool ok = writer.pack(src, out, opts, [this](int cur, int total) -> bool {
            m_progress = total > 0 ? (float)cur / (float)total : 0.0f;
            return !m_cancel.load();
        });
        m_progress = 1.0f;
        m_working = false;
        if (ok) set_status("Pack complete: " + out);
        else    set_status("Pack failed: " + writer.error());
    });
}
```

- [ ] **Step 3: Verify it compiles**

```bash
xmake build
```

- [ ] **Step 4: Commit**

```bash
git add src/app.h src/app.cpp
git commit -m "feat: app UI with unpack and pack tabs, animations, glow progress"
```

---

### Task 10: Final integration and build verification

- [ ] **Step 1: Verify full build**

```bash
cd D:/Backups/IDXUnpacker/Prophecy
xmake f -m release
xmake build
```

Fix any remaining compilation errors (missing includes, linker issues). Common fixes:
- Ensure `nfd.h` include path is correct: `vendor/nfd/src/include/nfd.h`
- Ensure LZMA SDK `_7ZIP_ST` is defined to avoid threading deps
- Ensure `crn_decomp.h` compiles without warnings (may need pragmas)

- [ ] **Step 2: Run the application**

```bash
xmake run
```

Expected: Window opens with dark theme, red accents, two tabs (Unpack/Pack), smooth fade-in animation.

- [ ] **Step 3: Test unpack with real IDX file**

Use a Dragon's Prophet `.idx` file from `D:\Backups\IDXUnpacker\` if available. Click Browse, select IDX, verify entries populate with stagger animation. Click Extract All, verify progress bar with glow fills smoothly.

- [ ] **Step 4: Test pack with a test folder**

Create a small test folder with a few files, select it in the Pack tab, choose LZMA compression, set output path, click Pack. Verify IDX + P001 files are created. Then unpack the newly created archive to verify roundtrip.

- [ ] **Step 5: Final commit**

```bash
git add -A
git commit -m "feat: Prophecy v1.0 - IDX unpacker/packer with ImGui UI"
```
