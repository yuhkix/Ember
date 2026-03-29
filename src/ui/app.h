#pragma once
#include "idx_reader.h"
#include "idx_writer.h"
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// Defined in main.cpp -- returns the main application HWND
HWND get_main_hwnd();

class App {
public:
    App();
    ~App();
    void render();

private:
    int m_active_tab = 0; // 0=Unpack, 1=Pack

    // Unpack state
    std::string m_idx_path;
    IdxReader   m_reader;
    bool        m_idx_loaded = false;
    std::string m_unpack_output_dir;

    // Pack state
    std::string    m_pack_source_dir;
    std::string    m_pack_output_path;
    int            m_pack_compression = 1; // 0=Raw, 1=LZMA, 2=CRN
    std::vector<std::string> m_pack_files_preview;

    // Worker thread
    std::thread       m_worker;
    std::atomic<bool> m_working{false};
    std::atomic<bool> m_cancel{false};
    std::atomic<float> m_progress{0.0f};

    // Status (thread-safe)
    std::mutex  m_status_mutex;
    std::string m_pending_status;
    bool        m_has_pending_status = false;

    void render_unpack_tab();
    void render_pack_tab();
    void render_status_bar();
    void start_unpack();
    void start_pack();
    void set_status(const std::string& msg);
};
