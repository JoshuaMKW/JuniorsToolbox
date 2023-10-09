#include "gui/application.hpp"
#include "gui/input.hpp"
#include "gui/util.hpp"

#include <J3D/J3DUniformBufferObject.hpp>

#include <string>
#include <iostream>


namespace Toolbox::UI {

void EditorApplication::Run() {
	Clock::time_point lastFrameTime, thisFrameTime;

	while (true) {
		lastFrameTime = thisFrameTime;
		thisFrameTime = Util::GetTime();

		if (!Execute(Util::GetDeltaTime(lastFrameTime, thisFrameTime)))
			break;
	}
}

void DealWithGLErrors(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
	std::cout << "GL CALLBACK: " << message << std::endl;
}

EditorApplication::EditorApplication() {
	mWindow = nullptr;
	mContext = nullptr;
}

bool EditorApplication::Setup() {
	// Initialize GLFW
	if (!glfwInit())
		return false;

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
	glfwWindowHint(GLFW_DEPTH_BITS, 32);
	glfwWindowHint(GLFW_SAMPLES, 4);

	mWindow = glfwCreateWindow(1280, 720, "Junior's Toolbox", nullptr, nullptr);
	if (mWindow == nullptr) {
		glfwTerminate();
		return false;
	}

	glfwSetKeyCallback(mWindow, Input::GLFWKeyCallback);
	glfwSetCursorPosCallback(mWindow, Input::GLFWMousePositionCallback);
	glfwSetMouseButtonCallback(mWindow, Input::GLFWMouseButtonCallback);
	glfwSetScrollCallback(mWindow, Input::GLFWMouseScrollCallback);

	glfwMakeContextCurrent(mWindow);
	gladLoadGL(glfwGetProcAddress);
	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glfwSwapInterval(1);

	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(DealWithGLErrors, nullptr);

	// Initialize imgui
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_None;

	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(mWindow, true);
	ImGui_ImplOpenGL3_Init("#version 150");

	//glEnable(GL_MULTISAMPLE);

	
	// Create viewer context
	mContext = new EditorContext();


	return true;
}

bool EditorApplication::Teardown() {
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	
	glfwDestroyWindow(mWindow);
	glfwTerminate();

	J3DUniformBufferObject::DestroyUBO();

	delete mContext;

	return true;
}

bool EditorApplication::Execute(float deltaTime) {
	// Try to make sure we return an error if anything's fucky
	if (mContext == nullptr || mWindow == nullptr || glfwWindowShouldClose(mWindow))
		return false;

	// Update viewer context
	mContext->Update(deltaTime);
	
	// Begin actual rendering
	glfwMakeContextCurrent(mWindow);
	glfwPollEvents();

	Input::UpdateInputState();

	// The context renders both the ImGui elements and the background elements.
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	// Update buffer size
	int width, height;
	glfwGetFramebufferSize(mWindow, &width, &height);
	glViewport(0, 0, width, height);

	// Clear buffers
	glClearColor(0.100f, 0.261f, 0.402f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Render viewer context
	mContext->Render(deltaTime);
	
	// Render imgui
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	// Swap buffers
	glfwSwapBuffers(mWindow);

	return true;
}


};