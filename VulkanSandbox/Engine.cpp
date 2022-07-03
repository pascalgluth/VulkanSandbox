#include "Engine.h"

#include "Window.h"
#include "VulkanRenderer.h"
#include "imgui/imgui.h"

Window window;
VulkanRenderer renderer;

bool updateWindow();
void update();

void Engine::Init()
{
    window.Init("Vulkan Sandbox", 1920, 1080);
    renderer.Init();

    while (updateWindow())
    {
        update();
        renderer.Draw();
    }

    renderer.Destroy();
    window.Destroy();
}

Window* Engine::GetWindow()
{
    return &window;
}

VulkanRenderer* Engine::GetRenderer()
{
    return &renderer;
}

bool updateWindow()
{
    return window.Update();
}

void update()
{
    renderer.Update(ImGui::GetIO().DeltaTime);
}