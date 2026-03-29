#pragma once
#include <imgui.h>
#include <string>
#include <vector>

namespace widgets {

struct FileListItem {
    std::string name;
    std::string size;
    std::string type;
    bool success = true;
};

struct FileListState {
    float scroll_offset = 0.0f;    // Current scroll position
    float target_scroll = 0.0f;    // Smooth scroll target
    int   selected = -1;           // Selected row index
    int   visible_count = 0;       // How many items are currently in the list
};

// Renders a custom file list. Returns true if selection changed.
// `id`: ImGui ID string
// `items`: vector of items to display (only renders items.size() rows, never more)
// `state`: persistent state (caller owns this)
// `max_height`: maximum height in pixels (0 = use all available height)
// `auto_scroll`: if true, smoothly scrolls to bottom when new items are added
bool FileList(const char* id, const std::vector<FileListItem>& items,
              FileListState& state, float max_height = 0.0f, bool auto_scroll = true);

// Simpler single-column list for the pack preview
bool SimpleList(const char* id, const std::vector<std::string>& items,
                FileListState& state, float max_height = 0.0f);

} // namespace widgets
