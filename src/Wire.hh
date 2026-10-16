#pragma once
// Jupyter wire protocol: multipart message -> Msg

#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace smgk {

struct Msg {
    std::string msg_type, msg_id, parent_msg_id, username;
    nlohmann::json content, metadata;
};

// frames after "<IDS|MSG>": signature, header, parent_header, metadata, content.
// The signature isn't verified: the viewer only reads, from a local kernel.
std::optional<Msg> parse_wire(const std::vector<std::string>& frames);

} // namespace smgk
