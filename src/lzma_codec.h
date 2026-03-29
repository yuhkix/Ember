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
