#pragma once

#include <deps/glad/gl.h>

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_glfw.h>
#include <GLFW/glfw3.h>

#include "gui/window.hpp"
#include "gui/scene/window.hpp"

namespace Toolbox::UI {

class MainApplication {
	GLFWwindow* m_render_window;
  std::vector<std::shared_ptr<IWindow>> m_windows;

	bool execute(float delta_time);

public:
	MainApplication();
	virtual ~MainApplication() {}

	bool setup();
	void run();
	bool teardown();
};

}