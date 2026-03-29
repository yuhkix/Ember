#include "lzma_codec.h"
#include <LzmaDec.h>
#include <LzmaEnc.h>
#include <cstring>

namespace lzma {

static void* LzmaAlloc(ISzAllocPtr, size_t size) { return malloc(size); }
static void LzmaFree(ISzAllocPtr, void* ptr) { free(ptr); }
static ISzAlloc g_alloc = { LzmaAlloc, LzmaFree };

std::vector<uint8_t> decompress(const uint8_t* data, size_t size) {
    if (size < 13) return {};

    uint64_t uncompressed_size = 0;
    for (int i = 0; i < 8; i++)
        uncompressed_size |= (uint64_t)data[5 + i] << (8 * i);

    if (uncompressed_size > 512 * 1024 * 1024) return {};

    std::vector<uint8_t> output(static_cast<size_t>(uncompressed_size));
    SizeT dest_len = output.size();
    SizeT src_len = size - 13;

    ELzmaStatus status;
    int res = LzmaDecode(
        output.data(), &dest_len,
        data + 13, &src_len,
        data, 5,
        LZMA_FINISH_END, &status, &g_alloc
    );

    if (res != SZ_OK) return {};
    output.resize(dest_len);
    return output;
}

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
    props.dictSize = 1 << 20;
    LzmaEnc_SetProps(enc, &props);

    Byte propsEncoded[5];
    SizeT propsSize = 5;
    LzmaEnc_WriteProperties(enc, propsEncoded, &propsSize);

    std::vector<uint8_t> result;
    result.insert(result.end(), propsEncoded, propsEncoded + 5);
    for (int i = 0; i < 8; i++)
        result.push_back(static_cast<uint8_t>((uint64_t)size >> (8 * i)));

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
