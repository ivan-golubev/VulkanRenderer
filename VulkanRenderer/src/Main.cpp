#include <SDL2/SDL.h>
#include <exception>
#include <format>
#include <cstdlib>

#include <chrono>
#include <thread>

import Application;
import ErrorHandling;
import Logging;

using namespace gg;

void MainLoop()
{
    using namespace std::chrono_literals;
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
                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_RESIZED && Application::IsInitialized())
                        Application::Get().OnWindowResized(event.window.data1, event.window.data2);
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
        // TODO: without this wait the camera won't move, need to investigate
        std::this_thread::sleep_for(1ms);
    }
}

int main()
{
    if(SDL_Init(SDL_INIT_VIDEO) != 0) 
    {
        DebugLog(DebugLevel::Error, L"Could not initialize SDL");
        return EXIT_FAILURE;
    }
    atexit(SDL_Quit);

    uint32_t const width{ 1920 };
    uint32_t const height{ 1080 };

    SDL_Window* window = SDL_CreateWindow("Vulkan Renderer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
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
