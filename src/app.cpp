#include "app.h"
#include <imgui.h>

App::App() {}
App::~App() {}

void App::render() {
    ImGui::Begin("##Main", nullptr, ImGuiWindowFlags_NoTitleBar);
    ImGui::Text("Prophecy - Loading...");
    ImGui::End();
}
