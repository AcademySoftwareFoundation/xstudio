# cpp-colors [![Build Status](https://travis-ci.org/gchudnov/cpp-colors.svg?branch=master)](https://travis-ci.org/gchudnov/cpp-colors)

A C++ header-only color format & conversion library.
* Parsing and formatting colors: #aarrggbb, argb(a, r, g, b) & rgb(r, g, b)
* Checking whether the given color is light or dark.
* parsing named colors: x11, wpf & .net naming schemes
* conversion between: bgr24, bgra32, bgr32, bgr565, bgr555, bgra5551, rgba32 and rgb32 pixel formats


### Directories

* **build** - directory for tests & examples
* **examples** - examples source code
* **include** - the source code of the library
* **test** - test source code

### More information

* [Wiki](https://github.com/gchudnov/cpp-colors/wiki)

### Tested compilers

* Linux (x86/64)
   * GCC 4.8, Boost 1.54
   * Clang 3.4, Boost 1.54
* Windows (x86/64)
   * MSVC 14, Boost 1.57

### Installation

This is a header only library, in order to use it make the cpp-colors-`include` directory available to your project and include the header file in your source code:

```c++
#include "cpp-colors/colors.h"
```

### Building Tests & Examples

This project tests and examples use the Cross-platform Make ([CMake](http://www.cmake.org/)) build system.
Tests depend on [Google Test Framework](https://code.google.com/p/googletest/). gtest-1.7.0 is recommended.

#### Linux

The recommended way is to create 'out of source' build:

```bash
cd cpp-colors/build
cmake ..
make
```

#### Windows

Visual Studio:
 
    Follow the directions at the link for running CMake on Windows:
    http://www.cmake.org/runningcmake/
    
    NOTE: Select the "build" folder as the location to build the binaries.


### Deleting all files Make & CMake created 

```
make clean-all
```

### Contact

[Grigoriy Chudnov] (mailto:g.chudnov@gmail.com)

### License

Distributed under the [The MIT License (MIT)](https://github.com/gchudnov/cpp-colors/blob/master/LICENSE).

