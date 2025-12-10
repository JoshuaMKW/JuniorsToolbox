find_package(freetype REQUIRED)

add_library(
	imgui
	STATIC
	${CMAKE_CURRENT_LIST_DIR}/imgui/imgui.cpp
	${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_demo.cpp
	${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_draw.cpp
	${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_widgets.cpp
  ${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_tables.cpp
  ${CMAKE_CURRENT_LIST_DIR}/imgui/backends/imgui_impl_glfw.cpp
  ${CMAKE_CURRENT_LIST_DIR}/imgui/backends/imgui_impl_opengl3.cpp
${CMAKE_CURRENT_LIST_DIR}/imgui/misc/freetype/imgui_freetype.cpp
)
target_compile_definitions( imgui PUBLIC IMGUI_ENABLE_FREETYPE IMGUI_USE_WCHAR32 )
target_include_directories( imgui PUBLIC ${CMAKE_CURRENT_LIST_DIR}/imgui ${CMAKE_CURRENT_LIST_DIR}/imgui/backends lib/glfw/include )
target_link_libraries( imgui PRIVATE nfd Freetype::Freetype )

set_target_properties( imgui PROPERTIES FOLDER "imgui" )
