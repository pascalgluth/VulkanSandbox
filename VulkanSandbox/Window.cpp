#include "Window.h"

#include <iostream>
#include <SDL.h>
#include <SDL_image.h>
#include <stb_image.h>

#include "imgui/imgui_impl_sdl.h"

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

    int channels;
    int iconWidth;
    int iconHeight;
    //stbi_uc* image = stbi_load("textures/LBI_Logo_Transparent.png", &iconWidth, &iconHeight, &channels, STBI_rgb_alpha);
    //m_icon = SDL_CreateRGBSurfaceFrom(image, iconWidth, iconHeight, 1, 32, 0x0f00,0x00f0,0x000f,0xf000);

    m_icon = IMG_Load("textures/LBI_Logo_Transparent.png");
    SDL_SetWindowIcon(m_window, m_icon);
    //stbi_image_free(image);
}

bool Window::Update()
{
    SDL_Event event;
    while(SDL_PollEvent(&event))
    {
        if (event.type == SDL_QUIT)
            return false;

        ImGui_ImplSDL2_ProcessEvent(&event);
    }

    return true;
}

void Window::Destroy()
{
    SDL_FreeSurface(m_icon);
    SDL_DestroyWindow(m_window);
    m_window = nullptr;
    SDL_Quit();
}

SDL_Window* Window::GetSDLWindow()
{
    return m_window;
}
