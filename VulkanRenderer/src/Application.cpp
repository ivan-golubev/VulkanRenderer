module;
#include <cstdint>
#include <DirectXMath.h>
#include <memory>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_video.h>
module Application;

import VulkanRenderer;
import Input;
import Logging;
import TimeManager;
import ErrorHandling;

namespace gg
{
	std::shared_ptr<Application> Application::INSTANCE{ nullptr };

	std::shared_ptr<Application> Application::Init(uint32_t width, uint32_t height, SDL_Window* windowHandle)
	{
		BreakIfFalse(!Application::IsInitialized());
		INSTANCE = std::make_shared<Application>(width, height, windowHandle);
		return INSTANCE;
	}

	void Application::Destroy()
	{
		BreakIfFalse(Application::IsInitialized());
		INSTANCE.reset();
	}

	bool Application::IsInitialized()
	{
		return INSTANCE.get() != nullptr;
	}

	std::shared_ptr<Application> Application::Get()
	{
		BreakIfFalse(Application::IsInitialized());
		return INSTANCE;
	}

	Application::Application(uint32_t width, uint32_t height, SDL_Window* windowHandle)
		: mTimeManager{ std::make_unique<TimeManager>() }
		, mInputManager{ std::make_unique<InputManager>() }
		, mRenderer{ std::make_unique<VulkanRenderer>(width, height, windowHandle)}
		, mModelLoader{ std::make_unique<ModelLoader>() }
	{
		/* Check for DirectX Math library support. */
		if (!DirectX::XMVerifyCPUSupport())
			throw std::exception("Failed to verify DirectX Math library support");
	}

	Application::~Application() 
	{
		DebugLog(DebugLevel::Info, "Shutting down the application");
	}

	std::shared_ptr<TimeManager> Application::GetTimeManager() 
	{
		return mTimeManager;
	}

	std::shared_ptr<InputManager> Application::GetInputManager()
	{
		return mInputManager;
	}

	std::shared_ptr<ModelLoader> Application::GetModelLoader()
	{
		return mModelLoader;
	}

	std::shared_ptr<VulkanRenderer> Application::GetRenderer()
	{
		return mRenderer;
	}

	void Application::Tick()
	{
		uint64_t dt = mTimeManager->Tick();
		if (!mPaused)
			mRenderer->Render(dt);
	}

	void Application::OnWindowResized(uint32_t width, uint32_t height)
	{
		mRenderer->OnWindowResized(width, height);
	}

	void Application::OnWindowMinimized()
	{
		mPaused = true;
	}

	void Application::OnWindowRestored()
	{
		mPaused = false;
	}

	void Application::OnKeyPressed(SDL_Keycode key, bool isDown)
	{
		mInputManager->OnKeyPressed(key, isDown);
	}

} // namespace gg 
