#include "FeedView.hh"

#include <algorithm>
#include <cstring>

#include <Magnum/GL/TextureFormat.h>
#include <Magnum/ImGuiIntegration/Widgets.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <imgui.h>

#include "Png.hh"

namespace smgk {

namespace {
const ImVec4 Dim{0.55f, 0.55f, 0.6f, 1.0f};
const ImVec4 Code{0.85f, 0.85f, 0.75f, 1.0f};
const ImVec4 Err{0.95f, 0.45f, 0.4f, 1.0f};
const ImVec4 Warn{0.95f, 0.7f, 0.35f, 1.0f};
const ImVec4 Note{0.55f, 0.8f, 0.95f, 1.0f};

std::string header(const Cell& c) {
    std::string n = c.exec_count >= 0 ? std::to_string(c.exec_count) : " ";
    if(c.kind == "note") return "note";
    if(c.kind == "push") return "push";
    if(c.kind == "run") return "run [" + n + "]";
    return "In [" + n + "]";
}
} // namespace

void FeedView::clear() {
    _textures.clear();
    _sizes.clear();
    _zoom_key.clear();
    _last_cells = 0;
}

FeedView::Texture* FeedView::texture(const std::string& key, const std::string& png_b64) {
    auto it = _textures.find(key);
    if(it != _textures.end()) return &it->second;
    auto img = decode_png_b64(png_b64);
    if(!img) return nullptr;

    // Magnum's image origin is bottom-left; PNG rows run top-down
    const std::size_t row = static_cast<std::size_t>(img->w) * 4;
    for(int y = 0; y < img->h / 2; ++y)
        std::swap_ranges(img->px.begin() + y * row, img->px.begin() + (y + 1) * row, img->px.end() - (y + 1) * row);

    Texture t;
    t.size = {img->w, img->h};
    t.tex.setWrapping(Magnum::GL::SamplerWrapping::ClampToEdge)
        .setMinificationFilter(Magnum::GL::SamplerFilter::Linear)
        .setMagnificationFilter(Magnum::GL::SamplerFilter::Linear)
        .setStorage(1, Magnum::GL::TextureFormat::RGBA8, t.size)
        .setSubImage(0, {}, Magnum::ImageView2D{Magnum::PixelFormat::RGBA8Unorm, t.size, {img->px.data(), img->px.size()}});
    return &_textures.emplace(key, std::move(t)).first->second;
}

void FeedView::draw_image(const std::string& key, const std::string& png_b64) {
    auto sz = _sizes.find(key);
    if(sz == _sizes.end()) sz = _sizes.emplace(key, png_size_b64(png_b64).value_or(std::make_pair(0, 0))).first;
    auto [w, h] = sz->second;
    if(w <= 0 || h <= 0) {
        ImGui::TextColored(Err, "[unreadable image]");
        return;
    }
    const float avail = ImGui::GetContentRegionAvail().x;
    const float scale = std::min(1.0f, avail / static_cast<float>(w));
    const ImVec2 size{static_cast<float>(w) * scale, static_cast<float>(h) * scale};

    // only decode and upload what is actually scrolled into view
    if(!ImGui::IsRectVisible(size)) {
        ImGui::Dummy(size);
        return;
    }
    Texture* t = texture(key, png_b64);
    if(!t) {
        ImGui::TextColored(Err, "[unreadable image]");
        return;
    }
    Magnum::ImGuiIntegration::image(t->tex, {size.x, size.y});
    if(ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    if(ImGui::IsItemClicked()) {
        _zoom_key = key;
        _zoom = 1.0f;
    }
}

void FeedView::draw_cell(const Cell& c) {
    ImGui::PushID(c.msg_id.c_str());
    ImGui::TextColored(c.kind == "note" ? Note : Dim, "%s", header(c).c_str());
    ImGui::Indent();
    if(c.markdown) {
        ImGui::PushStyleColor(ImGuiCol_Text, Note);
        ImGui::TextWrapped("%s", c.source.c_str());
        ImGui::PopStyleColor();
    } else if(!c.source.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, Code);
        ImGui::TextUnformatted(c.source.c_str());
        ImGui::PopStyleColor();
    }
    for(std::size_t i = 0; i < c.outputs.size(); ++i) {
        const Output& o = c.outputs[i];
        switch(o.kind) {
        case Output::Kind::Image:
            draw_image(c.msg_id + "#" + std::to_string(i), o.png_b64);
            break;
        case Output::Kind::Error:
            ImGui::TextColored(Err, "%s", o.text.c_str());
            break;
        case Output::Kind::Stream:
            if(o.name == "stderr") ImGui::TextColored(Warn, "%s", o.text.c_str());
            else ImGui::TextUnformatted(o.text.c_str());
            break;
        case Output::Kind::Text:
            ImGui::TextUnformatted(o.text.c_str());
            break;
        }
    }
    ImGui::Unindent();
    ImGui::Separator();
    ImGui::PopID();
}

void FeedView::draw_zoom() {
    if(_zoom_key.empty()) return;
    auto it = _textures.find(_zoom_key);
    if(it == _textures.end()) {
        _zoom_key.clear();
        return;
    }
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({vp->WorkPos.x + vp->WorkSize.x * 0.05f, vp->WorkPos.y + vp->WorkSize.y * 0.05f}, ImGuiCond_Appearing);
    ImGui::SetNextWindowSize({vp->WorkSize.x * 0.9f, vp->WorkSize.y * 0.9f}, ImGuiCond_Appearing);
    bool open = true;
    ImGui::Begin("image (wheel: zoom, Esc: close)", &open,
                 ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings);
    if(ImGui::IsWindowHovered()) _zoom = std::clamp(_zoom * (1.0f + 0.1f * ImGui::GetIO().MouseWheel), 0.1f, 20.0f);
    if(ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Escape)) open = false;
    const Magnum::Vector2 size = Magnum::Vector2{it->second.size} * _zoom;
    Magnum::ImGuiIntegration::image(it->second.tex, size);
    ImGui::End();
    if(!open) _zoom_key.clear();
}

void FeedView::draw(const Feed& feed) {
    ImGui::BeginChild("##feed", {0, 0}, ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
    const bool at_bottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f;
    for(const Cell& c : feed.cells())
        draw_cell(c);
    // follow new output only when the user hasn't scrolled up to read something
    if(feed.cells().size() != _last_cells && at_bottom) ImGui::SetScrollHereY(1.0f);
    _last_cells = feed.cells().size();
    ImGui::EndChild();
    draw_zoom();
}

} // namespace smgk
