#pragma once
// the kernel's namespace summary, written by the recorder to .smgk/workspace.json

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace smgk {

struct Var {
    std::string name, type, size, value;
};

struct Workspace {
    int generation = 0;
    std::vector<Var> vars;
};

enum class VarColumn { Name, Type, Size, Value };

// malformed rows are skipped; a malformed document is nullopt
std::optional<Workspace> parse_workspace(const nlohmann::json& j);
// case-insensitive, stable; ties keep name order
void sort_vars(std::vector<Var>& vars, VarColumn column, bool ascending);
// case-insensitive substring of name or type; empty filter matches all
bool var_matches(const Var& v, std::string_view filter);

} // namespace smgk
