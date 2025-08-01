cmake_minimum_required(VERSION 3.0)

# install python package into lib/python directory
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} DESTINATION usr/lib/xstudio/python-plugins
    FILES_MATCHING PATTERN "*.py"
)
