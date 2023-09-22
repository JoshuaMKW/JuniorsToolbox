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
    "src/*.cpp"
    "include/*.hpp"
    "include/*.h"
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
    "include/io/*.hpp"
    "include/ImGuiFileDialog/*.h"
)


add_executable(JuniorsToolbox ${TOOLBOX_SRC})
add_subdirectory(lib/j3dultra)
target_link_libraries(JuniorsToolbox PRIVATE j3dultra)
target_include_directories(JuniorsToolbox PRIVATE "include" "lib/nlohmann" "lib/glm::glm", "lib/j3dultra")


add_custom_command(TARGET JuniorsToolbox PRE_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                       ${CMAKE_SOURCE_DIR}/Templates/ $<TARGET_FILE_DIR:JuniorsToolbox>/Templates/)