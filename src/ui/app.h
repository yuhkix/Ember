#pragma once
#include "idx_reader.h"
#include "idx_writer.h"
#include "hidx_reader.h"
#include "fsidx_reader.h"
#include "widgets.h"
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
    int m_active_tab = 0; // 0=Unpack, 1=Pack, 2=Inject, 3=Index

    // Unpack state
    std::string m_idx_path;
    IdxReader   m_reader;
    bool        m_idx_loaded = false;
    std::string m_unpack_output_dir;

    // Extracted files list (populated live during extraction)
    struct ExtractedFile {
        std::string name;
        std::string size_str;
        std::string type_str;
        bool        success;
    };
    std::mutex                  m_extracted_mutex;
    std::vector<ExtractedFile>  m_extracted_files;
    int                         m_extracted_stagger_base = 0; // for animation offset

    // Widget states for custom lists
    widgets::FileListState m_unpack_list_state;
    widgets::FileListState m_pack_list_state;
    widgets::FileListState m_hidx_list_state;
    widgets::FileListState m_fsidx_list_state;

    // Pack state
    std::string    m_pack_source_dir;
    std::string    m_pack_output_path;
    int            m_pack_compression = 0; // 0=Auto, 1=Raw, 2=LZMA, 3=CRN
    std::vector<std::string> m_pack_files_preview;

    // Inject state
    std::string m_inject_idx_path;      // existing .idx to inject into
    std::string m_inject_source_dir;    // files to inject
    int         m_inject_compression = 0; // 0=Auto, 1=Raw, 2=LZMA, 3=CRN
    std::vector<std::string> m_inject_files_preview;
    widgets::FileListState m_inject_list_state;

    // Worker thread
    std::thread       m_worker;
    std::atomic<bool> m_working{false};
    std::atomic<bool> m_cancel{false};
    std::atomic<float> m_progress{0.0f};

    // Status (thread-safe)
    std::mutex  m_status_mutex;
    std::string m_pending_status;
    bool        m_has_pending_status = false;

    // Index viewer state
    HidxReader   m_hidx_reader;
    FsidxReader  m_fsidx_reader;
    bool         m_hidx_loaded = false;
    bool         m_fsidx_loaded = false;
    std::string  m_hidx_path;
    std::string  m_fsidx_path;

    void render_unpack_tab();
    void render_pack_tab();
    void render_inject_tab();
    void render_index_tab();
    void render_status_bar();
    void start_unpack();
    void start_pack();
    void start_inject();
    void set_status(const std::string& msg);
};
