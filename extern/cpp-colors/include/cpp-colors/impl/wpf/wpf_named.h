#pragma once

#include <string>
#include <map>
#include <cstdint>
#include <stdexcept>
#include "wpf_constants.h"
#include "../named_utils.h"


namespace colors {
  namespace wpf {

    template<typename CharT>
    struct basic_named_color;

    template<>
    struct basic_named_color<char> {
      static const char *const black() {
        return "Black";
      }

      static const char *const blue() {
        return "Blue";
      }

      static const char *const brown() {
        return "Brown";
      }

      static const char *const cyan() {
        return "Cyan";
      }

      static const char *const dark_gray() {
        return "DarkGray";
      }

      static const char *const gray() {
        return "Gray";
      }

      static const char *const green() {
        return "Green";
      }

      static const char *const light_gray() {
        return "LightGray";
      }

      static const char *const magenta() {
        return "Magenta";
      }

      static const char *const orange() {
        return "Orange";
      }

      static const char *const purple() {
        return "Purple";
      }

      static const char *const red() {
        return "Red";
      }

      static const char *const transparent() {
        return "Transparent";
      }

      static const char *const white() {
        return "White";
      }

      static const char *const yellow() {
        return "Yellow";
      }
    };

    template<>
    struct basic_named_color<wchar_t> {
      static const wchar_t *const black() {
        return L"Black";
      }

      static const wchar_t *const blue() {
        return L"Blue";
      }

      static const wchar_t *const brown() {
        return L"Brown";
      }

      static const wchar_t *const cyan() {
        return L"Cyan";
      }

      static const wchar_t *const dark_gray() {
        return L"DarkGray";
      }

      static const wchar_t *const gray() {
        return L"Gray";
      }

      static const wchar_t *const green() {
        return L"Green";
      }

      static const wchar_t *const light_gray() {
        return L"LightGray";
      }

      static const wchar_t *const magenta() {
        return L"Magenta";
      }

      static const wchar_t *const orange() {
        return L"Orange";
      }

      static const wchar_t *const purple() {
        return L"Purple";
      }

      static const wchar_t *const red() {
        return L"Red";
      }

      static const wchar_t *const transparent() {
        return L"Transparent";
      }

      static const wchar_t *const white() {
        return L"White";
      }

      static const wchar_t *const yellow() {
        return L"Yellow";
      }
    };

    template<typename CharT>
    struct basic_color_mapper {
      typedef CharT char_type;
      typedef std::basic_string<char_type> string_type;

      typedef known_color known_color_type;
      typedef basic_named_color<char_type> named_color_type;

      typedef std::map<string_type, known_color> string_color_map;
      typedef std::map<uint32_t, string_type> color_string_map;

      // Build a name -> constant map
      static const string_color_map &get_string_to_color_map() {
        static string_color_map sm_;   // name -> value
        if (sm_.empty()) {
          sm_.insert(std::make_pair(to_lowercase(named_color_type::black()), known_color::black));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::blue()), known_color::blue));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::brown()), known_color::brown));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::cyan()), known_color::cyan));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::dark_gray()), known_color::dark_gray));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::gray()), known_color::gray));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::green()), known_color::green));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::light_gray()), known_color::light_gray));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::magenta()), known_color::magenta));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::orange()), known_color::orange));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::purple()), known_color::purple));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::red()), known_color::red));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::transparent()), known_color::transparent));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::white()), known_color::white));
          sm_.insert(std::make_pair(to_lowercase(named_color_type::yellow()), known_color::yellow));
        }

        return sm_;
      }

      // Build a name -> constant map
      static const color_string_map &get_color_to_string_map() {
        static color_string_map vm_;   // value -> name

        if (vm_.empty()) {
          vm_.insert(std::make_pair(value_of(known_color::black), named_color_type::black()));
          vm_.insert(std::make_pair(value_of(known_color::blue), named_color_type::blue()));
          vm_.insert(std::make_pair(value_of(known_color::brown), named_color_type::brown()));
          vm_.insert(std::make_pair(value_of(known_color::cyan), named_color_type::cyan()));
          vm_.insert(std::make_pair(value_of(known_color::dark_gray), named_color_type::dark_gray()));
          vm_.insert(std::make_pair(value_of(known_color::gray), named_color_type::gray()));
          vm_.insert(std::make_pair(value_of(known_color::green), named_color_type::green()));
          vm_.insert(std::make_pair(value_of(known_color::light_gray), named_color_type::light_gray()));
          vm_.insert(std::make_pair(value_of(known_color::magenta), named_color_type::magenta()));
          vm_.insert(std::make_pair(value_of(known_color::orange), named_color_type::orange()));
          vm_.insert(std::make_pair(value_of(known_color::purple), named_color_type::purple()));
          vm_.insert(std::make_pair(value_of(known_color::red), named_color_type::red()));
          vm_.insert(std::make_pair(value_of(known_color::transparent), named_color_type::transparent()));
          vm_.insert(std::make_pair(value_of(known_color::white), named_color_type::white()));
          vm_.insert(std::make_pair(value_of(known_color::yellow), named_color_type::yellow()));
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

  } // namespace wpf
} // namespace colors
