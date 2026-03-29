#include "idx_writer.h"
#include "hidx_reader.h"
#include "fsidx_reader.h"
#include "lzma_codec.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace fs = std::filesystem;

// Helper: write a T to FILE in little-endian (assumes LE host)
template<typename T>
static bool write_val(FILE* f, const T& val) {
    return std::fwrite(&val, sizeof(T), 1, f) == 1;
}

// Simple FNV-1a hash for filenames (32-bit)
static uint32_t fnv1a_hash(const std::string& s) {
    uint32_t hash = 0x811c9dc5u;
    for (char c : s) {
        // Case-insensitive, normalize separators
        char ch = (c == '/') ? '\\' : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        hash ^= static_cast<uint32_t>(ch);
        hash *= 0x01000193u;
    }
    return hash;
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
    uint8_t current_data_index = 0; // .p000 to match game convention
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

        // Determine compression for this file
        PackCompression file_comp = options.compression;
        if (file_comp == PackCompression::Auto) {
            // Auto-detect based on file extension
            fs::path ext_path(rel);
            std::string ext = ext_path.extension().string();
            // Lowercase the extension
            for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

            if (ext == ".dds" || ext == ".crn" || ext == ".png" || ext == ".jpg" ||
                ext == ".jpeg" || ext == ".tga" || ext == ".bmp" || ext == ".ogg" ||
                ext == ".wav" || ext == ".mp3") {
                // Already-compressed or binary media — store raw
                file_comp = PackCompression::Raw;
            } else if (file_size < 128) {
                // Tiny files — not worth compressing
                file_comp = PackCompression::Raw;
            } else {
                // Everything else — LZMA
                file_comp = PackCompression::Lzma;
            }
        }

        // Compress
        std::vector<uint8_t> payload;
        bool compressed = false;

        if (file_comp == PackCompression::Lzma && file_size > 0) {
            payload = lzma::compress(raw_data.data(), raw_data.size());
            if (!payload.empty() && payload.size() < file_size) {
                compressed = true;
            } else {
                payload.clear(); // compression didn't help — fall back to raw
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
        rec.fixed.field_0         = 2; // entry version (matches game format)
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

    // ---- 4. Write IDX file (new format with magic) ----
    FILE* idx = std::fopen(idx_output_path.c_str(), "wb");
    if (!idx) {
        m_error = "Cannot create IDX file: " + idx_output_path;
        return false;
    }

    // New format header: magic(4) + version(16) + count(4) = 24 bytes
    IdxHeaderNew hdr{};
    hdr.magic = IDX_MAGIC_NEW;
    std::strncpy(hdr.version, "3.0.1625.eu", sizeof(hdr.version));
    hdr.file_count = static_cast<uint32_t>(records.size());
    if (std::fwrite(&hdr, sizeof(IdxHeaderNew), 1, idx) != 1) {
        m_error = "Failed to write IDX header";
        std::fclose(idx);
        return false;
    }

    // Per-entry: 33 bytes fixed (field by field) + name_length bytes of filename
    for (auto& rec : records) {
        // field_8 = name length including null terminator
        uint32_t name_len = static_cast<uint32_t>(rec.filename.size()) + 1;
        rec.fixed.field_8 = name_len;

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

        // Filename (name_len bytes, null-terminated)
        if (std::fwrite(rec.filename.c_str(), 1, name_len, idx) != name_len) {
            m_error = "Failed to write IDX entry filename";
            std::fclose(idx);
            return false;
        }
    }

    std::fclose(idx);

    // ---- 5. Generate .hidx (hash index) ----
    {
        std::string hidx_path = base_dir + base_name + ".hidx";
        FILE* hf = std::fopen(hidx_path.c_str(), "wb");
        if (hf) {
            uint32_t magic = HIDX_MAGIC;
            uint32_t count = static_cast<uint32_t>(records.size());
            std::fwrite(&magic, 4, 1, hf);
            std::fwrite(&count, 4, 1, hf);

            // Calculate entry offsets in the IDX file
            // Header = 24 bytes, then entries are sequential
            uint32_t idx_offset = 24; // after IdxHeader
            for (const auto& rec : records) {
                HidxEntry he{};
                he.hash   = fnv1a_hash(rec.filename);
                he.flags  = 1;
                he.offset = idx_offset;
                // Entry size = 33 fixed bytes + filename length + 1 null
                he.size   = 33 + static_cast<uint32_t>(rec.filename.size()) + 1;
                std::fwrite(&he, sizeof(HidxEntry), 1, hf);

                idx_offset += he.size;
            }
            std::fclose(hf);
        }
    }

    // ---- 6. Generate .fsidx (filesystem index) ----
    {
        std::string fsidx_path = base_dir + base_name + ".fsidx";
        FILE* ff = std::fopen(fsidx_path.c_str(), "wb");
        if (ff) {
            // Header: magic+version(16) + field1(4) + count(4) = 24 bytes
            FsidxHeader fhdr{};
            // "p\x89WF" magic + version "1.0.0.em\0"
            fhdr.magic_version[0] = 0x70; // 'p'
            fhdr.magic_version[1] = 0x89;
            fhdr.magic_version[2] = 0x57; // 'W'
            fhdr.magic_version[3] = 0x46; // 'F'
            std::strncpy(reinterpret_cast<char*>(fhdr.magic_version + 4), "1.0.0.em", 12);
            fhdr.field1 = 0;
            fhdr.entry_count = static_cast<uint32_t>(records.size());
            std::fwrite(&fhdr, sizeof(FsidxHeader), 1, ff);

            // Per entry: 6 × uint32 (hash, hash2, data_hash, offset, size, flags)
            uint32_t data_offset_acc = 0;
            for (const auto& rec : records) {
                FsidxEntry fe{};
                fe.fields[0] = fnv1a_hash(rec.filename);           // filename hash
                fe.fields[1] = fnv1a_hash(rec.filename) ^ 0xA5A5;  // secondary hash
                fe.fields[2] = rec.fixed.data_offset;               // data file offset
                fe.fields[3] = rec.fixed.data_file_index;           // data file index
                fe.fields[4] = 0;                                    // reserved
                fe.fields[5] = 0;                                    // reserved
                std::fwrite(&fe, sizeof(FsidxEntry), 1, ff);
            }
            std::fclose(ff);
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// inject() — inject files into an existing IDX pack
// ---------------------------------------------------------------------------

// Helper to normalize a path for case-insensitive comparison
static std::string normalize_path(const std::string& s) {
    std::string out = s;
    for (auto& c : out) {
        if (c == '/') c = '\\';
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

bool IdxWriter::inject(const std::string& existing_idx,
                       const std::string& source_dir,
                       const PackOptions& options,
                       ProgressCallback progress) {
    m_error.clear();

    // ---- 1. Read ENTIRE original .idx as raw bytes ----
    std::vector<uint8_t> idx_bytes;
    {
        FILE* f = std::fopen(existing_idx.c_str(), "rb");
        if (!f) { m_error = "Cannot open IDX: " + existing_idx; return false; }
        std::fseek(f, 0, SEEK_END);
        size_t sz = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);
        idx_bytes.resize(sz);
        std::fread(idx_bytes.data(), 1, sz, f);
        std::fclose(f);
    }

    if (idx_bytes.size() < 24) { m_error = "IDX too small"; return false; }

    // ---- 2. Parse header to find entries (but keep raw bytes) ----
    IdxReader reader;
    if (!reader.open(existing_idx)) {
        m_error = "Cannot parse IDX: " + reader.error();
        return false;
    }
    const auto& orig_entries = reader.entries();

    // ---- 3. Scan source directory ----
    std::vector<std::string> inject_rel_paths;
    {
        std::error_code ec;
        fs::path src_root = fs::canonical(source_dir, ec);
        if (ec) { m_error = "Cannot resolve source dir"; return false; }
        for (auto& de : fs::recursive_directory_iterator(src_root, ec)) {
            if (ec) break;
            if (!de.is_regular_file()) continue;
            fs::path rel = fs::relative(de.path(), src_root, ec);
            if (ec) continue;
            std::string s = rel.string();
            std::replace(s.begin(), s.end(), '/', '\\');
            inject_rel_paths.push_back(std::move(s));
        }
    }
    if (inject_rel_paths.empty()) { m_error = "No files to inject"; return false; }

    // ---- 4. Build lookup: normalized_name -> index in inject_rel_paths ----
    std::unordered_map<std::string, size_t> inject_map;
    for (size_t i = 0; i < inject_rel_paths.size(); i++)
        inject_map[normalize_path(inject_rel_paths[i])] = i;

    // ---- 5. Backup .idx ----
    {
        std::string bak = existing_idx + ".bak";
        std::error_code ec;
        if (!fs::exists(bak, ec))
            fs::copy_file(existing_idx, bak, ec);
    }

    // ---- 6. Derive paths ----
    fs::path idx_p(existing_idx);
    std::string base_dir = idx_p.parent_path().string();
    if (!base_dir.empty() && base_dir.back() != '/' && base_dir.back() != '\\')
        base_dir += '/';
    std::string base_name = idx_p.stem().string();

    // ---- 7. Create a NEW data file for injected data (don't touch originals) ----
    uint8_t highest_data_index = 0;
    for (const auto& e : orig_entries)
        if (e.fixed.data_file_index > highest_data_index)
            highest_data_index = e.fixed.data_file_index;

    // Use NEXT index so original data files are completely untouched
    uint8_t inject_data_index = highest_data_index + 1;
    std::string data_path = make_data_path(base_dir, base_name, inject_data_index);

    FILE* df = std::fopen(data_path.c_str(), "wb");
    if (!df) { m_error = "Cannot create data file: " + data_path; return false; }

    fs::path src_root = fs::canonical(source_dir);
    uint8_t current_data_index = inject_data_index;
    uint32_t current_offset = 0;

    // ---- 8. Write data chunks for injected files, track offsets ----
    struct InjectedInfo { uint32_t data_offset; uint8_t data_file_index; };
    std::unordered_map<std::string, InjectedInfo> injected_info; // norm_path -> info

    int total = static_cast<int>(inject_rel_paths.size());
    for (int i = 0; i < total; i++) {
        const std::string& rel = inject_rel_paths[i];
        fs::path abs = src_root / rel;

        FILE* sf = std::fopen(abs.string().c_str(), "rb");
        if (!sf) { std::fclose(df); m_error = "Cannot read: " + abs.string(); return false; }
        std::fseek(sf, 0, SEEK_END);
        size_t file_size = std::ftell(sf);
        std::fseek(sf, 0, SEEK_SET);
        std::vector<uint8_t> raw_data(file_size);
        if (file_size > 0) std::fread(raw_data.data(), 1, file_size, sf);
        std::fclose(sf);

        // Compress
        std::vector<uint8_t> payload;
        bool compressed = false;
        PackCompression file_comp = options.compression;
        if (file_comp == PackCompression::Auto) {
            std::string ext = fs::path(rel).extension().string();
            for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (ext == ".dds" || ext == ".crn" || ext == ".png" || ext == ".jpg" ||
                ext == ".ogg" || ext == ".wav" || ext == ".mp3" || file_size < 128)
                file_comp = PackCompression::Raw;
            else
                file_comp = PackCompression::Lzma;
        }
        if (file_comp == PackCompression::Lzma && file_size > 0) {
            payload = lzma::compress(raw_data.data(), raw_data.size());
            if (!payload.empty() && payload.size() < file_size) compressed = true;
            else payload.clear();
        }
        if (!compressed) payload = std::move(raw_data);

        uint32_t payload_size = static_cast<uint32_t>(payload.size());

        DataChunkHeader chdr{};
        chdr.total_size = payload_size + 28;
        chdr.compressed_size = compressed ? static_cast<uint32_t>(file_size) : payload_size;

        std::fwrite(&chdr, sizeof(DataChunkHeader), 1, df);
        if (payload_size > 0) std::fwrite(payload.data(), 1, payload_size, df);

        injected_info[normalize_path(rel)] = { current_offset, current_data_index };
        current_offset += 28 + payload_size;

        if (progress && !progress(i + 1, total)) {
            std::fclose(df);
            m_error = "Cancelled";
            return false;
        }
    }
    std::fclose(df);

    // ---- 9. Binary-patch the .idx: only modify data_offset + data_file_index for replaced entries ----
    // Scan through raw bytes to find each entry and patch in-place
    size_t pos = 24; // skip header
    for (uint32_t i = 0; i < reader.header().file_count && pos + 33 <= idx_bytes.size(); i++) {
        // Read filename from raw bytes (field_8 at offset 29 from entry start)
        uint32_t name_len = 0;
        std::memcpy(&name_len, &idx_bytes[pos + 29], 4);

        // Sanity check
        if (name_len == 0 || pos + 33 + name_len > idx_bytes.size()) break;

        // Extract filename from raw bytes
        std::string entry_name(reinterpret_cast<const char*>(&idx_bytes[pos + 33]), name_len);
        // Trim at null
        size_t null_pos = entry_name.find('\0');
        if (null_pos != std::string::npos) entry_name.resize(null_pos);

        // Check if this entry should be replaced
        auto it = injected_info.find(normalize_path(entry_name));
        if (it != injected_info.end()) {
            // Patch data_offset (4 bytes at offset +12 from entry start)
            std::memcpy(&idx_bytes[pos + 12], &it->second.data_offset, 4);
            // Patch data_file_index (1 byte at offset +24 from entry start)
            idx_bytes[pos + 24] = it->second.data_file_index;
        }

        pos += 33 + name_len; // advance to next entry
    }

    // ---- 10. Write patched .idx bytes back (exact same size for replaced-only) ----
    FILE* idx = std::fopen(existing_idx.c_str(), "wb");
    if (!idx) { m_error = "Cannot write IDX"; return false; }
    std::fwrite(idx_bytes.data(), 1, idx_bytes.size(), idx);
    std::fclose(idx);

    // .hidx and .fsidx are NOT touched — they remain exactly as original
    return true;
}
