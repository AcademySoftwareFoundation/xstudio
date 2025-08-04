#pragma once

#include <limits>
#include <cstdint>


namespace colors {

  // PixelFormat
  namespace pixel_format {
    enum pixel_format {
      unknown,            // Surface format is unknown

      indexed1,           // 1-bit Indexed. Specifies that the pixel format is 1 bit per pixel and that it uses indexed color. The color table therefore has two colors in it. 
      indexed4,           // 4-bit Indexed. Specifies that the format is 4 bits per pixel, indexed. 
      indexed8,           // 8-bit Indexed. Specifies that the format is 8 bits per pixel, indexed. The color table therefore has 256 colors in it. 
      gray16,             // 16-bit Grayscale. The pixel format is 16 bits per pixel. The color information specifies 65536 shades of gray. 

      bgr24,              // 24-bit RGB pixel format with 8 bits per channel.                                              r8g8b8
      color, /* bgra32 */ // 32-bit ARGB pixel format with alpha, using 8 bits per channel.                                a8r8g8b8
      bgr32,              // 32-bit RGB pixel format, where 8 bits are reserved for each color.                            x8r8g8b8
      bgr565,             // 16-bit RGB pixel format with 5 bits for red, 6 bits for green, and 5 bits for blue.           r5g6b5
      bgr555,             // 16-bit pixel format where 5 bits are reserved for each color.                                 x1r5g5b5
      bgra5551,           // 16-bit pixel format where 5 bits are reserved for each color and 1 bit is reserved for alpha. a1r5g5b5
      rgba32,             // 32-bit ABGR pixel format with alpha, using 8 bits per channel.                                a8b8g8r8
      rgb32,              // 32-bit BGR pixel format, where 8 bits are reserved for each color.                            x8b8g8r8

      bgrap32,            // PARGB32. Specifies that the format is 32 bits per pixel; 8 bits each are used for the alpha, red, green, and blue components. The red, green, and blue components are premultiplied, according to the alpha component. 
      bgrap64,            // PARGB64. Specifies that the format is 64 bits per pixel; 16 bits each are used for the alpha, red, green, and blue components. The red, green, and blue components are premultiplied according to the alpha component.

      bgr48,              // 48-bit RGB. Specifies that the format is 48 bits per pixel; 16 bits each are used for the red, green, and blue components. 
      bgra64,             // 64-bit ARGB. Specifies that the format is 64 bits per pixel; 16 bits each are used for the alpha, red, green, and blue components. 
    };
  }

  namespace pixel_format_flags {
    enum pixel_format_flags {
      has_alpha = 0x00000001,    // Format has the Alpha channel
      compressed = 0x00000002,    // Compressed format
      floating = 0x00000004,    // Floating point format
      depth = 0x00000008,    // Depth format (depth textures)
    };
  }


  // Traits of a pixel format
  template <size_t F>
  struct pixel_traits;


  // R8G8B8 (Bgr24)
  template <>
  struct pixel_traits<pixel_format::bgr24> {
    typedef uint8_t channel_type; // type to fit the channel value
    typedef uint32_t  pixel_type;   // Compact representation of a pixel

    // Format that traits describe
    static const pixel_format::pixel_format format = pixel_format::bgr24;

    // Tha name of the format
    static const char* const name() { return "R8G8B8 (Bgr24)"; }

    // The number of bytes one element takes (size per element)
    static const uint8_t size = 3;

    // A combination of one or more flags
    static const uint32_t flags = 0;

    // Bits per component (R, G, B, A)
    static const uint8_t bits_r = 8;
    static const uint8_t bits_g = 8;
    static const uint8_t bits_b = 8;
    static const uint8_t bits_a = 0;

    // Mask of a component (R, G, B, A)
    static const pixel_type mask_r = 0x00FF0000;
    static const pixel_type mask_g = 0x0000FF00;
    static const pixel_type mask_b = 0x000000FF;
    static const pixel_type mask_a = 0x00000000;

    // Shift of the component (R, G, B, A)
    static const pixel_type shift_r = 16;
    static const pixel_type shift_g = 8;
    static const pixel_type shift_b = 0;
    static const pixel_type shift_a = 0;
  };


  // A8R8G8B8 (Color; Bgra32)
  template <>
  struct pixel_traits<pixel_format::color> {
    typedef uint8_t channel_type; // type to fit the channel value
    typedef uint32_t  pixel_type;   // Compact representation of a pixel

    // Format that traits describe
    static const pixel_format::pixel_format format = pixel_format::color;

    // Tha name of the format
    static const char* const name() { return "A8R8G8B8 (Bgra32)"; }

    // The number of bytes one element takes (size per element)
    static const uint8_t size = 4;

    // A combination of one or more flags
    static const uint32_t flags = pixel_format_flags::has_alpha;

    // Bits per component (R, G, B, A)
    static const uint8_t bits_r = 8;
    static const uint8_t bits_g = 8;
    static const uint8_t bits_b = 8;
    static const uint8_t bits_a = 8;

    // Mask of a component (R, G, B, A)
    static const pixel_type mask_r = 0x00FF0000;
    static const pixel_type mask_g = 0x0000FF00;
    static const pixel_type mask_b = 0x000000FF;
    static const pixel_type mask_a = 0xFF000000;

    // Shift of the component (R, G, B, A)
    static const pixel_type shift_r = 16;
    static const pixel_type shift_g = 8;
    static const pixel_type shift_b = 0;
    static const pixel_type shift_a = 24;
  };


  // X8R8G8B8 (Bgr32)
  template <>
  struct pixel_traits<pixel_format::bgr32> {
    typedef uint8_t channel_type; // type to fit the channel value
    typedef uint32_t  pixel_type;   // Compact representation of a pixel

    // Format that traits describe
    static const pixel_format::pixel_format format = pixel_format::bgr32;

    // Tha name of the format
    static const char* const name() { return "X8R8G8B8 (Bgr32)"; }

    // The number of bytes one element takes (size per element)
    static const uint8_t size = 4;

    // A combination of one or more flags
    static const uint32_t flags = 0;

    // Bits per component (R, G, B, A)
    static const uint8_t bits_r = 8;
    static const uint8_t bits_g = 8;
    static const uint8_t bits_b = 8;
    static const uint8_t bits_a = 0;

    // Mask of a component (R, G, B, A)
    static const pixel_type mask_r = 0x00FF0000;
    static const pixel_type mask_g = 0x0000FF00;
    static const pixel_type mask_b = 0x000000FF;
    static const pixel_type mask_a = 0x00000000;

    // Shift of the component (R, G, B, A)
    static const pixel_type shift_r = 16;
    static const pixel_type shift_g = 8;
    static const pixel_type shift_b = 0;
    static const pixel_type shift_a = 0;
  };


  // A8B8G8R8 (Rgba32)
  template <>
  struct pixel_traits<pixel_format::rgba32> {
    typedef uint8_t channel_type; // type to fit the channel value
    typedef uint32_t  pixel_type;   // Compact representation of a pixel

    // Format that traits describe
    static const pixel_format::pixel_format format = pixel_format::rgba32;

    // Tha name of the format
    static const char* const name() { return "A8B8G8R8 (Rgba32)"; }

    // The number of bytes one element takes (size per element)
    static const uint8_t size = 4;

    // A combination of one or more flags
    static const uint32_t flags = pixel_format_flags::has_alpha;

    // Bits per component (R, G, B, A)
    static const uint8_t bits_r = 8;
    static const uint8_t bits_g = 8;
    static const uint8_t bits_b = 8;
    static const uint8_t bits_a = 8;

    // Mask of a component (R, G, B, A)
    static const pixel_type mask_r = 0x000000FF;
    static const pixel_type mask_g = 0x0000FF00;
    static const pixel_type mask_b = 0x00FF0000;
    static const pixel_type mask_a = 0xFF000000;

    // Shift of the component (R, G, B, A)
    static const pixel_type shift_r = 0;
    static const pixel_type shift_g = 8;
    static const pixel_type shift_b = 16;
    static const pixel_type shift_a = 24;
  };


  // X8B8G8R8 (Rgb32)
  template <>
  struct pixel_traits<pixel_format::rgb32> {
    typedef uint8_t channel_type; // type to fit the channel value
    typedef uint32_t  pixel_type;   // Compact representation of a pixel

    // Format that traits describe
    static const pixel_format::pixel_format format = pixel_format::rgb32;

    // Tha name of the format
    static const char* const name() { return "X8R8G8B8 (Rgb32)"; }

    // The number of bytes one element takes (size per element)
    static const uint8_t size = 4;

    // A combination of one or more flags
    static const uint32_t flags = 0;

    // Bits per component (R, G, B, A)
    static const uint8_t bits_r = 8;
    static const uint8_t bits_g = 8;
    static const uint8_t bits_b = 8;
    static const uint8_t bits_a = 8;

    // Mask of a component (R, G, B, A)
    static const pixel_type mask_r = 0x000000FF;
    static const pixel_type mask_g = 0x0000FF00;
    static const pixel_type mask_b = 0x00FF0000;
    static const pixel_type mask_a = 0x00000000;

    // Shift of the component (R, G, B, A)
    static const pixel_type shift_r = 0;
    static const pixel_type shift_g = 8;
    static const pixel_type shift_b = 16;
    static const pixel_type shift_a = 0;
  };


  // R5G6B5 (Bgr565)
  template <>
  struct pixel_traits<pixel_format::bgr565> {
    typedef uint8_t  channel_type; // type to fit the channel value
    typedef uint16_t pixel_type;   // Compact representation of a pixel

    // Format that traits describe
    static const pixel_format::pixel_format format = pixel_format::bgr565;

    // Tha name of the format
    static const char* const name() { return "R5G6B5 (Bgr565)"; }

    // The number of bytes one element takes (size per element)
    static const uint8_t size = 2;

    // A combination of one or more flags
    static const uint32_t flags = 0;

    // Bits per component (R, G, B, A)
    static const uint8_t bits_r = 5;
    static const uint8_t bits_g = 6;
    static const uint8_t bits_b = 5;
    static const uint8_t bits_a = 0;

    // Mask of a component (R, G, B, A)
    static const pixel_type mask_r = 0xF800;
    static const pixel_type mask_g = 0x07E0;
    static const pixel_type mask_b = 0x001F;
    static const pixel_type mask_a = 0x0000;

    // Shift of the component (R, G, B, A)
    static const pixel_type shift_r = 11;
    static const pixel_type shift_g = 5;
    static const pixel_type shift_b = 0;
    static const pixel_type shift_a = 0;
  };


  // X1R5G5B5 (Bgr555)
  template <>
  struct pixel_traits<pixel_format::bgr555> {
    typedef uint8_t  channel_type; // type to fit the channel value
    typedef uint16_t pixel_type;   // Compact representation of a pixel

    // Format that traits describe
    static const pixel_format::pixel_format format = pixel_format::bgr555;

    // Tha name of the format
    static const char* const name() { return "X1R5G5B5 (Bgr555)"; }

    // The number of bytes one element takes (size per element)
    static const uint8_t size = 2;

    // A combination of one or more flags
    static const uint32_t flags = 0;

    // Bits per component (R, G, B, A)
    static const uint8_t bits_r = 5;
    static const uint8_t bits_g = 5;
    static const uint8_t bits_b = 5;
    static const uint8_t bits_a = 0;

    // Mask of a component (R, G, B, A)
    static const pixel_type mask_r = 0x7C00;
    static const pixel_type mask_g = 0x03E0;
    static const pixel_type mask_b = 0x001F;
    static const pixel_type mask_a = 0x0000;

    // Shift of the component (R, G, B, A)
    static const pixel_type shift_r = 10;
    static const pixel_type shift_g = 5;
    static const pixel_type shift_b = 0;
    static const pixel_type shift_a = 0;
  };


  // A1R5G5B5 (Bgra5551)
  template <>
  struct pixel_traits<pixel_format::bgra5551> {
    typedef uint8_t  channel_type; // type to fit the channel value
    typedef uint16_t pixel_type;   // Compact representation of a pixel

    // Format that traits describe
    static const pixel_format::pixel_format format = pixel_format::bgra5551;

    // Tha name of the format
    static const char* const name() { return "A1R5G5B5 (Bgra5551)"; }

    // The number of bytes one element takes (size per element)
    static const uint8_t size = 2;

    // A combination of one or more flags
    static const uint32_t flags = pixel_format_flags::has_alpha;

    // Bits per component (R, G, B, A)
    static const uint8_t bits_r = 5;
    static const uint8_t bits_g = 5;
    static const uint8_t bits_b = 5;
    static const uint8_t bits_a = 1;

    // Mask of a component (R, G, B, A)
    static const pixel_type mask_r = 0x7C00;
    static const pixel_type mask_g = 0x03E0;
    static const pixel_type mask_b = 0x001F;
    static const pixel_type mask_a = 0x8000;

    // Shift of the component (R, G, B, A)
    static const pixel_type shift_r = 10;
    static const pixel_type shift_g = 5;
    static const pixel_type shift_b = 0;
    static const pixel_type shift_a = 15;
  };


  typedef pixel_traits<pixel_format::color>    bgra32_traits;
  typedef pixel_traits<pixel_format::bgr32>    bgr32_traits;
  typedef pixel_traits<pixel_format::rgba32>   rgba32_traits;
  typedef pixel_traits<pixel_format::rgb32>    rgb32_traits;
  typedef pixel_traits<pixel_format::bgr24>    bgr24_traits;
  typedef pixel_traits<pixel_format::bgr565>   bgr565_traits;
  typedef pixel_traits<pixel_format::bgr555>   bgr555_traits;
  typedef pixel_traits<pixel_format::bgra5551> bgra5551_traits;


} // namespace colors
