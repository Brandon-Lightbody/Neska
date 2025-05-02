project(Neska LANGUAGES CXX)

file(GLOB_RECURSE NES_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

add_executable(Neska
  ${NES_SOURCES}
)

set_property(TARGET Neska PROPERTY VS_DEBUGGER_WORKING_DIRECTORY
             "${CMAKE_CURRENT_BINARY_DIR}")

target_include_directories(Neska PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(Neska PRIVATE
  SDL3::SDL3
  imgui::imgui
)

add_custom_command(
    TARGET Neska POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/roms"
        "$<TARGET_FILE_DIR:Neska>/roms"
)
