#pragma once
// SUB socket on a kernel's IOPub channel, received on a background thread

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

#include "Session.hh"
#include "Wire.hh"

namespace smgk {

class IopubClient {
  public:
    explicit IopubClient(const ConnInfo& conn);
    ~IopubClient();
    IopubClient(const IopubClient&) = delete;
    IopubClient& operator=(const IopubClient&) = delete;

    // messages received since the last call, in arrival order; unparseable ones are dropped
    std::vector<Msg> drain();

  private:
    void run(std::string endpoint);

    std::atomic<bool> _stop{false};
    std::mutex _mutex;
    std::vector<Msg> _inbox;
    std::thread _thread;
};

} // namespace smgk
