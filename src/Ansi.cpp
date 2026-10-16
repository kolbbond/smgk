#include "Ansi.hh"

namespace smgk {

std::string strip_ansi(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for(std::size_t i = 0; i < s.size(); ++i) {
        if(s[i] != '\x1b' || i + 1 >= s.size() || s[i + 1] != '[') {
            if(s[i] != '\x1b') out += s[i];
            continue;
        }
        i += 2;
        // final byte is in 0x40..0x7e; an unterminated sequence swallows the rest
        while(i < s.size() && (s[i] < 0x40 || s[i] > 0x7e))
            ++i;
    }
    return out;
}

} // namespace smgk
