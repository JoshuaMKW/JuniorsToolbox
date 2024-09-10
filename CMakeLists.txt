# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

# Require C++23 for stacktrace etc
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
    "src/bti/*.cpp"
    "include/bti/*.hpp"
    "src/core/*.cpp"
    "include/core/*.hpp"
    "src/core/application/*.cpp"
    "include/core/application/*.hpp"
    "src/core/event/*.cpp"
    "include/core/event/*.hpp"
    "src/core/input/*.cpp"
    "include/core/input/*.hpp"
    "src/core/mimedata/*.cpp"
    "include/core/mimedata/*.hpp"
    "src/core/keybind/*.cpp"
    "include/core/keybind/*.hpp"
    "src/core/time/*.cpp"
    "include/core/time/*.hpp"
    "src/dolphin/*.cpp"
    "include/dolphin/*.hpp"
    "src/dolphin/interpreter/*.cpp"
    "include/dolphin/interpreter/*.hpp"
    "src/dolphin/interpreter/instructions/*.cpp"
    "include/dolphin/interpreter/instructions/*.hpp"
    "src/game/*.cpp"
    "include/game/*.hpp"
    "src/gui/dolphin/*.cpp"
    "include/gui/dolphin/*.hpp"
    "src/gui/event/*.cpp"
    "include/gui/event/*.hpp"
    "src/gui/dragdrop/*.cpp"
    "include/gui/dragdrop/*.hpp"
    "src/gui/image/*.cpp"
    "include/gui/image/*.hpp"
    "src/gui/layer/*.cpp"
    "include/gui/layer/*.hpp"
    "src/gui/pad/*.cpp"
    "include/gui/pad/*.hpp"
    "src/gui/widget/*.cpp"
    "include/gui/widget/*.hpp"
    "src/image/*.cpp"
    "include/image/*.hpp"
    "src/objlib/*.cpp"
    "include/objlib/*.hpp"
    "src/pad/*.cpp"
    "include/pad/*.hpp"
    "src/platform/*.cpp"
    "include/platform/*.hpp"
    "src/rail/*.cpp"
    "include/rail/*.hpp"
    "src/rarc/*.cpp"
    "include/rarc/*.hpp"
    "src/scene/*.cpp"
    "include/scene/*.hpp"
    "src/window/*.cpp"
    "include/window/*.hpp"
    "src/objlib/meta/*.cpp"
    "include/objlib/meta/*.hpp"
    "src/io/*.cpp"
    "src/gui/*.cpp"
    "include/gui/*.hpp"
    "src/gui/logging/*.cpp"
    "include/gui/logging/*.hpp"
    "src/gui/settings/*.cpp"
    "include/gui/settings/*.hpp"
    "src/gui/scene/*.cpp"
    "include/gui/scene/*.hpp"
    "src/gui/property/*.cpp"
    "include/gui/property/*.hpp"
    "include/io/*.hpp"
    "lib/ImGuiFileDialog/*.h"
    "lib/ImGuiFileDialog/*.cpp"
)


add_executable(JuniorsToolbox ${TOOLBOX_SRC})

include(lib/imgui.cmake)
add_subdirectory(lib/J3DUltra)
add_subdirectory(lib/libbti)
add_subdirectory(lib/glfw)

if(MSVC)
    # target_compile_options(j3dultra PRIVATE /w)
    target_compile_options(JuniorsToolbox PRIVATE /FS /MP)
else()
    # target_compile_options(j3dultra PRIVATE -w)
endif()

# find_package(ICU 61.0 COMPONENTS uc i18n REQUIRED)
find_package(Iconv REQUIRED)

if(CMAKE_COMPILER_IS_GNUCXX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK REQUIRED gtk+-3.0)
    include_directories(${GTK_INCLUDE_DIRS})
    target_link_libraries(JuniorsToolbox PRIVATE j3dultra imgui glfw ${ICONV_LIBRARIES} tbb X11 GL ${GTK_LIBRARIES} glib)
  if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 14)
    target_link_libraries(JuniorsToolbox PRIVATE stdc++exp)
  else()
    target_link_libraries(JuniorsToolbox PRIVATE stdc++_libbacktrace)
  endif()
else()
#  target_link_libraries(JuniorsToolbox PRIVATE j3dultra imgui glfw ICU::uc ICU::i18n)
  target_link_libraries(JuniorsToolbox PRIVATE j3dultra imgui glfw Iconv::Iconv)
endif()

find_library(NFD_PATH libnfd.a REQUIRED PATHS lib/nativefiledialog/build/lib/Release/x64)
target_link_libraries(JuniorsToolbox PRIVATE ${NFD_PATH})

if (WIN32)
#  link this library so we can find Nahimic service
#  this is because it crashes window corner resizing :P
    target_link_libraries(JuniorsToolbox PRIVATE Advapi32)
endif()

target_compile_definitions(JuniorsToolbox PRIVATE NOMINMAX IMGUI_DEFINE_MATH_OPERATORS)

target_include_directories(JuniorsToolbox PRIVATE "include" "lib" "lib/imgui" "lib/imgui/backends" "lib/nlohmann" "lib/glm::glm" "lib/J3DUltra" "lib/libbti" "lib/ImGuiFileDialog" "lib/glfw" "lib/nativefiledialog/src/include")


add_custom_command(TARGET JuniorsToolbox PRE_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                       ${CMAKE_SOURCE_DIR}/Fonts/ $<TARGET_FILE_DIR:JuniorsToolbox>/Fonts/)
add_custom_command(TARGET JuniorsToolbox PRE_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                       ${CMAKE_SOURCE_DIR}/Images/ $<TARGET_FILE_DIR:JuniorsToolbox>/Images/)
add_custom_command(TARGET JuniorsToolbox PRE_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                       ${CMAKE_SOURCE_DIR}/Templates/ $<TARGET_FILE_DIR:JuniorsToolbox>/Templates/)
add_custom_command(TARGET JuniorsToolbox PRE_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                       ${CMAKE_SOURCE_DIR}/Themes/ $<TARGET_FILE_DIR:JuniorsToolbox>/Themes/)
