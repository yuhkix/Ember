#pragma once
#include "idx_reader.h" // reuse IdxHeader, IdxEntryFixed, DataChunkHeader structs
#include <string>
#include <vector>
#include <functional>

enum class PackCompression { Auto, Raw, Lzma, Crunch };

struct PackOptions {
    PackCompression compression = PackCompression::Auto;
    uint32_t max_data_file_size = 0; // 0 = single file, else split threshold
};

class IdxWriter {
public:
    bool pack(const std::string& source_dir,
              const std::string& idx_output_path,
              const PackOptions& options,
              ProgressCallback progress = nullptr);

    // Inject files into an existing IDX pack
    // existing_idx: path to the existing .idx file to modify
    // source_dir: directory containing files to inject
    // Files in source_dir with the same relative path as existing entries will REPLACE them
    // New files are ADDED
    bool inject(const std::string& existing_idx,
                const std::string& source_dir,
                const PackOptions& options,
                ProgressCallback progress = nullptr);

    const std::string& error() const { return m_error; }
private:
    std::string m_error;
};
