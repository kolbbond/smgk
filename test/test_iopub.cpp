#include <chrono>
#include <optional>
#include <thread>

#include <zmq.hpp>

#include "Iopub.hh"
#include "fixture.hh"
#include "test_util.hh"

using namespace std::chrono;

int main() {
    zmq::context_t ctx;
    zmq::socket_t pub(ctx, zmq::socket_type::pub);
    pub.bind("tcp://127.0.0.1:*");
    std::string endpoint = pub.get(zmq::sockopt::last_endpoint);
    int port = std::stoi(endpoint.substr(endpoint.rfind(':') + 1));

    auto frames = fixture_frames("wire_execute_input.json");
    std::optional<smgk::IopubClient> client;
    client.emplace(smgk::ConnInfo{"tcp", "127.0.0.1", "", port});
    {
        std::this_thread::sleep_for(milliseconds(200)); // PUB/SUB slow joiner

        for(int i = 0; i < 3; ++i) {
            auto f = frames;
            nlohmann::json content = {{"code", "cell " + std::to_string(i)}, {"execution_count", i}};
            f.back() = content.dump();
            for(std::size_t k = 0; k < f.size(); ++k)
                pub.send(zmq::buffer(f[k]), k + 1 < f.size() ? zmq::send_flags::sndmore : zmq::send_flags::none);
        }
        pub.send(zmq::str_buffer("garbage"), zmq::send_flags::none);

        std::vector<smgk::Msg> got;
        auto deadline = steady_clock::now() + seconds(2);
        while(got.size() < 3 && steady_clock::now() < deadline) {
            for(auto& m : client->drain())
                got.push_back(std::move(m));
            std::this_thread::sleep_for(milliseconds(10));
        }
        CHECK(got.size() == 3);
        for(std::size_t i = 0; i < got.size(); ++i)
            CHECK(got[i].content["code"] == "cell " + std::to_string(i));
    }
    auto t0 = steady_clock::now();
    client.reset();
    CHECK(steady_clock::now() - t0 < milliseconds(500));
    TEST_RETURN();
}
