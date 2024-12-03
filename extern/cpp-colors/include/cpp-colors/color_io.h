#pragma once

#include <iosfwd>
#include <iomanip>
#include <string>
#include <sstream>
#include <limits>
#include <type_traits>
#include <cstdint>
#include <cctype>
#include <cassert>
#include "color.h"
#include "color_named.h"


namespace colors {

  namespace {

    template <typename T>
    struct symbols;

    template <>
    struct symbols<char> {
      static const char sharp = '#';
      static const char zero = '0';

      static const char a = 'a';
      static const char r = 'r';
      static const char g = 'g';
      static const char b = 'b';

      static const char round_bracket_open = '(';
      static const char round_bracket_close = ')';
      static const char comma = ',';

      static const char number_0 = '0';
      static const char number_9 = '9';
      static const char char_A = 'A';
      static const char char_F = 'F';
      static const char char_a = 'a';
      static const char char_f = 'f';

      static const char* const rgb_tag() { return "rgb"; }
      static const char* const argb_tag() { return "argb"; }

      static const char to_lower(char ch) {
        return static_cast<char>(std::tolower(ch));
      }
    };

    template <>
    struct symbols<wchar_t> {
      static const wchar_t sharp = L'#';
      static const wchar_t zero = L'0';

      static const wchar_t a = L'a';
      static const wchar_t r = L'r';
      static const wchar_t g = L'g';
      static const wchar_t b = L'b';

      static const wchar_t number_0 = L'0';
      static const wchar_t number_9 = L'9';
      static const wchar_t char_A = L'A';
      static const wchar_t char_F = L'F';
      static const wchar_t char_a = L'a';
      static const wchar_t char_f = L'f';

      static const wchar_t round_bracket_open = L'(';
      static const wchar_t round_bracket_close = L')';
      static const wchar_t comma = L',';

      static const wchar_t* const rgb_tag() { return L"rgb"; }
      static const wchar_t* const argb_tag() { return L"argb"; }

      static const wchar_t to_lower(wchar_t ch)
      {
        return static_cast<wchar_t>(towlower(ch));
      }
    };


    // Convert to [Color RGB with Alpha argb(a,r,g,b)]
    template <typename CharT, typename T, typename PixelTraits, bool is_floating_point>
    struct to_argb_str_dispatch;

    template <typename CharT, typename T, typename PixelTraits>
    struct to_argb_str_dispatch<CharT, T, PixelTraits, false> {
      typedef basic_color<T, PixelTraits> color_type;
      typedef std::basic_string<CharT>    string_type;

      color_type c;

      to_argb_str_dispatch(const color_type& ci)
        : c(ci) {
      }

      operator string_type() {
        typedef std::basic_ostringstream<CharT> ostringstream_type;
        ostringstream_type oss;

        oss << symbols<CharT>::argb_tag() << symbols<CharT>::round_bracket_open
          << static_cast<uint32_t>(c.a) << symbols<CharT>::comma
          << static_cast<uint32_t>(c.r) << symbols<CharT>::comma
          << static_cast<uint32_t>(c.g) << symbols<CharT>::comma
          << static_cast<uint32_t>(c.b)
          << symbols<CharT>::round_bracket_close;

        return oss.str();
      }
    };

    template <typename CharT, typename T, typename PixelTraits>
    struct to_argb_str_dispatch<CharT, T, PixelTraits, true> {
      typedef basic_color<uint8_t, PixelTraits> color_type;
      typedef basic_color<T, PixelTraits>       color_double_type;
      typedef std::basic_string<CharT>          string_type;

      color_type c;

      to_argb_str_dispatch(const color_double_type& cf)
        : c(cf) {
      }

      operator string_type() {
        return to_argb_str_dispatch<CharT, typename color_type::value_type, typename color_type::pixel_traits_type, false>(c);
      }
    };


    // Convert to [Color RGB rgb(r,g,b)], ignore alpha
    template <typename CharT, typename T, typename PixelTraits, bool is_floating_point>
    struct to_rgb_str_dispatch;

    template <typename CharT, typename T, typename PixelTraits>
    struct to_rgb_str_dispatch<CharT, T, PixelTraits, false> {
      typedef basic_color<T, PixelTraits> color_type;
      typedef std::basic_string<CharT>    string_type;

      color_type c;

      to_rgb_str_dispatch(const color_type& ci)
        : c(ci) {
      }

      operator string_type() {
        typedef std::basic_ostringstream<CharT> ostringstream_type;
        ostringstream_type oss;

        oss << symbols<CharT>::rgb_tag() << symbols<CharT>::round_bracket_open
          << static_cast<uint32_t>(c.r) << symbols<CharT>::comma
          << static_cast<uint32_t>(c.g) << symbols<CharT>::comma
          << static_cast<uint32_t>(c.b)
          << symbols<CharT>::round_bracket_close;

        return oss.str();
      }
    };

    template <typename CharT, typename T, typename PixelTraits>
    struct to_rgb_str_dispatch<CharT, T, PixelTraits, true> {
      typedef basic_color<uint8_t, PixelTraits> color_type;
      typedef basic_color<T, PixelTraits>       color_double_type;
      typedef std::basic_string<CharT>          string_type;

      color_type c;

      to_rgb_str_dispatch(const color_double_type& cf)
        : c(cf) {
      }

      operator string_type() {
        return to_rgb_str_dispatch<CharT, typename color_type::value_type, typename color_type::pixel_traits_type, false>(c);
      }
    };


    // Convert to [Color HEX with Alpha (#AARRGGBB)]
    template <typename CharT, typename T, typename PixelTraits, bool is_floating_point>
    struct to_ahex_str_dispatch;

    template <typename CharT, typename T, typename PixelTraits>
    struct to_ahex_str_dispatch<CharT, T, PixelTraits, false> {
      typedef basic_color<T, PixelTraits> color_type;
      typedef std::basic_string<CharT>    string_type;

      color_type c;

      to_ahex_str_dispatch(const color_type& ci)
        : c(ci) {
      }

      operator string_type() {
        typedef std::basic_ostringstream<CharT> ostringstream_type;
        ostringstream_type oss;

        oss << symbols<CharT>::sharp << std::hex
          << std::setw(2) << std::setfill(symbols<CharT>::zero) << static_cast<uint32_t>(c.a)
          << std::setw(2) << std::setfill(symbols<CharT>::zero) << static_cast<uint32_t>(c.r)
          << std::setw(2) << std::setfill(symbols<CharT>::zero) << static_cast<uint32_t>(c.g)
          << std::setw(2) << std::setfill(symbols<CharT>::zero) << static_cast<uint32_t>(c.b);

        return oss.str();
      }
    };

    template <typename CharT, typename T, typename PixelTraits>
    struct to_ahex_str_dispatch<CharT, T, PixelTraits, true> {
      typedef basic_color<uint8_t, PixelTraits> color_type;
      typedef basic_color<T, PixelTraits>       color_double_type;
      typedef std::basic_string<CharT>          string_type;

      color_type c;

      to_ahex_str_dispatch(const color_double_type& cf)
        : c(cf) {
      }

      operator string_type() {
        return to_ahex_str_dispatch<CharT, typename color_type::value_type, typename color_type::pixel_traits_type, false>(c);
      }
    };


    // Convert to [Color HEX (#RRGGBB)], ignore alpha
    template <typename CharT, typename T, typename PixelTraits, bool is_floating_point>
    struct to_hex_str_dispatch;

    template <typename CharT, typename T, typename PixelTraits>
    struct to_hex_str_dispatch<CharT, T, PixelTraits, false> {
      typedef basic_color<T, PixelTraits> color_type;
      typedef std::basic_string<CharT>    string_type;

      color_type c;

      to_hex_str_dispatch(const color_type& ci)
        : c(ci) {
      }

      operator string_type() {
        typedef std::basic_ostringstream<CharT> ostringstream_type;
        ostringstream_type oss;

        oss << symbols<CharT>::sharp << std::hex
          << std::setw(2) << std::setfill(symbols<CharT>::zero) << static_cast<uint32_t>(c.r)
          << std::setw(2) << std::setfill(symbols<CharT>::zero) << static_cast<uint32_t>(c.g)
          << std::setw(2) << std::setfill(symbols<CharT>::zero) << static_cast<uint32_t>(c.b);

        return oss.str();
      }
    };

    template <typename CharT, typename T, typename PixelTraits>
    struct to_hex_str_dispatch<CharT, T, PixelTraits, true> {
      typedef basic_color<uint8_t, PixelTraits> color_type;
      typedef basic_color<T, PixelTraits>       color_double_type;
      typedef std::basic_string<CharT>          string_type;

      color_type c;

      to_hex_str_dispatch(const color_double_type& cf)
        : c(cf) {
      }

      operator string_type() {
        return to_hex_str_dispatch<CharT, typename color_type::value_type, typename color_type::pixel_traits_type, false>(c);
      }
    };


    template <typename T, typename PixelTraits>
    void assign_color_value(basic_color<T, PixelTraits>& c, size_t idx, typename basic_color<T, PixelTraits>::channel_type value) {
      assert(idx < 4 && "IndexOutOfRange");

      switch (idx) {
      case 0:
        c.a = value;
        break;
      case 1:
        c.r = value;
        break;
      case 2:
        c.g = value;
        break;
      case 3:
        c.b = value;
        break;
      };
    }


  } // end of anonymous namespace



  // Write named color
  template <typename ColorMapper, typename CharT, typename CharTraits, typename T, typename PixelTraits>
  inline std::basic_ostream<CharT, CharTraits>& write_named_color(std::basic_ostream<CharT, CharTraits>& os, const basic_color<T, PixelTraits>& c) {
    typedef basic_named_color_converter<ColorMapper> color_converter;

    if (color_converter::is_named(c.value())) {
      os << color_converter::name(c.value());
    }

    return os;
  }

  // Read named color
  template <typename ColorMapper, typename CharT, typename CharTraits, typename T, typename PixelTraits>
  inline std::basic_istream<CharT, CharTraits>& read_named_color(std::basic_istream<CharT, CharTraits>& is, basic_color<T, PixelTraits>& c) {
    typedef std::basic_string<CharT, CharTraits> string_type;
    typedef basic_named_color_converter<ColorMapper> named_converter;

    string_type name;
    if (is >> name) {
      if (named_converter::is_named(name)) {
        c = basic_color<T, PixelTraits>(value_of(named_converter::value(name)));
      }
      else {
        is.setstate(std::ios_base::failbit);
      }
    }

    return is;
  }



  // Convert to [Color HEX (#RRGGBB)], ignore alpha
  template <typename CharT, typename T, typename PixelTraits>
  inline std::basic_string<CharT> to_hex_str(const basic_color<T, PixelTraits>& c) {
    typedef basic_color<T, PixelTraits> color_type;
    return to_hex_str_dispatch<CharT, T, PixelTraits, std::is_floating_point<typename color_type::value_type>::value >(c);
  }

  // Convert to [Color HEX with Alpha (#AARRGGBB)]
  template <typename CharT, typename T, typename PixelTraits>
  inline std::basic_string<CharT> to_ahex_str(const basic_color<T, PixelTraits>& c) {
    typedef basic_color<T, PixelTraits> color_type;
    return to_ahex_str_dispatch<CharT, T, PixelTraits, std::is_floating_point<typename color_type::value_type>::value >(c);
  }

  // Convert to [Color RGB rgb(r,g,b)], ignore alpha
  template <typename CharT, typename T, typename PixelTraits>
  inline std::basic_string<CharT> to_rgb_str(const basic_color<T, PixelTraits>& c) {
    typedef basic_color<T, PixelTraits> color_type;
    return to_rgb_str_dispatch<CharT, T, PixelTraits, std::is_floating_point<typename color_type::value_type>::value >(c);
  }

  // Convert to [Color RGB with Alpha argb(a,r,g,b)]
  template <typename CharT, typename T, typename PixelTraits>
  inline std::basic_string<CharT> to_argb_str(const basic_color<T, PixelTraits>& c) {
    typedef basic_color<T, PixelTraits> color_type;
    return to_argb_str_dispatch<CharT, T, PixelTraits, std::is_floating_point<typename color_type::value_type>::value >(c);
  }



  template <typename CharT, typename CharTraits, typename T, typename PixelTraits>
  inline std::basic_ostream<CharT, CharTraits>& operator<<(std::basic_ostream<CharT, CharTraits>& os, const basic_color<T, PixelTraits>& c) {
    std::basic_ostringstream<CharT, CharTraits, std::allocator<CharT> > oss;

    oss.flags(os.flags());
    oss.imbue(os.getloc());
    oss.precision(os.precision());

    oss << to_argb_str<CharT>(c);

    return os << oss.str().c_str();
  }


  template <typename CharT, typename CharTraits, typename T, typename PixelTraits>
  void parse_hex(std::basic_istream<CharT, CharTraits>& is, basic_color<T, PixelTraits>& c) {
    typedef basic_color<T, PixelTraits> color_type;
    typedef typename color_type::channel_type    channel_type;
    typedef std::basic_stringstream<CharT, CharTraits, std::allocator<CharT> > stringstream_type;

    // allowed format #RRGGBB or #AARRGGBB
    CharT ch;
    if (is >> ch && ch != symbols<CharT>::sharp) {
      is.putback(ch);
      is.setstate(std::ios_base::failbit);
    }
    else if (!is.fail()) {
      bool is_argb = true;
      const size_t data_len = 4;
      for (size_t i = 0; i != data_len; ++i) {
        // parse HEX
        channel_type chv = 0;
        const size_t hex_len = 2;
        for (size_t j = 0; j != hex_len; ++j) {
          if (is >> ch && ((ch <= symbols<CharT>::number_9 && symbols<CharT>::number_0 <= ch) ||
            (symbols<CharT>::char_a <= ch && ch <= symbols<CharT>::char_f) ||
            (symbols<CharT>::char_A <= ch && ch <= symbols<CharT>::char_F))) {
            CharT ch1 = symbols<CharT>::to_lower(ch);
            channel_type val = 0;
            if (ch1 <= symbols<CharT>::number_9 && symbols<CharT>::number_0 <= ch1) {
              val = static_cast<channel_type>(ch1 - symbols<CharT>::number_0);
            }
            else { // ('a' <= ch1 && ch1 <= 'f')
              val = static_cast<channel_type>(10 + (ch1 - symbols<CharT>::char_a));
            }

            chv <<= 4;
            chv |= val;
          }
          else {
            is.putback(ch);

            if (i < data_len - 1) {
              is.setstate(std::ios_base::failbit);
            }
            else {
              is.clear();
              is_argb = false;
            }

            break;
          }
        }

        if (!is)
          break;

        // #AARRGGBB
        assign_color_value(c, i, chv);
      } // for

      if (!is_argb) {
        // we have RGB instead of ARGB, adjust colors
        c.b = c.g;
        c.g = c.r;
        c.r = c.a;
        c.a = color_type::element_traits_type::max_a();
      }
    }
  }


  template <typename CharT, typename CharTraits, typename T, typename PixelTraits>
  void parse_argb(std::basic_istream<CharT, CharTraits>& is, basic_color<T, PixelTraits>& c) {
    typedef basic_color<T, PixelTraits> color_type;
    typedef typename color_type::channel_type    channel_type;
    typedef std::basic_stringstream<CharT, CharTraits, std::allocator<CharT> > stringstream_type;

    CharT ch;
    if (is >> ch && ch != symbols<CharT>::a) {
      is.putback(ch);
      is.setstate(std::ios_base::failbit);
    }
    else if (!is.fail()) {
      if (is >> ch && ch != symbols<CharT>::r) {
        is.putback(ch);
        is.setstate(std::ios_base::failbit);
      }
      else if (!is.fail()) {
        if (is >> ch && ch != symbols<CharT>::g) {
          is.putback(ch);
          is.setstate(std::ios_base::failbit);
        }
        else if (!is.fail()) {
          if (is >> ch && ch != symbols<CharT>::b) {
            is.putback(ch);
            is.setstate(std::ios_base::failbit);
          }
          else if (!is.fail()) {
            if (is >> ch && ch != symbols<CharT>::round_bracket_open) {
              is.putback(ch);
              is.setstate(std::ios_base::failbit);
            }
            else if (!is.fail()) {
              const size_t data_len = 4;
              for (size_t i = 0; i != data_len; ++i) {
                unsigned int chv;
                bool is_stop = false;
                if (is >> std::ws >> chv >> ch && ch != symbols<CharT>::comma) {
                  is.putback(ch);
                  if (i < data_len - 1)
                    is.setstate(std::ios_base::failbit);

                  is_stop = true;
                }

                assign_color_value(c, i, static_cast<channel_type>(chv));

                if (is_stop)
                  break;
              }

              if (is >> ch && ch != symbols<CharT>::round_bracket_close) {
                is.putback(ch);
                is.setstate(std::ios_base::failbit);
              }
            }
          }
        }
      }
    }
  }


  template <typename CharT, typename CharTraits, typename T, typename PixelTraits>
  void parse_rgb(std::basic_istream<CharT, CharTraits>& is, basic_color<T, PixelTraits>& c) {
    typedef basic_color<T, PixelTraits> color_type;
    typedef typename color_type::channel_type    channel_type;
    typedef std::basic_stringstream<CharT, CharTraits, std::allocator<CharT> > stringstream_type;

    CharT ch;
    if (is >> ch && ch != symbols<CharT>::r) {
      is.putback(ch);
      is.setstate(std::ios_base::failbit);
    }
    else if (!is.fail()) {
      if (is >> ch && ch != symbols<CharT>::g) {
        is.putback(ch);
        is.setstate(std::ios_base::failbit);
      }
      else if (!is.fail()) {
        if (is >> ch && ch != symbols<CharT>::b) {
          is.putback(ch);
          is.setstate(std::ios_base::failbit);
        }
        else if (!is.fail()) {
          if (is >> ch && ch != symbols<CharT>::round_bracket_open)
          {
            is.putback(ch);
            is.setstate(std::ios_base::failbit);
          }
          else if (!is.fail()) {
            const size_t data_len = 3;
            for (size_t i = 0; i != data_len; ++i)
            {
              unsigned int chv;
              bool is_stop = false;
              if (is >> std::ws >> chv >> ch && ch != symbols<CharT>::comma) {
                is.putback(ch);
                if (i < data_len - 1)
                  is.setstate(std::ios_base::failbit);

                is_stop = true;
              }

              assign_color_value(c, i + 1, static_cast<channel_type>(chv)); // i==0 --- Alpha

              if (is_stop)
                break;
            }

            if (is >> ch && ch != symbols<CharT>::round_bracket_close) {
              is.putback(ch);
              is.setstate(std::ios_base::failbit);
            }
          }
        }
      }
    }
  }


  template <typename CharT, typename CharTraits, typename T, typename PixelTraits>
  inline std::basic_istream<CharT, CharTraits>& operator>>(std::basic_istream<CharT, CharTraits>& is, basic_color<T, PixelTraits>& c) {
    typedef basic_color<T, PixelTraits> color_type;
    typedef typename color_type::channel_type    channel_type;
    typedef std::basic_stringstream<CharT, CharTraits, std::allocator<CharT> > stringstream_type;

    // Parse Formats:
    // * Color HEX                  -- #RRGGBB
    // * Color HEX with Alpha(AHEX) -- #AARRGGBB
    // * Color RGB                  -- rgb(r,g,b)
    // * Color RGB with Alpha(ARGB) -- argb(a,r,g,b)
    // OR
    // * We recognize one of the named colors. See: color_named.h

    CharT ch;
    if (is >> ch) {
      is.putback(ch);
      if (ch != symbols<CharT>::sharp && ch != symbols<CharT>::r && ch != symbols<CharT>::a) {
        //read_named_color<wpf_color_mapper>(is, c);
        is.setstate(std::ios_base::failbit);
      }
      else {
        if (ch == symbols<CharT>::sharp) {
          parse_hex(is, c);
        }
        else if (ch == symbols<CharT>::a) {
          parse_argb(is, c);
        }
        else if (ch == symbols<CharT>::r) {
          parse_rgb(is, c);
        }
        else {
          is.setstate(std::ios_base::failbit);
        }
      }
    }
    else {
      is.setstate(std::ios_base::failbit);
    }

    return is;
  }


} // namespace colors
