#include "Png.hh"
#include "fixture.hh"
#include "test_util.hh"

static std::string b64(const std::string& bytes) {
    static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for(std::size_t i = 0; i < bytes.size(); i += 3) {
        unsigned n = static_cast<unsigned char>(bytes[i]) << 16;
        if(i + 1 < bytes.size()) n |= static_cast<unsigned char>(bytes[i + 1]) << 8;
        if(i + 2 < bytes.size()) n |= static_cast<unsigned char>(bytes[i + 2]);
        out += tbl[(n >> 18) & 63];
        out += tbl[(n >> 12) & 63];
        out += i + 1 < bytes.size() ? tbl[(n >> 6) & 63] : '=';
        out += i + 2 < bytes.size() ? tbl[n & 63] : '=';
    }
    return out;
}

int main() {
    const std::string png = b64(read_fixture("red4.png"));

    auto img = smgk::decode_png_b64(png);
    CHECK(img && img->w == 4 && img->h == 4 && img->px.size() == 4u * 4u * 4u);
    CHECK(img && img->px[0] == 255 && img->px[1] == 0 && img->px[2] == 0 && img->px[3] == 255);

    std::string wrapped;
    for(std::size_t i = 0; i < png.size(); ++i) {
        wrapped += png[i];
        if(i % 8 == 7) wrapped += '\n';
    }
    auto img2 = smgk::decode_png_b64(wrapped);
    CHECK(img2 && img2->w == 4 && img2->px == img->px);

    auto size = smgk::png_size_b64(png);
    CHECK(size && size->first == 4 && size->second == 4);

    // real matplotlib output: IHDR followed by tEXt metadata chunks
    const std::string plot = b64(read_fixture("plot.png"));
    auto psize = smgk::png_size_b64(plot);
    CHECK(psize && psize->first == 386 && psize->second == 264);
    auto pimg = smgk::decode_png_b64(plot);
    CHECK(pimg && pimg->w == 386 && pimg->h == 264);

    CHECK(!smgk::decode_png_b64("not base64 at all !!").has_value());
    CHECK(!smgk::decode_png_b64(b64("not a png")).has_value());
    CHECK(!smgk::decode_png_b64("").has_value());
    TEST_RETURN();
}
