module;
#include <cstdint>
export module Input;

export namespace gg
{
	enum InputAction {
		RaiseCamera,
		LowerCamera,
		MoveCameraLeft,
		MoveCameraRight,
		MoveCameraForward,
		MoveCameraBack,
		TurnCameraLeft,
		TurnCameraRight,
		LookCameraUp,
		LookCameraDown,
		Count
	};

	class InputManager {
	public:
		void OnWindowMessage(uint32_t uMsg, uint32_t wParam);
		void SetKeyDown(InputAction a, bool value);
		void ClearKeys();
		bool IsKeyDown(InputAction a) const;
		float GetPlayerSpeed(uint64_t deltaMs) const;
	private:
		bool Keys[InputAction::Count];
		static constexpr float PlayerSpeed{ 1.5f };
	};

} // namespace gg
