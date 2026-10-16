#pragma once
// finding a project's kernel: <root>/.smgk/{state.json,kernel.json}, written by the smgk CLI

#include <filesystem>
#include <optional>
#include <string>

namespace smgk {

struct ConnInfo {
    std::string transport, ip, key;
    int iopub_port = 0;
};

struct SessionState {
    int generation = 0;
    std::filesystem::path root, connection_file, notebook;
};

// nearest .smgk/ at or above start
std::optional<std::filesystem::path> find_state_dir(std::filesystem::path start);
std::optional<ConnInfo> read_connection(const std::filesystem::path& file);
// paths come back absolute, resolved against the project root (state_dir's parent)
std::optional<SessionState> read_state(const std::filesystem::path& state_dir);

} // namespace smgk
