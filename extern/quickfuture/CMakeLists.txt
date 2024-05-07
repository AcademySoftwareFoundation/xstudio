set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 COMPONENTS Core Quick REQUIRED)

FILE(GLOB_RECURSE SOURCES src/*.cpp src/*.h)

add_library(quickfuture STATIC ${SOURCES})
target_compile_definitions(quickfuture PUBLIC QUICK_FUTURE_BUILD_PLUGIN)

target_link_libraries(quickfuture PUBLIC Qt5::Core Qt5::Quick)
target_include_directories(quickfuture PUBLIC src)