// smgk-view: watches a project's Jupyter kernel and shows its cells, text and plots.

#include <smg/GuiBase.hh>
#include <imgui.h>

int main(int argc, char** argv) {
    smg::GuiConfig config;
    config.title = "smgk";
    config.size = { 900, 1000 };
    smg::GuiBase gui({ argc, argv }, config);

    gui.add_callback([]() {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("##smgk", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
        ImGui::End();
        return 0;
    });

    while(gui.mainLoopIteration()) {}
    return 0;
}
