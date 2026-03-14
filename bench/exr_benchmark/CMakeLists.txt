cmake_minimum_required(VERSION 3.28)
project(exr_benchmark LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenEXR CONFIG REQUIRED)
find_package(Imath CONFIG REQUIRED)

add_executable(exr_benchmark
    exr_benchmark.cpp
)

target_link_libraries(exr_benchmark PRIVATE
    OpenEXR::OpenEXR
    Imath::Imath
)

if(MSVC)
    target_compile_options(exr_benchmark PRIVATE /MP /W3)
    target_compile_definitions(exr_benchmark PRIVATE NOMINMAX _CRT_SECURE_NO_WARNINGS)
endif()
