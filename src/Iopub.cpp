#include "Iopub.hh"

#include <zmq.hpp>

namespace smgk {

IopubClient::IopubClient(const ConnInfo& conn) {
    _thread = std::thread(&IopubClient::run, this, conn.transport + "://" + conn.ip + ":" + std::to_string(conn.iopub_port));
}

IopubClient::~IopubClient() {
    _stop = true;
    _thread.join();
}

std::vector<Msg> IopubClient::drain() {
    std::lock_guard<std::mutex> lock(_mutex);
    std::vector<Msg> out;
    out.swap(_inbox);
    return out;
}

void IopubClient::run(std::string endpoint) {
    zmq::context_t ctx;
    zmq::socket_t sub(ctx, zmq::socket_type::sub);
    sub.set(zmq::sockopt::linger, 0);
    sub.set(zmq::sockopt::subscribe, "");
    sub.connect(endpoint);

    zmq::pollitem_t item{sub.handle(), 0, ZMQ_POLLIN, 0};
    while(!_stop) {
        // bounded poll so the destructor's join returns promptly
        if(zmq::poll(&item, 1, std::chrono::milliseconds(100)) <= 0) continue;
        std::vector<std::string> frames;
        zmq::message_t part;
        do {
            if(!sub.recv(part, zmq::recv_flags::none)) break;
            frames.emplace_back(part.to_string());
        } while(part.more());
        if(auto m = parse_wire(frames)) {
            std::lock_guard<std::mutex> lock(_mutex);
            _inbox.push_back(std::move(*m));
        }
    }
}

} // namespace smgk
