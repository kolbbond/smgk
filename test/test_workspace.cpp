#include "Workspace.hh"
#include "test_util.hh"

using json = nlohmann::json;
using smgk::Var;
using smgk::VarColumn;

static std::vector<std::string> names(const std::vector<Var>& vs) {
    std::vector<std::string> out;
    for(const auto& v : vs)
        out.push_back(v.name);
    return out;
}

int main() {
    json j = {{"generation", 2},
              {"vars",
               {{{"name", "b"}, {"type", "ndarray"}, {"size", "(10, 3) float64"}, {"value", "min 0 … max 1"}},
                {{"name", "a"}, {"type", "int"}, {"size", ""}, {"value", "5"}},
                "not a row",
                {{"name", "Df"}, {"type", "DataFrame"}, {"size", "500×2"}, {"value", ""}}}}};
    auto ws = smgk::parse_workspace(j);
    CHECK(ws && ws->generation == 2 && ws->vars.size() == 3);
    CHECK(ws && ws->vars[0].size == "(10, 3) float64" && ws->vars[1].value == "5");

    CHECK(!smgk::parse_workspace(json::parse("[1, 2]")).has_value());
    CHECK(!smgk::parse_workspace(json::object()).has_value());
    CHECK(!smgk::parse_workspace(json(json::value_t::discarded)).has_value());

    auto vs = ws->vars;
    smgk::sort_vars(vs, VarColumn::Name, true);
    CHECK((names(vs) == std::vector<std::string>{"a", "b", "Df"})); // case-insensitive
    smgk::sort_vars(vs, VarColumn::Name, false);
    CHECK((names(vs) == std::vector<std::string>{"Df", "b", "a"}));
    smgk::sort_vars(vs, VarColumn::Type, true);
    CHECK((names(vs) == std::vector<std::string>{"Df", "a", "b"}));

    CHECK(smgk::var_matches(ws->vars[0], ""));
    CHECK(smgk::var_matches(ws->vars[0], "B"));
    CHECK(smgk::var_matches(ws->vars[0], "ndarr"));
    CHECK(!smgk::var_matches(ws->vars[0], "frame"));
    CHECK(smgk::var_matches(ws->vars[2], "frame"));
    TEST_RETURN();
}
