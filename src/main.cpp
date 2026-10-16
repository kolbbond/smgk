// smgk-view: watches a project's Jupyter kernel and shows its cells, text and plots.
//   smgk-view [dir]     dir (default: cwd) is anywhere inside a project started with `smgk start`

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>

#include <imgui.h>
#include <smg/GuiBase.hh>

#include "Feed.hh"
#include "FeedView.hh"
#include "Iopub.hh"
#include "Session.hh"
#include "WorkspaceView.hh"

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

namespace {

class App {
  public:
    explicit App(fs::path start) : _start(std::move(start)) {}

    void frame() {
        if(Clock::now() - _last_check > std::chrono::seconds(1)) refresh();
        if(_iopub)
            for(const auto& m : _iopub->drain())
                _feed.apply(m);
        draw();
    }

    [[nodiscard]] std::size_t cells() const { return _feed.cells().size(); }
    [[nodiscard]] int generation() const { return _state ? _state->generation : 0; }
    [[nodiscard]] std::size_t vars() const { return _workspace.size(); }

    // also runs once up front, so the first frame already shows history
    void refresh() {
        _last_check = Clock::now();
        auto dir = smgk::find_state_dir(_start);
        auto st = dir ? smgk::read_state(*dir) : std::nullopt;
        if(!st) {
            if(_state) disconnect();
            return;
        }
        // a new kernel (restart, or a crash's successor) means new ports and a new notebook
        if(!_state || st->generation != _state->generation || st->kernel_pid != _state->kernel_pid) connect(*st);
        read_workspace(*dir / "workspace.json");
    }

  private:
    void disconnect() {
        _iopub.reset();
        _feed.reset();
        _view.clear();
        _workspace.clear();
        _workspace_mtime = {};
        _state.reset();
    }

    // the recorder rewrites this after each cell; one left by a previous kernel is ignored
    void read_workspace(const fs::path& file) {
        std::error_code ec;
        const auto mtime = fs::last_write_time(file, ec);
        if(ec || mtime == _workspace_mtime) return;
        _workspace_mtime = mtime;
        std::ifstream in(file);
        auto ws = smgk::parse_workspace(nlohmann::json::parse(in, nullptr, false));
        if(ws && _state && ws->generation == _state->generation) _workspace.set(std::move(ws->vars));
        else _workspace.clear();
    }

    void connect(const smgk::SessionState& st) {
        disconnect();
        auto conn = smgk::read_connection(st.connection_file);
        if(!conn) return;
        // subscribe before reading history: anything after the recorder's last write then
        // arrives live instead of falling in the gap
        _iopub = std::make_unique<smgk::IopubClient>(*conn);
        std::ifstream nb(st.notebook);
        if(nb) _feed.load_notebook(nlohmann::json::parse(nb, nullptr, false));
        _state = st;
        // goes to .smgk/view.log; tests also sync on it
        std::printf("smgk-view: connected generation=%d kernel_pid=%ld cells=%zu\n", st.generation, st.kernel_pid, _feed.cells().size());
        std::fflush(stdout);
    }

    void draw() {
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(vp->WorkSize);
        ImGui::Begin("##smgk", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoBringToFrontOnFocus);
        if(!_state) {
            const char* msg = "no kernel: run smgk start";
            const ImVec2 size = ImGui::CalcTextSize(msg);
            ImGui::SetCursorPos({(vp->WorkSize.x - size.x) * 0.5f, (vp->WorkSize.y - size.y) * 0.5f});
            ImGui::TextDisabled("%s", msg);
        } else {
            status_bar();
            if(ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_W)) _show_workspace = !_show_workspace;
            if(_show_workspace) {
                const float width = std::max(260.0f, ImGui::GetContentRegionAvail().x * 0.34f);
                ImGui::BeginChild("##main", {ImGui::GetContentRegionAvail().x - width - ImGui::GetStyle().ItemSpacing.x, 0});
                _view.draw(_feed);
                ImGui::EndChild();
                ImGui::SameLine();
                ImGui::BeginChild("##workspace", {0, 0}, ImGuiChildFlags_Borders);
                ImGui::TextDisabled("Workspace  (Ctrl+W)");
                _workspace.draw();
                ImGui::EndChild();
            } else {
                _view.draw(_feed);
            }
        }
        ImGui::End();
    }

    void status_bar() {
        const std::string& s = _feed.status();
        const ImU32 color = s == "busy"   ? IM_COL32(240, 180, 60, 255)
                            : s == "idle" ? IM_COL32(90, 200, 110, 255)
                                          : IM_COL32(120, 120, 120, 255);
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float r = ImGui::GetTextLineHeight() * 0.35f;
        ImGui::GetWindowDrawList()->AddCircleFilled({p.x + r, p.y + ImGui::GetTextLineHeight() * 0.5f}, r, color);
        ImGui::Dummy({r * 2.5f, ImGui::GetTextLineHeight()});
        ImGui::SameLine();
        ImGui::TextDisabled("%s  gen %d  %s", s.c_str(), _state->generation, _state->root.string().c_str());
        ImGui::Separator();
    }

    fs::path _start;
    std::optional<smgk::SessionState> _state;
    std::unique_ptr<smgk::IopubClient> _iopub;
    smgk::Feed _feed;
    smgk::FeedView _view;
    smgk::WorkspaceView _workspace;
    fs::file_time_type _workspace_mtime{};
    bool _show_workspace = true;
    Clock::time_point _last_check{};
};

} // namespace

int main(int argc, char** argv) {
    // only a leading positional counts: smg's own flags take values (--smg-frames N)
    fs::path start = argc > 1 && argv[1][0] != '-' ? fs::path(argv[1]) : fs::current_path();

    smg::GuiConfig config;
    config.title = "smgk";
    config.size = {1300, 1000}; // feed plus the workspace panel
    smg::GuiBase gui({argc, argv}, config);
    // ImGui would otherwise drop imgui.ini into the cwd, i.e. the user's project
    ImGui::GetIO().IniFilename = nullptr;

    App app(start);
    app.refresh();
    gui.add_callback([&app]() {
        app.frame();
        return 0;
    });

    while(gui.mainLoopIteration()) {}
    // one line for scripts and tests to check what was shown
    std::printf("smgk-view: cells=%zu generation=%d vars=%zu\n", app.cells(), app.generation(), app.vars());
    return 0;
}
