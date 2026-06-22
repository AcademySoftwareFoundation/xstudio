#include <stdlib.h>
#include <string>
#include <sstream>
#include <iostream>
#include "cpp-colors/colors.h"

using namespace std;
using namespace colors;

void print_color_value(const std::string& name, uint32_t value) {
  cout << hex << setfill(' ') << setw(10) << name << " = " << setw(8) << setfill('0') << value << endl;
}

int main(int argc, char* argv[]) {

  color c(wpf_colors::blue());

  print_color_value("bgra32", c.value());
  print_color_value("bgr32", c.value<pixel_format::bgr32>());
  print_color_value("rgba32", c.value<pixel_format::rgba32>());
  print_color_value("rgb32", c.value<pixel_format::rgb32>());
  print_color_value("bgr24", c.value<pixel_format::bgr24>());
  print_color_value("bgr565", c.value<pixel_format::bgr565>());
  print_color_value("bgr555", c.value<pixel_format::bgra5551>());

  return EXIT_SUCCESS;
}
