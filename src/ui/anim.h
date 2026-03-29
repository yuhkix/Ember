#pragma once
#include <imgui.h>
#include <unordered_map>
#include <string>

namespace anim {
void init();
void update(); // call once per frame
float lerp(const char* id, float target, float speed = 6.0f); // smooth value transition
float tab_fade(int active_tab); // tab switch fade, returns 0..1
float row_fade(int row_index); // staggered row fade-in
void  reset_stagger(); // call when list content changes
void  set_status(const std::string& msg, float duration = 3.0f);
float status_alpha();
const std::string& status_text();
float window_fade(); // initial window fade-in, returns 0..1
} // namespace anim
