#pragma once

class Window;
class VulkanRenderer;

namespace Engine
{
	void Init();

	Window* GetWindow();
	VulkanRenderer* GetRenderer();
}
