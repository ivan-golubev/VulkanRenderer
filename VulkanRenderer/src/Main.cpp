#include <SDL2/SDL.h>
#include <exception>
#include <format>
#include <cstdlib>

import Application;
import ErrorHandling;
import Logging;

using namespace gg;

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
                case SDL_KEYDOWN:
                case SDL_KEYUP:
                {
                    SDL_Keycode key{ event.key.keysym.sym };
                    if (key == SDLK_ESCAPE)
                        isRunning = false;
                    else if (Application::IsInitialized())
                        Application::Get().OnKeyPressed(key, event.type == SDL_KEYDOWN);
                    break;
                }
                default:
                    // Do nothing.
                    break;
            }
        }
        Application::Get().Tick();
    }
}

int main()
{
    if(SDL_Init(SDL_INIT_VIDEO) != 0) 
    {
        DebugLog(DebugLevel::Error, L"Could not initialize SDL");
        return EXIT_FAILURE;
    }
    uint32_t const width{ 1920 };
    uint32_t const height{ 1080 };

    SDL_Window* window = SDL_CreateWindow("Vulkan Window", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_VULKAN);
    if(nullptr == window) 
    {
        DebugLog(DebugLevel::Error, L"Could not create SDL window");
        return EXIT_FAILURE;
    }

    try
    {
        Application::Init(width, height, window);
        DebugLog(DebugLevel::Info, L"Successfully initialized the Vulkan application");
    }
    catch (std::exception const& e)
    {
        std::wstring const errorMsg = ToWString(std::format("Caught exception with message: {}", e.what()));
        DebugLog(DebugLevel::Error, errorMsg);
        return EXIT_FAILURE;
    }

    MainLoop();
    Application::Destroy();
    return EXIT_SUCCESS;
}
