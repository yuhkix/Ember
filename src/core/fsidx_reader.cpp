#include "fsidx_reader.h"

#include <cstdio>
#include <cstring>

bool FsidxReader::open(const std::string& path) {
    m_entries.clear();
    m_error.clear();
    std::memset(&m_header, 0, sizeof(m_header));

    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        m_error = "Cannot open FSIDX file: " + path;
        return false;
    }

    // Read the 24-byte header (16 magic_version + 4 field1 + 4 entry_count)
    if (std::fread(&m_header, sizeof(FsidxHeader), 1, f) != 1) {
        m_error = "Failed to read FSIDX header";
        std::fclose(f);
        return false;
    }

    // Basic sanity: first 4 bytes should be "p\x89WF"
    if (m_header.magic_version[0] != 'p'  ||
        m_header.magic_version[1] != 0x89 ||
        m_header.magic_version[2] != 'W'  ||
        m_header.magic_version[3] != 'F') {
        m_error = "Bad FSIDX magic (expected p\\x89WF)";
        std::fclose(f);
        return false;
    }

    uint32_t count = m_header.entry_count;
    m_entries.resize(count);

    // Read all entries (each 24 bytes)
    if (count > 0) {
        size_t read = std::fread(m_entries.data(), sizeof(FsidxEntry), count, f);
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

std::string FsidxReader::version_string() const {
    // Version string starts at byte 4 of magic_version, null-terminated
    const char* start = reinterpret_cast<const char*>(&m_header.magic_version[4]);
    // Compute max length (12 bytes remaining)
    size_t max_len = sizeof(m_header.magic_version) - 4;
    size_t len = strnlen(start, max_len);
    return std::string(start, len);
}
