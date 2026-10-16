#include "Session.hh"

#include <fstream>
#include <system_error>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace smgk {

namespace {
nlohmann::json read_json(const fs::path& file) {
    std::ifstream in(file);
    if(!in) return nlohmann::json(nlohmann::json::value_t::discarded);
    return nlohmann::json::parse(in, nullptr, false);
}
} // namespace

std::optional<fs::path> find_state_dir(fs::path start) {
    std::error_code ec;
    fs::path dir = fs::weakly_canonical(start, ec);
    if(ec) return std::nullopt;
    while(true) {
        if(fs::is_directory(dir / ".smgk", ec)) return dir / ".smgk";
        if(dir == dir.parent_path()) return std::nullopt;
        dir = dir.parent_path();
    }
}

std::optional<ConnInfo> read_connection(const fs::path& file) {
    auto j = read_json(file);
    if(!j.is_object() || !j.contains("iopub_port")) return std::nullopt;
    return ConnInfo{j.value("transport", "tcp"), j.value("ip", "127.0.0.1"), j.value("key", ""), j.value("iopub_port", 0)};
}

std::optional<SessionState> read_state(const fs::path& state_dir) {
    auto j = read_json(state_dir / "state.json");
    if(!j.is_object() || !j.contains("generation")) return std::nullopt;
    SessionState s;
    s.root = state_dir.parent_path();
    s.generation = j.value("generation", 0);
    s.connection_file = s.root / j.value("connection_file", ".smgk/kernel.json");
    s.notebook = s.root / j.value("notebook", "");
    return s;
}

} // namespace smgk
