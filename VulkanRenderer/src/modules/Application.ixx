module;
#include <cstdint>
#include <memory>
#include <windows.h>
#include <SDL2/SDL_video.h>
export module Application;

import VulkanRenderer;
import Input;
import TimeManager;

using awesome::input::InputManager;
using awesome::renderer::VulkanRenderer;
using awesome::time::TimeManager;

namespace awesome::application 
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
		void OnWindowMessage(uint32_t uMsg, uint32_t wParam);

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
} // namespace awesome::application 
