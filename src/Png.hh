#pragma once
// PNG payloads from Jupyter display_data ("image/png" is base64, possibly line-wrapped)

#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace smgk {

struct Rgba {
    int w = 0, h = 0;
    std::vector<std::uint8_t> px; // w*h*4, rows top to bottom
};

std::optional<Rgba> decode_png_b64(std::string_view b64);
// dimensions from the header only, for layout before a texture exists
std::optional<std::pair<int, int>> png_size_b64(std::string_view b64);

} // namespace smgk
