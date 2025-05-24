cmake_minimum_required(VERSION 3.20)
project(NeskaRoot LANGUAGES C CXX)

set(BUILD_SHARED_LIBS OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SDL3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

set(QUILL_BUILD_SHARED_LIBS OFF)
find_package(quill CONFIG REQUIRED)

add_subdirectory(Neska)

target_link_libraries(Neska PRIVATE SDL3::SDL3)
target_link_libraries(Neska PRIVATE quill::quill)

if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

