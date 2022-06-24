module;
#include <cstdint>
#include <SDL2/SDL_keycode.h>
#include <cstring> // memset
module Input;

namespace gg
{
	void InputManager::OnKeyPressed(SDL_Keycode key, bool isDown)
	{
		if (key == SDLK_w)
			Keys[MoveCameraForward] = isDown;
		else if (key == SDLK_s)
			Keys[MoveCameraBack] = isDown;
		else if (key == SDLK_a)
			Keys[MoveCameraLeft] = isDown;
		else if (key == SDLK_d)
			Keys[MoveCameraRight] = isDown;
		else if (key == SDLK_q)
			Keys[RaiseCamera] = isDown;
		else if (key == SDLK_e)
			Keys[LowerCamera] = isDown;
		else if (key == SDLK_UP)
			Keys[LookCameraUp] = isDown;
		else if (key == SDLK_DOWN)
			Keys[LookCameraDown] = isDown;
		else if (key == SDLK_LEFT)
			Keys[TurnCameraLeft] = isDown;
		else if (key == SDLK_RIGHT)
			Keys[TurnCameraRight] = isDown;
	}

	void InputManager::SetKeyDown(InputAction a, bool value) 
	{
		Keys[a] = value;
	}

	void InputManager::ClearKeys() 
	{
		memset(Keys, 0, sizeof(Keys));
	}

	bool InputManager::IsKeyDown(InputAction a) const 
	{
		return Keys[a];
	}

	float InputManager::GetPlayerSpeed(uint64_t deltaMs) const 
	{
		return static_cast<float>(PlayerSpeed / 1000 * deltaMs);
	}

} // namespace gg
