#pragma once
// The feed: cells in arrival order, outputs grouped under the request that produced them.
// Mirrors python/smgk/recorder.py so the live view matches the saved notebook.

#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "Wire.hh"

namespace smgk {

struct Output {
    enum class Kind { Stream, Text, Image, Error };
    Kind kind = Kind::Text;
    std::string text;
    std::string png_b64;
    std::string name; // stream name
};

struct Cell {
    std::string msg_id, kind; // kind: cell | run | push | note
    int exec_count = -1;
    std::string source;
    bool markdown = false;
    std::vector<Output> outputs;
};

class Feed {
  public:
    // returns true when the feed changed
    bool apply(const Msg& m);
    // history from the recorder's .ipynb; live messages for these cells are then ignored,
    // since the recorder only writes at idle, after all of a cell's output arrived
    void load_notebook(const nlohmann::json& nb);
    void reset();

    [[nodiscard]] const std::vector<Cell>& cells() const { return _cells; }
    [[nodiscard]] const std::string& status() const { return _status; }

  private:
    Cell& cell_for(const std::string& parent, const std::string& kind, bool markdown);

    std::vector<Cell> _cells;
    std::unordered_map<std::string, std::size_t> _index;
    std::set<std::string> _loaded;
    std::string _status = "unknown";
};

} // namespace smgk
