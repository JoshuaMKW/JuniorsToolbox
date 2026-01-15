# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.25)

project(JuniorsToolbox VERSION 0.2.0)

# Require C++23 for stacktrace etc
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

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
    
    "src/gui/bootstrap/*.cpp"
    "include/gui/bootstrap/*.hpp"
    "src/gui/bootstrap/project/*.cpp"
    "include/gui/bootstrap/project/*.hpp"

    "src/gui/appmain/*.cpp"
    "include/gui/appmain/*.hpp"
    "src/gui/appmain/debugger/*.cpp"
    "include/gui/appmain/debugger/*.hpp"
    "src/gui/appmain/dolphin/*.cpp"
    "include/gui/appmain/dolphin/*.hpp"
    "src/gui/appmain/new_item/*.cpp"
    "include/gui/appmain/new_item/*.hpp"
    "src/gui/appmain/pad/*.cpp"
    "include/gui/appmain/pad/*.hpp"
    "src/gui/appmain/project/*.cpp"
    "include/gui/appmain/project/*.hpp"
    "src/gui/appmain/property/*.cpp"
    "include/gui/appmain/property/*.hpp"
    "src/gui/appmain/settings/*.cpp"
    "include/gui/appmain/settings/*.hpp"
    "src/gui/appmain/scene/*.cpp"
    "include/gui/appmain/scene/*.hpp"
    "src/gui/appmain/status/*.cpp"
    "include/gui/appmain/status/*.hpp"
    "src/gui/appmain/updater/*.cpp"
    "include/gui/appmain/updater/*.hpp"
    
    "src/gui/*.cpp"
    "include/gui/*.hpp"
    "src/gui/event/*.cpp"
    "include/gui/event/*.hpp"
    "src/gui/dragdrop/*.cpp"
    "src/gui/dragdrop/p_*.hpp"
    "include/gui/dragdrop/*.hpp"
    "src/gui/logging/*.cpp"
    "include/gui/logging/*.hpp"
    "src/gui/image/*.cpp"
    "include/gui/image/*.hpp"
    "src/gui/layer/*.cpp"
    "include/gui/layer/*.hpp"
    "src/gui/widget/*.cpp"
    "include/gui/widget/*.hpp"
    
    "src/github/*.cpp"
    "include/github/*.hpp"
    "src/image/*.cpp"
    "include/image/*.hpp"
    "src/model/*.cpp"
    "include/model/*.hpp"
    "src/objlib/*.cpp"
    "include/objlib/*.hpp"
    "src/objlib/helpers/*.cpp"
    "src/objlib/helpers/*.hpp"
    "src/pad/*.cpp"
    "include/pad/*.hpp"
    "src/platform/*.cpp"
    "include/platform/*.hpp"
    "src/project/*.cpp"
    "include/project/*.hpp"
    "src/rail/*.cpp"
    "include/rail/*.hpp"
    "src/rarc/*.cpp"
    "include/rarc/*.hpp"
    "src/resource/*.cpp"
    "include/resource/*.hpp"
    "src/scene/*.cpp"
    "include/scene/*.hpp"
    "src/window/*.cpp"
    "include/window/*.hpp"
    "src/objlib/meta/*.cpp"
    "include/objlib/meta/*.hpp"
    "src/watchdog/*.cpp"
    "include/watchdog/*.hpp"
    "include/io/*.hpp"
    "lib/libgcm/*.hpp"
    "lib/libgcm/*.cpp"
    "lib/librii/*.hpp"
    "lib/librii/*.cpp"
    "lib/imgui_markdown/*.hpp"
)


add_executable(JuniorsToolbox ${TOOLBOX_SRC})

target_compile_features(JuniorsToolbox PUBLIC cxx_std_23)

# set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(NFD_OVERRIDE_RECENT_WITH_DEFAULT ON CACHE BOOL "Force NFD to use default path" FORCE)

include(lib/imgui.cmake)
add_subdirectory(lib/J3DUltra)
add_subdirectory(lib/libbti)
add_subdirectory(lib/glfw)
add_subdirectory(lib/nativefiledialog-extended)
add_subdirectory(lib/netpp)

target_link_libraries(JuniorsToolbox PRIVATE netpp)

if(MSVC)
    # target_compile_options(j3dultra PRIVATE /w)
    target_compile_options(JuniorsToolbox PRIVATE /FS /MP /permissive- /volatile:iso /Zc:preprocessor /EHsc /Zc:__cplusplus /Zc:externConstexpr /Zc:throwingNew)
    target_compile_options(JuniorsToolbox PUBLIC /ZI)
    target_link_options(JuniorsToolbox PUBLIC /INCREMENTAL)
else()
    # target_compile_options(j3dultra PRIVATE -w)
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
find_package(ICU REQUIRED COMPONENTS uc i18n)
# find_package(Iconv REQUIRED)
find_package(git2 REQUIRED)
include_directories(${GIT2_INCLUDE_DIR})

if(CMAKE_COMPILER_IS_GNUCXX)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GTK REQUIRED gtk+-3.0)
  include_directories(${GTK_INCLUDE_DIRS})
  target_link_libraries(JuniorsToolbox PRIVATE j3dultra imgui glfw ${ICONV_LIBRARIES} tbb X11 GL ${GTK_LIBRARIES})
  if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 14)
    target_link_libraries(JuniorsToolbox PRIVATE stdc++exp)
  else()
    target_link_libraries(JuniorsToolbox PRIVATE stdc++_libbacktrace)
  endif()
else()
  message(STATUS "Linking to" ${GIT2_LIBRARIES})
  target_link_libraries(JuniorsToolbox PRIVATE j3dultra imgui glfw ICU::uc ICU::i18n ${GIT2_LIBRARIES})
#  target_link_libraries(JuniorsToolbox PRIVATE j3dultra imgui glfw Iconv::Iconv)
endif()

if (WIN32)
#  link this library so we can find Nahimic service
#  this is because it crashes window corner resizing :P
    target_link_libraries(JuniorsToolbox PRIVATE Advapi32 Comctl32)
endif()

target_compile_definitions(JuniorsToolbox PRIVATE TOOLBOX_VERSION_TAG=\"v${PROJECT_VERSION}\")
target_compile_definitions(JuniorsToolbox PRIVATE TOOLBOX_VERSION_MAJOR=${PROJECT_VERSION_MAJOR})
target_compile_definitions(JuniorsToolbox PRIVATE TOOLBOX_VERSION_MINOR=${PROJECT_VERSION_MINOR})
target_compile_definitions(JuniorsToolbox PRIVATE TOOLBOX_VERSION_PATCH=${PROJECT_VERSION_PATCH})
target_compile_definitions(JuniorsToolbox PRIVATE NOMINMAX IMGUI_ENABLE_FREETYPE IMGUI_USE_WCHAR32 IMGUI_DEFINE_MATH_OPERATORS GLM_ENABLE_EXPERIMENTAL _DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR)

target_include_directories(JuniorsToolbox PRIVATE
    "include"
    "src"
    "lib"
    "lib/imgui"
    "lib/imgui/backends"
    "lib/nlohmann"
    "lib/glfw"
    "lib/glm::glm"
    "lib/J3DUltra"
    "lib/libbti"
    "lib/libgcm"
    "lib/imgui_markdown"
    "lib/nativefiledialog-extended/src/include"
    "lib/netpp")
    
target_include_directories(JuniorsToolbox PRIVATE ${OPENSSL_INCLUDE_DIR})

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
