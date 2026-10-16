#pragma once
// MATLAB-style Workspace panel: sortable, filterable table of the kernel's variables

#include <vector>

#include "Workspace.hh"

namespace smgk {

class WorkspaceView {
  public:
    void set(std::vector<Var> vars);
    void clear() { set({}); }
    void draw();
    [[nodiscard]] std::size_t size() const { return _vars.size(); }

  private:
    std::vector<Var> _vars;
    char _filter[128] = {};
    bool _resort = true;
};

} // namespace smgk
