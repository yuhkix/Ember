#include "idx_writer.h"
#include "lzma_codec.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

// Helper: write a T to FILE in little-endian (assumes LE host)
template<typename T>
static bool write_val(FILE* f, const T& val) {
    return std::fwrite(&val, sizeof(T), 1, f) == 1;
}

// Format data file path: base_path + ".p001", ".p002", etc.
static std::string make_data_path(const std::string& base_dir,
                                  const std::string& base_name,
                                  uint8_t index) {
    char suffix[8];
    std::snprintf(suffix, sizeof(suffix), ".p%03u", static_cast<unsigned>(index));
    return base_dir + base_name + suffix;
}

bool IdxWriter::pack(const std::string& source_dir,
                     const std::string& idx_output_path,
                     const PackOptions& options,
                     ProgressCallback progress) {
    m_error.clear();

    // ---- 1. Collect files ----
    std::vector<std::string> rel_paths;
    {
        std::error_code ec;
        fs::path src_root = fs::canonical(source_dir, ec);
        if (ec) {
            m_error = "Cannot resolve source directory: " + source_dir;
            return false;
        }

        for (auto& de : fs::recursive_directory_iterator(src_root, ec)) {
            if (ec) break;
            if (!de.is_regular_file()) continue;
            fs::path rel = fs::relative(de.path(), src_root, ec);
            if (ec) continue;
            // Normalize to backslash (matches original IDX convention)
            std::string s = rel.string();
            std::replace(s.begin(), s.end(), '/', '\\');
            rel_paths.push_back(std::move(s));
        }
        if (ec && rel_paths.empty()) {
            m_error = "Failed to scan source directory: " + ec.message();
            return false;
        }
    }

    // Sort for deterministic output
    std::sort(rel_paths.begin(), rel_paths.end());

    if (rel_paths.empty()) {
        m_error = "No files found in source directory";
        return false;
    }

    // ---- Derive base directory and base name from idx_output_path ----
    fs::path idx_p(idx_output_path);
    std::string base_dir = idx_p.parent_path().string();
    if (!base_dir.empty() && base_dir.back() != '/' && base_dir.back() != '\\')
        base_dir += '/';
    std::string base_name = idx_p.stem().string();

    fs::path src_root = fs::canonical(source_dir);

    // ---- 2. Open first data file ----
    uint8_t current_data_index = 1;
    std::string data_path = make_data_path(base_dir, base_name, current_data_index);
    FILE* df = std::fopen(data_path.c_str(), "wb");
    if (!df) {
        m_error = "Cannot create data file: " + data_path;
        return false;
    }
    uint32_t current_offset = 0;

    // ---- Per-entry records for IDX ----
    struct EntryRecord {
        IdxEntryFixed fixed;
        std::string   filename;
    };
    std::vector<EntryRecord> records;
    records.reserve(rel_paths.size());

    int total = static_cast<int>(rel_paths.size());

    // ---- 3. Process each file ----
    for (int i = 0; i < total; ++i) {
        const std::string& rel = rel_paths[i];

        // Build absolute source path using original source_dir
        fs::path abs = src_root / rel;

        // Read source file
        FILE* sf = std::fopen(abs.string().c_str(), "rb");
        if (!sf) {
            m_error = "Cannot open source file: " + abs.string();
            std::fclose(df);
            return false;
        }
        std::fseek(sf, 0, SEEK_END);
        size_t file_size = static_cast<size_t>(std::ftell(sf));
        std::fseek(sf, 0, SEEK_SET);

        std::vector<uint8_t> raw_data(file_size);
        if (file_size > 0) {
            if (std::fread(raw_data.data(), 1, file_size, sf) != file_size) {
                m_error = "Failed to read source file: " + abs.string();
                std::fclose(sf);
                std::fclose(df);
                return false;
            }
        }
        std::fclose(sf);

        // Compress if requested
        std::vector<uint8_t> payload;
        bool compressed = false;

        if (options.compression == PackCompression::Lzma && file_size > 0) {
            payload = lzma::compress(raw_data.data(), raw_data.size());
            if (!payload.empty() && payload.size() < file_size) {
                compressed = true;
            } else {
                payload.clear(); // fall back to raw
            }
        }

        if (!compressed) {
            payload = std::move(raw_data);
        }

        uint32_t payload_size = static_cast<uint32_t>(payload.size());

        // Check if we need to split to a new data file
        if (options.max_data_file_size > 0 && current_offset > 0 &&
            (current_offset + 28 + payload_size) > options.max_data_file_size) {
            std::fclose(df);
            ++current_data_index;
            data_path = make_data_path(base_dir, base_name, current_data_index);
            df = std::fopen(data_path.c_str(), "wb");
            if (!df) {
                m_error = "Cannot create data file: " + data_path;
                return false;
            }
            current_offset = 0;
        }

        // Build DataChunkHeader
        DataChunkHeader chdr{};
        if (compressed) {
            // Compressed: total_size = payload + 28, compressed_size = original file size
            chdr.total_size      = payload_size + 28;
            chdr.compressed_size = static_cast<uint32_t>(file_size);
        } else {
            // Raw marker: total_size = payload + 28, compressed_size = payload size
            // (total_size == compressed_size + 28 signals raw)
            chdr.total_size      = payload_size + 28;
            chdr.compressed_size = payload_size;
        }

        // Write DataChunkHeader (28 bytes)
        if (std::fwrite(&chdr, sizeof(DataChunkHeader), 1, df) != 1) {
            m_error = "Failed to write data chunk header";
            std::fclose(df);
            return false;
        }

        // Write payload
        if (payload_size > 0) {
            if (std::fwrite(payload.data(), 1, payload_size, df) != payload_size) {
                m_error = "Failed to write payload data";
                std::fclose(df);
                return false;
            }
        }

        // Record entry
        EntryRecord rec{};
        rec.fixed.data_offset     = current_offset;
        rec.fixed.data_file_index = current_data_index;
        rec.filename              = rel;
        records.push_back(std::move(rec));

        current_offset += 28 + payload_size;

        // Progress callback
        if (progress) {
            if (!progress(i + 1, total)) {
                std::fclose(df);
                m_error = "Cancelled by user";
                return false;
            }
        }
    }

    std::fclose(df);

    // ---- 4. Write IDX file ----
    FILE* idx = std::fopen(idx_output_path.c_str(), "wb");
    if (!idx) {
        m_error = "Cannot create IDX file: " + idx_output_path;
        return false;
    }

    // 24-byte header
    IdxHeader hdr{};
    hdr.file_count = static_cast<uint32_t>(records.size());
    if (std::fwrite(&hdr, sizeof(IdxHeader), 1, idx) != 1) {
        m_error = "Failed to write IDX header";
        std::fclose(idx);
        return false;
    }

    // Per-entry: 33 bytes fixed (field by field) + null-terminated filename
    for (const auto& rec : records) {
        if (!write_val(idx, rec.fixed.field_0) ||
            !write_val(idx, rec.fixed.field_1) ||
            !write_val(idx, rec.fixed.field_2) ||
            !write_val(idx, rec.fixed.data_offset) ||
            !write_val(idx, rec.fixed.field_4) ||
            !write_val(idx, rec.fixed.field_5) ||
            !write_val(idx, rec.fixed.data_file_index) ||
            !write_val(idx, rec.fixed.field_7) ||
            !write_val(idx, rec.fixed.field_8)) {
            m_error = "Failed to write IDX entry fixed fields";
            std::fclose(idx);
            return false;
        }

        // Null-terminated filename
        if (std::fwrite(rec.filename.c_str(), 1, rec.filename.size() + 1, idx) !=
            rec.filename.size() + 1) {
            m_error = "Failed to write IDX entry filename";
            std::fclose(idx);
            return false;
        }
    }

    std::fclose(idx);
    return true;
}
