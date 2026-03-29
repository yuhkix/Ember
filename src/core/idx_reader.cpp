#include "idx_reader.h"
#include "lzma_codec.h"
#include "crn_codec.h"

#include <cstdio>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

// Helper: read a T from FILE in little-endian (assumes LE host)
template<typename T>
static bool read_val(FILE* f, T& out) {
    return std::fread(&out, sizeof(T), 1, f) == 1;
}

// ---- resolve_data_file ----
std::string IdxReader::resolve_data_file(uint8_t index) const {
    char suffix[8];
    std::snprintf(suffix, sizeof(suffix), ".p%03u", static_cast<unsigned>(index));
    return m_base_dir + m_base_name + suffix;
}

// ---- detect_compression ----
CompressionType IdxReader::detect_compression(const DataChunkHeader& hdr,
                                               const std::string& data_file,
                                               uint32_t data_offset) const {
    // If total_size == compressed_size + 28 the data is stored raw
    if (hdr.total_size == hdr.compressed_size + 28)
        return CompressionType::Raw;

    // Peek at the first 2 bytes after the 28-byte chunk header
    FILE* f = std::fopen(data_file.c_str(), "rb");
    if (!f)
        return CompressionType::Raw;

    std::fseek(f, static_cast<long>(data_offset) + 28, SEEK_SET);
    uint8_t tag[2] = {};
    std::fread(tag, 1, 2, f);
    std::fclose(f);

    // LZMA: first byte is the properties byte (commonly 0x5D but can vary)
    // Standard LZMA props byte encodes lc/lp/pb: valid range is 0..224
    if (tag[0] == 0x5D)
        return CompressionType::Lzma;

    // CRN: magic bytes "Hx" (0x48 0x78) at start
    if (tag[0] == 0x48 && tag[1] == 0x78)
        return CompressionType::Crunch;

    // Fallback: try LZMA if properties byte looks valid (lc+lp <= 8, pb <= 4)
    // props = pb * 45 + lp * 9 + lc, max = 4*45+4*9+8 = 224
    if (tag[0] <= 224)
        return CompressionType::Lzma;

    // Unknown compression — treat as raw
    return CompressionType::Raw;
}

// ---- open ----
bool IdxReader::open(const std::string& idx_path) {
    m_entries.clear();
    m_error.clear();

    FILE* f = std::fopen(idx_path.c_str(), "rb");
    if (!f) {
        m_error = "Cannot open IDX file: " + idx_path;
        return false;
    }

    m_idx_path = idx_path;

    // Derive base directory and base name (without extension)
    fs::path p(idx_path);
    m_base_dir  = p.parent_path().string();
    if (!m_base_dir.empty() && m_base_dir.back() != '/' && m_base_dir.back() != '\\')
        m_base_dir += '/';
    m_base_name = p.stem().string();

    // ---- Header (24 bytes) ----
    if (std::fread(&m_header, sizeof(IdxHeader), 1, f) != 1) {
        m_error = "Failed to read IDX header";
        std::fclose(f);
        return false;
    }

    m_entries.reserve(m_header.file_count);

    for (uint32_t i = 0; i < m_header.file_count; ++i) {
        IdxEntry entry{};

        // ---- Fixed part (33 bytes, read field by field) ----
        if (!read_val(f, entry.fixed.field_0) ||
            !read_val(f, entry.fixed.field_1) ||
            !read_val(f, entry.fixed.field_2) ||
            !read_val(f, entry.fixed.data_offset) ||
            !read_val(f, entry.fixed.field_4) ||
            !read_val(f, entry.fixed.field_5) ||
            !read_val(f, entry.fixed.data_file_index) ||
            !read_val(f, entry.fixed.field_7) ||
            !read_val(f, entry.fixed.field_8)) {
            m_error = "Failed to read entry #" + std::to_string(i) + " fixed fields";
            std::fclose(f);
            return false;
        }

        // ---- Null-terminated filename ----
        {
            char ch;
            while (std::fread(&ch, 1, 1, f) == 1) {
                if (ch == '\0') break;
                entry.filename += ch;
                // Safety: filenames shouldn't be longer than 260 chars
                if (entry.filename.size() > 260) {
                    m_error = "Entry #" + std::to_string(i) + ": filename too long (likely parsing error at offset " +
                              std::to_string(ftell(f)) + ")";
                    std::fclose(f);
                    return false;
                }
            }
            // Validate: filename should contain only printable ASCII
            for (char c : entry.filename) {
                if (static_cast<unsigned char>(c) < 0x20 || static_cast<unsigned char>(c) > 0x7E) {
                    m_error = "Entry #" + std::to_string(i) + ": filename contains non-printable chars (likely format mismatch)";
                    std::fclose(f);
                    return false;
                }
            }
        }

        // ---- Resolve data file path ----
        entry.data_file_path = resolve_data_file(entry.fixed.data_file_index);

        // ---- Read DataChunkHeader from the data file ----
        FILE* df = std::fopen(entry.data_file_path.c_str(), "rb");
        if (!df) {
            m_error = "Cannot open data file: " + entry.data_file_path;
            std::fclose(f);
            return false;
        }

        std::fseek(df, static_cast<long>(entry.fixed.data_offset), SEEK_SET);
        if (std::fread(&entry.data_header, sizeof(DataChunkHeader), 1, df) != 1) {
            m_error = "Failed to read data chunk header for entry #" + std::to_string(i);
            std::fclose(df);
            std::fclose(f);
            return false;
        }
        std::fclose(df);

        // ---- Detect compression ----
        entry.compression = detect_compression(entry.data_header,
                                                entry.data_file_path,
                                                entry.fixed.data_offset);

        m_entries.push_back(std::move(entry));
    }

    std::fclose(f);
    return true;
}

// ---- extract_entry ----
bool IdxReader::extract_entry(const IdxEntry& entry, const std::string& output_dir) {
    // Build output path, preserving sub-directories encoded in the filename
    fs::path out_path = fs::path(output_dir) / entry.filename;
    fs::create_directories(out_path.parent_path());

    FILE* df = std::fopen(entry.data_file_path.c_str(), "rb");
    if (!df) {
        m_error = "Cannot open data file: " + entry.data_file_path;
        return false;
    }

    // Seek past the 28-byte DataChunkHeader
    std::fseek(df, static_cast<long>(entry.fixed.data_offset) + 28, SEEK_SET);

    bool ok = false;

    switch (entry.compression) {
    case CompressionType::Raw: {
        // Raw: read compressed_size bytes and write directly
        std::vector<uint8_t> buf(entry.data_header.compressed_size);
        if (std::fread(buf.data(), 1, buf.size(), df) == buf.size()) {
            FILE* of = std::fopen(out_path.string().c_str(), "wb");
            if (of) {
                ok = (std::fwrite(buf.data(), 1, buf.size(), of) == buf.size());
                std::fclose(of);
            }
        }
        break;
    }
    case CompressionType::Lzma: {
        // LZMA: read (total_size - 28) bytes, decompress
        uint32_t payload = entry.data_header.total_size - 28;
        std::vector<uint8_t> buf(payload);
        if (std::fread(buf.data(), 1, buf.size(), df) == buf.size()) {
            auto decoded = lzma::decompress(buf.data(), buf.size());
            if (!decoded.empty()) {
                FILE* of = std::fopen(out_path.string().c_str(), "wb");
                if (of) {
                    ok = (std::fwrite(decoded.data(), 1, decoded.size(), of) == decoded.size());
                    std::fclose(of);
                }
            } else {
                // LZMA decompression failed — fallback to raw copy
                std::fseek(df, static_cast<long>(entry.fixed.data_offset) + 28, SEEK_SET);
                uint32_t raw_sz = entry.data_header.compressed_size;
                std::vector<uint8_t> raw(raw_sz);
                if (std::fread(raw.data(), 1, raw_sz, df) == raw_sz) {
                    FILE* of = std::fopen(out_path.string().c_str(), "wb");
                    if (of) {
                        ok = (std::fwrite(raw.data(), 1, raw_sz, of) == raw_sz);
                        std::fclose(of);
                    }
                }
            }
        }
        break;
    }
    case CompressionType::Crunch: {
        // Crunch: read (total_size - 28) bytes, transcode to DDS
        uint32_t payload = entry.data_header.total_size - 28;
        std::vector<uint8_t> buf(payload);
        if (std::fread(buf.data(), 1, buf.size(), df) == buf.size()) {
            auto dds = crn::crn_to_dds(buf.data(), buf.size());
            if (!dds.empty()) {
                // Replace original extension with .dds
                fs::path dds_path = out_path;
                dds_path.replace_extension(".dds");
                FILE* of = std::fopen(dds_path.string().c_str(), "wb");
                if (of) {
                    ok = (std::fwrite(dds.data(), 1, dds.size(), of) == dds.size());
                    std::fclose(of);
                }
            }
        }
        break;
    }
    }

    std::fclose(df);
    return ok;
}

// ---- extract_all ----
int IdxReader::extract_all(const std::string& output_dir, ProgressCallback progress) {
    int succeeded = 0;
    int total = static_cast<int>(m_entries.size());

    for (int i = 0; i < total; ++i) {
        if (extract_entry(m_entries[i], output_dir))
            ++succeeded;

        if (progress) {
            if (!progress(i + 1, total))
                break; // caller requested cancellation
        }
    }

    return succeeded;
}
