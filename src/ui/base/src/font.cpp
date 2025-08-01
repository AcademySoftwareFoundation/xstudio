// SPDX-License-Identifier: Apache-2.0
#include <chrono>
#include <thread>
#include <iostream>
#include <sstream>

#include <ft2build.h>
#include <freetype/freetype.h>
#include <freetype/ftoutln.h>
#include <freetype/ftglyph.h>

#include <ImfHeader.h> // staticInitialize
#include <ImfOutputFile.h>
#include <ImfChannelList.h>

#include "xstudio/ui/font.hpp"
#include "xstudio/utility/chrono.hpp"
#include "xstudio/utility/helpers.hpp"

#define NUM_POINTS_IN_BEZIER_ARC 5
#define VECTOR_FONT_DECOMPOSE_SCALE 128

/* forward declaration of this function from tessellation_helpers.cpp */
namespace xstudio::ui::tesslator {
bool tessellate_vtx_loop_nil(
    std::vector<unsigned int> &result,
    const std::vector<Imath::V2f>::iterator shape_outline_begin,
    const std::vector<Imath::V2f>::iterator shape_outline_end,
    const int offset);
void make_glyph_sdf(
    std::vector<float> &buffer,
    const int buffer_width,
    const int buffer_height,
    const Imath::V2f bdb_bottom_left,
    const Imath::V2f bdb_top_right,
    const std::vector<Imath::V2f>::const_iterator shape_outline_begin,
    const std::vector<Imath::V2f>::const_iterator shape_outline_end);
} // namespace xstudio::ui::tesslator

using namespace xstudio::ui;

std::map<std::string, std::string> Fonts::available_fonts() {

    // TODO: grep for ttf files in the /fonts/ folder, maybe freetype can give
    // use the font 'name' from the .ttf file?
    return {
        {"Vera Mono", xstudio::utility::xstudio_root("/fonts/VeraMono.ttf")},
        {"Overpass Regular", xstudio::utility::xstudio_root("/fonts/Overpass-Regular.ttf")}};
}

AlphaBitmapFont::AlphaBitmapFont(const std::string &font_path, const int glyph_pixel_size)
    : glyph_pixel_size_(glyph_pixel_size) {
    generate_atlas(font_path, glyph_pixel_size);
}

void AlphaBitmapFont::generate_atlas(const std::string &font_path, const int glyph_pixel_size) {

    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        throw std::runtime_error("ERROR::FREETYPE: Could not init FreeType Library");
    }

    FT_Face face;
    if (FT_New_Face(ft, font_path.c_str(), 0, &face)) {
        std::stringstream err;
        err << "ERROR::FREETYPE: Could not load font \"" << font_path << "\"";
        FT_Done_FreeType(ft);
        throw std::runtime_error(err.str().c_str());
    }

    FT_Set_Pixel_Sizes(face, 0, glyph_pixel_size);

    // first we have to store the bitmaps temporarily before we splat the into
    // the atlas, as we don't know the atlas dimensions until we've got all the
    // bitmaps
    std::vector<std::vector<uint8_t>> glyph_bitmaps;

    int max_glyph_height = 0;
    int max_glyph_width  = 0;

    for (unsigned char c = 0; c < 128; c++) {
        // load character glyph
        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {

            spdlog::warn("{} Failed to load Glyph : index = {} ", __PRETTY_FUNCTION__, (int)c);
            continue;
        }

        max_glyph_height = std::max(max_glyph_height, (int)face->glyph->bitmap.rows);
        max_glyph_width  = std::max(max_glyph_width, (int)face->glyph->bitmap.width);

        // store the glyph bitmap
        glyph_bitmaps.emplace_back(std::vector<uint8_t>());
        glyph_bitmaps.back().resize(face->glyph->bitmap.width * face->glyph->bitmap.rows);
        memcpy(
            glyph_bitmaps.back().data(),
            face->glyph->bitmap.buffer,
            face->glyph->bitmap.width * face->glyph->bitmap.rows);

        character_metrics_[c].width_          = face->glyph->bitmap.width;
        character_metrics_[c].height_         = face->glyph->bitmap.rows;
        character_metrics_[c].hori_bearing_x_ = face->glyph->bitmap_left;
        character_metrics_[c].hori_bearing_y_ = face->glyph->bitmap_top;
        character_metrics_[c].hori_advance_   = face->glyph->advance.x;
    }

    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    if (character_metrics_.size() < 32) {

        std::stringstream err;
        err << "ERROR::FREETYPE: Failed to load at least 32 glyph bitmaps for font \""
            << font_path << "\"";
        throw std::runtime_error(err.str().c_str());
    }

    // Let's put (about) 16 glyphs in a row within the width of the total atlas,
    // then start a new row
    atlas_width_ = max_glyph_width * 16;

    int x          = 0;
    int y          = 0;
    int row_height = 0;

    // add small texel gap between the glyph bitmaps within the atlas, we need
    // this so that they don't bleed together when bilin interpolation happens
    // on the edge of the glyph during texture lookup at render time.
    const int gap = 2;

    for (auto &p : character_metrics_) {

        CharacterMetrics &c = p.second;
        row_height          = std::max(row_height, c.height_);
        if ((x + c.width_) > atlas_width_) {
            y += row_height + gap;
            row_height = c.height_;
            x          = 0;
        }

        c.atlas_offset_x_ = x;
        c.atlas_offset_y_ = y;
        x += c.width_ + gap;
    }

    atlas_height_ = ((y + row_height) / 16 + 1) * 16;

    atlas_.resize(atlas_width_ * atlas_height_);

    auto mpb_it = glyph_bitmaps.begin();

    for (auto &p : character_metrics_) {

        auto mpb = *mpb_it;
        mpb_it++;
        CharacterMetrics &c = p.second;

        const uint8_t *dc = mpb.data();
        uint8_t *da = atlas_.data() + atlas_width_ * c.atlas_offset_y_ + c.atlas_offset_x_;
        for (int i = 0; i < c.height_; ++i) {
            memcpy(da, dc, c.width_);
            da += atlas_width_;
            dc += c.width_;
        }
    }
}

void AlphaBitmapFont::render_text2(
    const std::string &text,
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    const Imath::V2f position,
    const float wrap_width,
    const float text_size,
    const utility::ColourTriplet &colour,
    const Justification &just,
    const float line_spacing,
    const float viewport_du_dx) const {
    std::vector<float> vertices;

    precompute_text_rendering_vertex_layout(
        vertices, text, position, wrap_width, text_size, just, line_spacing);

    render_text(
        vertices,
        transform_window_to_viewport_space,
        transform_viewport_to_image_space,
        colour,
        viewport_du_dx,
        text_size);
}


void AlphaBitmapFont::compute_wrap_indeces(
    std::vector<std::string::const_iterator> &result,
    const std::string &text,
    const float box_width,
    const float scale) const {

    result.clear();
    std::string::const_iterator c               = text.begin();
    std::string::const_iterator last_word_begin = text.begin();
    float x                                     = 0.0f;
    while (c != text.end()) {

        const CharacterMetrics &character = get_character(*c);
        float right_edge = x + character.hori_bearing_x_ * scale + character.width_ * scale;
        if (*c == ' ') {
            last_word_begin = c;
            last_word_begin++;
            if (last_word_begin == text.end())
                break;

        } else if (*c == '\n' || *c == '\r') {
            result.push_back(c);
            x               = 0;
            right_edge      = 0.0f;
            last_word_begin = text.begin();
        }

        x += character.hori_advance_ * scale; // bitshift by 6 to get value in pixels (2^6 = 64)

        if (right_edge > box_width && last_word_begin != text.begin() &&
            !(result.size() && result.back() == last_word_begin)) {
            // need to wrap ... go back to last space
            c = last_word_begin;
            result.push_back(c);
            x = get_character(*c).hori_advance_ * scale;
        }
        c++;
    }
}

std::string::const_iterator AlphaBitmapFont::viewport_position_to_cursor(
    const Imath::V2f viewport_position,
    const std::string &text,
    const Imath::V2f position,
    const float wrap_width,
    const float text_size,
    const Justification &just,
    const float line_spacing) const {

    const float scale = text_size * 2.0f / (1920.0f * glyph_pixel_size());

    float x = position.x;

    std::vector<std::string::const_iterator> wrap_points;
    compute_wrap_indeces(wrap_points, text, wrap_width, scale);

    auto wrap_point = wrap_points.begin();

    const float line_height = line_spacing * scale * glyph_pixel_size();

    // which line of text does viewport_position land in (in the Y axis)?
    const int line = floor(
        (viewport_position.y - (position.y - line_spacing * scale * glyph_pixel_size())) /
        line_height);

    if (line < 0)
        return text.end(); // viewport_position is beyond y min of the text bounding box
    if (line > (int)wrap_points.size())
        return text.end(); // viewport_position is beyond y max of the text bounding box

    // which character in the text corresponds to the start of the line that was clicked in?
    std::string::const_iterator c =
        line && wrap_points.size() ? wrap_points[line - 1] : text.begin();
    auto line_end =
        line < (int)wrap_points.size() ? wrap_points.begin() + line : wrap_points.end();

    while (c != text.end()) {

        const CharacterMetrics &character = get_character(*c);

        if (line_end != wrap_points.end() && *line_end == c) {

            // maybe skip back so we are inserting *before* the break
            // point in the line
            if (c != text.begin())
                c--;
            // if ((*c == '\n' || *c == '\r') && c != text.begin()) c--;
            break;
        }

        // skip non-printable
        if (*c < 32) {
            c++;
            continue;
        }


        x += character.hori_advance_ * scale; // bitshift by 6 to get value in pixels (2^6 = 64)

        // the next character is further right than viewport_position ... break
        if (x > viewport_position.x)
            break;

        c++;
    }

    if (c == text.end() && text.size())
        c--;

    return c;
}

std::string::const_iterator AlphaBitmapFont::cursor_up_or_down(
    const std::string::const_iterator current_cursor_pos,
    const bool cursor_up,
    const std::string &text,
    const float wrap_width,
    const float text_size,
    const Justification &just,
    const float line_spacing) const {

    // we want to find a cursor when the user has hit up or down
    // arrow key. To do this use simple click - get coordinate
    // of current cursor, and move it up or down by 1 line height
    // and then use existing function to convert mouse click to
    // a cursor

    Imath::V2f cursor_position_now = get_cursor_screen_position(
        text,
        Imath::V2f(0.0f, 0.0f),
        wrap_width,
        text_size,
        just,
        line_spacing,
        current_cursor_pos);

    float line_height = line_spacing * text_size * 2.0f / (1920.0f);

    Imath::V2f try_cursor_position = cursor_position_now;
    if (cursor_up) {
        // note cursor position is bottom of cursor, so
        // virtual 'click' is half a line above
        try_cursor_position.y -= line_height * 1.5f;
    } else {
        try_cursor_position.y += line_height * 0.5f;
    }

    return viewport_position_to_cursor(
        try_cursor_position,
        text,
        Imath::V2f(0.0f, 0.0f),
        wrap_width,
        text_size,
        just,
        line_spacing);
}

Imath::V2f AlphaBitmapFont::get_cursor_screen_position(
    const std::string &text,
    const Imath::V2f position,
    const float wrap_width,
    const float text_size,
    const Justification &just,
    const float line_spacing,
    const std::string::const_iterator cursor) const {

    // N.B. the 'text_size' defines the font size in pixels if the viewport is width
    // fitted to a 1080p display. The 2.0 comes from the fact that xstudio's coordinate
    // system spans from -1.0 to +1.0 where the left and right edges of the image land.
    const float scale = text_size * 2.0f / (1920.0f * glyph_pixel_size());

    float x = position.x;
    float y = position.y;

    std::vector<std::string::const_iterator> wrap_points;
    compute_wrap_indeces(wrap_points, text, wrap_width, scale);

    std::string::const_iterator c = text.begin();
    auto wrap_point               = wrap_points.begin();

    while (true) {

        if (wrap_point != wrap_points.end() && *wrap_point == c) {
            y += line_spacing * scale * glyph_pixel_size();
            x = position.x;
            wrap_point++;
        }

        if (c == cursor || c == text.end())
            break;

        // skip non-printable
        if (c != text.end() && *c >= 32) {
            const CharacterMetrics &character = get_character(*c);
            x += character.hori_advance_ *
                 scale; // bitshift by 6 to get value in pixels (2^6 = 64)
        }

        c++;
    }

    // need this to get the cursor looking 'right' in the overlay
    const float fudge = scale * glyph_pixel_size() * 0.05f;
    return Imath::V2f(x, y + fudge);
}


Imath::Box2f AlphaBitmapFont::precompute_text_rendering_vertex_layout(
    std::vector<float> &result,
    const std::string &text,
    const Imath::V2f position,
    const float wrap_width,
    const float text_size,
    const Justification &just,
    const float line_spacing) const {

    // N.B. the 'text_size' defines the font size in pixels if the viewport is width
    // fitted to a 1080p display. The 2.0 comes from the fact that xstudio's coordinate
    // system spans from -1.0 to +1.0 where the left and right edges of the image land.
    const float scale = text_size * 2.0f / (1920.0f * glyph_pixel_size());

    float x = position.x;
    float y = position.y;

    std::vector<std::string::const_iterator> wrap_points;
    compute_wrap_indeces(wrap_points, text, wrap_width, scale);

    std::string::const_iterator c;
    auto wrap_point = wrap_points.begin();

    result.resize(4 * 4 * text.size());
    float *_vv = result.data();

    Imath::Box2f bounding_box;
    bounding_box.min.x = position.x - 0.2f * line_spacing * scale * glyph_pixel_size();
    bounding_box.min.y = position.y - line_spacing * scale * glyph_pixel_size();

    bounding_box.max.x = bounding_box.min.x + wrap_width;

    int i = 0;
    for (c = text.begin(); c != text.end(); c++) {

        const CharacterMetrics &character = get_character(*c);

        if (wrap_point != wrap_points.end() && *wrap_point == c) {
            y += line_spacing * scale * glyph_pixel_size();
            x = position.x;
            wrap_point++;
        }

        // skip non-printable
        if (*c < 32)
            continue;

        auto atlas_offset_x_ = float(character.atlas_offset_x_);
        auto atlas_offset_y_ = float(character.atlas_offset_y_);

        auto wr = float(character.width_);
        auto hr = float(character.height_);

        float xpos = x + character.hori_bearing_x_ * scale;
        float ypos = y - character.hori_bearing_y_ * scale;

        float w = character.width_ * scale;
        float h = character.height_ * scale;
        // update VBO for each character
        float vertices[4][4] = {
            // NOLINT
            {xpos, ypos + h, atlas_offset_x_, atlas_offset_y_ + hr},
            {xpos, ypos, atlas_offset_x_, atlas_offset_y_},
            {xpos + w, ypos, atlas_offset_x_ + wr, atlas_offset_y_},
            {xpos + w, ypos + h, atlas_offset_x_ + wr, atlas_offset_y_ + hr}};
        memcpy(_vv, vertices, sizeof(vertices));
        _vv += 16;
        x += character.hori_advance_ * scale; // bitshift by 6 to get value in pixels (2^6 = 64)
    }

    if (wrap_points.size() && wrap_points.back() == text.end()) {
        // this means there's a carriage return right at the end of 'text'
        y += line_spacing * scale * glyph_pixel_size();
    }

    bounding_box.max.y = y + 0.25f * line_spacing * scale * glyph_pixel_size();
    return bounding_box;
}


const AlphaBitmapFont::CharacterMetrics &AlphaBitmapFont::get_character(char c) const {
    const auto p = character_metrics_.find(c);
    if (p == character_metrics_.end()) {
        // Note that character 'zero' is the 'no glyph' glyph
        return character_metrics_.begin()->second;
    }
    return p->second;
}

namespace {

struct Bezier2ndOrder {

    Bezier2ndOrder(const Imath::V2f &a, const Imath::V2f &b, const Imath::V2f &c)
        : a_(a), b_(b), c_(c) {}

    Imath::V2f eval(const float u) {

        const float u2 = u * u;
        return a_ * (1.0f - u) * (1.0f - u) + b_ * (1.0f - ((1.0f - u) * (1.0f - u)) - u2) +
               c_ * (u2);
    }

    const Imath::V2f a_, b_, c_;
};

struct Bezier3rdOrder {

    Bezier3rdOrder(
        const Imath::V2f &a, const Imath::V2f &b, const Imath::V2f &c, const Imath::V2f &d)
        : a_(a), b_(b), c_(c), d_(d) {}

    Imath::V2f eval(const float u) {

        const float u2 = u * u;
        const float u3 = u2 * u;

        return a_ * (1.0f - 3.0f * u + 3.0f * u2 - u3) +
               b_ * (3.0f * u - 6.0f * u2 + 3.0f * u3) + c_ * (3.0f * u2 - 3.0f * u3) +
               d_ * (u3);
    }

    const Imath::V2f a_, b_, c_, d_;
};

int my_move_to(const FT_Vector *to, void *data) {
    auto *d = (VectorFont *)data;
    d->move_to(Imath::V2f(to->x, to->y));
    return 0;
}

int my_line_to(const FT_Vector *to, void *data) {

    auto *d = (VectorFont *)data;
    d->line_to(Imath::V2f(to->x, to->y));
    return 0;
}

int my_conic_to(const FT_Vector *control, const FT_Vector *to, void *data) {

    auto *d = (VectorFont *)data;
    d->conic_to(Imath::V2f(control->x, control->y), Imath::V2f(to->x, to->y));

    return 0;
}

int my_cubic_to(
    const FT_Vector *control1, const FT_Vector *control2, const FT_Vector *to, void *data) {

    auto *d = (VectorFont *)data;
    d->cubic_to(
        Imath::V2f(control1->x, control1->y),
        Imath::V2f(control2->x, control2->y),
        Imath::V2f(to->x, to->y));
    return 0;
}
} // namespace


VectorFont::VectorFont(const std::string &font_path, const float glyph_pixel_size) {
    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        std::cout << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
        return;
    }

    FT_Face face;
    if (FT_New_Face(ft, font_path.c_str(), 0, &face)) {
        std::cout << "ERROR::FREETYPE: Failed to load font" << std::endl;
        return;
    }


    FT_Set_Pixel_Sizes(face, 0, VECTOR_FONT_DECOMPOSE_SCALE);

    FT_Outline_Funcs myfuncs;
    myfuncs.move_to  = my_move_to;
    myfuncs.line_to  = my_line_to;
    myfuncs.conic_to = my_conic_to;
    myfuncs.cubic_to = my_cubic_to;
    myfuncs.shift    = 0;
    myfuncs.delta    = 1;

    // essentially, we rescale the font to one 'unit' in size by dividing through
    // by the pixel size and the factor 64 that freetype applies to all the
    // outline coordinates
    const float rescale = 1.0f / (64.0f * VECTOR_FONT_DECOMPOSE_SCALE);

    for (unsigned char c = 0; c < 128; c++) {
        // load character glyph
        if (FT_Load_Char(face, c, FT_LOAD_NO_BITMAP)) {
            std::cout << "ERROR::FREETYTPE: Failed to load Glyph" << std::endl;
            continue;
        }

        if (face->glyph->format == FT_GLYPH_FORMAT_OUTLINE) {

            character_under_construction_ = CharacterMetrics();
            character_under_construction_.hori_bearing_x_ =
                float(face->glyph->metrics.horiBearingX) * rescale;
            character_under_construction_.hori_bearing_y_ =
                float(face->glyph->metrics.horiBearingY) * rescale;
            character_under_construction_.hori_advance_ =
                float(face->glyph->metrics.horiAdvance) * rescale;
            character_under_construction_.width_ = float(face->glyph->metrics.width) * rescale;
            character_under_construction_.height_ =
                float(face->glyph->metrics.height) * rescale;

            FT_Outline outline = face->glyph->outline;
            int r              = FT_Outline_Decompose(&outline, &myfuncs, this);

            glyph_shape_decomposition_complete();

            character_metrics_.insert(
                std::pair<char, CharacterMetrics>(c, character_under_construction_));
        }
    }

    for (auto &outline_point : outline_points_)
        outline_point = outline_point * rescale;
}

const VectorFont::CharacterMetrics &VectorFont::get_character(char c) const {
    const auto p = character_metrics_.find(c);
    if (p == character_metrics_.end()) {
        // Note that character 'zero' is the 'no glyph' glyph
        return character_metrics_.begin()->second;
    }
    return p->second;
}

void VectorFont::move_to(const Imath::V2f &v) {

    // move_to always gets called at the beggining of each shape within a
    // glyph, so we need to tesselate the last shape of the current glyph before
    // we start building a new one
    glyph_shape_decomposition_complete();

    if (!(outline_points_.size() && outline_points_.back() == v)) {
        outline_points_.push_back(v);
    }
}

void VectorFont::line_to(const Imath::V2f &v) {
    if (!(outline_points_.size() && outline_points_.back() == v)) {
        outline_points_.push_back(v);
    }
}

void VectorFont::conic_to(const Imath::V2f &c, const Imath::V2f &v) {

    Bezier2ndOrder bez(outline_points_.back(), c, v);

    // TODO: adaptive bezier curve sampling, using some accuracy metric
    for (float u = 0.1; u <= 1.0f; u += 0.1f) {
        const Imath::V2f _v = bez.eval(u);
        if (!(outline_points_.size() && outline_points_.back() == _v)) {
            outline_points_.push_back(_v);
        }
    }
}

void VectorFont::cubic_to(const Imath::V2f &c1, const Imath::V2f &c2, const Imath::V2f &v) {

    Bezier3rdOrder bez(outline_points_.back(), c1, c2, v);

    // TODO: adaptive bezier curve sampling, using some accuracy metric
    for (float u = 0.1; u <= 1.0f; u += 0.1f) {
        const Imath::V2f _v = bez.eval(u);
        if (!(outline_points_.size() && outline_points_.back() == _v)) {
            outline_points_.push_back(_v);
        }
    }
}

void VectorFont::glyph_shape_decomposition_complete() {

    // it's possible we call this from move_to at the very start of a new
    // glyph, in which case we haven't got a new shape outline to tessellate,
    // so we test for that possibility here
    if (last_tesselated_point_index_ == outline_points_.size()) {
        return;
    }

    // we need check if the start and end points of this outline loop are the same
    // vertex, and erase it if so as duplicated adjacent vertices in the loop breaks
    // the computation of the SDF
    if (*(outline_points_.begin() + last_tesselated_point_index_) == outline_points_.back()) {
        outline_points_.pop_back();
    }

    size_t i0     = triangle_indeces_.size();
    bool positive = tesslator::tessellate_vtx_loop_nil(
        triangle_indeces_,
        outline_points_.begin() + last_tesselated_point_index_,
        outline_points_.end(),
        last_tesselated_point_index_);

    ShapeIndexDetails index_details = {
        i0,
        (triangle_indeces_.size() - i0),
        last_tesselated_point_index_,
        outline_points_.size() - last_tesselated_point_index_};

    last_tesselated_point_index_ = outline_points_.size();

    if (positive) {
        character_under_construction_.positive_shapes_.push_back(index_details);
    } else {
        character_under_construction_.negative_shapes_.push_back(index_details);
    }
}

std::map<std::string, std::shared_ptr<SDFBitmapFont>> SDFBitmapFont::available_fonts() {

    static std::map<std::string, std::shared_ptr<SDFBitmapFont>> res;

    if (res.empty()) {
        for (const auto &f : Fonts::available_fonts()) {
            try {
                res[f.first] = std::make_shared<SDFBitmapFont>(f.second, 96);
            } catch (std::exception &e) {
                spdlog::warn("Failed to load font: {}.", e.what());
            }
        }
    }

    return res;
}

std::shared_ptr<SDFBitmapFont> SDFBitmapFont::font_by_name(const std::string &name) {

    for (auto &[fontName, fontPtr] : available_fonts()) {
        if (name == fontName) {
            return fontPtr;
        }
    }

    const auto &fonts = SDFBitmapFont::available_fonts();
    if (!fonts.empty()) {
        return fonts.begin()->second;
    }

    return std::shared_ptr<SDFBitmapFont>();
}

void SDFBitmapFont::generate_atlas(const std::string &font_path, const int glyph_pixel_size) {
    auto t0 = xstudio::utility::clock::now();

    // create a vector font to give us outline loops of the shapes that make
    // up each glyph
    VectorFont vector_font(font_path, glyph_pixel_size);


    // pads space around the glyph for the sdf render, otherwise when sampling
    // the texture we may hit texels from neighbouring glyphs (depending on
    // zoom factor etc)
    const float pad = 0.05f;

    // let's find out the maximum glyph width ...
    int max_glyph_width = 0;
    for (unsigned char c = 0; c < 128; c++) {
        const VectorFont::CharacterMetrics &vector_character = vector_font.get_character(c);
        const int buffer_width =
            (int)round(glyph_pixel_size * (vector_character.width_ + pad * 2.0f));
        max_glyph_width = std::max(max_glyph_width, buffer_width);
    }

    // Let's put (about) 16 glyphs in a row within the width of the total atlas,
    // then start a new row
    atlas_width_ = max_glyph_width * 16;

    // now work out the total height of the atlas as we pack the glyphs in
    // from left to right and bottom to top
    int x          = 0;
    int y          = 0;
    int row_height = 0;
    for (unsigned char c = 0; c < 128; c++) {
        const VectorFont::CharacterMetrics &vector_character = vector_font.get_character(c);

        const int buffer_width =
            (int)round(glyph_pixel_size * (vector_character.width_ + pad * 2.0f));
        const int buffer_height =
            (int)round(glyph_pixel_size * (vector_character.height_ + pad * 2.0f));

        if ((x + buffer_width) > atlas_width_) {
            x = 0;
            y += row_height;
            row_height = 0;
        }
        row_height = std::max(row_height, buffer_height);

        character_metrics_[c].width_          = buffer_width;
        character_metrics_[c].height_         = buffer_height;
        character_metrics_[c].atlas_offset_x_ = x;
        character_metrics_[c].atlas_offset_y_ = y;
        character_metrics_[c].hori_bearing_x_ =
            (vector_character.hori_bearing_x_ - pad) * glyph_pixel_size;
        character_metrics_[c].hori_bearing_y_ =
            (vector_character.hori_bearing_y_ + pad) * glyph_pixel_size;
        character_metrics_[c].hori_advance_ = vector_character.hori_advance_ * glyph_pixel_size;
        x += buffer_width;
    }

    // rounding up to 16, just in case this plays better with OpenGL texture sizes
    atlas_height_ = 16 * ((y + row_height) / 16 + 1);

    // create a buffer for float data
    atlas_.resize(atlas_width_ * atlas_height_ * 4, -glyph_pixel_size * 0.5f);

    // put the main work in a function so we can easily thread it as this is
    // a bit expensive (see notes in tesslator::make_glyph_sdf)
    auto work_func = [=](std::vector<unsigned char> &jobs,
                         std::mutex &m,
                         const VectorFont &vector_font) mutable {
        while (true) {

            m.lock();
            if (!jobs.size()) {
                m.unlock();
                break;
            }
            unsigned char c = jobs.back();
            jobs.pop_back();
            m.unlock();

            const VectorFont::CharacterMetrics &vector_character = vector_font.get_character(c);
            if (!vector_character.positive_shapes_.size()) {
                continue;
            }

            auto outline_verts_it = vector_font.outline_vertices().begin();

            const int buffer_width =
                (int)round(glyph_pixel_size * (vector_character.width_ + pad * 2.0f));
            const int buffer_height =
                (int)round(glyph_pixel_size * (vector_character.height_ + pad * 2.0f));

            // we need separate buffers for positive and negative shapes that make
            // up a glyph
            std::vector<float> buffer(buffer_width * buffer_height, 1000.0f);
            std::vector<float> neg_buffer(buffer_width * buffer_height, 1000.0f);

            // these coordinates determine the area (in glyph space) that the SDF buffers map to
            const Imath::V2f bottom_left_coord =
                Imath::V2f(
                    vector_character.hori_bearing_x_,
                    vector_character.hori_bearing_y_ - vector_character.height_) +
                Imath::V2f(-pad, -pad);

            const Imath::V2f top_right_coord =
                Imath::V2f(
                    vector_character.hori_bearing_x_ + vector_character.width_,
                    vector_character.hori_bearing_y_) +
                Imath::V2f(pad, pad);

            // compute positivev shapes SDF
            for (const auto &shape_details : vector_character.positive_shapes_) {

                auto outline_verts_begin =
                    outline_verts_it + shape_details.first_outline_point_index_;
                auto outline_verts_end =
                    outline_verts_begin + shape_details.num_outline_points_;

                tesslator::make_glyph_sdf(
                    buffer,
                    buffer_width,
                    buffer_height,
                    bottom_left_coord,
                    top_right_coord,
                    outline_verts_begin,
                    outline_verts_end);
            }

            for (const auto &shape_details : vector_character.negative_shapes_) {

                auto outline_verts_begin =
                    outline_verts_it + shape_details.first_outline_point_index_;
                auto outline_verts_end =
                    outline_verts_begin + shape_details.num_outline_points_;

                tesslator::make_glyph_sdf(
                    neg_buffer,
                    buffer_width,
                    buffer_height,
                    bottom_left_coord,
                    top_right_coord,
                    outline_verts_begin,
                    outline_verts_end);
            }

            // now copy the glyph SDF into the atlas - note this is thread
            // safe as glyphs don't overlap in the atlas whatsoever
            auto *a = (float *)atlas_.data();
            for (size_t i = 0; i < buffer.size(); ++i) {
                int x = i % buffer_width + character_metrics_[c].atlas_offset_x_;
                int y = (buffer_height - i / buffer_width - 1) +
                        character_metrics_[c].atlas_offset_y_;
                a[x + y * atlas_width_] = std::min(buffer[i], neg_buffer[i]);
            }
        }
    };

    std::vector<unsigned char> jobs;
    for (unsigned char c = 0; c < 128; c++) {
        jobs.push_back(c);
    }

    std::mutex m;
    const int nthreads = 8;
    std::vector<std::thread> ts(nthreads);
    for (int i = 0; i < nthreads; ++i) {
        ts[i] = std::thread(work_func, std::ref(jobs), std::ref(m), std::ref(vector_font));
    }

    for (int i = 0; i < nthreads; ++i) {
        ts[i].join();
    }
}