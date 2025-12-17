// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <Imath/ImathVec.h>

#include "xstudio/utility/json_store.hpp"
#include "xstudio/ui/font.hpp"


namespace xstudio {
namespace ui {
    namespace canvas {

        struct Caption {

            // JSON serialisation requires default constructible types
            Caption() : id_(utility::Uuid::generate()) {}

            Caption(
                const Imath::V2f position,
                const float wrap_width,
                const float font_size,
                const utility::ColourTriplet colour,
                const float opacity,
                const Justification justification,
                const std::string font_name,
                const utility::ColourTriplet background_colour,
                const float background_opacity);

            Caption(const Caption &o) :
                text_(o.text_), 
                position_(o.position_), 
                wrap_width_(o.wrap_width_), 
                font_size_(o.font_size_), 
                font_name_(o.font_name_), 
                colour_(o.colour_), 
                opacity_(o.opacity_), 
                justification_(o.justification_), 
                background_colour_(o.background_colour_), 
                background_opacity_(o.background_opacity_), 
                bounding_box_(o.bounding_box_), 
                vertices_(o.vertices_),
                hash_(o.hash_),
                id_(o.id_)
                {
                    cursor_position_ = text_.begin() + std::distance(o.text_.begin(), o.cursor_position_);
                }
             
            bool operator==(const Caption &o) const;

            void modify_text(const std::string &t);

            void set_cursor_position(const Imath::V2f &screen_pos);

            void set_position(const Imath::V2f &image_pos) { 
                position_ = image_pos; 
                update_vertices();
            }

            void set_wrap_width(const float w) { 
                wrap_width_ = w; 
                update_vertices();
            }

            void set_font_size(const float sz) { 
                font_size_ = sz; 
                update_vertices();
            }

            void set_opacity(const float o) { 
                opacity_ = o; 
            }

            void set_bg_opacity(const float o) { 
                background_opacity_ = o; 
            }

            void set_colour(const utility::ColourTriplet &c) { 
                colour_ = c; 
            }

            void set_bg_colour(const utility::ColourTriplet &c) { 
                background_colour_ = c; 
            }

            void set_font_name(const std::string &nm) { 
                font_name_ = nm; 
                update_vertices();
            }

            void move_cursor(int key);

            std::array<Imath::V2f, 2> cursor_position_on_image() const;

            Imath::Box2f & bounding_box() const { return bounding_box_; }

            std::vector<float> & vertices() const {
                return vertices_;
            }

            std::size_t hash() const { return hash_; }

            const std::string & text() const { return text_; }
            const std::string & font_name() const { return font_name_; }
            const utility::ColourTriplet & colour() const { return colour_; }
            const utility::ColourTriplet & background_colour() const { return background_colour_; }
            const utility::Uuid & id() const { return id_; }
            
            [[nodiscard]] float opacity() const { return opacity_; }
            [[nodiscard]] float background_opacity() const { return background_opacity_; }
            [[nodiscard]] float wrap_width() const { return wrap_width_; }
            [[nodiscard]] float font_size() const { return font_size_; }
            [[nodiscard]] Imath::V2f position() const { return position_; }
            
          private:

            utility::Uuid id_;
            std::string text_;
            Imath::V2f position_;
            float wrap_width_;
            float font_size_;
            std::string font_name_;
            utility::ColourTriplet colour_{utility::ColourTriplet(1.0f, 1.0f, 1.0f)};
            float opacity_;
            Justification justification_;
            utility::ColourTriplet background_colour_ = {
                utility::ColourTriplet(0.0f, 0.0f, 0.0f)};
            float background_opacity_;

            void update_hash();
            void update_vertices();

            mutable std::size_t hash_;
            mutable Imath::Box2f bounding_box_;
            mutable std::vector<float> vertices_;
            std::string::const_iterator cursor_position_;

            friend void from_json(const nlohmann::json &j, Caption &c);
            friend void to_json(nlohmann::json &j, const Caption &c);

        };

        void from_json(const nlohmann::json &j, Caption &c);
        void to_json(nlohmann::json &j, const Caption &c);

    } // end namespace canvas
} // end namespace ui
} // end namespace xstudio
