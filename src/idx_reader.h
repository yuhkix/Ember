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
    IdxEntryFixed   fixed;
    std::string     filename;
    DataChunkHeader data_header;
    CompressionType compression;
    std::string     data_file_path;
};

using ProgressCallback = std::function<bool(int current, int total)>;

class IdxReader {
public:
    bool open(const std::string& idx_path);
    const IdxHeader& header() const { return m_header; }
    const std::vector<IdxEntry>& entries() const { return m_entries; }
    const std::string& error() const { return m_error; }
    int extract_all(const std::string& output_dir, ProgressCallback progress = nullptr);
    bool extract_entry(const IdxEntry& entry, const std::string& output_dir);

private:
    IdxHeader              m_header = {};
    std::vector<IdxEntry>  m_entries;
    std::string            m_idx_path;
    std::string            m_base_dir;
    std::string            m_base_name;
    std::string            m_error;

    std::string resolve_data_file(uint8_t index) const;
    CompressionType detect_compression(const DataChunkHeader& hdr,
                                        const std::string& data_file,
                                        uint32_t data_offset) const;
};
