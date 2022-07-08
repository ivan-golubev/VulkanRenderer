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
import ModelLoader;

namespace gg 
{
	export class Application 
	{
	public:
		static std::shared_ptr<Application> Init(uint32_t width, uint32_t height, SDL_Window*);
		static void Destroy();
		static bool IsInitialized();
		static std::shared_ptr<Application> Get();

		Application(uint32_t width, uint32_t height, SDL_Window*);
		~Application();
		void Tick();
		void OnWindowResized(uint32_t width, uint32_t height);
		void OnWindowMinimized();
		void OnWindowRestored();
		void OnKeyPressed(SDL_Keycode, bool isDown);

		std::shared_ptr<InputManager> GetInputManager();
		std::shared_ptr<ModelLoader> GetModelLoader();
		std::shared_ptr<TimeManager> GetTimeManager();
		std::shared_ptr<VulkanRenderer> GetRenderer();
	private:

		static std::shared_ptr<Application> INSTANCE;

		bool mPaused{ false };

		std::shared_ptr<InputManager> mInputManager;
		std::shared_ptr<ModelLoader> mModelLoader;
		std::shared_ptr<TimeManager> mTimeManager;
		std::shared_ptr<VulkanRenderer> mRenderer;
	};
} // namespace gg 
