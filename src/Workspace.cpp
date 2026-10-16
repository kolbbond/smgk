#include "Workspace.hh"

#include <algorithm>
#include <cctype>

namespace smgk {

namespace {

std::string lower(std::string_view s) {
    std::string out(s);
    for(char& c : out)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

const std::string& field(const Var& v, VarColumn c) {
    switch(c) {
    case VarColumn::Type:
        return v.type;
    case VarColumn::Size:
        return v.size;
    case VarColumn::Value:
        return v.value;
    case VarColumn::Name:
        break;
    }
    return v.name;
}

} // namespace

std::optional<Workspace> parse_workspace(const nlohmann::json& j) {
    if(!j.is_object() || !j.contains("vars") || !j["vars"].is_array()) return std::nullopt;
    Workspace ws;
    ws.generation = j.value("generation", 0);
    for(const auto& row : j["vars"]) {
        if(!row.is_object() || !row.contains("name") || !row["name"].is_string()) continue;
        auto str = [&](const char* k) { return row.contains(k) && row[k].is_string() ? row[k].get<std::string>() : std::string{}; };
        ws.vars.push_back({str("name"), str("type"), str("size"), str("value")});
    }
    return ws;
}

void sort_vars(std::vector<Var>& vars, VarColumn column, bool ascending) {
    std::stable_sort(vars.begin(), vars.end(), [&](const Var& a, const Var& b) {
        std::string fa = lower(field(a, column)), fb = lower(field(b, column));
        if(fa == fb) return lower(a.name) < lower(b.name);
        return ascending ? fa < fb : fa > fb;
    });
}

bool var_matches(const Var& v, std::string_view filter) {
    if(filter.empty()) return true;
    const std::string f = lower(filter);
    return lower(v.name).find(f) != std::string::npos || lower(v.type).find(f) != std::string::npos;
}

} // namespace smgk
