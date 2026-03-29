#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct FsidxHeader {
    uint8_t  magic_version[16]; // "p\x89WF" + version string
    uint32_t field1;
    uint32_t entry_count;
};

struct FsidxEntry {
    uint32_t fields[6]; // 24 bytes, 6 x uint32
};

class FsidxReader {
public:
    bool open(const std::string& path);
    const FsidxHeader& header() const { return m_header; }
    std::string version_string() const;
    const std::vector<FsidxEntry>& entries() const { return m_entries; }
    uint32_t entry_count() const { return static_cast<uint32_t>(m_entries.size()); }
    const std::string& error() const { return m_error; }

private:
    FsidxHeader m_header = {};
    std::vector<FsidxEntry> m_entries;
    std::string m_error;
};
