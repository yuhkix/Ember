#pragma once
#include <cstdint>
#include <string>
#include <vector>

static constexpr uint32_t HIDX_MAGIC = 0x09795222;

struct HidxEntry {
    uint32_t hash;
    uint32_t flags;
    uint32_t offset;
    uint32_t size;
};

class HidxReader {
public:
    bool open(const std::string& path);
    const std::vector<HidxEntry>& entries() const { return m_entries; }
    uint32_t entry_count() const { return static_cast<uint32_t>(m_entries.size()); }
    const std::string& error() const { return m_error; }

    // Lookup entry by hash, returns nullptr if not found
    const HidxEntry* find_by_hash(uint32_t hash) const;

private:
    std::vector<HidxEntry> m_entries;
    std::string m_error;
};
