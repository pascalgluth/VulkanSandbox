#pragma once

#include <SDL.h>

class Window
{
public:
	Window();
	~Window();
	
	void Init(const char* windowName = "Vulkan Sandbox", const int width = 1280, const int height = 720);
	bool Update();
	void Destroy();

	SDL_Window* GetSDLWindow();

private:
	SDL_Window* m_window;
	SDL_Surface* m_icon;
	
};
