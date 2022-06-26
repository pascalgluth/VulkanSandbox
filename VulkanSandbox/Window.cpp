#include "Window.h"

#include <SDL.h>

Window::Window()
{
    m_window = nullptr;
}

Window::~Window()
{
}

void Window::Init(const char* windowName, const int width, const int height)
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER);
    m_window = SDL_CreateWindow(windowName, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN);
    SDL_SetWindowResizable(m_window, SDL_FALSE);
}

bool Window::Update()
{
    SDL_Event event;
    while(SDL_PollEvent(&event))
    {
        if (event.type == SDL_QUIT)
            return false;
    }

    return true;
}

void Window::Destroy()
{
    SDL_DestroyWindow(m_window);
    m_window = nullptr;
    SDL_Quit();
}

SDL_Window* Window::GetSDLWindow()
{
    return m_window;
}
