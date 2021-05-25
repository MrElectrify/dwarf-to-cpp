cmake_minimum_required(VERSION 3.15)

project(DWARFToCPP)

set(CMAKE_CXX_STANDARD 20)

include(cmake/CompilerWarnings.cmake)

add_subdirectory(extern)
add_subdirectory(src)
add_subdirectory(DWARFToCPP)