#include "app.h"
#include "theme.h"
#include "anim.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <nfd.h>

#include <cstdio>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const char* compression_label(CompressionType c) {
    switch (c) {
    case CompressionType::Raw:    return "Raw";
    case CompressionType::Lzma:   return "LZMA";
    case CompressionType::Crunch: return "CRN";
    }
    return "???";
}

static std::string format_size(uint32_t bytes) {
    if (bytes < 1024)
        return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0);
        return buf;
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f MB", bytes / (1024.0 * 1024.0));
    return buf;
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

App::App() {
    NFD_Init();
}

App::~App() {
    m_cancel = true;
    if (m_worker.joinable())
        m_worker.join();
    NFD_Quit();
}

// ---------------------------------------------------------------------------
// Thread-safe status
// ---------------------------------------------------------------------------

void App::set_status(const std::string& msg) {
    std::lock_guard<std::mutex> lock(m_status_mutex);
    m_pending_status = msg;
    m_has_pending_status = true;
}

// ---------------------------------------------------------------------------
// Custom title bar
// ---------------------------------------------------------------------------

static constexpr float TITLE_BAR_H   = 32.0f;
static constexpr float TITLE_BTN_W   = 46.0f;

static void render_title_bar() {
    HWND hwnd = get_main_hwnd();
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 pos = vp->Pos;
    ImVec2 size = ImVec2(vp->Size.x, TITLE_BAR_H);

    // Set up a frameless window for the title bar
    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.07f, 0.07f, 0.07f, 1.0f));

    ImGuiWindowFlags tb_flags = ImGuiWindowFlags_NoTitleBar |
                                ImGuiWindowFlags_NoResize   |
                                ImGuiWindowFlags_NoMove     |
                                ImGuiWindowFlags_NoScrollbar |
                                ImGuiWindowFlags_NoScrollWithMouse |
                                ImGuiWindowFlags_NoCollapse |
                                ImGuiWindowFlags_NoBringToFrontOnFocus |
                                ImGuiWindowFlags_NoNavFocus |
                                ImGuiWindowFlags_NoFocusOnAppearing;

    ImGui::Begin("##TitleBar", nullptr, tb_flags);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Draw bottom border line
    dl->AddLine(
        ImVec2(pos.x, pos.y + TITLE_BAR_H - 1),
        ImVec2(pos.x + size.x, pos.y + TITLE_BAR_H - 1),
        IM_COL32(0x2A, 0x2A, 0x2A, 0xFF));

    // -- Left side: title text --
    float text_y = pos.y + (TITLE_BAR_H - ImGui::GetFontSize()) * 0.5f;

    // "PROPHECY" in red
    const char* title_red = "PROPHECY";
    dl->AddText(ImVec2(pos.x + 12.0f, text_y),
                IM_COL32(0xFF, 0x22, 0x22, 0xFF), title_red);
    float red_w = ImGui::CalcTextSize(title_red).x;

    // Subtitle in grey
    const char* title_grey = "  Dragon's Prophet IDX Tool";
    dl->AddText(ImVec2(pos.x + 12.0f + red_w, text_y),
                IM_COL32(0x80, 0x80, 0x80, 0xFF), title_grey);

    // -- Right side: window control buttons --
    float btn_y = pos.y;
    float btn_x_start = pos.x + size.x - (TITLE_BTN_W * 3.0f);

    // Helper: draw a title bar button, returns true if clicked
    struct BtnResult { bool clicked; bool hovered; };
    auto draw_button = [&](int index, bool is_close) -> BtnResult {
        float bx = btn_x_start + TITLE_BTN_W * index;
        ImVec2 bmin(bx, btn_y);
        ImVec2 bmax(bx + TITLE_BTN_W, btn_y + TITLE_BAR_H);

        // Use invisible button for click detection
        ImGui::SetCursorScreenPos(bmin);
        char id[32];
        snprintf(id, sizeof(id), "##tb_%d", index);
        ImGui::InvisibleButton(id, ImVec2(TITLE_BTN_W, TITLE_BAR_H));

        bool hovered = ImGui::IsItemHovered();
        bool active  = ImGui::IsItemActive();
        bool clicked = ImGui::IsItemClicked(0);

        // Hover/active backgrounds
        if (active) {
            ImU32 bg = is_close ? IM_COL32(0xCC, 0x11, 0x11, 0xFF)
                                : IM_COL32(0x50, 0x50, 0x50, 0xFF);
            dl->AddRectFilled(bmin, bmax, bg);
        } else if (hovered) {
            ImU32 bg = is_close ? IM_COL32(0xE8, 0x1A, 0x1A, 0xFF)
                                : IM_COL32(0x3A, 0x3A, 0x3A, 0xFF);
            dl->AddRectFilled(bmin, bmax, bg);
        }

        return { clicked, hovered };
    };

    // Button 0: Minimize (horizontal line)
    {
        auto r = draw_button(0, false);
        float cx = btn_x_start + TITLE_BTN_W * 0.5f;
        float cy = btn_y + TITLE_BAR_H * 0.5f;
        ImU32 col = r.hovered ? IM_COL32(0xFF, 0xFF, 0xFF, 0xFF)
                              : IM_COL32(0xCC, 0xCC, 0xCC, 0xFF);
        dl->AddLine(ImVec2(cx - 6.0f, cy), ImVec2(cx + 6.0f, cy), col, 1.2f);
        if (r.clicked) ShowWindow(hwnd, SW_MINIMIZE);
    }

    // Button 1: Maximize/Restore (rectangle or overlapping rectangles)
    {
        auto r = draw_button(1, false);
        float cx = btn_x_start + TITLE_BTN_W * 1.0f + TITLE_BTN_W * 0.5f;
        float cy = btn_y + TITLE_BAR_H * 0.5f;
        ImU32 col = r.hovered ? IM_COL32(0xFF, 0xFF, 0xFF, 0xFF)
                              : IM_COL32(0xCC, 0xCC, 0xCC, 0xFF);
        bool maximized = IsZoomed(hwnd);
        if (maximized) {
            // Two overlapping rectangles (restore icon)
            float s = 4.5f;
            float off = 2.0f;
            dl->AddRect(ImVec2(cx - s + off, cy - s),
                        ImVec2(cx + s, cy + s - off), col, 0.0f, 0, 1.2f);
            dl->AddRect(ImVec2(cx - s, cy - s + off),
                        ImVec2(cx + s - off, cy + s), col, 0.0f, 0, 1.2f);
        } else {
            // Single rectangle (maximize icon)
            float s = 5.0f;
            dl->AddRect(ImVec2(cx - s, cy - s),
                        ImVec2(cx + s, cy + s), col, 0.0f, 0, 1.2f);
        }
        if (r.clicked) {
            ShowWindow(hwnd, maximized ? SW_RESTORE : SW_MAXIMIZE);
        }
    }

    // Button 2: Close (X)
    {
        auto r = draw_button(2, true);
        float cx = btn_x_start + TITLE_BTN_W * 2.0f + TITLE_BTN_W * 0.5f;
        float cy = btn_y + TITLE_BAR_H * 0.5f;
        ImU32 col = r.hovered ? IM_COL32(0xFF, 0xFF, 0xFF, 0xFF)
                              : IM_COL32(0xCC, 0xCC, 0xCC, 0xFF);
        float s = 5.0f;
        dl->AddLine(ImVec2(cx - s, cy - s), ImVec2(cx + s, cy + s), col, 1.2f);
        dl->AddLine(ImVec2(cx + s, cy - s), ImVec2(cx - s, cy + s), col, 1.2f);
        if (r.clicked) PostMessageW(hwnd, WM_CLOSE, 0, 0);
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

// ---------------------------------------------------------------------------
// render()
// ---------------------------------------------------------------------------

void App::render() {
    // Flush pending status from worker thread
    {
        std::lock_guard<std::mutex> lock(m_status_mutex);
        if (m_has_pending_status) {
            anim::set_status(m_pending_status);
            m_has_pending_status = false;
        }
    }

    // Render custom title bar first
    render_title_bar();

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    // Position main content below the title bar
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x, vp->Pos.y + TITLE_BAR_H));
    ImGui::SetNextWindowSize(ImVec2(vp->Size.x, vp->Size.y - TITLE_BAR_H));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoResize   |
                             ImGuiWindowFlags_NoMove     |
                             ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::Begin("##Main", nullptr, flags);

    // (Title is now in the custom title bar, no need for in-content title)

    ImGui::Separator();

    // Tab bar
    if (ImGui::BeginTabBar("##Tabs")) {
        if (ImGui::BeginTabItem("Unpack")) {
            m_active_tab = 0;
            float alpha = anim::tab_fade(0);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * alpha);
            render_unpack_tab();
            ImGui::PopStyleVar();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Pack")) {
            m_active_tab = 1;
            float alpha = anim::tab_fade(1);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * alpha);
            render_pack_tab();
            ImGui::PopStyleVar();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    // Status bar at the bottom
    render_status_bar();

    ImGui::End();
}

// ---------------------------------------------------------------------------
// render_unpack_tab()
// ---------------------------------------------------------------------------

void App::render_unpack_tab() {
    // --- IDX File row ---
    ImGui::Text("IDX File:");
    ImGui::SameLine();
    {
        char buf[512];
        std::memset(buf, 0, sizeof(buf));
        std::strncpy(buf, m_idx_path.c_str(), sizeof(buf) - 1);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 80.0f);
        ImGui::InputText("##idx_path", buf, sizeof(buf), ImGuiInputTextFlags_ReadOnly);
    }
    ImGui::SameLine();
    if (ImGui::Button("Browse##idx")) {
        nfdu8char_t* outPath = nullptr;
        nfdu8filteritem_t filter = { "IDX files", "idx" };
        nfdopendialogu8args_t args = {};
        args.filterList  = &filter;
        args.filterCount = 1;
        if (NFD_OpenDialogU8_With(&outPath, &args) == NFD_OKAY) {
            m_idx_path = outPath;
            NFD_FreePathU8(outPath);

            // Open the IDX
            if (m_reader.open(m_idx_path)) {
                m_idx_loaded = true;
                // Default output dir = IDX path without extension
                fs::path p(m_idx_path);
                m_unpack_output_dir = (p.parent_path() / p.stem()).string();
                anim::reset_stagger();
                set_status("Loaded " + std::to_string(m_reader.entries().size()) + " entries");
            } else {
                m_idx_loaded = false;
                set_status("Error: " + m_reader.error());
            }
        }
    }

    // --- Output dir row ---
    ImGui::Text("Output:  ");
    ImGui::SameLine();
    {
        char buf[512];
        std::memset(buf, 0, sizeof(buf));
        std::strncpy(buf, m_unpack_output_dir.c_str(), sizeof(buf) - 1);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 80.0f);
        if (ImGui::InputText("##out_dir", buf, sizeof(buf))) {
            m_unpack_output_dir = buf;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Browse##outdir")) {
        nfdu8char_t* outPath = nullptr;
        if (NFD_PickFolderU8(&outPath, nullptr) == NFD_OKAY) {
            m_unpack_output_dir = outPath;
            NFD_FreePathU8(outPath);
        }
    }

    ImGui::Spacing();

    // --- Extract All / Cancel buttons ---
    {
        bool disabled = !m_idx_loaded || m_working;
        if (disabled) ImGui::BeginDisabled();
        if (ImGui::Button("Extract All")) {
            start_unpack();
        }
        if (disabled) ImGui::EndDisabled();
    }

    if (m_working && m_active_tab == 0) {
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            m_cancel = true;
        }
    }

    // --- Progress bar ---
    if (m_working || m_progress > 0.0f) {
        float smoothed = anim::lerp("unpack_progress", m_progress);
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        ImVec2 avail  = ImGui::GetContentRegionAvail();
        float bar_h   = 20.0f;
        ImVec2 bar_min = cursor;
        ImVec2 bar_max = ImVec2(cursor.x + avail.x, cursor.y + bar_h);
        theme::render_glow_progress(bar_min, bar_max, smoothed);
        ImGui::Dummy(ImVec2(avail.x, bar_h + 4.0f));

        // Percentage text centered
        char pct_buf[16];
        std::snprintf(pct_buf, sizeof(pct_buf), "%d%%", static_cast<int>(smoothed * 100.0f));
        ImVec2 text_size = ImGui::CalcTextSize(pct_buf);
        ImVec2 text_pos(
            bar_min.x + (bar_max.x - bar_min.x - text_size.x) * 0.5f,
            bar_min.y + (bar_h - text_size.y) * 0.5f
        );
        ImGui::GetWindowDrawList()->AddText(text_pos, IM_COL32(255, 255, 255, 220), pct_buf);
    }

    ImGui::Spacing();

    // --- File list table ---
    if (m_idx_loaded) {
        const auto& entries = m_reader.entries();
        float table_h = ImGui::GetContentRegionAvail().y - 8.0f;
        if (table_h < 100.0f) table_h = 100.0f;

        ImGuiTableFlags tflags = ImGuiTableFlags_Borders    |
                                 ImGuiTableFlags_RowBg      |
                                 ImGuiTableFlags_Resizable  |
                                 ImGuiTableFlags_ScrollY;

        if (ImGui::BeginTable("##FileList", 5, tflags, ImVec2(0, table_h))) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Name",       ImGuiTableColumnFlags_None, 300.0f);
            ImGui::TableSetupColumn("Offset",     ImGuiTableColumnFlags_None, 100.0f);
            ImGui::TableSetupColumn("Size",       ImGuiTableColumnFlags_None, 100.0f);
            ImGui::TableSetupColumn("Compressed", ImGuiTableColumnFlags_None, 100.0f);
            ImGui::TableSetupColumn("Type",       ImGuiTableColumnFlags_None, 60.0f);
            ImGui::TableHeadersRow();

            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(entries.size()));
            while (clipper.Step()) {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                    const auto& e = entries[row];
                    float row_alpha = anim::row_fade(row);
                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * row_alpha);

                    ImGui::TableNextRow();

                    // Name
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(e.filename.c_str());

                    // Offset
                    ImGui::TableSetColumnIndex(1);
                    char off_buf[16];
                    std::snprintf(off_buf, sizeof(off_buf), "%08X", e.fixed.data_offset);
                    ImGui::TextUnformatted(off_buf);

                    // Size (total_size)
                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextUnformatted(format_size(e.data_header.total_size).c_str());

                    // Compressed size
                    ImGui::TableSetColumnIndex(3);
                    ImGui::TextUnformatted(format_size(e.data_header.compressed_size).c_str());

                    // Type
                    ImGui::TableSetColumnIndex(4);
                    ImGui::TextUnformatted(compression_label(e.compression));

                    ImGui::PopStyleVar();
                }
            }

            ImGui::EndTable();
        }
    }
}

// ---------------------------------------------------------------------------
// render_pack_tab()
// ---------------------------------------------------------------------------

void App::render_pack_tab() {
    // --- Source folder row ---
    ImGui::Text("Source Folder:");
    ImGui::SameLine();
    {
        char buf[512];
        std::memset(buf, 0, sizeof(buf));
        std::strncpy(buf, m_pack_source_dir.c_str(), sizeof(buf) - 1);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 80.0f);
        if (ImGui::InputText("##pack_src", buf, sizeof(buf))) {
            m_pack_source_dir = buf;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Browse##packsrc")) {
        nfdu8char_t* outPath = nullptr;
        if (NFD_PickFolderU8(&outPath, nullptr) == NFD_OKAY) {
            m_pack_source_dir = outPath;
            NFD_FreePathU8(outPath);

            // Scan directory for preview
            m_pack_files_preview.clear();
            std::error_code ec;
            for (auto& de : fs::recursive_directory_iterator(m_pack_source_dir, ec)) {
                if (ec) break;
                if (!de.is_regular_file()) continue;
                fs::path rel = fs::relative(de.path(), m_pack_source_dir, ec);
                if (!ec)
                    m_pack_files_preview.push_back(rel.string());
            }
            std::sort(m_pack_files_preview.begin(), m_pack_files_preview.end());
            anim::reset_stagger();
            set_status("Found " + std::to_string(m_pack_files_preview.size()) + " files to pack");
        }
    }

    // --- Output IDX row ---
    ImGui::Text("Output IDX:   ");
    ImGui::SameLine();
    {
        char buf[512];
        std::memset(buf, 0, sizeof(buf));
        std::strncpy(buf, m_pack_output_path.c_str(), sizeof(buf) - 1);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 80.0f);
        if (ImGui::InputText("##pack_out", buf, sizeof(buf))) {
            m_pack_output_path = buf;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Browse##packout")) {
        nfdu8char_t* outPath = nullptr;
        nfdu8filteritem_t filter = { "IDX files", "idx" };
        nfdsavedialogu8args_t args = {};
        args.filterList  = &filter;
        args.filterCount = 1;
        args.defaultName = "output.idx";
        if (NFD_SaveDialogU8_With(&outPath, &args) == NFD_OKAY) {
            m_pack_output_path = outPath;
            NFD_FreePathU8(outPath);
        }
    }

    ImGui::Spacing();

    // --- Compression selector ---
    ImGui::Text("Compression:");
    ImGui::SameLine();
    ImGui::RadioButton("Raw",  &m_pack_compression, 0);
    ImGui::SameLine();
    ImGui::RadioButton("LZMA", &m_pack_compression, 1);
    ImGui::SameLine();
    ImGui::RadioButton("CRN",  &m_pack_compression, 2);

    ImGui::Spacing();

    // --- Pack / Cancel buttons ---
    {
        bool disabled = m_pack_source_dir.empty() || m_pack_output_path.empty() || m_working;
        if (disabled) ImGui::BeginDisabled();
        if (ImGui::Button("Pack")) {
            start_pack();
        }
        if (disabled) ImGui::EndDisabled();
    }

    if (m_working && m_active_tab == 1) {
        ImGui::SameLine();
        if (ImGui::Button("Cancel##pack")) {
            m_cancel = true;
        }
    }

    // --- Progress bar ---
    if (m_working || m_progress > 0.0f) {
        float smoothed = anim::lerp("pack_progress", m_progress);
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        ImVec2 avail  = ImGui::GetContentRegionAvail();
        float bar_h   = 20.0f;
        ImVec2 bar_min = cursor;
        ImVec2 bar_max = ImVec2(cursor.x + avail.x, cursor.y + bar_h);
        theme::render_glow_progress(bar_min, bar_max, smoothed);
        ImGui::Dummy(ImVec2(avail.x, bar_h + 4.0f));

        char pct_buf[16];
        std::snprintf(pct_buf, sizeof(pct_buf), "%d%%", static_cast<int>(smoothed * 100.0f));
        ImVec2 text_size = ImGui::CalcTextSize(pct_buf);
        ImVec2 text_pos(
            bar_min.x + (bar_max.x - bar_min.x - text_size.x) * 0.5f,
            bar_min.y + (bar_h - text_size.y) * 0.5f
        );
        ImGui::GetWindowDrawList()->AddText(text_pos, IM_COL32(255, 255, 255, 220), pct_buf);
    }

    ImGui::Spacing();

    // --- File preview table ---
    if (!m_pack_files_preview.empty()) {
        float table_h = ImGui::GetContentRegionAvail().y - 8.0f;
        if (table_h < 100.0f) table_h = 100.0f;

        ImGuiTableFlags tflags = ImGuiTableFlags_Borders   |
                                 ImGuiTableFlags_RowBg     |
                                 ImGuiTableFlags_ScrollY;

        if (ImGui::BeginTable("##PackPreview", 1, tflags, ImVec2(0, table_h))) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("File", ImGuiTableColumnFlags_None);
            ImGui::TableHeadersRow();

            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(m_pack_files_preview.size()));
            while (clipper.Step()) {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                    float row_alpha = anim::row_fade(row);
                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * row_alpha);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(m_pack_files_preview[row].c_str());

                    ImGui::PopStyleVar();
                }
            }

            ImGui::EndTable();
        }
    }
}

// ---------------------------------------------------------------------------
// render_status_bar()
// ---------------------------------------------------------------------------

void App::render_status_bar() {
    float alpha = anim::status_alpha();
    if (alpha > 0.0f) {
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() - ImGui::GetFrameHeightWithSpacing() - 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
        ImGui::TextUnformatted(anim::status_text().c_str());
        ImGui::PopStyleVar();
    }
}

// ---------------------------------------------------------------------------
// start_unpack()
// ---------------------------------------------------------------------------

void App::start_unpack() {
    if (m_worker.joinable())
        m_worker.join();

    m_working  = true;
    m_cancel   = false;
    m_progress = 0.0f;

    std::string out_dir = m_unpack_output_dir;

    m_worker = std::thread([this, out_dir]() {
        int result = m_reader.extract_all(out_dir, [this](int current, int total) -> bool {
            m_progress = static_cast<float>(current) / static_cast<float>(total);
            return !m_cancel.load();
        });

        if (m_cancel) {
            set_status("Extraction cancelled");
        } else {
            set_status("Extracted " + std::to_string(result) + " files");
        }

        m_progress = m_cancel ? 0.0f : 1.0f;
        m_working  = false;
    });
}

// ---------------------------------------------------------------------------
// start_pack()
// ---------------------------------------------------------------------------

void App::start_pack() {
    if (m_worker.joinable())
        m_worker.join();

    m_working  = true;
    m_cancel   = false;
    m_progress = 0.0f;

    std::string src   = m_pack_source_dir;
    std::string out   = m_pack_output_path;
    int comp          = m_pack_compression;

    m_worker = std::thread([this, src, out, comp]() {
        PackOptions opts;
        switch (comp) {
        case 0: opts.compression = PackCompression::Raw;    break;
        case 1: opts.compression = PackCompression::Lzma;   break;
        case 2: opts.compression = PackCompression::Crunch;  break;
        default: opts.compression = PackCompression::Lzma;   break;
        }

        IdxWriter writer;
        bool ok = writer.pack(src, out, opts, [this](int current, int total) -> bool {
            m_progress = static_cast<float>(current) / static_cast<float>(total);
            return !m_cancel.load();
        });

        if (m_cancel) {
            set_status("Packing cancelled");
        } else if (ok) {
            set_status("Pack complete: " + out);
        } else {
            set_status("Pack error: " + writer.error());
        }

        m_progress = m_cancel ? 0.0f : 1.0f;
        m_working  = false;
    });
}
