#include <gtest/gtest.h>
#include <cstdint>
#include <string>
#include <sstream>
#include <boost/lexical_cast.hpp>

#include "cpp-colors/colors.h"

using namespace std;
using namespace colors;


TEST(ColorFloat, CanBeCreatedFromWpfConstant) {
  colorF c1 = wpf_colors::brown();
  ASSERT_EQ(0xFFA52A2A, c1.value());
}


TEST(ColorInt, CanBeCreatedFromDotNetConstant) {
  color c2 = dotnet_colors::brown();
  ASSERT_EQ(0xFFA52A2A, c2.value());
}

TEST(ColorInt, CanBeCreatedFromX11Constant) {
  color c2 = x11_colors::brown();
  ASSERT_EQ(0xFFA52A2A, c2.value());
}


TEST(ColorInt, CanBeCreatedFromConstant) {
  color c2 = wpf_colors::brown();
  ASSERT_EQ(0xFFA52A2A, c2.value());
}

TEST(ColorInt, CanBeAssignedFromColorFloat) {
  color c1 = wpf_colors::brown();
  colorF c3 = c1;
  ASSERT_EQ(c3, c1);
}

TEST(ColorFloat, CanBeAssignedFromColorInt) {
  colorF c2 = wpf_colors::brown();
  color c4 = c2;
  ASSERT_EQ(c2, c4);
}

TEST(ColorARGB, CanBeAssignedFromColorFloat) {
  colorF c4 = wpf_colors::brown();
  color_abrg c5 = c4;
  ASSERT_EQ(c5, c4);
}

TEST(ColorRed, CanBeFetchedFromAConstant) {
  color cr = wpf_colors::red();
  ASSERT_EQ(0xFFFF0000, cr.value());
}

TEST(ColorGreen, CanBeFetchedFromAConstant) {
  color cr = wpf_colors::green();
  ASSERT_EQ(0xFF008000, cr.value());
}

TEST(ColorBlue, CanBeFetchedFromAConstant) {
  color cr = wpf_colors::blue();
  ASSERT_EQ(0xFF0000FF, cr.value());
}

TEST(ColorWhite, CanBeFetchedFromAConstant) {
  color cr = wpf_colors::white();
  ASSERT_EQ(0xFFFFFFFF, cr.value());
}

TEST(ColorBlack, CanBeFetchedFromAConstant) {
  color cr = wpf_colors::black();
  ASSERT_EQ(0xFF000000, cr.value());
}

TEST(Color, CanBeConverted) {
  color c(value_of(wpf_known_color::blue));

  ASSERT_EQ(0xFF0000FF, c.value()); // color, bgra32
  ASSERT_EQ(0x000000FF, c.value<pixel_format::bgr32>());
  ASSERT_EQ(0xFFFF0000, c.value<pixel_format::rgba32>());
  ASSERT_EQ(0x00FF0000, c.value<pixel_format::rgb32>());
  ASSERT_EQ(0x000000FF, c.value<pixel_format::bgr24>());
  ASSERT_EQ(0x0000001F, c.value<pixel_format::bgr565>());
  ASSERT_EQ(0x0000001F, c.value<pixel_format::bgr555>());
  ASSERT_EQ(0x0000801F, c.value<pixel_format::bgra5551>());

  colorF cf(c);
  ASSERT_DOUBLE_EQ(1.0, cf.a);
  ASSERT_DOUBLE_EQ(0.0, cf.r);
  ASSERT_DOUBLE_EQ(0.0, cf.g);
  ASSERT_DOUBLE_EQ(1.0, cf.b);

  c = color(128, 0, 0, 128);
  cf = c;
  ASSERT_NEAR(0.501, cf.a, 0.001);
  ASSERT_NEAR(0.0, cf.r, 0.001);
  ASSERT_NEAR(0.0, cf.g, 0.001);
  ASSERT_NEAR(0.501, cf.b, 0.001);
}

TEST(Colors, CanBeAdded) {
  color c1 = wpf_colors::black();
  color c2(0x00010101);

  color c3 = c1 + c2;
  ASSERT_EQ(0xFF010101, c3.value());
}

TEST(Color, Constants) {
  color c1 = wpf_colors::red();
  ASSERT_EQ(color(value_of(wpf_known_color::red)), c1);

  color c2 = wpf_colors::blue();
  ASSERT_EQ(color(value_of(wpf_known_color::blue)), c2);

  color c3 = wpf_colors::green();
  ASSERT_EQ(color(value_of(wpf_known_color::green)), c3);
}

TEST(Color, CanBeRepresentedAsHEXStringValue) {
  color c1(0x01020304);

  // HEX
  std::string s1 = to_hex_str<char>(c1);
  ASSERT_EQ("#020304", s1);

  std::wstring ws1 = to_hex_str<wchar_t>(c1);
  ASSERT_EQ(L"#020304", ws1);

  std::string s2 = to_ahex_str<char>(c1);
  ASSERT_EQ("#01020304", s2);

  std::wstring ws2 = to_ahex_str<wchar_t>(c1);
  ASSERT_EQ(L"#01020304", ws2);


  color c2(255, 32, 64, 128);
  std::string sb = to_ahex_str<char>(c2);
  ASSERT_EQ("#ff204080", sb);
}

TEST(Color, CanBeRepresentedAsRGBStringValue) {
  color c1(0x01020304);

  // RGB
  std::string s3 = to_rgb_str<char>(c1);
  ASSERT_EQ("rgb(2,3,4)", s3);

  std::wstring ws3 = to_rgb_str<wchar_t>(c1);
  ASSERT_EQ(L"rgb(2,3,4)", ws3);

  std::string s4 = to_argb_str<char>(c1);
  ASSERT_EQ("argb(1,2,3,4)", s4);

  std::wstring ws4 = to_argb_str<wchar_t>(c1);
  ASSERT_EQ(L"argb(1,2,3,4)", ws4);


  color c2(255, 32, 64, 128);
  std::string sa = to_argb_str<char>(c2);
  ASSERT_EQ("argb(255,32,64,128)", sa);
}

TEST(Color, CanBeSavedToAStream) {
  color c1(0x01020304);

  std::ostringstream oss;
  oss << c1;
  std::string clr_str2 = oss.str();

  ASSERT_FALSE(clr_str2.empty());
  ASSERT_EQ("argb(1,2,3,4)", clr_str2);
}

TEST(Color, CanBeCastToString) {
  color c1(0x01020304);

  std::string clr_str = boost::lexical_cast<std::string>(c1);
  ASSERT_FALSE(clr_str.empty());
  ASSERT_EQ("argb(1,2,3,4)", clr_str);
}

TEST(ColorFloat, CanBeRepresentedAsString) {

  colorF c1(0.2, 0.6, 0.8); // r,g,b
  color  c2(255, 51, 153, 204);

  std::string sa = to_argb_str<char>(c1);
  ASSERT_EQ("argb(255,51,153,204)", sa);
  ASSERT_EQ(c2, c1);
}

TEST(ColorString, CanBeConvetedToColor) {
  color c2(255, 32, 64, 128); // actual value

  // ARGB
  ASSERT_EQ(c2, boost::lexical_cast<color>("#ff204080"));

  // RGB
  ASSERT_EQ(c2, boost::lexical_cast<color>("#204080"));

  // ARGB
  ASSERT_EQ(c2, boost::lexical_cast<color>("argb(255,32,64,128)"));

  // RGB
  ASSERT_EQ(c2, boost::lexical_cast<color>("rgb(32,64,128)"));

  // RGB with spaces
  ASSERT_EQ(c2, boost::lexical_cast<color>("rgb(32, 64, 128)"));
}

TEST(ColorWide, CanBeConvetedToColor) {
  color c2(255, 32, 64, 128); // actual value

  // ARGB
  ASSERT_EQ(c2, boost::lexical_cast<color>(L"#ff204080"));

  // RGB
  ASSERT_EQ(c2, boost::lexical_cast<color>(L"#204080"));

  // ARGB
  ASSERT_EQ(c2, boost::lexical_cast<color>(L"argb(255,32,64,128)"));

  // RGB
  ASSERT_EQ(c2, boost::lexical_cast<color>(L"rgb(32,64,128)"));

  // RGB with spaces
  ASSERT_EQ(c2, boost::lexical_cast<color>(L"rgb(32, 64, 128)"));
}

TEST(Color, CanBeConvertedToStringAndBack) {
  color c1(1, 2, 3, 4);
  string s1 = boost::lexical_cast<string>(c1);

  color c2 = boost::lexical_cast<color>(s1);

  ASSERT_EQ(c1, c2);
}

TEST(ColorComponents, CanBeReadAndWrite) {
  color c(255, 51, 153, 204);

  // access
  color::value_type a = get<0>(c);
  color::value_type r = get<1>(c);
  color::value_type g = get<2>(c);
  color::value_type b = get<3>(c);

  ASSERT_EQ(255, a);
  ASSERT_EQ(51, r);
  ASSERT_EQ(153, g);
  ASSERT_EQ(204, b);

  // modify
  get<0>(c) = 100;
  get<1>(c) = 50;
  get<2>(c) = 25;
  get<3>(c) = 5;

  ASSERT_EQ(100, c.a);
  ASSERT_EQ(50, c.r);
  ASSERT_EQ(25, c.g);
  ASSERT_EQ(5, c.b);

  // bounds test
  // get<10>(c); // will issue the compile-time error: `Color index is out of range`
}

TEST(NamedColors, CanBeCompared) {
  ASSERT_EQ(wpf_known_color::green, wpf_named_color_converter::value("Green"));
  ASSERT_EQ(wpf_known_color::green, wpf_named_color_converter::value("green"));
  ASSERT_EQ(wpf_known_color::green, wpf_named_color_converter::value("GREEN"));
}

TEST(Color, NameCanBeFetched) {
  ASSERT_EQ(wpf_named_color::green(), wpf_named_color_converter::name(wpf_known_color::green));
  ASSERT_EQ(wpf_named_color::white(), wpf_named_color_converter::name(wpf_known_color::white));
}

TEST(Color, CanBeVerifiedAsANamedOne) {
  uint32_t invalid_value = 0x11223344;
  std::string invalid_name = "SuperGreen";

  ASSERT_FALSE(wpf_named_color_converter::is_named(invalid_value));
  ASSERT_FALSE(wpf_named_color_converter::is_named(invalid_name));
}

TEST(Color, CanNotBeParsedFromInvalidNamedValue) {
  try {
    uint32_t invalid_value = 0x11223344;
    std::string dummy = wpf_named_color_converter::name(invalid_value); (void)dummy;
    FAIL() << "Must throw 'invalid_argument' exception.";
  }
  catch (std::invalid_argument& /*ex*/) {
    /* no-op */
  }
}

TEST(Color, CanNotBeParsedFromInvalidName) {
  try {
    std::string invalid_name = "SuperGreen";
    auto dummy = wpf_named_color_converter::value(invalid_name); (void)dummy;
    FAIL() << "Must throw 'invalid_argument' exception.";
  }
  catch (std::invalid_argument& /*ex*/) {
    /* no-op */
  }
}

TEST(Color, CanBeWrittenAsAName) {
  std::ostringstream oss;
  color c1 = wpf_colors::yellow();
  write_named_color<wpf_color_mapper>(oss, c1);

  ASSERT_EQ(std::string(wpf_named_color::yellow()), oss.str());
};

TEST(Color, CanBeReadFromName) {
  color c2;
  std::istringstream iss(wpf_named_color::yellow());
  read_named_color<wpf_color_mapper>(iss, c2);

  ASSERT_EQ(value_of(wpf_known_color::yellow), c2.value());
}

TEST(Brightness, CanBeMeasured) {
  color c1;

  c1 = boost::lexical_cast<color>(L"#ffef4444");
  ASSERT_EQ(brightness_value::dark, brightness(c1));

  c1 = boost::lexical_cast<color>(L"#ff009f75");
  ASSERT_EQ(brightness_value::dark, brightness(c1));

  c1 = boost::lexical_cast<color>(L"#fffaa31b");
  ASSERT_EQ(brightness_value::light, brightness(c1));

  c1 = boost::lexical_cast<color>(L"#ff88c6ed");
  ASSERT_EQ(brightness_value::light, brightness(c1));

  c1 = boost::lexical_cast<color>(L"#fffff000");
  ASSERT_EQ(brightness_value::light, brightness(c1));

  c1 = boost::lexical_cast<color>(L"#ff394ba0");
  ASSERT_EQ(brightness_value::dark, brightness(c1));

  c1 = boost::lexical_cast<color>(L"#ff82c341");
  ASSERT_EQ(brightness_value::light, brightness(c1));

  c1 = boost::lexical_cast<color>(L"#ffd54799");
  ASSERT_EQ(brightness_value::dark, brightness(c1));
}