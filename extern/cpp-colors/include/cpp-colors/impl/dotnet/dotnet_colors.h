#pragma once

#include "dotnet_constants.h"
#include "../named_utils.h"


namespace colors {
  namespace dotnet {

    template<typename ColorType>
    struct basic_colors {
      typedef ColorType color_type;

      basic_colors() = delete;
      ~basic_colors() = delete;
      basic_colors(const basic_colors&) = delete;
      basic_colors& operator =(const basic_colors&) = delete;

      static color_type alice_blue() {
        return color_type(value_of(known_color::alice_blue));
      }

      static color_type antique_white() {
        return color_type(value_of(known_color::antique_white));
      }

      static color_type aqua() {
        return color_type(value_of(known_color::aqua));
      }

      static color_type aquamarine() {
        return color_type(value_of(known_color::aquamarine));
      }

      static color_type azure() {
        return color_type(value_of(known_color::azure));
      }

      static color_type beige() {
        return color_type(value_of(known_color::beige));
      }

      static color_type bisque() {
        return color_type(value_of(known_color::bisque));
      }

      static color_type black() {
        return color_type(value_of(known_color::black));
      }

      static color_type blanched_almond() {
        return color_type(value_of(known_color::blanched_almond));
      }

      static color_type blue() {
        return color_type(value_of(known_color::blue));
      }

      static color_type blue_violet() {
        return color_type(value_of(known_color::blue_violet));
      }

      static color_type brown() {
        return color_type(value_of(known_color::brown));
      }

      static color_type burly_wood() {
        return color_type(value_of(known_color::burly_wood));
      }

      static color_type cadet_blue() {
        return color_type(value_of(known_color::cadet_blue));
      }

      static color_type chartreuse() {
        return color_type(value_of(known_color::chartreuse));
      }

      static color_type chocolate() {
        return color_type(value_of(known_color::chocolate));
      }

      static color_type coral() {
        return color_type(value_of(known_color::coral));
      }

      static color_type cornflower_blue() {
        return color_type(value_of(known_color::cornflower_blue));
      }

      static color_type cornsilk() {
        return color_type(value_of(known_color::cornsilk));
      }

      static color_type crimson() {
        return color_type(value_of(known_color::crimson));
      }

      static color_type cyan() {
        return color_type(value_of(known_color::cyan));
      }

      static color_type dark_blue() {
        return color_type(value_of(known_color::dark_blue));
      }

      static color_type dark_cyan() {
        return color_type(value_of(known_color::dark_cyan));
      }

      static color_type dark_goldenrod() {
        return color_type(value_of(known_color::dark_goldenrod));
      }

      static color_type dark_gray() {
        return color_type(value_of(known_color::dark_gray));
      }

      static color_type dark_green() {
        return color_type(value_of(known_color::dark_green));
      }

      static color_type dark_khaki() {
        return color_type(value_of(known_color::dark_khaki));
      }

      static color_type dark_magenta() {
        return color_type(value_of(known_color::dark_magenta));
      }

      static color_type dark_olive_green() {
        return color_type(value_of(known_color::dark_olive_green));
      }

      static color_type dark_orange() {
        return color_type(value_of(known_color::dark_orange));
      }

      static color_type dark_orchid() {
        return color_type(value_of(known_color::dark_orchid));
      }

      static color_type dark_red() {
        return color_type(value_of(known_color::dark_red));
      }

      static color_type dark_salmon() {
        return color_type(value_of(known_color::dark_salmon));
      }

      static color_type dark_sea_green() {
        return color_type(value_of(known_color::dark_sea_green));
      }

      static color_type dark_slate_blue() {
        return color_type(value_of(known_color::dark_slate_blue));
      }

      static color_type dark_slate_gray() {
        return color_type(value_of(known_color::dark_slate_gray));
      }

      static color_type dark_turquoise() {
        return color_type(value_of(known_color::dark_turquoise));
      }

      static color_type dark_violet() {
        return color_type(value_of(known_color::dark_violet));
      }

      static color_type deep_pink() {
        return color_type(value_of(known_color::deep_pink));
      }

      static color_type deep_sky_blue() {
        return color_type(value_of(known_color::deep_sky_blue));
      }

      static color_type dim_gray() {
        return color_type(value_of(known_color::dim_gray));
      }

      static color_type dodger_blue() {
        return color_type(value_of(known_color::dodger_blue));
      }

      static color_type firebrick() {
        return color_type(value_of(known_color::firebrick));
      }

      static color_type floral_white() {
        return color_type(value_of(known_color::floral_white));
      }

      static color_type forest_green() {
        return color_type(value_of(known_color::forest_green));
      }

      static color_type fuchsia() {
        return color_type(value_of(known_color::fuchsia));
      }

      static color_type gainsboro() {
        return color_type(value_of(known_color::gainsboro));
      }

      static color_type ghost_white() {
        return color_type(value_of(known_color::ghost_white));
      }

      static color_type gold() {
        return color_type(value_of(known_color::gold));
      }

      static color_type goldenrod() {
        return color_type(value_of(known_color::goldenrod));
      }

      static color_type gray() {
        return color_type(value_of(known_color::gray));
      }

      static color_type green() {
        return color_type(value_of(known_color::green));
      }

      static color_type green_yellow() {
        return color_type(value_of(known_color::green_yellow));
      }

      static color_type honeydew() {
        return color_type(value_of(known_color::honeydew));
      }

      static color_type hot_pink() {
        return color_type(value_of(known_color::hot_pink));
      }

      static color_type indian_red() {
        return color_type(value_of(known_color::indian_red));
      }

      static color_type indigo() {
        return color_type(value_of(known_color::indigo));
      }

      static color_type ivory() {
        return color_type(value_of(known_color::ivory));
      }

      static color_type khaki() {
        return color_type(value_of(known_color::khaki));
      }

      static color_type lavender() {
        return color_type(value_of(known_color::lavender));
      }

      static color_type lavender_blush() {
        return color_type(value_of(known_color::lavender_blush));
      }

      static color_type lawn_green() {
        return color_type(value_of(known_color::lawn_green));
      }

      static color_type lemon_chiffon() {
        return color_type(value_of(known_color::lemon_chiffon));
      }

      static color_type light_blue() {
        return color_type(value_of(known_color::light_blue));
      }

      static color_type light_coral() {
        return color_type(value_of(known_color::light_coral));
      }

      static color_type light_cyan() {
        return color_type(value_of(known_color::light_cyan));
      }

      static color_type light_goldenrod_yellow() {
        return color_type(value_of(known_color::light_goldenrod_yellow));
      }

      static color_type light_gray() {
        return color_type(value_of(known_color::light_gray));
      }

      static color_type light_green() {
        return color_type(value_of(known_color::light_green));
      }

      static color_type light_pink() {
        return color_type(value_of(known_color::light_pink));
      }

      static color_type light_salmon() {
        return color_type(value_of(known_color::light_salmon));
      }

      static color_type light_sea_green() {
        return color_type(value_of(known_color::light_sea_green));
      }

      static color_type light_sky_blue() {
        return color_type(value_of(known_color::light_sky_blue));
      }

      static color_type light_slate_gray() {
        return color_type(value_of(known_color::light_slate_gray));
      }

      static color_type light_steel_blue() {
        return color_type(value_of(known_color::light_steel_blue));
      }

      static color_type light_yellow() {
        return color_type(value_of(known_color::light_yellow));
      }

      static color_type lime() {
        return color_type(value_of(known_color::lime));
      }

      static color_type lime_green() {
        return color_type(value_of(known_color::lime_green));
      }

      static color_type linen() {
        return color_type(value_of(known_color::linen));
      }

      static color_type magenta() {
        return color_type(value_of(known_color::magenta));
      }

      static color_type maroon() {
        return color_type(value_of(known_color::maroon));
      }

      static color_type medium_aquamarine() {
        return color_type(value_of(known_color::medium_aquamarine));
      }

      static color_type medium_blue() {
        return color_type(value_of(known_color::medium_blue));
      }

      static color_type medium_orchid() {
        return color_type(value_of(known_color::medium_orchid));
      }

      static color_type medium_purple() {
        return color_type(value_of(known_color::medium_purple));
      }

      static color_type medium_sea_green() {
        return color_type(value_of(known_color::medium_sea_green));
      }

      static color_type medium_slate_blue() {
        return color_type(value_of(known_color::medium_slate_blue));
      }

      static color_type medium_spring_green() {
        return color_type(value_of(known_color::medium_spring_green));
      }

      static color_type medium_turquoise() {
        return color_type(value_of(known_color::medium_turquoise));
      }

      static color_type medium_violet_red() {
        return color_type(value_of(known_color::medium_violet_red));
      }

      static color_type midnight_blue() {
        return color_type(value_of(known_color::midnight_blue));
      }

      static color_type mint_cream() {
        return color_type(value_of(known_color::mint_cream));
      }

      static color_type misty_rose() {
        return color_type(value_of(known_color::misty_rose));
      }

      static color_type moccasin() {
        return color_type(value_of(known_color::moccasin));
      }

      static color_type navajo_white() {
        return color_type(value_of(known_color::navajo_white));
      }

      static color_type navy() {
        return color_type(value_of(known_color::navy));
      }

      static color_type old_lace() {
        return color_type(value_of(known_color::old_lace));
      }

      static color_type olive() {
        return color_type(value_of(known_color::olive));
      }

      static color_type olive_drab() {
        return color_type(value_of(known_color::olive_drab));
      }

      static color_type orange() {
        return color_type(value_of(known_color::orange));
      }

      static color_type orange_red() {
        return color_type(value_of(known_color::orange_red));
      }

      static color_type orchid() {
        return color_type(value_of(known_color::orchid));
      }

      static color_type pale_goldenrod() {
        return color_type(value_of(known_color::pale_goldenrod));
      }

      static color_type pale_green() {
        return color_type(value_of(known_color::pale_green));
      }

      static color_type pale_turquoise() {
        return color_type(value_of(known_color::pale_turquoise));
      }

      static color_type pale_violet_red() {
        return color_type(value_of(known_color::pale_violet_red));
      }

      static color_type papaya_whip() {
        return color_type(value_of(known_color::papaya_whip));
      }

      static color_type peach_puff() {
        return color_type(value_of(known_color::peach_puff));
      }

      static color_type peru() {
        return color_type(value_of(known_color::peru));
      }

      static color_type pink() {
        return color_type(value_of(known_color::pink));
      }

      static color_type plum() {
        return color_type(value_of(known_color::plum));
      }

      static color_type powder_blue() {
        return color_type(value_of(known_color::powder_blue));
      }

      static color_type purple() {
        return color_type(value_of(known_color::purple));
      }

      static color_type red() {
        return color_type(value_of(known_color::red));
      }

      static color_type rosy_brown() {
        return color_type(value_of(known_color::rosy_brown));
      }

      static color_type royal_blue() {
        return color_type(value_of(known_color::royal_blue));
      }

      static color_type saddle_brown() {
        return color_type(value_of(known_color::saddle_brown));
      }

      static color_type salmon() {
        return color_type(value_of(known_color::salmon));
      }

      static color_type sandy_brown() {
        return color_type(value_of(known_color::sandy_brown));
      }

      static color_type sea_green() {
        return color_type(value_of(known_color::sea_green));
      }

      static color_type sea_shell() {
        return color_type(value_of(known_color::sea_shell));
      }

      static color_type sienna() {
        return color_type(value_of(known_color::sienna));
      }

      static color_type silver() {
        return color_type(value_of(known_color::silver));
      }

      static color_type sky_blue() {
        return color_type(value_of(known_color::sky_blue));
      }

      static color_type slate_blue() {
        return color_type(value_of(known_color::slate_blue));
      }

      static color_type slate_gray() {
        return color_type(value_of(known_color::slate_gray));
      }

      static color_type snow() {
        return color_type(value_of(known_color::snow));
      }

      static color_type spring_green() {
        return color_type(value_of(known_color::spring_green));
      }

      static color_type steel_blue() {
        return color_type(value_of(known_color::steel_blue));
      }

      static color_type tan() {
        return color_type(value_of(known_color::tan));
      }

      static color_type teal() {
        return color_type(value_of(known_color::teal));
      }

      static color_type thistle() {
        return color_type(value_of(known_color::thistle));
      }

      static color_type tomato() {
        return color_type(value_of(known_color::tomato));
      }

      static color_type transparent() {
        return color_type(value_of(known_color::transparent));
      }

      static color_type turquoise() {
        return color_type(value_of(known_color::turquoise));
      }

      static color_type violet() {
        return color_type(value_of(known_color::violet));
      }

      static color_type wheat() {
        return color_type(value_of(known_color::wheat));
      }

      static color_type white() {
        return color_type(value_of(known_color::white));
      }

      static color_type white_smoke() {
        return color_type(value_of(known_color::white_smoke));
      }

      static color_type yellow() {
        return color_type(value_of(known_color::yellow));
      }

      static color_type yellow_green() {
        return color_type(value_of(known_color::yellow_green));
      }
    };

  } // namespace dotnet
} // namespace colors
