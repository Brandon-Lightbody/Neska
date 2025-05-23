cmake_minimum_required(VERSION 3.20)
project(NeskaRoot LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SDL3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(fmt REQUIRED)

add_subdirectory(Neska)

target_link_libraries(Neska PRIVATE fmt::fmt)
target_link_libraries(Neska PRIVATE SDL3::SDL3)

target_compile_options(Neska PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
