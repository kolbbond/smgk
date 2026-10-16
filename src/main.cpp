// smgk-view: watches a project's Jupyter kernel and shows its cells, text and plots.
//   smgk-view [dir]     dir (default: cwd) is anywhere inside a project started with `smgk start`

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

    // also runs once up front, so the first frame already shows history
    void refresh() {
        _last_check = Clock::now();
        auto dir = smgk::find_state_dir(_start);
        auto st = dir ? smgk::read_state(*dir) : std::nullopt;
        if(!st) {
            if(_state) disconnect();
            return;
        }
        if(!_state || st->generation != _state->generation) connect(*st);
    }

  private:
    void disconnect() {
        _iopub.reset();
        _feed.reset();
        _view.clear();
        _state.reset();
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
            _view.draw(_feed);
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
    Clock::time_point _last_check{};
};

} // namespace

int main(int argc, char** argv) {
    // only a leading positional counts: smg's own flags take values (--smg-frames N)
    fs::path start = argc > 1 && argv[1][0] != '-' ? fs::path(argv[1]) : fs::current_path();

    smg::GuiConfig config;
    config.title = "smgk";
    config.size = {900, 1000};
    smg::GuiBase gui({argc, argv}, config);

    App app(start);
    app.refresh();
    gui.add_callback([&app]() {
        app.frame();
        return 0;
    });

    while(gui.mainLoopIteration()) {}
    // one line for scripts and tests to check what was shown
    std::printf("smgk-view: cells=%zu generation=%d\n", app.cells(), app.generation());
    return 0;
}
