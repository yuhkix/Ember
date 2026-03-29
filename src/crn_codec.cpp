#include "crn_codec.h"
#include <cstring>
#include <algorithm>

// Suppress MSVC warnings from the old crn_decomp.h header
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4018) // signed/unsigned mismatch
#pragma warning(disable : 4244) // conversion, possible loss of data
#pragma warning(disable : 4267) // conversion from 'size_t' to smaller type
#pragma warning(disable : 4389) // signed/unsigned mismatch
#pragma warning(disable : 4456) // declaration hides previous local
#pragma warning(disable : 4457) // declaration hides function parameter
#pragma warning(disable : 4458) // declaration hides class member
#pragma warning(disable : 4505) // unreferenced function removed
#pragma warning(disable : 4701) // potentially uninitialized variable
#pragma warning(disable : 4702) // unreachable code
#endif

// Ensure WIN32 is defined so crn_decomp.h selects the correct platform paths
// (MSVC defines _WIN32 automatically but crn_decomp.h checks WIN32).
#if defined(_WIN32) && !defined(WIN32)
#define WIN32
#endif

// Include crn_decomp.h with full implementation (header + body).
// The header is guarded by CRND_INCLUDE_CRND_H; the implementation by CRND_HEADER_FILE_ONLY.
// Neither is defined here, so both the declarations and implementation are compiled.
#include "crn_decomp.h"

#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace crn {

// DDS pixel format structure
struct DDS_PIXELFORMAT {
    uint32_t size;
    uint32_t flags;
    uint32_t fourCC;
    uint32_t rgbBitCount;
    uint32_t rBitMask;
    uint32_t gBitMask;
    uint32_t bBitMask;
    uint32_t aBitMask;
};

// DDS header structure
struct DDS_HEADER {
    uint32_t size;
    uint32_t flags;
    uint32_t height;
    uint32_t width;
    uint32_t pitchOrLinearSize;
    uint32_t depth;
    uint32_t mipMapCount;
    uint32_t reserved1[11];
    DDS_PIXELFORMAT ddspf;
    uint32_t caps;
    uint32_t caps2;
    uint32_t caps3;
    uint32_t caps4;
    uint32_t reserved2;
};

static uint32_t get_fourcc(crnd::crn_format fmt)
{
    switch (fmt) {
        case crnd::cCRNFmtDXT1:     return 0x31545844; // "DXT1"
        case crnd::cCRNFmtDXT3:     return 0x33545844; // "DXT3"
        case crnd::cCRNFmtDXT5:     return 0x35545844; // "DXT5"
        case crnd::cCRNFmtDXT5A:    return 0x31495441; // "ATI1"
        case crnd::cCRNFmtDXN_XY:   return 0x32495441; // "ATI2"
        // Swizzled DXT5 variants map to DXT5
        case crnd::cCRNFmtDXT5_CCxY:
        case crnd::cCRNFmtDXT5_xGxR:
        case crnd::cCRNFmtDXT5_xGBR:
        case crnd::cCRNFmtDXT5_AGBR:
            return 0x35545844; // "DXT5"
        case crnd::cCRNFmtDXN_YX:   return 0x32495441; // "ATI2"
        default:                     return 0;
    }
}

static uint32_t get_bytes_per_block(crnd::crn_format fmt)
{
    switch (fmt) {
        case crnd::cCRNFmtDXT1:
        case crnd::cCRNFmtDXT5A:
            return 8;
        default:
            return 16;
    }
}

std::vector<uint8_t> crn_to_dds(const uint8_t* crn_data, size_t crn_size)
{
    if (!crn_data || crn_size == 0)
        return {};

    // Get texture info from CRN data
    crnd::crn_texture_info tex_info;
    if (!crnd::crnd_get_texture_info(crn_data, static_cast<crnd::uint32>(crn_size), &tex_info))
        return {};

    const uint32_t width  = tex_info.m_width;
    const uint32_t height = tex_info.m_height;
    const uint32_t levels = tex_info.m_levels;
    const auto     format = tex_info.m_format;
    const uint32_t fourcc = get_fourcc(format);
    const uint32_t bytes_per_block = get_bytes_per_block(format);

    if (fourcc == 0)
        return {};

    // Begin unpacking
    crnd::crnd_unpack_context ctx = crnd::crnd_unpack_begin(crn_data, static_cast<crnd::uint32>(crn_size));
    if (!ctx)
        return {};

    // Calculate total size of all mip levels
    size_t total_surface_size = 0;
    for (uint32_t level = 0; level < levels; ++level) {
        const uint32_t level_width  = std::max(1u, width >> level);
        const uint32_t level_height = std::max(1u, height >> level);
        const uint32_t blocks_x = (level_width + 3) / 4;
        const uint32_t blocks_y = (level_height + 3) / 4;
        total_surface_size += static_cast<size_t>(blocks_x) * blocks_y * bytes_per_block;
    }

    // Build DDS output: magic(4) + header(124) + surface data
    const size_t dds_size = 4 + sizeof(DDS_HEADER) + total_surface_size;
    std::vector<uint8_t> dds(dds_size, 0);

    // Write DDS magic
    dds[0] = 'D'; dds[1] = 'D'; dds[2] = 'S'; dds[3] = ' ';

    // Fill DDS header
    auto* hdr = reinterpret_cast<DDS_HEADER*>(dds.data() + 4);
    hdr->size   = 124;
    // DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_LINEARSIZE
    hdr->flags  = 0x1 | 0x2 | 0x4 | 0x1000 | 0x80000;
    hdr->height = height;
    hdr->width  = width;

    // pitchOrLinearSize for the top mip
    {
        const uint32_t blocks_x = (width + 3) / 4;
        const uint32_t blocks_y = (height + 3) / 4;
        hdr->pitchOrLinearSize = blocks_x * blocks_y * bytes_per_block;
    }

    hdr->depth       = 1;
    hdr->mipMapCount = levels;

    // Pixel format
    hdr->ddspf.size  = 32;
    hdr->ddspf.flags = 4; // DDPF_FOURCC
    hdr->ddspf.fourCC = fourcc;

    // Caps
    hdr->caps = 0x1000; // DDSCAPS_TEXTURE

    // Mipmapped textures
    if (levels > 1) {
        hdr->caps  |= 0x400008; // DDSCAPS_COMPLEX | DDSCAPS_MIPMAP
        hdr->flags |= 0x20000;  // DDSD_MIPMAPCOUNT
    }

    // Transcode each mip level
    size_t offset = 4 + sizeof(DDS_HEADER);
    for (uint32_t level = 0; level < levels; ++level) {
        const uint32_t level_width  = std::max(1u, width >> level);
        const uint32_t level_height = std::max(1u, height >> level);
        const uint32_t blocks_x = (level_width + 3) / 4;
        const uint32_t blocks_y = (level_height + 3) / 4;
        const uint32_t row_pitch = blocks_x * bytes_per_block;
        const uint32_t level_size = blocks_x * blocks_y * bytes_per_block;

        void* dst = dds.data() + offset;
        if (!crnd::crnd_unpack_level(ctx, &dst, level_size, row_pitch, level)) {
            crnd::crnd_unpack_end(ctx);
            return {};
        }

        offset += level_size;
    }

    crnd::crnd_unpack_end(ctx);
    return dds;
}

} // namespace crn
