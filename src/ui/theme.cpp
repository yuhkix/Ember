#include "theme.h"

namespace theme {

static ImVec4 hex(unsigned int r, unsigned int g, unsigned int b, float a = 1.0f) {
    return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a);
}

void apply_dark_red() {
    ImGuiStyle& s = ImGui::GetStyle();

    // Rounding
    s.WindowRounding    = 6.0f;
    s.ChildRounding     = 4.0f;
    s.FrameRounding     = 4.0f;
    s.PopupRounding     = 4.0f;
    s.ScrollbarRounding = 4.0f;
    s.GrabRounding      = 4.0f;
    s.TabRounding       = 4.0f;

    // Padding & spacing
    s.WindowPadding   = ImVec2(10, 10);
    s.FramePadding    = ImVec2(8, 4);
    s.ItemSpacing     = ImVec2(8, 6);
    s.ItemInnerSpacing = ImVec2(6, 4);
    s.ScrollbarSize   = 14.0f;
    s.GrabMinSize     = 12.0f;
    s.WindowBorderSize = 1.0f;
    s.ChildBorderSize  = 1.0f;
    s.PopupBorderSize  = 1.0f;
    s.FrameBorderSize  = 0.0f;
    s.TabBorderSize    = 0.0f;

    // Alignment
    s.WindowTitleAlign = ImVec2(0.5f, 0.5f);

    ImVec4* c = s.Colors;

    // Base palette
    const ImVec4 bg       = hex(0x0D, 0x0D, 0x0D);          // #0D0D0D
    const ImVec4 panel    = hex(0x1A, 0x1A, 0x1A);          // #1A1A1A
    const ImVec4 border   = hex(0x2A, 0x2A, 0x2A);          // #2A2A2A
    const ImVec4 surface  = hex(0x22, 0x22, 0x22);          // slightly lighter panel
    const ImVec4 red      = hex(0xFF, 0x22, 0x22);          // #FF2222
    const ImVec4 red_hov  = hex(0xFF, 0x44, 0x44);          // #FF4444
    const ImVec4 red_act  = hex(0xCC, 0x11, 0x11);          // #CC1111
    const ImVec4 red_dim  = hex(0xFF, 0x22, 0x22, 0.35f);   // dim red for headers
    const ImVec4 red_dimh = hex(0xFF, 0x44, 0x44, 0.50f);   // dim red hover
    const ImVec4 red_dima = hex(0xCC, 0x11, 0x11, 0.60f);   // dim red active
    const ImVec4 text     = hex(0xE8, 0xE8, 0xE8);          // light grey text
    const ImVec4 text_dis = hex(0x66, 0x66, 0x66);          // disabled text
    const ImVec4 zero     = ImVec4(0, 0, 0, 0);

    // Text
    c[ImGuiCol_Text]                  = text;
    c[ImGuiCol_TextDisabled]          = text_dis;
    c[ImGuiCol_TextLink]              = red_hov;
    c[ImGuiCol_TextSelectedBg]        = hex(0xFF, 0x22, 0x22, 0.30f);

    // Windows
    c[ImGuiCol_WindowBg]              = bg;
    c[ImGuiCol_ChildBg]               = panel;
    c[ImGuiCol_PopupBg]               = hex(0x14, 0x14, 0x14, 0.96f);
    c[ImGuiCol_Border]                = border;
    c[ImGuiCol_BorderShadow]          = zero;

    // Frames (inputs, checkboxes, etc.)
    c[ImGuiCol_FrameBg]               = surface;
    c[ImGuiCol_FrameBgHovered]        = hex(0x2E, 0x2E, 0x2E);
    c[ImGuiCol_FrameBgActive]         = hex(0x38, 0x38, 0x38);

    // Title bar
    c[ImGuiCol_TitleBg]               = hex(0x12, 0x12, 0x12);
    c[ImGuiCol_TitleBgActive]         = hex(0x18, 0x08, 0x08);
    c[ImGuiCol_TitleBgCollapsed]      = hex(0x0D, 0x0D, 0x0D, 0.60f);

    // Menu bar
    c[ImGuiCol_MenuBarBg]             = panel;

    // Scrollbar
    c[ImGuiCol_ScrollbarBg]           = hex(0x10, 0x10, 0x10, 0.60f);
    c[ImGuiCol_ScrollbarGrab]         = hex(0x3A, 0x3A, 0x3A);
    c[ImGuiCol_ScrollbarGrabHovered]  = hex(0x4A, 0x4A, 0x4A);
    c[ImGuiCol_ScrollbarGrabActive]   = red;

    // Check mark, slider
    c[ImGuiCol_CheckMark]             = red;
    c[ImGuiCol_SliderGrab]            = red;
    c[ImGuiCol_SliderGrabActive]      = red_hov;

    // Buttons
    c[ImGuiCol_Button]                = hex(0xFF, 0x22, 0x22, 0.65f);
    c[ImGuiCol_ButtonHovered]         = red_hov;
    c[ImGuiCol_ButtonActive]          = red_act;

    // Headers (collapsing headers, selectable, menu items)
    c[ImGuiCol_Header]                = red_dim;
    c[ImGuiCol_HeaderHovered]         = red_dimh;
    c[ImGuiCol_HeaderActive]          = red_dima;

    // Separators
    c[ImGuiCol_Separator]             = border;
    c[ImGuiCol_SeparatorHovered]      = red_hov;
    c[ImGuiCol_SeparatorActive]       = red;

    // Resize grip
    c[ImGuiCol_ResizeGrip]            = hex(0xFF, 0x22, 0x22, 0.20f);
    c[ImGuiCol_ResizeGripHovered]     = hex(0xFF, 0x22, 0x22, 0.50f);
    c[ImGuiCol_ResizeGripActive]      = red;

    // Input text cursor
    c[ImGuiCol_InputTextCursor]       = red;

    // Tabs
    c[ImGuiCol_Tab]                   = hex(0x1E, 0x1E, 0x1E);
    c[ImGuiCol_TabHovered]            = hex(0xFF, 0x22, 0x22, 0.60f);
    c[ImGuiCol_TabSelected]           = hex(0xFF, 0x22, 0x22, 0.50f);
    c[ImGuiCol_TabSelectedOverline]   = red;
    c[ImGuiCol_TabDimmed]             = hex(0x14, 0x14, 0x14);
    c[ImGuiCol_TabDimmedSelected]     = hex(0x28, 0x10, 0x10);
    c[ImGuiCol_TabDimmedSelectedOverline] = hex(0x80, 0x11, 0x11);

    // Docking
    c[ImGuiCol_DockingPreview]        = hex(0xFF, 0x22, 0x22, 0.50f);
    c[ImGuiCol_DockingEmptyBg]        = hex(0x0A, 0x0A, 0x0A);

    // Plots
    c[ImGuiCol_PlotLines]             = red;
    c[ImGuiCol_PlotLinesHovered]      = red_hov;
    c[ImGuiCol_PlotHistogram]         = red;
    c[ImGuiCol_PlotHistogramHovered]  = red_hov;

    // Tables
    c[ImGuiCol_TableHeaderBg]         = hex(0x18, 0x18, 0x18);
    c[ImGuiCol_TableBorderStrong]     = border;
    c[ImGuiCol_TableBorderLight]      = hex(0x22, 0x22, 0x22);
    c[ImGuiCol_TableRowBg]            = zero;
    c[ImGuiCol_TableRowBgAlt]         = hex(0xFF, 0xFF, 0xFF, 0.02f);

    // Tree lines
    c[ImGuiCol_TreeLines]             = border;

    // Drag & drop
    c[ImGuiCol_DragDropTarget]        = red;
    c[ImGuiCol_DragDropTargetBg]      = hex(0xFF, 0x22, 0x22, 0.10f);

    // Unsaved marker
    c[ImGuiCol_UnsavedMarker]         = red;

    // Nav
    c[ImGuiCol_NavCursor]             = red;
    c[ImGuiCol_NavWindowingHighlight] = hex(0xFF, 0xFF, 0xFF, 0.70f);
    c[ImGuiCol_NavWindowingDimBg]     = hex(0x00, 0x00, 0x00, 0.50f);

    // Modal
    c[ImGuiCol_ModalWindowDimBg]      = hex(0x00, 0x00, 0x00, 0.60f);
}

void render_glow_rect(ImVec2 min, ImVec2 max, float intensity) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    // 3 expanding layers of semi-transparent red
    for (int i = 1; i <= 3; ++i) {
        float expand = static_cast<float>(i) * 4.0f;
        float alpha  = (0.15f / static_cast<float>(i)) * intensity;
        ImU32 col    = IM_COL32(255, 34, 34, static_cast<int>(alpha * 255));
        dl->AddRectFilled(
            ImVec2(min.x - expand, min.y - expand),
            ImVec2(max.x + expand, max.y + expand),
            col, 6.0f);
    }
}

void render_glow_progress(ImVec2 min, ImVec2 max, float fraction) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float rounding = 4.0f;

    // Dark background
    dl->AddRectFilled(min, max, IM_COL32(0x10, 0x10, 0x10, 0xFF), rounding);

    if (fraction <= 0.0f) return;
    if (fraction > 1.0f) fraction = 1.0f;

    float fill_x = min.x + (max.x - min.x) * fraction;
    ImVec2 fill_max(fill_x, max.y);

    // Glow behind the filled portion (3 layers)
    for (int i = 1; i <= 3; ++i) {
        float expand = static_cast<float>(i) * 3.0f;
        float alpha  = 0.12f / static_cast<float>(i);
        ImU32 col    = IM_COL32(255, 34, 34, static_cast<int>(alpha * 255));
        dl->AddRectFilled(
            ImVec2(min.x - expand, min.y - expand),
            ImVec2(fill_x + expand, max.y + expand),
            col, rounding + expand * 0.5f);
    }

    // Filled portion
    dl->AddRectFilled(min, fill_max, IM_COL32(0xFF, 0x22, 0x22, 0xDD), rounding);

    // Subtle lighter highlight on top half of filled area
    float mid_y = min.y + (max.y - min.y) * 0.5f;
    dl->AddRectFilled(min, ImVec2(fill_x, mid_y), IM_COL32(0xFF, 0x66, 0x44, 0x30), rounding);

    // Border around the whole bar
    dl->AddRect(min, max, IM_COL32(0x2A, 0x2A, 0x2A, 0xFF), rounding);
}

} // namespace theme
