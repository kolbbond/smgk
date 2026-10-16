#pragma once
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "Wire.hh"

inline std::string read_fixture(const std::string& name) {
    std::ifstream in(std::string(SMGK_FIXTURES) + "/" + name, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

inline std::vector<std::string> fixture_frames(const std::string& name) {
    return nlohmann::json::parse(read_fixture(name)).get<std::vector<std::string>>();
}

// synthetic IOPub message, for model tests that don't need real wire bytes
inline smgk::Msg make_msg(const std::string& type, const std::string& parent, nlohmann::json content,
                          const std::string& username = "user") {
    smgk::Msg m;
    m.msg_type = type;
    m.msg_id = type + "-" + parent;
    m.parent_msg_id = parent;
    m.username = username;
    m.content = std::move(content);
    m.metadata = nlohmann::json::object();
    return m;
}
