#include <UI/ImGuiBackend.h>

#include <Rendering/Window/Window.h>

#include <imgui.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

void ImGuiBackend::CreateContext(IWindow* window)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	ImGui::StyleColorsDark();

	// The default font is 13px at 96 DPI. Content scale is 1.0 at 96 DPI.
	f32 scaleX = 1.f;
	f32 scaleY = 1.f;
	glfwGetWindowContentScale(static_cast<GLFWwindow*>(window->GetNativeHandle()), &scaleX, &scaleY);

	ImFontConfig fontConfig;
	fontConfig.SizePixels = 13.f * scaleX;
	io.Fonts->AddFontDefault(&fontConfig);
}
