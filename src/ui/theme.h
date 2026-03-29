#pragma once
#include <imgui.h>

namespace theme {
void apply_dark_red();
void render_glow_rect(ImVec2 min, ImVec2 max, float intensity = 1.0f);
void render_glow_progress(ImVec2 min, ImVec2 max, float fraction);
} // namespace theme
