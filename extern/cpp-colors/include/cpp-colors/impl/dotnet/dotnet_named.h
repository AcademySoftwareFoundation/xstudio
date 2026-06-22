#pragma once

#include <string>
#include <map>
#include <cstdint>
#include <stdexcept>
#include "dotnet_constants.h"
#include "../named_utils.h"


namespace colors {
  namespace dotnet {

    template<typename CharT>
    struct basic_named_color;

    template<>
    struct basic_named_color<char> {
      static const char *const alice_blue() {
        return "AliceBlue";
      }

      static const char *const antique_white() {
        return "AntiqueWhite";
      }

      static const char *const aqua() {
        return "Aqua";
      }

      static const char *const aquamarine() {
        return "Aquamarine";
      }

      static const char *const azure() {
        return "Azure";
      }

      static const char *const beige() {
        return "Beige";
      }

      static const char *const bisque() {
        return "Bisque";
      }

      static const char *const black() {
        return "Black";
      }

      static const char *const blanched_almond() {
        return "BlanchedAlmond";
      }

      static const char *const blue() {
        return "Blue";
      }

      static const char *const blue_violet() {
        return "BlueViolet";
      }

      static const char *const brown() {
        return "Brown";
      }

      static const char *const burly_wood() {
        return "BurlyWood";
      }

      static const char *const cadet_blue() {
        return "CadetBlue";
      }

      static const char *const chartreuse() {
        return "Chartreuse";
      }

      static const char *const chocolate() {
        return "Chocolate";
      }

      static const char *const coral() {
        return "Coral";
      }

      static const char *const cornflower_blue() {
        return "CornflowerBlue";
      }

      static const char *const cornsilk() {
        return "Cornsilk";
      }

      static const char *const crimson() {
        return "Crimson";
      }

      static const char *const cyan() {
        return "Cyan";
      }

      static const char *const dark_blue() {
        return "DarkBlue";
      }

      static const char *const dark_cyan() {
        return "DarkCyan";
      }

      static const char *const dark_goldenrod() {
        return "DarkGoldenrod";
      }

      static const char *const dark_gray() {
        return "DarkGray";
      }

      static const char *const dark_green() {
        return "DarkGreen";
      }

      static const char *const dark_khaki() {
        return "DarkKhaki";
      }

      static const char *const dark_magenta() {
        return "DarkMagenta";
      }

      static const char *const dark_olive_green() {
        return "DarkOliveGreen";
      }

      static const char *const dark_orange() {
        return "DarkOrange";
      }

      static const char *const dark_orchid() {
        return "DarkOrchid";
      }

      static const char *const dark_red() {
        return "DarkRed";
      }

      static const char *const dark_salmon() {
        return "DarkSalmon";
      }

      static const char *const dark_sea_green() {
        return "DarkSeaGreen";
      }

      static const char *const dark_slate_blue() {
        return "DarkSlateBlue";
      }

      static const char *const dark_slate_gray() {
        return "DarkSlateGray";
      }

      static const char *const dark_turquoise() {
        return "DarkTurquoise";
      }

      static const char *const dark_violet() {
        return "DarkViolet";
      }

      static const char *const deep_pink() {
        return "DeepPink";
      }

      static const char *const deep_sky_blue() {
        return "DeepSkyBlue";
      }

      static const char *const dim_gray() {
        return "DimGray";
      }

      static const char *const dodger_blue() {
        return "DodgerBlue";
      }

      static const char *const firebrick() {
        return "Firebrick";
      }

      static const char *const floral_white() {
        return "FloralWhite";
      }

      static const char *const forest_green() {
        return "ForestGreen";
      }

      static const char *const fuchsia() {
        return "Fuchsia";
      }

      static const char *const gainsboro() {
        return "Gainsboro";
      }

      static const char *const ghost_white() {
        return "GhostWhite";
      }

      static const char *const gold() {
        return "Gold";
      }

      static const char *const goldenrod() {
        return "Goldenrod";
      }

      static const char *const gray() {
        return "Gray";
      }

      static const char *const green() {
        return "Green";
      }

      static const char *const green_yellow() {
        return "GreenYellow";
      }

      static const char *const honeydew() {
        return "Honeydew";
      }

      static const char *const hot_pink() {
        return "HotPink";
      }

      static const char *const indian_red() {
        return "IndianRed";
      }

      static const char *const indigo() {
        return "Indigo";
      }

      static const char *const ivory() {
        return "Ivory";
      }

      static const char *const khaki() {
        return "Khaki";
      }

      static const char *const lavender() {
        return "Lavender";
      }

      static const char *const lavender_blush() {
        return "LavenderBlush";
      }

      static const char *const lawn_green() {
        return "LawnGreen";
      }

      static const char *const lemon_chiffon() {
        return "LemonChiffon";
      }

      static const char *const light_blue() {
        return "LightBlue";
      }

      static const char *const light_coral() {
        return "LightCoral";
      }

      static const char *const light_cyan() {
        return "LightCyan";
      }

      static const char *const light_goldenrod_yellow() {
        return "LightGoldenrodYellow";
      }

      static const char *const light_gray() {
        return "LightGray";
      }

      static const char *const light_green() {
        return "LightGreen";
      }

      static const char *const light_pink() {
        return "LightPink";
      }

      static const char *const light_salmon() {
        return "LightSalmon";
      }

      static const char *const light_sea_green() {
        return "LightSeaGreen";
      }

      static const char *const light_sky_blue() {
        return "LightSkyBlue";
      }

      static const char *const light_slate_gray() {
        return "LightSlateGray";
      }

      static const char *const light_steel_blue() {
        return "LightSteelBlue";
      }

      static const char *const light_yellow() {
        return "LightYellow";
      }

      static const char *const lime() {
        return "Lime";
      }

      static const char *const lime_green() {
        return "LimeGreen";
      }

      static const char *const linen() {
        return "Linen";
      }

      static const char *const magenta() {
        return "Magenta";
      }

      static const char *const maroon() {
        return "Maroon";
      }

      static const char *const medium_aquamarine() {
        return "MediumAquamarine";
      }

      static const char *const medium_blue() {
        return "MediumBlue";
      }

      static const char *const medium_orchid() {
        return "MediumOrchid";
      }

      static const char *const medium_purple() {
        return "MediumPurple";
      }

      static const char *const medium_sea_green() {
        return "MediumSeaGreen";
      }

      static const char *const medium_slate_blue() {
        return "MediumSlateBlue";
      }

      static const char *const medium_spring_green() {
        return "MediumSpringGreen";
      }

      static const char *const medium_turquoise() {
        return "MediumTurquoise";
      }

      static const char *const medium_violet_red() {
        return "MediumVioletRed";
      }

      static const char *const midnight_blue() {
        return "MidnightBlue";
      }

      static const char *const mint_cream() {
        return "MintCream";
      }

      static const char *const misty_rose() {
        return "MistyRose";
      }

      static const char *const moccasin() {
        return "Moccasin";
      }

      static const char *const navajo_white() {
        return "NavajoWhite";
      }

      static const char *const navy() {
        return "Navy";
      }

      static const char *const old_lace() {
        return "OldLace";
      }

      static const char *const olive() {
        return "Olive";
      }

      static const char *const olive_drab() {
        return "OliveDrab";
      }

      static const char *const orange() {
        return "Orange";
      }

      static const char *const orange_red() {
        return "OrangeRed";
      }

      static const char *const orchid() {
        return "Orchid";
      }

      static const char *const pale_goldenrod() {
        return "PaleGoldenrod";
      }

      static const char *const pale_green() {
        return "PaleGreen";
      }

      static const char *const pale_turquoise() {
        return "PaleTurquoise";
      }

      static const char *const pale_violet_red() {
        return "PaleVioletRed";
      }

      static const char *const papaya_whip() {
        return "PapayaWhip";
      }

      static const char *const peach_puff() {
        return "PeachPuff";
      }

      static const char *const peru() {
        return "Peru";
      }

      static const char *const pink() {
        return "Pink";
      }

      static const char *const plum() {
        return "Plum";
      }

      static const char *const powder_blue() {
        return "PowderBlue";
      }

      static const char *const purple() {
        return "Purple";
      }

      static const char *const red() {
        return "Red";
      }

      static const char *const rosy_brown() {
        return "RosyBrown";
      }

      static const char *const royal_blue() {
        return "RoyalBlue";
      }

      static const char *const saddle_brown() {
        return "SaddleBrown";
      }

      static const char *const salmon() {
        return "Salmon";
      }

      static const char *const sandy_brown() {
        return "SandyBrown";
      }

      static const char *const sea_green() {
        return "SeaGreen";
      }

      static const char *const sea_shell() {
        return "SeaShell";
      }

      static const char *const sienna() {
        return "Sienna";
      }

      static const char *const silver() {
        return "Silver";
      }

      static const char *const sky_blue() {
        return "SkyBlue";
      }

      static const char *const slate_blue() {
        return "SlateBlue";
      }

      static const char *const slate_gray() {
        return "SlateGray";
      }

      static const char *const snow() {
        return "Snow";
      }

      static const char *const spring_green() {
        return "SpringGreen";
      }

      static const char *const steel_blue() {
        return "SteelBlue";
      }

      static const char *const tan() {
        return "Tan";
      }

      static const char *const teal() {
        return "Teal";
      }

      static const char *const thistle() {
        return "Thistle";
      }

      static const char *const tomato() {
        return "Tomato";
      }

      static const char *const transparent() {
        return "Transparent";
      }

      static const char *const turquoise() {
        return "Turquoise";
      }

      static const char *const violet() {
        return "Violet";
      }

      static const char *const wheat() {
        return "Wheat";
      }

      static const char *const white() {
        return "White";
      }

      static const char *const white_smoke() {
        return "WhiteSmoke";
      }

      static const char *const yellow() {
        return "Yellow";
      }

      static const char *const yellow_green() {
        return "YellowGreen";
      }
    };

    template<>
    struct basic_named_color<wchar_t> {
      static const wchar_t *const alice_blue() {
        return L"AliceBlue";
      }

      static const wchar_t *const antique_white() {
        return L"AntiqueWhite";
      }

      static const wchar_t *const aqua() {
        return L"Aqua";
      }

      static const wchar_t *const aquamarine() {
        return L"Aquamarine";
      }

      static const wchar_t *const azure() {
        return L"Azure";
      }

      static const wchar_t *const beige() {
        return L"Beige";
      }

      static const wchar_t *const bisque() {
        return L"Bisque";
      }

      static const wchar_t *const black() {
        return L"Black";
      }

      static const wchar_t *const blanched_almond() {
        return L"BlanchedAlmond";
      }

      static const wchar_t *const blue() {
        return L"Blue";
      }

      static const wchar_t *const blue_violet() {
        return L"BlueViolet";
      }

      static const wchar_t *const brown() {
        return L"Brown";
      }

      static const wchar_t *const burly_wood() {
        return L"BurlyWood";
      }

      static const wchar_t *const cadet_blue() {
        return L"CadetBlue";
      }

      static const wchar_t *const chartreuse() {
        return L"Chartreuse";
      }

      static const wchar_t *const chocolate() {
        return L"Chocolate";
      }

      static const wchar_t *const coral() {
        return L"Coral";
      }

      static const wchar_t *const cornflower_blue() {
        return L"CornflowerBlue";
      }

      static const wchar_t *const cornsilk() {
        return L"Cornsilk";
      }

      static const wchar_t *const crimson() {
        return L"Crimson";
      }

      static const wchar_t *const cyan() {
        return L"Cyan";
      }

      static const wchar_t *const dark_blue() {
        return L"DarkBlue";
      }

      static const wchar_t *const dark_cyan() {
        return L"DarkCyan";
      }

      static const wchar_t *const dark_goldenrod() {
        return L"DarkGoldenrod";
      }

      static const wchar_t *const dark_gray() {
        return L"DarkGray";
      }

      static const wchar_t *const dark_green() {
        return L"DarkGreen";
      }

      static const wchar_t *const dark_khaki() {
        return L"DarkKhaki";
      }

      static const wchar_t *const dark_magenta() {
        return L"DarkMagenta";
      }

      static const wchar_t *const dark_olive_green() {
        return L"DarkOliveGreen";
      }

      static const wchar_t *const dark_orange() {
        return L"DarkOrange";
      }

      static const wchar_t *const dark_orchid() {
        return L"DarkOrchid";
      }

      static const wchar_t *const dark_red() {
        return L"DarkRed";
      }

      static const wchar_t *const dark_salmon() {
        return L"DarkSalmon";
      }

      static const wchar_t *const dark_sea_green() {
        return L"DarkSeaGreen";
      }

      static const wchar_t *const dark_slate_blue() {
        return L"DarkSlateBlue";
      }

      static const wchar_t *const dark_slate_gray() {
        return L"DarkSlateGray";
      }

      static const wchar_t *const dark_turquoise() {
        return L"DarkTurquoise";
      }

      static const wchar_t *const dark_violet() {
        return L"DarkViolet";
      }

      static const wchar_t *const deep_pink() {
        return L"DeepPink";
      }

      static const wchar_t *const deep_sky_blue() {
        return L"DeepSkyBlue";
      }

      static const wchar_t *const dim_gray() {
        return L"DimGray";
      }

      static const wchar_t *const dodger_blue() {
        return L"DodgerBlue";
      }

      static const wchar_t *const firebrick() {
        return L"Firebrick";
      }

      static const wchar_t *const floral_white() {
        return L"FloralWhite";
      }

      static const wchar_t *const forest_green() {
        return L"ForestGreen";
      }

      static const wchar_t *const fuchsia() {
        return L"Fuchsia";
      }

      static const wchar_t *const gainsboro() {
        return L"Gainsboro";
      }

      static const wchar_t *const ghost_white() {
        return L"GhostWhite";
      }

      static const wchar_t *const gold() {
        return L"Gold";
      }

      static const wchar_t *const goldenrod() {
        return L"Goldenrod";
      }

      static const wchar_t *const gray() {
        return L"Gray";
      }

      static const wchar_t *const green() {
        return L"Green";
      }

      static const wchar_t *const green_yellow() {
        return L"GreenYellow";
      }

      static const wchar_t *const honeydew() {
        return L"Honeydew";
      }

      static const wchar_t *const hot_pink() {
        return L"HotPink";
      }

      static const wchar_t *const indian_red() {
        return L"IndianRed";
      }

      static const wchar_t *const indigo() {
        return L"Indigo";
      }

      static const wchar_t *const ivory() {
        return L"Ivory";
      }

      static const wchar_t *const khaki() {
        return L"Khaki";
      }

      static const wchar_t *const lavender() {
        return L"Lavender";
      }

      static const wchar_t *const lavender_blush() {
        return L"LavenderBlush";
      }

      static const wchar_t *const lawn_green() {
        return L"LawnGreen";
      }

      static const wchar_t *const lemon_chiffon() {
        return L"LemonChiffon";
      }

      static const wchar_t *const light_blue() {
        return L"LightBlue";
      }

      static const wchar_t *const light_coral() {
        return L"LightCoral";
      }

      static const wchar_t *const light_cyan() {
        return L"LightCyan";
      }

      static const wchar_t *const light_goldenrod_yellow() {
        return L"LightGoldenrodYellow";
      }

      static const wchar_t *const light_gray() {
        return L"LightGray";
      }

      static const wchar_t *const light_green() {
        return L"LightGreen";
      }

      static const wchar_t *const light_pink() {
        return L"LightPink";
      }

      static const wchar_t *const light_salmon() {
        return L"LightSalmon";
      }

      static const wchar_t *const light_sea_green() {
        return L"LightSeaGreen";
      }

      static const wchar_t *const light_sky_blue() {
        return L"LightSkyBlue";
      }

      static const wchar_t *const light_slate_gray() {
        return L"LightSlateGray";
      }

      static const wchar_t *const light_steel_blue() {
        return L"LightSteelBlue";
      }

      static const wchar_t *const light_yellow() {
        return L"LightYellow";
      }

      static const wchar_t *const lime() {
        return L"Lime";
      }

      static const wchar_t *const lime_green() {
        return L"LimeGreen";
      }

      static const wchar_t *const linen() {
        return L"Linen";
      }

      static const wchar_t *const magenta() {
        return L"Magenta";
      }

      static const wchar_t *const maroon() {
        return L"Maroon";
      }

      static const wchar_t *const medium_aquamarine() {
        return L"MediumAquamarine";
      }

      static const wchar_t *const medium_blue() {
        return L"MediumBlue";
      }

      static const wchar_t *const medium_orchid() {
        return L"MediumOrchid";
      }

      static const wchar_t *const medium_purple() {
        return L"MediumPurple";
      }

      static const wchar_t *const medium_sea_green() {
        return L"MediumSeaGreen";
      }

      static const wchar_t *const medium_slate_blue() {
        return L"MediumSlateBlue";
      }

      static const wchar_t *const medium_spring_green() {
        return L"MediumSpringGreen";
      }

      static const wchar_t *const medium_turquoise() {
        return L"MediumTurquoise";
      }

      static const wchar_t *const medium_violet_red() {
        return L"MediumVioletRed";
      }

      static const wchar_t *const midnight_blue() {
        return L"MidnightBlue";
      }

      static const wchar_t *const mint_cream() {
        return L"MintCream";
      }

      static const wchar_t *const misty_rose() {
        return L"MistyRose";
      }

      static const wchar_t *const moccasin() {
        return L"Moccasin";
      }

      static const wchar_t *const navajo_white() {
        return L"NavajoWhite";
      }

      static const wchar_t *const navy() {
        return L"Navy";
      }

      static const wchar_t *const old_lace() {
        return L"OldLace";
      }

      static const wchar_t *const olive() {
        return L"Olive";
      }

      static const wchar_t *const olive_drab() {
        return L"OliveDrab";
      }

      static const wchar_t *const orange() {
        return L"Orange";
      }

      static const wchar_t *const orange_red() {
        return L"OrangeRed";
      }

      static const wchar_t *const orchid() {
        return L"Orchid";
      }

      static const wchar_t *const pale_goldenrod() {
        return L"PaleGoldenrod";
      }

      static const wchar_t *const pale_green() {
        return L"PaleGreen";
      }

      static const wchar_t *const pale_turquoise() {
        return L"PaleTurquoise";
      }

      static const wchar_t *const pale_violet_red() {
        return L"PaleVioletRed";
      }

      static const wchar_t *const papaya_whip() {
        return L"PapayaWhip";
      }

      static const wchar_t *const peach_puff() {
        return L"PeachPuff";
      }

      static const wchar_t *const peru() {
        return L"Peru";
      }

      static const wchar_t *const pink() {
        return L"Pink";
      }

      static const wchar_t *const plum() {
        return L"Plum";
      }

      static const wchar_t *const powder_blue() {
        return L"PowderBlue";
      }

      static const wchar_t *const purple() {
        return L"Purple";
      }

      static const wchar_t *const red() {
        return L"Red";
      }

      static const wchar_t *const rosy_brown() {
        return L"RosyBrown";
      }

      static const wchar_t *const royal_blue() {
        return L"RoyalBlue";
      }

      static const wchar_t *const saddle_brown() {
        return L"SaddleBrown";
      }

      static const wchar_t *const salmon() {
        return L"Salmon";
      }

      static const wchar_t *const sandy_brown() {
        return L"SandyBrown";
      }

      static const wchar_t *const sea_green() {
        return L"SeaGreen";
      }

      static const wchar_t *const sea_shell() {
        return L"SeaShell";
      }

      static const wchar_t *const sienna() {
        return L"Sienna";
      }

      static const wchar_t *const silver() {
        return L"Silver";
      }

      static const wchar_t *const sky_blue() {
        return L"SkyBlue";
      }

      static const wchar_t *const slate_blue() {
        return L"SlateBlue";
      }

      static const wchar_t *const slate_gray() {
        return L"SlateGray";
      }

      static const wchar_t *const snow() {
        return L"Snow";
      }

      static const wchar_t *const spring_green() {
        return L"SpringGreen";
      }

      static const wchar_t *const steel_blue() {
        return L"SteelBlue";
      }

      static const wchar_t *const tan() {
        return L"Tan";
      }

      static const wchar_t *const teal() {
        return L"Teal";
      }

      static const wchar_t *const thistle() {
        return L"Thistle";
      }

      static const wchar_t *const tomato() {
        return L"Tomato";
      }

      static const wchar_t *const transparent() {
        return L"Transparent";
      }

      static const wchar_t *const turquoise() {
        return L"Turquoise";
      }

      static const wchar_t *const violet() {
        return L"Violet";
      }

      static const wchar_t *const wheat() {
        return L"Wheat";
      }

      static const wchar_t *const white() {
        return L"White";
      }

      static const wchar_t *const white_smoke() {
        return L"WhiteSmoke";
      }

      static const wchar_t *const yellow() {
        return L"Yellow";
      }

      static const wchar_t *const yellow_green() {
        return L"YellowGreen";
      }
    };

    template<typename CharT>
    struct basic_color_mapper {
      typedef CharT char_type;
      typedef std::basic_string <char_type> string_type;

      typedef known_color known_color_type;
      typedef basic_named_color<CharT> named_color_type;

      typedef std::map <string_type, known_color> string_color_map;
      typedef std::map <uint32_t, string_type> color_string_map;

      // Build a name -> constant map
      static const string_color_map &get_string_to_color_map() {
        static string_color_map sm_;   // name -> value
        if (sm_.empty()) {
          sm_.insert(std::make_pair(to_lowercase(named_color_type::alice_blue()), known_color::alice_blue));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::antique_white()), known_color::antique_white));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::aqua()), known_color::aqua));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::aquamarine()), known_color::aquamarine));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::azure()), known_color::azure));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::beige()), known_color::beige));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::bisque()), known_color::bisque));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::black()), known_color::black));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::blanched_almond()), known_color::blanched_almond));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::blue()), known_color::blue));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::blue_violet()), known_color::blue_violet));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::brown()), known_color::brown));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::burly_wood()), known_color::burly_wood));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::cadet_blue()), known_color::cadet_blue));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::chartreuse()), known_color::chartreuse));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::chocolate()), known_color::chocolate));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::coral()), known_color::coral));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::cornflower_blue()), known_color::cornflower_blue));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::cornsilk()), known_color::cornsilk));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::crimson()), known_color::crimson));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::cyan()), known_color::cyan));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::dark_blue()), known_color::dark_blue));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::dark_cyan()), known_color::dark_cyan));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::dark_goldenrod()), known_color::dark_goldenrod));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::dark_gray()), known_color::dark_gray));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::dark_green()), known_color::dark_green));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::dark_khaki()), known_color::dark_khaki));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::dark_magenta()), known_color::dark_magenta));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::dark_olive_green()), known_color::dark_olive_green));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::dark_orange()), known_color::dark_orange));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::dark_orchid()), known_color::dark_orchid));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::dark_red()), known_color::dark_red));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::dark_salmon()), known_color::dark_salmon));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::dark_sea_green()), known_color::dark_sea_green));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::dark_slate_blue()), known_color::dark_slate_blue));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::dark_slate_gray()), known_color::dark_slate_gray));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::dark_turquoise()), known_color::dark_turquoise));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::dark_violet()), known_color::dark_violet));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::deep_pink()), known_color::deep_pink));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::deep_sky_blue()), known_color::deep_sky_blue));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::dim_gray()), known_color::dim_gray));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::dodger_blue()), known_color::dodger_blue));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::firebrick()), known_color::firebrick));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::floral_white()), known_color::floral_white));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::forest_green()), known_color::forest_green));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::fuchsia()), known_color::fuchsia));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::gainsboro()), known_color::gainsboro));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::ghost_white()), known_color::ghost_white));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::gold()), known_color::gold));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::goldenrod()), known_color::goldenrod));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::gray()), known_color::gray));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::green()), known_color::green));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::green_yellow()), known_color::green_yellow));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::honeydew()), known_color::honeydew));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::hot_pink()), known_color::hot_pink));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::indian_red()), known_color::indian_red));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::indigo()), known_color::indigo));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::ivory()), known_color::ivory));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::khaki()), known_color::khaki));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::lavender()), known_color::lavender));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::lavender_blush()), known_color::lavender_blush));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::lawn_green()), known_color::lawn_green));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::lemon_chiffon()), known_color::lemon_chiffon));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::light_blue()), known_color::light_blue));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::light_coral()), known_color::light_coral));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::light_cyan()), known_color::light_cyan));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::light_goldenrod_yellow()), known_color::light_goldenrod_yellow));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::light_gray()), known_color::light_gray));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::light_green()), known_color::light_green));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::light_pink()), known_color::light_pink));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::light_salmon()), known_color::light_salmon));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::light_sea_green()), known_color::light_sea_green));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::light_sky_blue()), known_color::light_sky_blue));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::light_slate_gray()), known_color::light_slate_gray));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::light_steel_blue()), known_color::light_steel_blue));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::light_yellow()), known_color::light_yellow));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::lime()), known_color::lime));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::lime_green()), known_color::lime_green));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::linen()), known_color::linen));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::magenta()), known_color::magenta));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::maroon()), known_color::maroon));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::medium_aquamarine()), known_color::medium_aquamarine));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::medium_blue()), known_color::medium_blue));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::medium_orchid()), known_color::medium_orchid));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::medium_purple()), known_color::medium_purple));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::medium_sea_green()), known_color::medium_sea_green));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::medium_slate_blue()), known_color::medium_slate_blue));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::medium_spring_green()), known_color::medium_spring_green));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::medium_turquoise()), known_color::medium_turquoise));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::medium_violet_red()), known_color::medium_violet_red));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::midnight_blue()), known_color::midnight_blue));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::mint_cream()), known_color::mint_cream));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::misty_rose()), known_color::misty_rose));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::moccasin()), known_color::moccasin));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::navajo_white()), known_color::navajo_white));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::navy()), known_color::navy));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::old_lace()), known_color::old_lace));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::olive()), known_color::olive));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::olive_drab()), known_color::olive_drab));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::orange()), known_color::orange));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::orange_red()), known_color::orange_red));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::orchid()), known_color::orchid));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::pale_goldenrod()), known_color::pale_goldenrod));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::pale_green()), known_color::pale_green));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::pale_turquoise()), known_color::pale_turquoise));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::pale_violet_red()), known_color::pale_violet_red));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::papaya_whip()), known_color::papaya_whip));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::peach_puff()), known_color::peach_puff));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::peru()), known_color::peru));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::pink()), known_color::pink));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::plum()), known_color::plum));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::powder_blue()), known_color::powder_blue));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::purple()), known_color::purple));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::red()), known_color::red));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::rosy_brown()), known_color::rosy_brown));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::royal_blue()), known_color::royal_blue));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::saddle_brown()), known_color::saddle_brown));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::salmon()), known_color::salmon));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::sandy_brown()), known_color::sandy_brown));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::sea_green()), known_color::sea_green));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::sea_shell()), known_color::sea_shell));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::sienna()), known_color::sienna));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::silver()), known_color::silver));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::sky_blue()), known_color::sky_blue));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::slate_blue()), known_color::slate_blue));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::slate_gray()), known_color::slate_gray));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::snow()), known_color::snow));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::spring_green()), known_color::spring_green));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::steel_blue()), known_color::steel_blue));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::tan()), known_color::tan));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::teal()), known_color::teal));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::thistle()), known_color::thistle));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::tomato()), known_color::tomato));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::transparent()), known_color::transparent));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::turquoise()), known_color::turquoise));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::violet()), known_color::violet));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::wheat()), known_color::wheat));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::white()), known_color::white));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::white_smoke()), known_color::white_smoke));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::yellow()), known_color::yellow));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::yellow_green()), known_color::yellow_green));
        }

        return sm_;
      }

      // Build a name -> constant map
      static const color_string_map &get_color_to_string_map() {
        static color_string_map vm_;   // value -> name

        if (vm_.empty()) {
          vm_.insert(std::make_pair(value_of(known_color::alice_blue), named_color_type::alice_blue()));
          vm_.insert(std::make_pair(value_of(known_color::antique_white), named_color_type::antique_white()));
          vm_.insert(std::make_pair(value_of(known_color::aqua), named_color_type::aqua()));
          vm_.insert(std::make_pair(value_of(known_color::aquamarine), named_color_type::aquamarine()));
          vm_.insert(std::make_pair(value_of(known_color::azure), named_color_type::azure()));
          vm_.insert(std::make_pair(value_of(known_color::beige), named_color_type::beige()));
          vm_.insert(std::make_pair(value_of(known_color::bisque), named_color_type::bisque()));
          vm_.insert(std::make_pair(value_of(known_color::black), named_color_type::black()));
          vm_.insert(std::make_pair(value_of(known_color::blanched_almond), named_color_type::blanched_almond()));
          vm_.insert(std::make_pair(value_of(known_color::blue), named_color_type::blue()));
          vm_.insert(std::make_pair(value_of(known_color::blue_violet), named_color_type::blue_violet()));
          vm_.insert(std::make_pair(value_of(known_color::brown), named_color_type::brown()));
          vm_.insert(std::make_pair(value_of(known_color::burly_wood), named_color_type::burly_wood()));
          vm_.insert(std::make_pair(value_of(known_color::cadet_blue), named_color_type::cadet_blue()));
          vm_.insert(std::make_pair(value_of(known_color::chartreuse), named_color_type::chartreuse()));
          vm_.insert(std::make_pair(value_of(known_color::chocolate), named_color_type::chocolate()));
          vm_.insert(std::make_pair(value_of(known_color::coral), named_color_type::coral()));
          vm_.insert(std::make_pair(value_of(known_color::cornflower_blue), named_color_type::cornflower_blue()));
          vm_.insert(std::make_pair(value_of(known_color::cornsilk), named_color_type::cornsilk()));
          vm_.insert(std::make_pair(value_of(known_color::crimson), named_color_type::crimson()));
          vm_.insert(std::make_pair(value_of(known_color::cyan), named_color_type::cyan()));
          vm_.insert(std::make_pair(value_of(known_color::dark_blue), named_color_type::dark_blue()));
          vm_.insert(std::make_pair(value_of(known_color::dark_cyan), named_color_type::dark_cyan()));
          vm_.insert(std::make_pair(value_of(known_color::dark_goldenrod), named_color_type::dark_goldenrod()));
          vm_.insert(std::make_pair(value_of(known_color::dark_gray), named_color_type::dark_gray()));
          vm_.insert(std::make_pair(value_of(known_color::dark_green), named_color_type::dark_green()));
          vm_.insert(std::make_pair(value_of(known_color::dark_khaki), named_color_type::dark_khaki()));
          vm_.insert(std::make_pair(value_of(known_color::dark_magenta), named_color_type::dark_magenta()));
          vm_.insert(std::make_pair(value_of(known_color::dark_olive_green), named_color_type::dark_olive_green()));
          vm_.insert(std::make_pair(value_of(known_color::dark_orange), named_color_type::dark_orange()));
          vm_.insert(std::make_pair(value_of(known_color::dark_orchid), named_color_type::dark_orchid()));
          vm_.insert(std::make_pair(value_of(known_color::dark_red), named_color_type::dark_red()));
          vm_.insert(std::make_pair(value_of(known_color::dark_salmon), named_color_type::dark_salmon()));
          vm_.insert(std::make_pair(value_of(known_color::dark_sea_green), named_color_type::dark_sea_green()));
          vm_.insert(std::make_pair(value_of(known_color::dark_slate_blue), named_color_type::dark_slate_blue()));
          vm_.insert(std::make_pair(value_of(known_color::dark_slate_gray), named_color_type::dark_slate_gray()));
          vm_.insert(std::make_pair(value_of(known_color::dark_turquoise), named_color_type::dark_turquoise()));
          vm_.insert(std::make_pair(value_of(known_color::dark_violet), named_color_type::dark_violet()));
          vm_.insert(std::make_pair(value_of(known_color::deep_pink), named_color_type::deep_pink()));
          vm_.insert(std::make_pair(value_of(known_color::deep_sky_blue), named_color_type::deep_sky_blue()));
          vm_.insert(std::make_pair(value_of(known_color::dim_gray), named_color_type::dim_gray()));
          vm_.insert(std::make_pair(value_of(known_color::dodger_blue), named_color_type::dodger_blue()));
          vm_.insert(std::make_pair(value_of(known_color::firebrick), named_color_type::firebrick()));
          vm_.insert(std::make_pair(value_of(known_color::floral_white), named_color_type::floral_white()));
          vm_.insert(std::make_pair(value_of(known_color::forest_green), named_color_type::forest_green()));
          vm_.insert(std::make_pair(value_of(known_color::fuchsia), named_color_type::fuchsia()));
          vm_.insert(std::make_pair(value_of(known_color::gainsboro), named_color_type::gainsboro()));
          vm_.insert(std::make_pair(value_of(known_color::ghost_white), named_color_type::ghost_white()));
          vm_.insert(std::make_pair(value_of(known_color::gold), named_color_type::gold()));
          vm_.insert(std::make_pair(value_of(known_color::goldenrod), named_color_type::goldenrod()));
          vm_.insert(std::make_pair(value_of(known_color::gray), named_color_type::gray()));
          vm_.insert(std::make_pair(value_of(known_color::green), named_color_type::green()));
          vm_.insert(std::make_pair(value_of(known_color::green_yellow), named_color_type::green_yellow()));
          vm_.insert(std::make_pair(value_of(known_color::honeydew), named_color_type::honeydew()));
          vm_.insert(std::make_pair(value_of(known_color::hot_pink), named_color_type::hot_pink()));
          vm_.insert(std::make_pair(value_of(known_color::indian_red), named_color_type::indian_red()));
          vm_.insert(std::make_pair(value_of(known_color::indigo), named_color_type::indigo()));
          vm_.insert(std::make_pair(value_of(known_color::ivory), named_color_type::ivory()));
          vm_.insert(std::make_pair(value_of(known_color::khaki), named_color_type::khaki()));
          vm_.insert(std::make_pair(value_of(known_color::lavender), named_color_type::lavender()));
          vm_.insert(std::make_pair(value_of(known_color::lavender_blush), named_color_type::lavender_blush()));
          vm_.insert(std::make_pair(value_of(known_color::lawn_green), named_color_type::lawn_green()));
          vm_.insert(std::make_pair(value_of(known_color::lemon_chiffon), named_color_type::lemon_chiffon()));
          vm_.insert(std::make_pair(value_of(known_color::light_blue), named_color_type::light_blue()));
          vm_.insert(std::make_pair(value_of(known_color::light_coral), named_color_type::light_coral()));
          vm_.insert(std::make_pair(value_of(known_color::light_cyan), named_color_type::light_cyan()));
          vm_.insert(std::make_pair(value_of(known_color::light_goldenrod_yellow), named_color_type::light_goldenrod_yellow()));
          vm_.insert(std::make_pair(value_of(known_color::light_gray), named_color_type::light_gray()));
          vm_.insert(std::make_pair(value_of(known_color::light_green), named_color_type::light_green()));
          vm_.insert(std::make_pair(value_of(known_color::light_pink), named_color_type::light_pink()));
          vm_.insert(std::make_pair(value_of(known_color::light_salmon), named_color_type::light_salmon()));
          vm_.insert(std::make_pair(value_of(known_color::light_sea_green), named_color_type::light_sea_green()));
          vm_.insert(std::make_pair(value_of(known_color::light_sky_blue), named_color_type::light_sky_blue()));
          vm_.insert(std::make_pair(value_of(known_color::light_slate_gray), named_color_type::light_slate_gray()));
          vm_.insert(std::make_pair(value_of(known_color::light_steel_blue), named_color_type::light_steel_blue()));
          vm_.insert(std::make_pair(value_of(known_color::light_yellow), named_color_type::light_yellow()));
          vm_.insert(std::make_pair(value_of(known_color::lime), named_color_type::lime()));
          vm_.insert(std::make_pair(value_of(known_color::lime_green), named_color_type::lime_green()));
          vm_.insert(std::make_pair(value_of(known_color::linen), named_color_type::linen()));
          vm_.insert(std::make_pair(value_of(known_color::magenta), named_color_type::magenta()));
          vm_.insert(std::make_pair(value_of(known_color::maroon), named_color_type::maroon()));
          vm_.insert(std::make_pair(value_of(known_color::medium_aquamarine), named_color_type::medium_aquamarine()));
          vm_.insert(std::make_pair(value_of(known_color::medium_blue), named_color_type::medium_blue()));
          vm_.insert(std::make_pair(value_of(known_color::medium_orchid), named_color_type::medium_orchid()));
          vm_.insert(std::make_pair(value_of(known_color::medium_purple), named_color_type::medium_purple()));
          vm_.insert(std::make_pair(value_of(known_color::medium_sea_green), named_color_type::medium_sea_green()));
          vm_.insert(std::make_pair(value_of(known_color::medium_slate_blue), named_color_type::medium_slate_blue()));
          vm_.insert(std::make_pair(value_of(known_color::medium_spring_green), named_color_type::medium_spring_green()));
          vm_.insert(std::make_pair(value_of(known_color::medium_turquoise), named_color_type::medium_turquoise()));
          vm_.insert(std::make_pair(value_of(known_color::medium_violet_red), named_color_type::medium_violet_red()));
          vm_.insert(std::make_pair(value_of(known_color::midnight_blue), named_color_type::midnight_blue()));
          vm_.insert(std::make_pair(value_of(known_color::mint_cream), named_color_type::mint_cream()));
          vm_.insert(std::make_pair(value_of(known_color::misty_rose), named_color_type::misty_rose()));
          vm_.insert(std::make_pair(value_of(known_color::moccasin), named_color_type::moccasin()));
          vm_.insert(std::make_pair(value_of(known_color::navajo_white), named_color_type::navajo_white()));
          vm_.insert(std::make_pair(value_of(known_color::navy), named_color_type::navy()));
          vm_.insert(std::make_pair(value_of(known_color::old_lace), named_color_type::old_lace()));
          vm_.insert(std::make_pair(value_of(known_color::olive), named_color_type::olive()));
          vm_.insert(std::make_pair(value_of(known_color::olive_drab), named_color_type::olive_drab()));
          vm_.insert(std::make_pair(value_of(known_color::orange), named_color_type::orange()));
          vm_.insert(std::make_pair(value_of(known_color::orange_red), named_color_type::orange_red()));
          vm_.insert(std::make_pair(value_of(known_color::orchid), named_color_type::orchid()));
          vm_.insert(std::make_pair(value_of(known_color::pale_goldenrod), named_color_type::pale_goldenrod()));
          vm_.insert(std::make_pair(value_of(known_color::pale_green), named_color_type::pale_green()));
          vm_.insert(std::make_pair(value_of(known_color::pale_turquoise), named_color_type::pale_turquoise()));
          vm_.insert(std::make_pair(value_of(known_color::pale_violet_red), named_color_type::pale_violet_red()));
          vm_.insert(std::make_pair(value_of(known_color::papaya_whip), named_color_type::papaya_whip()));
          vm_.insert(std::make_pair(value_of(known_color::peach_puff), named_color_type::peach_puff()));
          vm_.insert(std::make_pair(value_of(known_color::peru), named_color_type::peru()));
          vm_.insert(std::make_pair(value_of(known_color::pink), named_color_type::pink()));
          vm_.insert(std::make_pair(value_of(known_color::plum), named_color_type::plum()));
          vm_.insert(std::make_pair(value_of(known_color::powder_blue), named_color_type::powder_blue()));
          vm_.insert(std::make_pair(value_of(known_color::purple), named_color_type::purple()));
          vm_.insert(std::make_pair(value_of(known_color::red), named_color_type::red()));
          vm_.insert(std::make_pair(value_of(known_color::rosy_brown), named_color_type::rosy_brown()));
          vm_.insert(std::make_pair(value_of(known_color::royal_blue), named_color_type::royal_blue()));
          vm_.insert(std::make_pair(value_of(known_color::saddle_brown), named_color_type::saddle_brown()));
          vm_.insert(std::make_pair(value_of(known_color::salmon), named_color_type::salmon()));
          vm_.insert(std::make_pair(value_of(known_color::sandy_brown), named_color_type::sandy_brown()));
          vm_.insert(std::make_pair(value_of(known_color::sea_green), named_color_type::sea_green()));
          vm_.insert(std::make_pair(value_of(known_color::sea_shell), named_color_type::sea_shell()));
          vm_.insert(std::make_pair(value_of(known_color::sienna), named_color_type::sienna()));
          vm_.insert(std::make_pair(value_of(known_color::silver), named_color_type::silver()));
          vm_.insert(std::make_pair(value_of(known_color::sky_blue), named_color_type::sky_blue()));
          vm_.insert(std::make_pair(value_of(known_color::slate_blue), named_color_type::slate_blue()));
          vm_.insert(std::make_pair(value_of(known_color::slate_gray), named_color_type::slate_gray()));
          vm_.insert(std::make_pair(value_of(known_color::snow), named_color_type::snow()));
          vm_.insert(std::make_pair(value_of(known_color::spring_green), named_color_type::spring_green()));
          vm_.insert(std::make_pair(value_of(known_color::steel_blue), named_color_type::steel_blue()));
          vm_.insert(std::make_pair(value_of(known_color::tan), named_color_type::tan()));
          vm_.insert(std::make_pair(value_of(known_color::teal), named_color_type::teal()));
          vm_.insert(std::make_pair(value_of(known_color::thistle), named_color_type::thistle()));
          vm_.insert(std::make_pair(value_of(known_color::tomato), named_color_type::tomato()));
          vm_.insert(std::make_pair(value_of(known_color::transparent), named_color_type::transparent()));
          vm_.insert(std::make_pair(value_of(known_color::turquoise), named_color_type::turquoise()));
          vm_.insert(std::make_pair(value_of(known_color::violet), named_color_type::violet()));
          vm_.insert(std::make_pair(value_of(known_color::wheat), named_color_type::wheat()));
          vm_.insert(std::make_pair(value_of(known_color::white), named_color_type::white()));
          vm_.insert(std::make_pair(value_of(known_color::white_smoke), named_color_type::white_smoke()));
          vm_.insert(std::make_pair(value_of(known_color::yellow), named_color_type::yellow()));
          vm_.insert(std::make_pair(value_of(known_color::yellow_green), named_color_type::yellow_green()));
        }

        return vm_;
      }

    private:
      basic_color_mapper() = delete;
      ~basic_color_mapper() = delete;
      basic_color_mapper(const basic_color_mapper&) = delete;
      basic_color_mapper& operator =(const basic_color_mapper&) = delete;

    private:
      // Convert string to lower case
    static string_type to_lowercase(const string_type &str) {
        static std::locale loc;
        std::string result;
        result.reserve(str.size());

        for (auto elem : str)
            result += std::tolower(elem, loc);

        return result;
    }
    };

  } // namespace dotnet
} // namespace colors
