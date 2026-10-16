#include "fixture.hh"
#include "test_util.hh"

int main() {
    auto frames = fixture_frames("wire_execute_input.json");
    auto m = smgk::parse_wire(frames);
    CHECK(m.has_value());
    if(m) {
        CHECK(m->msg_type == "execute_input");
        CHECK(m->content["code"] == "print(\"hi\")");
        CHECK(!m->msg_id.empty());
        CHECK(!m->parent_msg_id.empty());
        CHECK(m->username == "kolbbond"); // jupyter_client defaults to the OS user; fixture captured as kolbbond
    }

    auto no_delim = frames;
    for(auto& f : no_delim)
        if(f == "<IDS|MSG>") f = "nope";
    CHECK(!smgk::parse_wire(no_delim).has_value());

    auto bad = frames;
    bad.back() = "{not json";
    CHECK(!smgk::parse_wire(bad).has_value());

    CHECK(!smgk::parse_wire({"topic", "<IDS|MSG>", "sig"}).has_value());
    TEST_RETURN();
}
