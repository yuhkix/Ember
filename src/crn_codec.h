#pragma once
#include <cstdint>
#include <vector>

namespace crn {
// Transcode CRN data to DDS file format.
// Returns a complete DDS file (magic + header + surface data), or empty on failure.
std::vector<uint8_t> crn_to_dds(const uint8_t* crn_data, size_t crn_size);
} // namespace crn
