#include <chrono>
#include <filesystem>
#include <fstream>

#include "Session.hh"
#include "test_util.hh"

namespace fs = std::filesystem;

static fs::path scratch() {
    auto p = fs::temp_directory_path() / ("smgk-session-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(p);
    return fs::canonical(p);
}

static void write(const fs::path& p, const std::string& s) { std::ofstream(p) << s; }

int main() {
    fs::path root = scratch();
    fs::create_directories(root / ".smgk");
    fs::create_directories(root / "a" / "b");
    write(root / ".smgk" / "kernel.json", R"({"transport": "tcp", "ip": "127.0.0.1", "iopub_port": 51234, "key": "abc", "shell_port": 1})");
    write(
        root / ".smgk" / "state.json",
        R"({"generation": 3, "kernel_pid": 1, "recorder_pid": 2, "connection_file": ".smgk/kernel.json", "notebook": "notebooks/x.ipynb", "python": "p"})");

    auto sd = smgk::find_state_dir(root / "a" / "b");
    CHECK(sd && *sd == root / ".smgk");

    fs::path lonely = scratch();
    CHECK(!smgk::find_state_dir(lonely).has_value());

    auto conn = smgk::read_connection(root / ".smgk" / "kernel.json");
    CHECK(conn && conn->iopub_port == 51234 && conn->ip == "127.0.0.1" && conn->transport == "tcp" && conn->key == "abc");
    CHECK(!smgk::read_connection(root / "missing.json").has_value());

    auto st = smgk::read_state(root / ".smgk");
    CHECK(st && st->generation == 3);
    CHECK(st && st->root == root && st->connection_file == root / ".smgk" / "kernel.json" &&
          st->notebook == root / "notebooks" / "x.ipynb");

    write(root / ".smgk" / "state.json", "{broken");
    CHECK(!smgk::read_state(root / ".smgk").has_value());

    fs::remove_all(root);
    fs::remove_all(lonely);
    TEST_RETURN();
}
