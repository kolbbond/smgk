#pragma once

#include <string>
#include <string_view>

namespace smgk {

// drops CSI escape sequences (ESC '[' params final-byte), e.g. IPython's colored tracebacks
std::string strip_ansi(std::string_view s);

} // namespace smgk
