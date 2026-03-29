#include "widgets.h"
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>

namespace widgets {

// ---------------------------------------------------------------------------
// Theme colors
// ---------------------------------------------------------------------------
static constexpr ImU32 COL_BG           = IM_COL32(20, 20, 20, 255);
static constexpr ImU32 COL_HEADER_BG    = IM_COL32(30, 30, 30, 255);
static constexpr ImU32 COL_ROW_EVEN     = IM_COL32(22, 22, 22, 255);
static constexpr ImU32 COL_ROW_ODD      = IM_COL32(26, 26, 26, 255);
static constexpr ImU32 COL_ROW_HOVER    = IM_COL32(50, 15, 15, 255);
static constexpr ImU32 COL_ROW_SELECTED = IM_COL32(60, 20, 20, 255);
static constexpr ImU32 COL_TEXT         = IM_COL32(220, 220, 220, 255);
static constexpr ImU32 COL_HEADER_TEXT  = IM_COL32(160, 160, 160, 255);
static constexpr ImU32 COL_SEPARATOR    = IM_COL32(42, 42, 42, 255);
static constexpr ImU32 COL_SCROLLBAR_BG = IM_COL32(20, 20, 20, 255);
static constexpr ImU32 COL_SCROLLBAR    = IM_COL32(60, 60, 60, 255);
static constexpr ImU32 COL_SCROLLBAR_HV = IM_COL32(80, 20, 20, 255);
static constexpr ImU32 COL_OK           = IM_COL32(76, 255, 76, 255);
static constexpr ImU32 COL_ERR          = IM_COL32(255, 76, 76, 255);

// Fixed column widths (right-aligned columns)
static constexpr float COL_SIZE_W   = 90.0f;
static constexpr float COL_TYPE_W   = 50.0f;
static constexpr float COL_STATUS_W = 40.0f;
static constexpr float SCROLLBAR_W  = 10.0f;
static constexpr float TEXT_PAD     = 6.0f;

// Smooth scroll interpolation speed
static constexpr float SCROLL_SPEED = 12.0f;

// ---------------------------------------------------------------------------
// Custom scrollbar (shared)
// ---------------------------------------------------------------------------
static void draw_scrollbar(ImDrawList* dl, ImVec2 area_min, ImVec2 area_max,
                           float content_h, float visible_h, float& scroll_offset) {
    if (content_h <= visible_h)
        return; // No scrollbar needed

    float sb_x = area_max.x - SCROLLBAR_W;
    float sb_y = area_min.y;
    float sb_h = area_max.y - area_min.y;

    // Scrollbar background
    dl->AddRectFilled(ImVec2(sb_x, sb_y), ImVec2(sb_x + SCROLLBAR_W, sb_y + sb_h),
                      COL_SCROLLBAR_BG);

    // Thumb size and position
    float ratio     = visible_h / content_h;
    float thumb_h   = std::max(20.0f, sb_h * ratio);
    float max_scroll = content_h - visible_h;
    float scroll_ratio = (max_scroll > 0.0f) ? (scroll_offset / max_scroll) : 0.0f;
    float thumb_y   = sb_y + scroll_ratio * (sb_h - thumb_h);

    ImVec2 thumb_min(sb_x + 2.0f, thumb_y);
    ImVec2 thumb_max(sb_x + SCROLLBAR_W - 2.0f, thumb_y + thumb_h);

    // Hover detection
    ImVec2 mouse = ImGui::GetIO().MousePos;
    bool hovered = (mouse.x >= sb_x && mouse.x <= sb_x + SCROLLBAR_W &&
                    mouse.y >= sb_y && mouse.y <= sb_y + sb_h);

    ImU32 thumb_col = hovered ? COL_SCROLLBAR_HV : COL_SCROLLBAR;
    dl->AddRectFilled(thumb_min, thumb_max, thumb_col, 3.0f);
}

// ---------------------------------------------------------------------------
// FileList
// ---------------------------------------------------------------------------
bool FileList(const char* id, const std::vector<FileListItem>& items,
              FileListState& state, float max_height, bool auto_scroll) {
    if (items.empty())
        return false;

    bool selection_changed = false;
    const int count = static_cast<int>(items.size());

    // Row height
    const float row_h = ImGui::GetTextLineHeightWithSpacing() + 4.0f;
    const float header_h = row_h;
    const float content_h = count * row_h;
    const float total_h = header_h + content_h;

    // Available height
    float avail_h = ImGui::GetContentRegionAvail().y;
    if (max_height > 0.0f)
        avail_h = std::min(avail_h, max_height);

    // Child height: fit content or cap at available
    float child_h = std::min(total_h, avail_h);
    if (child_h < header_h + row_h)
        child_h = std::min(header_h + row_h, avail_h);

    // Visible area for rows (below header)
    float visible_rows_h = child_h - header_h;

    // Auto-scroll: set target to bottom when new items arrive
    if (auto_scroll && count != state.visible_count) {
        float max_scroll = std::max(0.0f, content_h - visible_rows_h);
        state.target_scroll = max_scroll;
    }
    state.visible_count = count;

    // Clamp target scroll
    float max_scroll = std::max(0.0f, content_h - visible_rows_h);
    state.target_scroll = std::clamp(state.target_scroll, 0.0f, max_scroll);

    // Smooth scroll interpolation
    float dt = ImGui::GetIO().DeltaTime;
    float diff = state.target_scroll - state.scroll_offset;
    if (std::abs(diff) < 0.5f)
        state.scroll_offset = state.target_scroll;
    else
        state.scroll_offset += diff * std::min(1.0f, SCROLL_SPEED * dt);

    state.scroll_offset = std::clamp(state.scroll_offset, 0.0f, max_scroll);

    // Begin child region
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImColor(COL_BG).Value);
    ImGui::PushStyleColor(ImGuiCol_Border, ImColor(COL_SEPARATOR).Value);

    ImGui::BeginChild(id, ImVec2(0.0f, child_h), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 win_min = ImGui::GetWindowPos();
    ImVec2 win_size = ImGui::GetWindowSize();
    ImVec2 win_max(win_min.x + win_size.x, win_min.y + win_size.y);

    float total_w = win_size.x;
    bool needs_scrollbar = (content_h > visible_rows_h);
    float content_w = needs_scrollbar ? (total_w - SCROLLBAR_W) : total_w;

    // Column positions (right-to-left for fixed columns)
    float status_x = win_min.x + content_w - COL_STATUS_W;
    float type_x   = status_x - COL_TYPE_W;
    float size_x   = type_x - COL_SIZE_W;
    float name_x   = win_min.x;
    float name_w   = size_x - name_x;

    // --- Header ---
    {
        ImVec2 hdr_min = win_min;
        ImVec2 hdr_max(win_min.x + content_w, win_min.y + header_h);
        dl->AddRectFilled(hdr_min, hdr_max, COL_HEADER_BG);

        float text_y = win_min.y + (header_h - ImGui::GetTextLineHeight()) * 0.5f;

        dl->AddText(ImVec2(name_x + TEXT_PAD, text_y), COL_HEADER_TEXT, "Name");
        dl->AddText(ImVec2(size_x + TEXT_PAD, text_y), COL_HEADER_TEXT, "Size");
        dl->AddText(ImVec2(type_x + TEXT_PAD, text_y), COL_HEADER_TEXT, "Type");
        dl->AddText(ImVec2(status_x + TEXT_PAD, text_y), COL_HEADER_TEXT, "Status");

        // Header separator
        dl->AddLine(ImVec2(win_min.x, hdr_max.y),
                    ImVec2(win_min.x + content_w, hdr_max.y), COL_SEPARATOR);

        // Column separators in header
        dl->AddLine(ImVec2(size_x, hdr_min.y), ImVec2(size_x, hdr_max.y), COL_SEPARATOR);
        dl->AddLine(ImVec2(type_x, hdr_min.y), ImVec2(type_x, hdr_max.y), COL_SEPARATOR);
        dl->AddLine(ImVec2(status_x, hdr_min.y), ImVec2(status_x, hdr_max.y), COL_SEPARATOR);
    }

    // --- Rows ---
    float rows_top = win_min.y + header_h;
    float rows_bot = win_max.y;

    // Clip to row area
    dl->PushClipRect(ImVec2(win_min.x, rows_top), ImVec2(win_min.x + content_w, rows_bot), true);

    // Mouse handling
    ImVec2 mouse = ImGui::GetIO().MousePos;
    bool mouse_in_rows = (mouse.x >= win_min.x && mouse.x < win_min.x + content_w &&
                          mouse.y >= rows_top && mouse.y < rows_bot);

    // Mouse wheel scrolling
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            state.target_scroll -= wheel * row_h * 3.0f;
            state.target_scroll = std::clamp(state.target_scroll, 0.0f, max_scroll);
        }
    }

    // Determine visible row range
    int first_visible = static_cast<int>(state.scroll_offset / row_h);
    if (first_visible < 0) first_visible = 0;
    int last_visible = static_cast<int>((state.scroll_offset + visible_rows_h) / row_h) + 1;
    if (last_visible > count) last_visible = count;

    for (int i = first_visible; i < last_visible; ++i) {
        float row_y = rows_top + (i * row_h) - state.scroll_offset;
        ImVec2 row_min(win_min.x, row_y);
        ImVec2 row_max(win_min.x + content_w, row_y + row_h);

        // Skip if fully outside visible area
        if (row_max.y < rows_top || row_min.y > rows_bot)
            continue;

        // Background
        bool hovered = (mouse_in_rows &&
                        mouse.y >= row_min.y && mouse.y < row_max.y);
        bool selected = (i == state.selected);

        ImU32 bg;
        if (selected)
            bg = COL_ROW_SELECTED;
        else if (hovered)
            bg = COL_ROW_HOVER;
        else
            bg = (i & 1) ? COL_ROW_ODD : COL_ROW_EVEN;

        dl->AddRectFilled(row_min, row_max, bg);

        // Click to select
        if (hovered && ImGui::IsMouseClicked(0)) {
            if (state.selected != i) {
                state.selected = i;
                selection_changed = true;
            }
        }

        // Text
        float text_y = row_y + (row_h - ImGui::GetTextLineHeight()) * 0.5f;
        const auto& item = items[i];

        // Name (clipped to column)
        dl->PushClipRect(ImVec2(name_x, row_min.y), ImVec2(size_x, row_max.y), true);
        dl->AddText(ImVec2(name_x + TEXT_PAD, text_y), COL_TEXT, item.name.c_str());
        dl->PopClipRect();

        // Size
        dl->PushClipRect(ImVec2(size_x, row_min.y), ImVec2(type_x, row_max.y), true);
        dl->AddText(ImVec2(size_x + TEXT_PAD, text_y), COL_TEXT, item.size.c_str());
        dl->PopClipRect();

        // Type
        dl->PushClipRect(ImVec2(type_x, row_min.y), ImVec2(status_x, row_max.y), true);
        dl->AddText(ImVec2(type_x + TEXT_PAD, text_y), COL_TEXT, item.type.c_str());
        dl->PopClipRect();

        // Status
        const char* status_text = item.success ? "OK" : "ERR";
        ImU32 status_col = item.success ? COL_OK : COL_ERR;
        dl->AddText(ImVec2(status_x + TEXT_PAD, text_y), status_col, status_text);

        // Row separator
        dl->AddLine(ImVec2(win_min.x, row_max.y),
                    ImVec2(win_min.x + content_w, row_max.y), COL_SEPARATOR);
    }

    dl->PopClipRect();

    // --- Scrollbar ---
    if (needs_scrollbar) {
        draw_scrollbar(dl, ImVec2(win_min.x, rows_top), win_max,
                       content_h, visible_rows_h, state.scroll_offset);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);

    return selection_changed;
}

// ---------------------------------------------------------------------------
// SimpleList
// ---------------------------------------------------------------------------
bool SimpleList(const char* id, const std::vector<std::string>& items,
                FileListState& state, float max_height) {
    if (items.empty())
        return false;

    bool selection_changed = false;
    const int count = static_cast<int>(items.size());

    const float row_h = ImGui::GetTextLineHeightWithSpacing() + 4.0f;
    const float content_h = count * row_h;

    float avail_h = ImGui::GetContentRegionAvail().y;
    if (max_height > 0.0f)
        avail_h = std::min(avail_h, max_height);

    float child_h = std::min(content_h, avail_h);
    if (child_h < row_h)
        child_h = std::min(row_h, avail_h);

    float visible_h = child_h;

    // Update visible count
    state.visible_count = count;

    // Clamp target scroll
    float max_scroll = std::max(0.0f, content_h - visible_h);
    state.target_scroll = std::clamp(state.target_scroll, 0.0f, max_scroll);

    // Smooth scroll
    float dt = ImGui::GetIO().DeltaTime;
    float diff = state.target_scroll - state.scroll_offset;
    if (std::abs(diff) < 0.5f)
        state.scroll_offset = state.target_scroll;
    else
        state.scroll_offset += diff * std::min(1.0f, SCROLL_SPEED * dt);

    state.scroll_offset = std::clamp(state.scroll_offset, 0.0f, max_scroll);

    // Begin child region
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImColor(COL_BG).Value);
    ImGui::PushStyleColor(ImGuiCol_Border, ImColor(COL_SEPARATOR).Value);

    ImGui::BeginChild(id, ImVec2(0.0f, child_h), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 win_min = ImGui::GetWindowPos();
    ImVec2 win_size = ImGui::GetWindowSize();
    ImVec2 win_max(win_min.x + win_size.x, win_min.y + win_size.y);

    float total_w = win_size.x;
    bool needs_scrollbar = (content_h > visible_h);
    float content_w = needs_scrollbar ? (total_w - SCROLLBAR_W) : total_w;

    // Clip to content area
    dl->PushClipRect(win_min, ImVec2(win_min.x + content_w, win_max.y), true);

    // Mouse handling
    ImVec2 mouse = ImGui::GetIO().MousePos;
    bool mouse_in_area = (mouse.x >= win_min.x && mouse.x < win_min.x + content_w &&
                          mouse.y >= win_min.y && mouse.y < win_max.y);

    // Mouse wheel
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            state.target_scroll -= wheel * row_h * 3.0f;
            state.target_scroll = std::clamp(state.target_scroll, 0.0f, max_scroll);
        }
    }

    // Visible rows
    int first_visible = static_cast<int>(state.scroll_offset / row_h);
    if (first_visible < 0) first_visible = 0;
    int last_visible = static_cast<int>((state.scroll_offset + visible_h) / row_h) + 1;
    if (last_visible > count) last_visible = count;

    for (int i = first_visible; i < last_visible; ++i) {
        float row_y = win_min.y + (i * row_h) - state.scroll_offset;
        ImVec2 row_min(win_min.x, row_y);
        ImVec2 row_max(win_min.x + content_w, row_y + row_h);

        if (row_max.y < win_min.y || row_min.y > win_max.y)
            continue;

        // Background
        bool hovered = (mouse_in_area &&
                        mouse.y >= row_min.y && mouse.y < row_max.y);
        bool selected = (i == state.selected);

        ImU32 bg;
        if (selected)
            bg = COL_ROW_SELECTED;
        else if (hovered)
            bg = COL_ROW_HOVER;
        else
            bg = (i & 1) ? COL_ROW_ODD : COL_ROW_EVEN;

        dl->AddRectFilled(row_min, row_max, bg);

        // Click to select
        if (hovered && ImGui::IsMouseClicked(0)) {
            if (state.selected != i) {
                state.selected = i;
                selection_changed = true;
            }
        }

        // Text
        float text_y = row_y + (row_h - ImGui::GetTextLineHeight()) * 0.5f;
        dl->AddText(ImVec2(win_min.x + TEXT_PAD, text_y), COL_TEXT, items[i].c_str());

        // Row separator
        dl->AddLine(ImVec2(win_min.x, row_max.y),
                    ImVec2(win_min.x + content_w, row_max.y), COL_SEPARATOR);
    }

    dl->PopClipRect();

    // Scrollbar
    if (needs_scrollbar) {
        draw_scrollbar(dl, win_min, win_max, content_h, visible_h, state.scroll_offset);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);

    return selection_changed;
}

} // namespace widgets
