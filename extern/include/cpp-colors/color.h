#pragma once

#include <algorithm>
#include <stdexcept>
#include <type_traits>
#include <cstdint>
#include "pixel_format.h"


namespace colors {

  namespace {

    // BITS-PER-CHANNEL conversion

    // SrcBPC < DstBPC
    struct less_converter {
      template <uint8_t SrcBPC, uint8_t DstBPC, typename T>
      static T run(T value) {
        if (value == 0) {
          return static_cast<T>(0);
        }
        else if (value == ((static_cast<T>(1) << SrcBPC) - 1)) {
          return static_cast<T>((1 << DstBPC) - 1);
        }

        return static_cast<T>(value * (1 << DstBPC) / ((1 << SrcBPC) - 1));
      }
    };

    // SrcBPC > DstBPC
    struct greater_conveter {
      template <uint8_t SrcBPC, uint8_t DstBPC, typename T>
      static T run(T value) {
        return static_cast<T>(value >> (SrcBPC - DstBPC));
      }
    };

    // SrcBPC == DstBPC
    struct equal_converter {
      template <uint8_t SrcBPC, uint8_t DstBPC, typename T>
      static T run(T value) {
        return value;
      }
    };

    // SrcBPC != DstBPC
    struct not_equal_converter {
      template <uint8_t SrcBPC, uint8_t DstBPC, typename T>
      static T run(T value) {
        typedef typename std::conditional < SrcBPC < DstBPC, less_converter, greater_conveter>::type conv_type;
        return conv_type::template run<SrcBPC, DstBPC>(value);
      }
    };

    // converts Value from the Source to Destination bits representation
    // SrcBPC ? DstBPC
    struct converter {
      template <uint8_t SrcBPC, uint8_t DstBPC, typename T>
      static T run(T value) {
        typedef typename std::conditional<SrcBPC == DstBPC, equal_converter, not_equal_converter>::type conv_type;
        return conv_type::template run<SrcBPC, DstBPC>(value);
      }
    };


    // represents color channel information
    template <typename T, typename PixelTraits>
    struct element_traits;


    // T is BYTE
    template <typename PixelTraits>
    struct element_traits<uint8_t, PixelTraits> {
      typedef uint8_t value_type;
      typedef typename PixelTraits::pixel_type pixel_type;

      static value_type min_value() { return 0x0; }
      static value_type max_a() { return (1 << PixelTraits::bits_a) - 1; }

      // Alpha: { 0 -> transparent; 255 -> opaque }

      // Convert <T> color value to the Destination Channel Bits Value
      // dstBPC - Destination BitsPerChannel
      template <uint8_t SrcBPC, uint8_t DstBPC>
      static typename PixelTraits::channel_type to_channel_value(value_type value) {
        return static_cast<typename PixelTraits::channel_type>(converter::run<SrcBPC, DstBPC>(static_cast<pixel_type>(value)));
      }

      // Convert Source Channel Value into the <T>
      template <uint8_t SrcBPC, uint8_t DstBPC>
      static value_type from_channel_value(pixel_type value) {
        return static_cast<value_type>(converter::run<SrcBPC, DstBPC>(value));
      }
    };

    // T is FLOAT
    template <typename PixelTraits>
    struct element_traits<double, PixelTraits> {
      typedef double value_type;
      typedef typename PixelTraits::pixel_type pixel_type;

      static double min_value() { return 0.0; }
      static double max_value() { return 1.0; }
      static double max_a() { return element_traits::max_value(); }

      // Alpha: { 0.0 -> transparent; 1.0 -> opaque }

      // Convert <FLOAT> color value to the Destination Channel Bits Value
      // dstBPC - Destination BitsPerChannel
      template <uint8_t SrcBPC, uint8_t DstBPC>
      static typename PixelTraits::channel_type to_channel_value(value_type value) {
        if (value <= min_value()) {
          return static_cast<typename PixelTraits::channel_type>(0);
        }
        else if (value >= max_value()) {
          return static_cast<typename PixelTraits::channel_type>((1 << DstBPC) - 1);
        }
        else {
          return static_cast<typename PixelTraits::channel_type>(value * (1 << DstBPC));
        }
      }

      // Convert Source Channel Value into the <FLOAT>
      template <uint8_t SrcBPC, uint8_t DstBPC>
      static value_type from_channel_value(pixel_type value) {
        return static_cast<value_type>(static_cast<value_type>(value) / static_cast<value_type>((1 << SrcBPC) - 1));
      }
    };

  } // anonymous namespace



  // represents a color of a single pixel
  // T           -- might be an INTEGER or FLOAT type
  // PixelTraits -- the format of packed color
  template <typename T, typename PixelTraits = bgra32_traits>
  struct basic_color {
    typedef PixelTraits                    pixel_traits_type;
    typedef element_traits<T, PixelTraits> element_traits_type;

    typedef typename pixel_traits_type::channel_type channel_type;
    typedef typename pixel_traits_type::pixel_type   pixel_type;

    typedef T value_type;

    template <typename U, typename UT>
    friend struct basic_color;

    T  a;    // Alpha Component
    T  r;    // Red Component
    T  g;    // Green Component
    T  b;    // Blue Component

    // ctor
    basic_color()
      : a(element_traits_type::max_a()),
      r(element_traits_type::min_value()),
      g(element_traits_type::min_value()),
      b(element_traits_type::min_value()) {
      /* no-op */
    }

    basic_color(const T& alpha, const T& red, const T& green, const T& blue)
      : a(alpha),
      r(red),
      g(green),
      b(blue) {
      /* no-op */
    }

    basic_color(const T& red, const T& green, const T& blue)
      : a(element_traits_type::max_a()), // max value --> opaque
      r(red),
      g(green),
      b(blue) {
      /* no-op */
    }

    // constructs a color using the packed value
    basic_color(typename PixelTraits::pixel_type argb) {
      basic_color temp = basic_color::create_from_value<PixelTraits>(argb);
      std::swap(*this, temp);
    }

    template <typename U, typename UT>
    basic_color(const basic_color<U, UT>& other) {
      typename PixelTraits::pixel_type packed = other.template traits_value<PixelTraits>();
      basic_color temp = basic_color::create_from_value<PixelTraits>(packed);
      std::swap(*this, temp);
    }

    // Get the packed color value, converted to the destination pixel format
    template <size_t N>
    uint32_t value() const {
      typedef pixel_traits<N> dest_traits_type;
      return this->traits_value<dest_traits_type>();
    }

    // Get the packed value of the color
    typename PixelTraits::pixel_type value() const {
      return this->traits_value<PixelTraits>();
    }

  private:
    // Creates a basic_color object from the packed color value
    template <typename ST>
    static basic_color create_from_value(typename ST::pixel_type value) {
      typedef typename ST::pixel_type src_pixel_type;

      return basic_color(
        (element_traits_type::template from_channel_value<ST::bits_a, PixelTraits::bits_a>((value & ST::mask_a) >> ST::shift_a)),
        (element_traits_type::template from_channel_value<ST::bits_r, PixelTraits::bits_r>((value & ST::mask_r) >> ST::shift_r)),
        (element_traits_type::template from_channel_value<ST::bits_g, PixelTraits::bits_g>((value & ST::mask_g) >> ST::shift_g)),
        (element_traits_type::template from_channel_value<ST::bits_b, PixelTraits::bits_b>((value & ST::mask_b) >> ST::shift_b))
        );
    }

    // Gets the packed value of the color using traits type
    template <typename DT>
    typename DT::pixel_type traits_value() const {
      typedef typename DT::pixel_type   dest_pixel_type;
      typedef typename DT::channel_type dest_channel_type;

      dest_pixel_type value = (
        ((static_cast<dest_pixel_type>(element_traits_type::template to_channel_value<PixelTraits::bits_a, DT::bits_a>(a)) << DT::shift_a) & DT::mask_a) |
        ((static_cast<dest_pixel_type>(element_traits_type::template to_channel_value<PixelTraits::bits_r, DT::bits_r>(r)) << DT::shift_r) & DT::mask_r) |
        ((static_cast<dest_pixel_type>(element_traits_type::template to_channel_value<PixelTraits::bits_g, DT::bits_g>(g)) << DT::shift_g) & DT::mask_g) |
        ((static_cast<dest_pixel_type>(element_traits_type::template to_channel_value<PixelTraits::bits_b, DT::bits_b>(b)) << DT::shift_b) & DT::mask_b)
        );
      return value;
    }

  public:
    // add
    basic_color& operator +=(const basic_color& other) {
      this->r += other.r;
      this->g += other.g;
      this->b += other.b;
      this->a += other.a;
      return *this;
    }

    // subsctract
    basic_color& operator -=(const basic_color& other) {
      this->r -= other.r;
      this->g -= other.g;
      this->b -= other.b;
      this->a -= other.a;
      return *this;
    }

    // modulate
    basic_color& operator *=(const basic_color& other) {
      this->r *= other.r;
      this->g *= other.g;
      this->b *= other.b;
      this->a *= other.a;
      return *this;
    }

    // scale
    basic_color& operator *=(const T& scalar) {
      this->r *= scalar;
      this->g *= scalar;
      this->b *= scalar;
      this->a *= scalar;
      return *this;
    }

    template <typename U>
    basic_color& operator *=(const U& scalar) {
      this->r *= scalar;
      this->g *= scalar;
      this->b *= scalar;
      this->a *= scalar;
      return *this;
    }

    // modulate
    basic_color& operator /=(const basic_color& other) {
      this->r /= other.r;
      this->g /= other.g;
      this->b /= other.b;
      this->a /= other.a;
      return *this;
    }

    // scale
    basic_color& operator /=(const T& scalar) {
      this->r /= scalar;
      this->g /= scalar;
      this->b /= scalar;
      this->a /= scalar;
      return *this;
    }

    template <typename U>
    basic_color& operator /=(const U& scalar) {
      this->r /= scalar;
      this->g /= scalar;
      this->b /= scalar;
      this->a /= scalar;
      return *this;
    }
  };



  template <typename T, typename PixelTraits>
  bool operator ==(const basic_color<T, PixelTraits>& lhs, const basic_color<T, PixelTraits>& rhs) {
    return (lhs.a == rhs.a &&
      lhs.r == rhs.r &&
      lhs.g == rhs.g &&
      lhs.b == rhs.b
      );
  }

  template <typename T, typename PixelTraits>
  bool operator !=(const basic_color<T, PixelTraits>& lhs, const basic_color<T, PixelTraits>& rhs) {
    return !(lhs == rhs);
  }



  template <typename T, typename U, typename PT, typename UT>
  bool operator ==(const basic_color<T, PT>& lhs, const basic_color<U, UT>& rhs) {
    basic_color<T, PT> other(rhs);
    return lhs == other;
  }

  template <typename T, typename U, typename PT, typename UT>
  bool operator !=(const basic_color<T, PT>& lhs, const basic_color<U, UT>& rhs) {
    return !(lhs == rhs);
  }



  template <typename T, typename PixelTraits>
  basic_color<T, PixelTraits> operator +(const basic_color<T, PixelTraits>& lhs, const basic_color<T, PixelTraits>& rhs) {
    basic_color<T, PixelTraits> result(lhs);
    result += rhs;
    return result;
  }

  template <typename T, typename PixelTraits>
  basic_color<T, PixelTraits> operator -(const basic_color<T, PixelTraits>& lhs, const basic_color<T, PixelTraits>& rhs) {
    basic_color<T, PixelTraits> result(lhs);
    result -= rhs;
    return result;
  }

  template <typename T, typename PixelTraits>
  basic_color<T, PixelTraits> operator *(const basic_color<T, PixelTraits>& lhs, const basic_color<T, PixelTraits>& rhs) {
    basic_color<T, PixelTraits> result(lhs);
    result *= rhs;
    return result;
  }

  template <typename T, typename PixelTraits>
  basic_color<T, PixelTraits> operator *(const T& lhs, const basic_color<T, PixelTraits>& rhs) {
    basic_color<T, PixelTraits> result(rhs);
    result *= lhs;
    return result;
  }

  template <typename T, typename PixelTraits>
  basic_color<T, PixelTraits> operator *(const basic_color<T, PixelTraits>& lhs, const T& rhs) {
    basic_color<T, PixelTraits> result(lhs);
    result *= rhs;
    return result;
  }

  template <typename U, typename T, typename PixelTraits>
  basic_color<T, PixelTraits> operator *(const U& lhs, const basic_color<T, PixelTraits>& rhs) {
    basic_color<T, PixelTraits> result(rhs);
    result *= lhs;
    return result;
  }

  template <typename U, typename T, typename PixelTraits>
  basic_color<T, PixelTraits> operator *(const basic_color<T, PixelTraits>& lhs, const U& rhs) {
    basic_color<T, PixelTraits> result(lhs);
    result *= rhs;
    return result;
  }

  template <typename T, typename PixelTraits>
  basic_color<T, PixelTraits> operator /(const basic_color<T, PixelTraits>& lhs, const basic_color<T, PixelTraits>& rhs) {
    basic_color<T, PixelTraits> result(lhs);
    result /= rhs;
    return result;
  }

  template <typename U, typename T, typename PixelTraits>
  basic_color<T, PixelTraits> operator /(const basic_color<T, PixelTraits>& lhs, const U& rhs) {
    basic_color<T, PixelTraits> result(lhs);
    result /= rhs;
    return result;
  }

  // Get the color-component by index
  template <size_t index, typename T, typename PixelTraits>
  inline T& get(basic_color<T, PixelTraits>& c) {
    typedef T* pointer_type;
    typedef typename PixelTraits::pixel_type pixel_type;
    typedef std::pair<pixel_type, pointer_type> pair_type; // shift, element

    static_assert(index < PixelTraits::size, "Color index is out of range.");

    pixel_type shift_a = PixelTraits::shift_a;
    pixel_type shift_r = PixelTraits::shift_r;
    pixel_type shift_g = PixelTraits::shift_g;
    pixel_type shift_b = PixelTraits::shift_b;

    pair_type arr[PixelTraits::size] = {
      pair_type(shift_a, &c.a),
      pair_type(shift_r, &c.r),
      pair_type(shift_g, &c.g),
      pair_type(shift_b, &c.b)
    };
    std::sort(arr, arr + PixelTraits::size, [](const pair_type& lhs, const pair_type& rhs) -> bool { return lhs.first > rhs.first; });

    return *(arr[index].second);
  }

  typedef basic_color<uint8_t> color;       /* color_argb */
  typedef basic_color<double>  colorF;      /* color_argbF */

  typedef basic_color<uint8_t, rgba32_traits> color_abrg;

} // namespace colors
