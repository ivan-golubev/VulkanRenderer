module;
#include <cstdint>
#include <memory>
#include <windows.h>
#include <SDL2/SDL_video.h>
#include <SDL2/SDL_keycode.h>
export module Application;

import VulkanRenderer;
import Input;
import TimeManager;

namespace gg 
{
	export class Application 
	{
	public:
		static Application& Init(uint32_t width, uint32_t height, SDL_Window*);
		static void Destroy();
		static bool IsInitialized();
		static Application& Get();

		void Tick();
		void OnWindowResized(uint32_t width, uint32_t height);
		void OnKeyPressed(SDL_Keycode, bool isDown);

		TimeManager& GetTimeManager() const;
		InputManager& GetInputManager() const;
	private:
		Application(uint32_t width, uint32_t height, SDL_Window*);
		~Application();

		static Application* INSTANCE;

		std::unique_ptr<TimeManager> mTimeManager{};
		std::unique_ptr<InputManager> mInputManager{};
		std::unique_ptr<VulkanRenderer> mRenderer{};
	};
} // namespace gg 
