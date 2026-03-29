#include "hidx_reader.h"

#include <cstdio>
#include <cstring>

template<typename T>
static bool read_val(FILE* f, T& out) {
    return std::fread(&out, sizeof(T), 1, f) == 1;
}

bool HidxReader::open(const std::string& path) {
    m_entries.clear();
    m_error.clear();

    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        m_error = "Cannot open HIDX file: " + path;
        return false;
    }

    // Read and verify magic
    uint32_t magic = 0;
    if (!read_val(f, magic)) {
        m_error = "Failed to read HIDX magic";
        std::fclose(f);
        return false;
    }

    if (magic != HIDX_MAGIC) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Bad HIDX magic: expected 0x%08X, got 0x%08X",
                      HIDX_MAGIC, magic);
        m_error = buf;
        std::fclose(f);
        return false;
    }

    // Read entry count
    uint32_t count = 0;
    if (!read_val(f, count)) {
        m_error = "Failed to read HIDX entry count";
        std::fclose(f);
        return false;
    }

    m_entries.resize(count);

    // Read all entries (each 16 bytes: hash, flags, offset, size)
    if (count > 0) {
        size_t read = std::fread(m_entries.data(), sizeof(HidxEntry), count, f);
        if (read != count) {
            m_error = "Expected " + std::to_string(count) + " entries, read " +
                      std::to_string(read);
            m_entries.clear();
            std::fclose(f);
            return false;
        }
    }

    std::fclose(f);
    return true;
}

const HidxEntry* HidxReader::find_by_hash(uint32_t hash) const {
    for (const auto& e : m_entries) {
        if (e.hash == hash)
            return &e;
    }
    return nullptr;
}
