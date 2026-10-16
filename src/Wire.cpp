#include "Wire.hh"

#include <algorithm>

namespace smgk {

namespace {
std::string str_or_empty(const nlohmann::json& j, const char* key) {
    auto it = j.find(key);
    return it != j.end() && it->is_string() ? it->get<std::string>() : std::string{};
}
} // namespace

std::optional<Msg> parse_wire(const std::vector<std::string>& frames) {
    auto delim = std::find(frames.begin(), frames.end(), "<IDS|MSG>");
    if(delim == frames.end() || frames.end() - delim < 6) return std::nullopt;

    nlohmann::json parts[4];
    for(int i = 0; i < 4; ++i) {
        parts[i] = nlohmann::json::parse(*(delim + 2 + i), nullptr, false);
        if(parts[i].is_discarded() || !parts[i].is_object()) return std::nullopt;
    }
    const auto& [header, parent, metadata, content] = parts;

    Msg m;
    m.msg_type = str_or_empty(header, "msg_type");
    m.msg_id = str_or_empty(header, "msg_id");
    m.parent_msg_id = str_or_empty(parent, "msg_id");
    m.username = str_or_empty(parent, "username");
    m.metadata = metadata;
    m.content = content;
    return m;
}

} // namespace smgk
