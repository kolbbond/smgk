#pragma once
// ImGui rendering of a Feed: code, text, errors, and plots as textures (created lazily when on screen)

#include <map>
#include <string>
#include <utility>

#include <Magnum/GL/Texture.h>
#include <Magnum/Math/Vector2.h>

#include "Feed.hh"

namespace smgk {

class FeedView {
  public:
    void draw(const Feed& feed);
    void clear();

  private:
    struct Texture {
        Magnum::GL::Texture2D tex;
        Magnum::Vector2i size;
    };

    void draw_cell(const Cell& cell);
    void draw_image(const std::string& key, const std::string& png_b64);
    void draw_zoom();
    Texture* texture(const std::string& key, const std::string& png_b64);

    std::map<std::string, Texture> _textures;
    std::map<std::string, std::pair<int, int>> _sizes;
    std::string _zoom_key;
    float _zoom = 1.0f;
    std::size_t _last_cells = 0;
};

} // namespace smgk
