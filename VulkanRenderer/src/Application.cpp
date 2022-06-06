module;
#include <cstdint>
#include <DirectXMath.h>
#include <memory>
#include <SDL2/SDL_video.h>
module Application;

import VulkanRenderer;
import Input;
import Logging;
import TimeManager;

using awesome::input::InputManager;
using awesome::renderer::VulkanRenderer;
using awesome::time::TimeManager;
using namespace awesome::logging;

namespace awesome::application
{
	Application* Application::INSTANCE{ nullptr };

	Application& Application::Init(uint32_t width, uint32_t height, SDL_Window* windowHandle)
	{
		assert(!INSTANCE);
		if (!INSTANCE)
			INSTANCE = new Application(width, height, windowHandle);
		return *INSTANCE;
	}

	void Application::Destroy()
	{
		assert(INSTANCE);
		delete INSTANCE;
	}

	bool Application::IsInitialized()
	{
		return nullptr != INSTANCE;
	}

	Application& Application::Get()
	{
		assert(INSTANCE);
		return *INSTANCE;
	}

	Application::Application(uint32_t width, uint32_t height, SDL_Window* windowHandle)
		: mTimeManager{ std::make_unique<TimeManager>() }
		, mInputManager{ std::make_unique<InputManager>() }
		, mRenderer{ std::make_unique<VulkanRenderer>(width, height, windowHandle)}
	{
		/* Check for DirectX Math library support. */
		if (!DirectX::XMVerifyCPUSupport())
			throw std::exception("Failed to verify DirectX Math library support");
	}

	Application::~Application() 
	{
		DebugLog(DebugLevel::Info, L"Shutting down the application");
	}

	TimeManager& Application::GetTimeManager() const 
	{
		return *mTimeManager;
	}

	InputManager& Application::GetInputManager() const 
	{
		return *mInputManager;
	}

	void Application::Tick()
	{
		uint64_t dt = mTimeManager->Tick();
		mRenderer->Render(dt);
	}

	void Application::OnWindowResized(uint32_t width, uint32_t height)
	{
		mRenderer->OnWindowResized(width, height);
	}

	void Application::OnWindowMessage(uint32_t uMsg, uint32_t wParam)
	{
		mInputManager->OnWindowMessage(uMsg, wParam);
	}

} // namespace awesome::application 
