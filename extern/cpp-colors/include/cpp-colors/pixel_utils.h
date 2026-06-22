#pragma once

#include <cassert>
#include <cstdint>
#include "pixel_format.h"
#include "color_error.h"

namespace colors {

    namespace detail {

      // Returns TRUE if the format has alpha channel
      template <size_t F>
      inline bool has_alpha() {
        return ((pixel_traits<F>::flags & pixel_format_flags::has_alpha) > 0);
      }

      // Returns TRUE of the format compressed
      template <size_t F>
      inline bool is_compressed() {
        return ((pixel_traits<F>::flags & pixel_format_flags::compressed) > 0);
      }

    } // namespace detail



    // Stride is the count of bytes between scanlines.
    // Generally speaking, the bits that make up the pixels of a bitmap are packed into rows.
    // A single row should be long enough to store one row of the bitmap's pixels.
    // The stride is the length of a row measured in bytes, rounded up to the nearest DWORD (4 bytes).
    // This allows bitmaps with fewer than 32 bits per pixel (bpp) to consume less memory while still
    // providing good performance. You can use the following function to calculate the stride for a given bitmap.
    //
    // width     - image width in pixels
    // bit_count - bits per pixel
    //
    inline uint32_t get_stride(uint32_t width, uint32_t bit_count) {
      assert(bit_count % 8 == 0);

      const uint32_t byteCount = bit_count / 8;
      const uint32_t stride = (width * byteCount + 3) & ~3;

      assert(0 == stride % 4);

      return stride;
    }


    // converts Value from the Source to Destination bits representstion
    template <typename T, typename U>
    inline T convert_bpc(T value, U srcBPC, U dstBPC) {
      T result;
      if (dstBPC < srcBPC) {
        result = static_cast<T>(value >> (srcBPC - dstBPC));
      }
      else if (srcBPC < dstBPC) {
        if (value == 0) {
          result = static_cast<T>(0);
        }
        else if (value == ((static_cast<T>(1) << srcBPC) - 1)) {
          result = static_cast<T>((1 << dstBPC) - 1);
        }
        else {
          result = static_cast<T>(value * (1 << dstBPC) / ((1 << srcBPC) - 1));
        }
      }
      else {
        result = static_cast<T>(value);
      }

      return result;
    }


    // Gets the size of a pixel (in bytes) for a given pixel format
    inline uint32_t get_pixel_size(pixel_format::pixel_format pft) {
      switch (pft)
      {
      case pixel_format::color:
        return bgra32_traits::size;

      case pixel_format::bgr32:
        return bgr32_traits::size;

      case pixel_format::rgba32:
        return rgba32_traits::size;

      case pixel_format::rgb32:
        return rgb32_traits::size;

      case pixel_format::bgr24:
        return bgr24_traits::size;

      case pixel_format::bgr565:
        return bgr565_traits::size;

      case pixel_format::bgr555:
        return bgr555_traits::size;

      case pixel_format::bgra5551:
        return bgra5551_traits::size;

      default:
        assert(false && "Cannot get the size of a pixel.");
        throw color_error("Cannot get the size of a pixel.");
      }
    }


    inline unsigned int get_alpha_mask(pixel_format::pixel_format pft) {
      switch (pft) {
      case pixel_format::color:
        return bgra32_traits::mask_a;

      case pixel_format::bgr32:
        return bgr32_traits::mask_a;

      case pixel_format::rgba32:
        return rgba32_traits::mask_a;

      case pixel_format::rgb32:
        return rgb32_traits::mask_a;

      case pixel_format::bgr24:
        return bgr24_traits::mask_a;

      case pixel_format::bgr565:
        return bgr565_traits::mask_a;

      case pixel_format::bgr555:
        return bgr555_traits::mask_a;

      case pixel_format::bgra5551:
        return bgra5551_traits::mask_a;

      default:
        assert(false && "Cannot retreive the pixel alpha mask.");
        throw color_error("Cannot retreive the pixel alpha mask.");
      }
    }


    inline unsigned int get_red_mask(pixel_format::pixel_format pft) {
      switch (pft) {
      case pixel_format::color:
        return bgra32_traits::mask_r;

      case pixel_format::bgr32:
        return bgr32_traits::mask_r;

      case pixel_format::rgba32:
        return rgba32_traits::mask_r;

      case pixel_format::rgb32:
        return rgb32_traits::mask_r;

      case pixel_format::bgr24:
        return bgr24_traits::mask_r;

      case pixel_format::bgr565:
        return bgr565_traits::mask_r;

      case pixel_format::bgr555:
        return bgr555_traits::mask_r;

      case pixel_format::bgra5551:
        return bgra5551_traits::mask_r;

      default:
        assert(false && "Cannot retreive the pixel red mask.");
        throw color_error("Cannot retreive the pixel red mask.");
      }
    }


    inline unsigned int get_green_mask(pixel_format::pixel_format pft) {
      switch (pft) {
      case pixel_format::color:
        return bgra32_traits::mask_g;

      case pixel_format::bgr32:
        return bgr32_traits::mask_g;

      case pixel_format::rgba32:
        return rgba32_traits::mask_g;

      case pixel_format::rgb32:
        return rgb32_traits::mask_g;

      case pixel_format::bgr24:
        return bgr24_traits::mask_g;

      case pixel_format::bgr565:
        return bgr565_traits::mask_g;

      case pixel_format::bgr555:
        return bgr555_traits::mask_g;

      case pixel_format::bgra5551:
        return bgra5551_traits::mask_g;

      default:
        assert(false && "Cannot retreive the pixel green mask.");
        throw color_error("Cannot retreive the pixel green mask.");
      }
    }


    inline unsigned int get_blue_mask(pixel_format::pixel_format pft) {
      switch (pft) {
      case pixel_format::color:
        return bgra32_traits::mask_b;

      case pixel_format::bgr32:
        return bgr32_traits::mask_b;

      case pixel_format::rgba32:
        return rgba32_traits::mask_b;

      case pixel_format::rgb32:
        return rgb32_traits::mask_b;

      case pixel_format::bgr24:
        return bgr24_traits::mask_b;

      case pixel_format::bgr565:
        return bgr565_traits::mask_b;

      case pixel_format::bgr555:
        return bgr555_traits::mask_b;

      case pixel_format::bgra5551:
        return bgra5551_traits::mask_b;

      default:
        assert(false && "Cannot retreive the pixel blue mask.");
        throw color_error("Cannot retreive the pixel blue mask.");
      }
    }


    inline unsigned int get_alpha_shift(pixel_format::pixel_format pft) {
      switch (pft) {
      case pixel_format::color:
        return bgra32_traits::shift_a;

      case pixel_format::bgr32:
        return bgr32_traits::shift_a;

      case pixel_format::rgba32:
        return rgba32_traits::shift_a;

      case pixel_format::rgb32:
        return rgb32_traits::shift_a;

      case pixel_format::bgr24:
        return bgr24_traits::shift_a;

      case pixel_format::bgr565:
        return bgr565_traits::shift_a;

      case pixel_format::bgr555:
        return bgr555_traits::shift_a;

      case pixel_format::bgra5551:
        return bgra5551_traits::shift_a;

      default:
        assert(false && "Cannot retreive the pixel alpha shift.");
        throw color_error("Cannot retreive the pixel alpha shift.");
      }
    }


    inline unsigned int get_red_shift(pixel_format::pixel_format pft) {
      switch (pft) {
      case pixel_format::color:
        return bgra32_traits::shift_r;

      case pixel_format::bgr32:
        return bgr32_traits::shift_r;

      case pixel_format::rgba32:
        return rgba32_traits::shift_r;

      case pixel_format::rgb32:
        return rgb32_traits::shift_r;

      case pixel_format::bgr24:
        return bgr24_traits::shift_r;

      case pixel_format::bgr565:
        return bgr565_traits::shift_r;

      case pixel_format::bgr555:
        return bgr555_traits::shift_r;

      case pixel_format::bgra5551:
        return bgra5551_traits::shift_r;

      default:
        assert(false && "Cannot retreive the pixel red shift.");
        throw color_error("Cannot retreive the pixel red shift.");
      }
    }


    inline unsigned int get_green_shift(pixel_format::pixel_format pft) {
      switch (pft) {
      case pixel_format::color:
        return bgra32_traits::shift_g;

      case pixel_format::bgr32:
        return bgr32_traits::shift_g;

      case pixel_format::rgba32:
        return rgba32_traits::shift_g;

      case pixel_format::rgb32:
        return rgb32_traits::shift_g;

      case pixel_format::bgr24:
        return bgr24_traits::shift_g;

      case pixel_format::bgr565:
        return bgr565_traits::shift_g;

      case pixel_format::bgr555:
        return bgr555_traits::shift_g;

      case pixel_format::bgra5551:
        return bgra5551_traits::shift_g;

      default:
        assert(false && "Cannot retreive the pixel green shift.");
        throw color_error("Cannot retreive the pixel green shift.");
      }
    }


    inline unsigned int get_blue_shift(pixel_format::pixel_format pft) {
      switch (pft) {
      case pixel_format::color:
        return bgra32_traits::shift_b;

      case pixel_format::bgr32:
        return bgr32_traits::shift_b;

      case pixel_format::rgba32:
        return rgba32_traits::shift_b;

      case pixel_format::rgb32:
        return rgb32_traits::shift_b;

      case pixel_format::bgr24:
        return bgr24_traits::shift_b;

      case pixel_format::bgr565:
        return bgr565_traits::shift_b;

      case pixel_format::bgr555:
        return bgr555_traits::shift_b;

      case pixel_format::bgra5551:
        return bgra5551_traits::shift_b;

      default:
        assert(false && "Cannot retreive the pixel blue shift.");
        throw color_error("Cannot retreive the pixel blue shift.");
      }
    }


    inline unsigned int get_red_bits(pixel_format::pixel_format pft) {
      switch (pft) {
      case pixel_format::color:
        return bgra32_traits::bits_r;

      case pixel_format::bgr32:
        return bgr32_traits::bits_r;

      case pixel_format::rgba32:
        return rgba32_traits::bits_r;

      case pixel_format::rgb32:
        return rgb32_traits::bits_r;

      case pixel_format::bgr24:
        return bgr24_traits::bits_r;

      case pixel_format::bgr565:
        return bgr565_traits::bits_r;

      case pixel_format::bgr555:
        return bgr555_traits::bits_r;

      case pixel_format::bgra5551:
        return bgra5551_traits::bits_r;

      default:
        assert(false && "Cannot retreive the pixel red bits.");
        throw color_error("Cannot retreive the pixel red bits.");
      }
    }


    inline unsigned int get_green_bits(pixel_format::pixel_format pft) {
      switch (pft) {
      case pixel_format::color:
        return bgra32_traits::bits_g;

      case pixel_format::bgr32:
        return bgr32_traits::bits_g;

      case pixel_format::rgba32:
        return rgba32_traits::bits_g;

      case pixel_format::rgb32:
        return rgb32_traits::bits_g;

      case pixel_format::bgr24:
        return bgr24_traits::bits_g;

      case pixel_format::bgr565:
        return bgr565_traits::bits_g;

      case pixel_format::bgr555:
        return bgr555_traits::bits_g;

      case pixel_format::bgra5551:
        return bgra5551_traits::bits_g;

      default:
        assert(false && "Cannot retreive the pixel green bits.");
        throw color_error("Cannot retreive the pixel green bits.");
      }
    }


    inline unsigned int get_blue_bits(pixel_format::pixel_format pft) {
      switch (pft) {
      case pixel_format::color:
        return bgra32_traits::bits_b;

      case pixel_format::bgr32:
        return bgr32_traits::bits_b;

      case pixel_format::rgba32:
        return rgba32_traits::bits_b;

      case pixel_format::rgb32:
        return rgb32_traits::bits_b;

      case pixel_format::bgr24:
        return bgr24_traits::bits_b;

      case pixel_format::bgr565:
        return bgr565_traits::bits_b;

      case pixel_format::bgr555:
        return bgr555_traits::bits_b;

      case pixel_format::bgra5551:
        return bgra5551_traits::bits_b;

      default:
        assert(false && "Cannot retreive the pixel blue bits.");
        throw color_error("Cannot retreive the pixel blue bits.");
      }
    }


    inline unsigned int get_alpha_bits(pixel_format::pixel_format pft) {
      switch (pft) {
      case pixel_format::color:
        return bgra32_traits::bits_a;

      case pixel_format::bgr32:
        return bgr32_traits::bits_a;

      case pixel_format::rgba32:
        return rgba32_traits::bits_a;

      case pixel_format::rgb32:
        return rgb32_traits::bits_a;

      case pixel_format::bgr24:
        return bgr24_traits::bits_a;

      case pixel_format::bgr565:
        return bgr565_traits::bits_a;

      case pixel_format::bgr555:
        return bgr555_traits::bits_a;

      case pixel_format::bgra5551:
        return bgra5551_traits::bits_a;

      default:
        assert(false && "Cannot retreive the pixel alpha bits.");
        throw color_error("Cannot retreive the pixel alpha bits.");
      }
    }


    // Returns <true> if the pixel format is compressed, false otherwise
    inline bool is_compressed(pixel_format::pixel_format pft) {
      switch (pft) {
      case pixel_format::color:
        return detail::is_compressed<pixel_format::color>();

      case pixel_format::bgr32:
        return detail::is_compressed<pixel_format::bgr32>();

      case pixel_format::rgba32:
        return detail::is_compressed<pixel_format::rgba32>();

      case pixel_format::rgb32:
        return detail::is_compressed<pixel_format::rgb32>();

      case pixel_format::bgr24:
        return detail::is_compressed<pixel_format::bgr24>();

      case pixel_format::bgr565:
        return detail::is_compressed<pixel_format::bgr565>();

      case pixel_format::bgr555:
        return detail::is_compressed<pixel_format::bgr555>();

      case pixel_format::bgra5551:
        return detail::is_compressed<pixel_format::bgra5551>();

      default:
        assert(false && "Cannot determine whether the given pixel format is compressed.");
        throw color_error("Cannot determine whether the given pixel format is compressed.");
      }
    }

} // namespace colors
