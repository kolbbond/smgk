#include "Png.hh"

#include <string>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include <stb_image.h>

namespace smgk {

namespace {

std::optional<std::string> base64_decode(std::string_view in) {
    auto val = [](char c) -> int {
        if(c >= 'A' && c <= 'Z') return c - 'A';
        if(c >= 'a' && c <= 'z') return c - 'a' + 26;
        if(c >= '0' && c <= '9') return c - '0' + 52;
        if(c == '+') return 62;
        if(c == '/') return 63;
        return -1;
    };
    std::string out;
    out.reserve(in.size() * 3 / 4);
    unsigned buf = 0;
    int bits = 0;
    for(char c : in) {
        if(c == '\n' || c == '\r' || c == ' ' || c == '\t') continue;
        if(c == '=') break;
        int v = val(c);
        if(v < 0) return std::nullopt;
        buf = (buf << 6) | static_cast<unsigned>(v);
        bits += 6;
        if(bits >= 8) {
            bits -= 8;
            out += static_cast<char>((buf >> bits) & 0xff);
        }
    }
    return out;
}

const stbi_uc* bytes(const std::string& s) { return reinterpret_cast<const stbi_uc*>(s.data()); }

} // namespace

std::optional<Rgba> decode_png_b64(std::string_view b64) {
    auto raw = base64_decode(b64);
    if(!raw || raw->empty()) return std::nullopt;
    int w = 0, h = 0, n = 0;
    stbi_uc* px = stbi_load_from_memory(bytes(*raw), static_cast<int>(raw->size()), &w, &h, &n, 4);
    if(!px) return std::nullopt;
    Rgba img{w, h, std::vector<std::uint8_t>(px, px + static_cast<std::size_t>(w) * h * 4)};
    stbi_image_free(px);
    return img;
}

std::optional<std::pair<int, int>> png_size_b64(std::string_view b64) {
    // read IHDR directly: stbi_info scans on to the first IDAT, which matplotlib's tEXt chunks
    // push past any fixed prefix. Signature (8) + length (4) + "IHDR" (4) + width, height (4 each).
    auto raw = base64_decode(b64.substr(0, 44));
    if(!raw || raw->size() < 24 || raw->compare(0, 8, "\x89PNG\r\n\x1a\n") != 0 || raw->compare(12, 4, "IHDR") != 0) return std::nullopt;
    auto be32 = [&](std::size_t at) {
        unsigned v = 0;
        for(std::size_t i = 0; i < 4; ++i)
            v = (v << 8) | static_cast<unsigned char>((*raw)[at + i]);
        return static_cast<int>(v);
    };
    int w = be32(16), h = be32(20);
    if(w <= 0 || h <= 0) return std::nullopt;
    return std::make_pair(w, h);
}

} // namespace smgk
