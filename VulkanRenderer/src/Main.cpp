#include <SDL2/SDL.h>
#include <exception>
#include <format>

import Application;
import ErrorHandling;
import Logging;

using awesome::application::Application;
using namespace awesome::logging;

void MainLoop()
{
    bool isRunning{ true };

    while (isRunning)
    {
        SDL_Event event;
        // Poll for user input.
        while (SDL_PollEvent(&event)) 
        {
            switch (event.type) 
            {
                case SDL_QUIT:
                    isRunning = false;
                    break;

                default:
                    // Do nothing.
                    break;
            }
            Application::Get().Tick();
        }
    }
}

int main()
{
    if(SDL_Init(SDL_INIT_VIDEO) != 0) 
    {
        DebugLog(DebugLevel::Error, L"Could not initialize SDL");
        return 1;
    }
    uint32_t const Width{ 1920 };
    uint32_t const Height{ 1080 };

    SDL_Window* window = SDL_CreateWindow("Vulkan Window", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, Width, Height, SDL_WINDOW_VULKAN);
    if(nullptr == window) 
    {
        DebugLog(DebugLevel::Error, L"Could not create SDL window");
        return 1;
    }

    try
    {
        Application::Init(Width, Height, window);
        DebugLog(DebugLevel::Info, L"Successfully initialized the Vulkan application");
    }
    catch (std::exception const& e)
    {
        std::wstring const errorMsg = ToWString(std::format("Caught exception with message: {}", e.what()));
        DebugLog(DebugLevel::Error, errorMsg);
        return 1;
    }

    MainLoop();
    Application::Destroy();
    return 0;
}
