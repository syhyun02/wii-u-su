// WindowSystem implementation without any windows: the emulated screens are not shown.
#include "Common/precompiled.h"
#include "gui/interface/WindowSystem.h"
#include "config/CemuConfig.h"

namespace
{
	WindowSystem::WindowInfo s_windowInfo;
}

namespace WindowSystem
{
	void ShowErrorDialog(std::string_view message, std::string_view title, std::optional<ErrorCategory>)
	{
		cemuLog_log(LogType::Force, "[dialog] {}: {}", title, message);
		fmt::print(stderr, "[dialog] {}: {}\n", title, message);
	}

	void Create() {}

	WindowInfo& GetWindowInfo() { return s_windowInfo; }

	void UpdateWindowTitles(bool isIdle, bool isLoading, double fps)
	{
		static auto last = std::chrono::steady_clock::now();
		auto now = std::chrono::steady_clock::now();
		if (now - last > std::chrono::seconds(5))
		{
			last = now;
			fmt::print("[status] idle={} loading={} fps={:.1f}\n", isIdle, isLoading, fps);
		}
	}
	void GetWindowSize(int& w, int& h) { w = 1280; h = 720; }
	void GetPadWindowSize(int& w, int& h) { w = 854; h = 480; }
	void GetWindowPhysSize(int& w, int& h) { w = 1280; h = 720; }
	void GetPadWindowPhysSize(int& w, int& h) { w = 854; h = 480; }
	double GetWindowDPIScale() { return 1.0; }
	double GetPadDPIScale() { return 1.0; }
	bool IsPadWindowOpen() { return false; }
	bool IsKeyDown(uint32) { return false; }
	bool IsKeyDown(PlatformKeyCodes) { return false; }
	std::string GetKeyCodeName(uint32 key) { return fmt::format("key{}", key); }
	bool InputConfigWindowHasFocus() { return false; }
	void NotifyGameLoaded() {}
	void NotifyGameExited() {}
	void RefreshGameList() {}
	bool IsFullScreen() { return false; }
	void CaptureInput(const ControllerState&, const ControllerState&) {}
}
