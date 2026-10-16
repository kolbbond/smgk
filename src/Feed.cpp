#include "Feed.hh"

#include "Ansi.hh"

namespace smgk {

using json = nlohmann::json;

namespace {

// nbformat stores multiline strings either whole or as a list of lines
std::string joined(const json& j) {
    if(j.is_string()) return j.get<std::string>();
    std::string s;
    if(j.is_array())
        for(const auto& part : j)
            if(part.is_string()) s += part.get<std::string>();
    return s;
}

std::string smgk_kind(const json& metadata) {
    if(!metadata.is_object()) return {};
    auto it = metadata.find("smgk");
    if(it == metadata.end() || !it->is_object()) return {};
    auto k = it->find("kind");
    return k != it->end() && k->is_string() ? k->get<std::string>() : std::string{};
}

std::string traceback_text(const json& tb) {
    std::string s;
    if(tb.is_array())
        for(const auto& line : tb)
            s += strip_ansi(joined(line)) + "\n";
    return s;
}

Output data_output(const json& data) {
    Output o;
    if(data.contains("image/png")) {
        o.kind = Output::Kind::Image;
        o.png_b64 = joined(data["image/png"]);
    } else {
        o.kind = Output::Kind::Text;
        o.text = joined(data.value("text/plain", json("")));
    }
    return o;
}

void add_stream(std::vector<Output>& outs, const std::string& name, const std::string& text) {
    if(!outs.empty() && outs.back().kind == Output::Kind::Stream && outs.back().name == name) {
        outs.back().text += text;
        return;
    }
    Output o;
    o.kind = Output::Kind::Stream;
    o.name = name;
    o.text = text;
    outs.push_back(std::move(o));
}

} // namespace

Cell& Feed::cell_for(const std::string& parent, const std::string& kind, bool markdown) {
    auto it = _index.find(parent);
    if(it != _index.end()) return _cells[it->second];
    Cell c;
    c.msg_id = parent;
    c.kind = kind;
    c.markdown = markdown;
    _index[parent] = _cells.size();
    _cells.push_back(std::move(c));
    return _cells.back();
}

bool Feed::apply(const Msg& m) {
    const json& c = m.content;
    if(m.msg_type == "status") {
        _status = c.value("execution_state", _status);
        return true;
    }
    if(m.parent_msg_id.empty() || _loaded.count(m.parent_msg_id)) return false;

    if(m.msg_type == "execute_input") {
        Cell& cell = cell_for(m.parent_msg_id, m.username == "smgk" ? "run" : "cell", false);
        cell.source = c.value("code", "");
        cell.exec_count = c.value("execution_count", -1);
        return true;
    }

    const bool display = m.msg_type == "display_data" || m.msg_type == "execute_result";
    if(m.msg_type != "stream" && m.msg_type != "error" && !display) return false;

    std::string kind = display ? smgk_kind(c.value("metadata", json::object())) : "";
    if(kind.empty()) kind = m.username == "smgk" ? "run" : "cell";
    if(kind == "note" && !_index.count(m.parent_msg_id)) {
        Cell& note = cell_for(m.parent_msg_id, kind, true);
        const json& data = c.value("data", json::object());
        note.source = joined(data.value("text/markdown", data.value("text/plain", json(""))));
        return true;
    }

    Cell& cell = cell_for(m.parent_msg_id, kind, false);
    if(cell.markdown) return false;
    if(m.msg_type == "stream") {
        add_stream(cell.outputs, c.value("name", "stdout"), c.value("text", ""));
    } else if(m.msg_type == "error") {
        Output o;
        o.kind = Output::Kind::Error;
        o.text = traceback_text(c.value("traceback", json::array()));
        cell.outputs.push_back(std::move(o));
    } else {
        cell.outputs.push_back(data_output(c.value("data", json::object())));
    }
    return true;
}

void Feed::load_notebook(const json& nb) {
    if(!nb.is_object() || !nb.contains("cells") || !nb["cells"].is_array()) return;
    std::size_t i = 0;
    for(const auto& jc : nb["cells"]) {
        const json& md = jc.value("metadata", json::object());
        std::string id = md.contains("smgk") ? md["smgk"].value("msg_id", "") : "";
        if(id.empty()) id = "nb-" + std::to_string(i);
        ++i;
        std::string kind = smgk_kind(md);
        if(kind.empty()) kind = "cell";

        Cell& cell = cell_for(id, kind, jc.value("cell_type", "") == "markdown");
        _loaded.insert(id);
        cell.source = joined(jc.value("source", json("")));
        const json& ec = jc.value("execution_count", json());
        cell.exec_count = ec.is_number_integer() ? ec.get<int>() : -1;
        if(cell.markdown) continue;

        for(const auto& jo : jc.value("outputs", json::array())) {
            const std::string type = jo.value("output_type", "");
            if(type == "stream") {
                add_stream(cell.outputs, jo.value("name", "stdout"), joined(jo.value("text", json(""))));
            } else if(type == "error") {
                Output o;
                o.kind = Output::Kind::Error;
                o.text = traceback_text(jo.value("traceback", json::array()));
                cell.outputs.push_back(std::move(o));
            } else if(type == "display_data" || type == "execute_result") {
                cell.outputs.push_back(data_output(jo.value("data", json::object())));
            }
        }
    }
}

void Feed::reset() {
    _cells.clear();
    _index.clear();
    _loaded.clear();
    _status = "unknown";
}

} // namespace smgk
