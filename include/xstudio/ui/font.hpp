// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/utility/uuid.hpp"
#include <set>
#include <Imath/ImathVec.h>

namespace xstudio {
namespace ui {

    enum Justification { JustifyLeft, JustifyRight, JustifyCentre };

    class Fonts {

      public:
        /**
         *  @brief Returns a map of font names and paths to the source font
         *  files that are available as part of the xSTUDIO build
         */
        static std::map<std::string, std::string> available_fonts();
    };
    /**
     *  @brief Font classes.
     *
     *  @details
     *   Thes classes provide a simple interface for loading common font definition
     *   files such as .ttf files. The font glyphs are made accessible for efficient
     *   drawing to screen using one of 3 methods - alpha bitmap, SDF (signed
     *   distance function) bitmap or Vector. In each case the glyphs are packed
     *   into a single 'atlas' ... either a texture image for the bitmap methods or
     *   a vector of vertices and vector of triangle indeces for vector method.
     *   An 'address' into the atlas is provided for each character in the set to
     *   allow the appropriate texture lookup offsets, or element array offset, to
     *   be applied.
     *
     *   Currently implementation is achieved using freetype library
     */

    class AlphaBitmapFont {

      public:
        /**
         *  @brief Construct the font atlas from path to any font file readable
         *  by the freetype library.
         */
        AlphaBitmapFont(const std::string &font_path, const int glyph_pixel_size);


        AlphaBitmapFont(const int glyph_pixel_size) : glyph_pixel_size_(glyph_pixel_size) {}

        ~AlphaBitmapFont() = default;

        /**
         * @brief Render text into some graphics surce - this depends on graphics
         * api specific implementations of virtual render_text method
         */
        void render_text2(
            const std::string &text,
            const Imath::M44f &transform_window_to_viewport_space,
            const Imath::M44f &transform_viewport_to_image_space,
            const Imath::V2f position,
            const float wrap_width,
            const float text_size,
            const utility::ColourTriplet &colour,
            const Justification &just,
            const float line_spacing,
            const float viewport_du_dx) const;

        /**
         * @brief For text that is static, use this function once only to compute
         * the vertex data required to render given text to screen and store the
         * vertices. Use result with the render_text method.
         *
         * returns: bounding box
         */
        [[nodiscard]] Imath::Box2f precompute_text_rendering_vertex_layout(
            std::vector<float> &result,
            const std::string &text,
            const Imath::V2f position,
            const float wrap_width,
            const float text_size,
            const Justification &just,
            const float line_spacing) const;

        /**
         * @brief Given a position in the viewport, does it correspond to a character
         * in some text
         */
        [[nodiscard]] std::string::const_iterator viewport_position_to_cursor(
            const Imath::V2f viewport_position,
            const std::string &text,
            const Imath::V2f position,
            const float wrap_width,
            const float text_size,
            const Justification &just,
            const float line_spacing) const;

        /**
         * @brief For given cursor position in 'text', move it up or down in the
         * wrapped text box.
         */
        [[nodiscard]] std::string::const_iterator cursor_up_or_down(
            const std::string::const_iterator current_cursor_pos,
            const bool cursor_up,
            const std::string &text,
            const float wrap_width,
            const float text_size,
            const Justification &just,
            const float line_spacing) const;

        /**
         * @brief For given string and 'cursor' as an interator within the string,
         * compute the screen position of the corresponding character right bounds.
         */

        [[nodiscard]] Imath::V2f get_cursor_screen_position(
            const std::string &text,
            const Imath::V2f position,
            const float wrap_width,
            const float text_size,
            const Justification &just,
            const float line_spacing,
            const std::string::const_iterator cursor) const;


        /**
         * @brief Using pre-computed vertex buffer data, render the text into
         * given graphics surface. Graphics API specific subclasses of this class
         * (e.g. OpenGL, Metal) will re-implement this function to actually do
         * the rendering
         */
        virtual void render_text(
            const std::vector<float> &precomputed_vertex_buffer,
            const Imath::M44f &transform_window_to_viewport_space,
            const Imath::M44f &transform_viewport_to_image_space,
            const utility::ColourTriplet &colour,
            const float viewport_du_dx,
            const float text_size,
            const float opacity = 0.0f) const {}


        /**
         *  @brief Returns the underlying base size of the glyph bitmap in pixels.
         *  For a character whose height is 1.0, say, then the height of its bitmap
         *  will equal the glyph_pixel_size_
         */
        [[nodiscard]] int glyph_pixel_size() const { return glyph_pixel_size_; }

      protected:
        /**
         *  @brief Returns the width of the font atlas in texels.
         */
        [[nodiscard]] int atlas_width() const { return atlas_width_; }

        /**
         *  @brief Returns the height of the font atlas in texels.
         */
        [[nodiscard]] int atlas_height() const { return atlas_height_; }

        /**
         *  @brief Returns the texture map texel data representing the entire atlas
         *  map of glyphs laid out in atlas_height rows of width atlas_width as
         *  unsigned 8 bit data, giving the opacity of the glyphs as a monochrome
         *  image.
         */
        [[nodiscard]] const std::vector<uint8_t> &atlas() const { return atlas_; }

        virtual void generate_atlas(const std::string &font_path, const int glyph_pixel_size);

        virtual void compute_wrap_indeces(
            std::vector<std::string::const_iterator> &wrap_points,
            const std::string &text,
            const float box_width,
            const float scale) const;

        const int glyph_pixel_size_;

        struct CharacterMetrics {

            float hori_bearing_x_; // Offset from baseline to left/top of glyph
            float hori_bearing_y_; // Offset from baseline to left/top of glyph
            float hori_advance_;   // Offset from baseline to left/top of glyph
            int width_, height_;
            int atlas_offset_x_, atlas_offset_y_;
        };

        /**
         *  @brief Returns the CharacterMetrics structure for the given character.
         *  Use the offset values to address (texture lookup offset) into the atlas
         *  texture map. The Bearing and Advance values are used for moving the
         *  draw locus for the next character in a string of text.
         */
        [[nodiscard]] const CharacterMetrics &get_character(char c) const;

        std::map<char, CharacterMetrics> character_metrics_;
        std::vector<uint8_t> atlas_;
        int atlas_width_;
        int atlas_height_;
    };

    class VectorFont {

      public:
        /**
         *  @brief Construct the font atlas from path to any font file readable
         *  by the freetype library.
         */
        VectorFont(const std::string &font_path, const float glyph_pixel_size);

        ~VectorFont() = default;

        /**
         *  @brief Returns the vector of all of the outline vertices describing the
         *  entire set of glyphs for the font. This can be used to statically upload
         *  the vertices to the GPU using a buffer object.
         */
        [[nodiscard]] const std::vector<Imath::V2f> &outline_vertices() const {
            return outline_points_;
        }

        /**
         *  @brief Returns the vector of all of the triangle vertex indeces needed
         *  to plot all of the glyphs in the font. This can be used to statically
         *  upload the triangle vertex indeces to the GPU using a buffer object.
         */
        [[nodiscard]] const std::vector<unsigned int> triangle_indeces() const {
            return triangle_indeces_;
        }

        struct ShapeIndexDetails {
            size_t first_triangle_index_;
            size_t num_triangles_;
            size_t first_outline_point_index_;
            size_t num_outline_points_;
        };

        struct CharacterMetrics {

            CharacterMetrics()                          = default;
            CharacterMetrics(const CharacterMetrics &o) = default;

            float hori_bearing_x_ = {0}; // Offset from baseline to left/top of glyph
            float hori_bearing_y_ = {0}; // Offset from baseline to left/top of glyph
            float hori_advance_   = {0}; // Offset from baseline to left/top of glyph
            float width_          = {0};
            float height_         = {0};

            /* A glyph is made up of one or more filled 'shapes'. Each shape has been
            tesselated into triangles. The offset into the monolithic
            triangle_indeces() map for the start of the shape's triangles and the
            number of triangles in the stream thereafter is provided here. */
            std::vector<ShapeIndexDetails> positive_shapes_;

            /* As above, except these shapes cut out (i.e. they are negative), such
            as the hole in the middle of the letter 'o' for example. */
            std::vector<ShapeIndexDetails> negative_shapes_;
        };

        /**
         *  @brief Returns the CharacterMetrics structure for the given character.
         */
        [[nodiscard]] const CharacterMetrics &get_character(char c) const;

        /**
         *  For building glyph shape data
         */
        void move_to(const Imath::V2f &);
        void line_to(const Imath::V2f &);
        void conic_to(const Imath::V2f &, const Imath::V2f &);
        void cubic_to(const Imath::V2f &, const Imath::V2f &, const Imath::V2f &);

      private:
        void glyph_shape_decomposition_complete();

        std::map<char, CharacterMetrics> character_metrics_;
        CharacterMetrics character_under_construction_;
        std::vector<Imath::V2f> outline_points_;
        std::vector<unsigned int> triangle_indeces_;
        size_t last_tesselated_point_index_ = {0};
    };

    class SDFBitmapFont : public AlphaBitmapFont {

      public:
        /**
         *  @brief Construct the font atlas from path to any font file readable
         *  by the freetype library. The atlas is a float32 map, each pixel being
         *  a signed-distance-function relating to the given glyph boundary contour(s)
         */
        SDFBitmapFont(const std::string &font_path, const int glyph_pixel_size)
            : AlphaBitmapFont(glyph_pixel_size) {
            generate_atlas(font_path, glyph_pixel_size);
        }

        ~SDFBitmapFont() = default;

        static std::map<std::string, std::shared_ptr<SDFBitmapFont>> available_fonts();

        static std::shared_ptr<SDFBitmapFont> font_by_name(const std::string &name);

      protected:
        void generate_atlas(const std::string &font_path, const int glyph_pixel_size) override;
    };


} // namespace ui
} // namespace xstudio
