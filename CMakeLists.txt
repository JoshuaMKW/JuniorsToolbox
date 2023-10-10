# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

# Require C++20
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project(JuniorsToolbox)


file(GLOB TOOLBOX_SRC
    "lib/*.hpp"
    "lib/*.cpp"
    "src/*.cpp"
    "include/*.hpp"
    "include/*.h"
    "src/bmg/*.cpp"
    "include/bmg/*.hpp"
    "src/objlib/*.cpp"
    "include/objlib/*.hpp"
    "src/rail/*.cpp"
    "include/rail/*.hpp"
    "src/scene/*.cpp"
    "include/scene/*.hpp"
    "src/window/*.cpp"
    "include/window/*.hpp"
    "src/objlib/meta/*.cpp"
    "include/objlib/meta/*.hpp"
    "src/io/*.cpp"
    "src/gui/*.hpp"
    "src/gui/*.cpp"
    "include/io/*.hpp"
    "lib/ImGuiFileDialog/*.h"
    "lib/ImGuiFileDialog/*.cpp"
)


add_executable(JuniorsToolbox ${TOOLBOX_SRC})

include(lib/imgui.cmake)
add_subdirectory(lib/J3DUltra)
add_subdirectory(lib/glfw)

find_package(ICU 61.0 COMPONENTS uc i18n REQUIRED)

if(CMAKE_COMPILER_IS_GNUCXX)
  target_link_libraries(JuniorsToolbox PRIVATE j3dultra imgui glfw ICU::uc ICU::i18n stdc++_libbacktrace)
else()
  target_link_libraries(JuniorsToolbox PRIVATE j3dultra imgui glfw ICU::uc ICU::i18n)
endif()

target_include_directories(JuniorsToolbox PRIVATE "include" "lib/imgui" "lib/imgui/backends" "lib/nlohmann" "lib/glm::glm" "lib/J3DUltra" "lib/ImGuiFileDialog" "lib/glfw")


add_custom_command(TARGET JuniorsToolbox PRE_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                       ${CMAKE_SOURCE_DIR}/Templates/ $<TARGET_FILE_DIR:JuniorsToolbox>/Templates/)
