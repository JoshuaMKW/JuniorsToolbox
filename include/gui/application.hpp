#pragma once

#include <deps/glad/gl.h>

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_glfw.h>
#include <GLFW/glfw3.h>

#include "gui/context.hpp"

namespace Toolbox::UI {

class EditorApplication {
	GLFWwindow* mWindow;
	EditorContext* mContext;

	bool Execute(float deltaTime);

public:
	EditorApplication();
	virtual ~EditorApplication() {}

	bool Setup();
	void Run();
	bool Teardown();
};

}