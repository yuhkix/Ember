#include "anim.h"
#include <algorithm>
#include <cmath>

namespace anim {

// ---------------------------------------------------------------------------
// File-static state
// ---------------------------------------------------------------------------

static float s_total_time    = 0.0f;
static float s_dt            = 0.0f;

// Lerp map
static std::unordered_map<std::string, float> s_lerp_values;

// Tab fade
static int   s_last_tab      = -1;
static float s_tab_alpha     = 1.0f;

// Staggered row fade
static float s_stagger_start = 0.0f;

// Status message
static std::string s_status_msg;
static float       s_status_duration  = 0.0f;
static float       s_status_remaining = 0.0f;

// Window fade
static bool  s_window_started = false;
static float s_window_alpha   = 0.0f;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static float ease_out(float t) {
    // Cubic ease-out: 1 - (1-t)^3
    float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void init() {
    s_total_time       = 0.0f;
    s_dt               = 0.0f;
    s_lerp_values.clear();
    s_last_tab         = -1;
    s_tab_alpha        = 1.0f;
    s_stagger_start    = 0.0f;
    s_status_msg.clear();
    s_status_duration  = 0.0f;
    s_status_remaining = 0.0f;
    s_window_started   = false;
    s_window_alpha     = 0.0f;
}

void update() {
    ImGuiIO& io = ImGui::GetIO();
    s_dt = io.DeltaTime;
    s_total_time += s_dt;

    // Window fade (ramp over ~300ms)
    if (s_window_started && s_window_alpha < 1.0f) {
        s_window_alpha += s_dt / 0.3f;
        if (s_window_alpha > 1.0f) s_window_alpha = 1.0f;
    }

    // Status timer countdown
    if (s_status_remaining > 0.0f) {
        s_status_remaining -= s_dt;
        if (s_status_remaining < 0.0f) s_status_remaining = 0.0f;
    }
}

float lerp(const char* id, float target, float speed) {
    auto it = s_lerp_values.find(id);
    if (it == s_lerp_values.end()) {
        s_lerp_values[id] = target;
        return target;
    }

    float& val = it->second;
    float diff = target - val;

    // Snap if very close
    if (std::fabs(diff) < 0.001f) {
        val = target;
        return val;
    }

    float step = std::min(s_dt * speed, 1.0f);
    val += diff * step;
    return val;
}

float tab_fade(int active_tab) {
    if (active_tab != s_last_tab) {
        s_last_tab  = active_tab;
        s_tab_alpha = 0.0f;
    }

    if (s_tab_alpha < 1.0f) {
        // Speed 5.0 means ~200ms to reach 1.0
        s_tab_alpha += s_dt * 5.0f;
        if (s_tab_alpha > 1.0f) s_tab_alpha = 1.0f;
    }

    return ease_out(s_tab_alpha);
}

float row_fade(int row_index) {
    float delay     = static_cast<float>(row_index) * 0.015f; // 15ms per row
    float elapsed   = s_total_time - s_stagger_start - delay;

    if (elapsed < 0.0f) return 0.0f;
    if (elapsed >= 0.2f) return 1.0f; // 200ms fade

    float t = elapsed / 0.2f;
    return ease_out(t);
}

void reset_stagger() {
    s_stagger_start = s_total_time;
}

void set_status(const std::string& msg, float duration) {
    s_status_msg       = msg;
    s_status_duration  = duration;
    s_status_remaining = duration;
}

float status_alpha() {
    if (s_status_remaining <= 0.0f) return 0.0f;

    // Fade in quickly (first 100ms)
    float elapsed = s_status_duration - s_status_remaining;
    float fade_in = std::min(elapsed / 0.1f, 1.0f);

    // Fade out in last 0.5s
    float fade_out = 1.0f;
    if (s_status_remaining < 0.5f) {
        fade_out = s_status_remaining / 0.5f;
    }

    return fade_in * fade_out;
}

const std::string& status_text() {
    return s_status_msg;
}

float window_fade() {
    if (!s_window_started) {
        s_window_started = true;
        s_window_alpha   = 0.0f;
    }
    return ease_out(s_window_alpha);
}

} // namespace anim
