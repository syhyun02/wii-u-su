// cemu-headless: boots a Wii U title or RPX without any UI.
//
//   cemu-headless -g <path to game/rpx> [--force-interpreter] [--forward-console-logging] [--mlc <dir>]
//
// Environment:
//   CEMU_HOME          directory for settings, mlc01, caches (default: ./cemu-home)
//   CEMU_RUN_SECONDS   stop after this many seconds (default: run until the title exits)
//
// The same file is the entry point of the WebAssembly build.
#include "Common/precompiled.h"
#include "gui/interface/WindowSystem.h"
#include "Cafe/CafeSystem.h"
#include "Cafe/TitleList/TitleList.h"
#include "Cafe/TitleList/TitleInfo.h"
#include "Cafe/TitleList/SaveList.h"
#include "Cafe/GraphicPack/GraphicPack2.h"
#include "Cafe/HW/Latte/Core/LatteOverlay.h"
#include "Cafe/OS/RPL/rpl.h"
#include "config/ActiveSettings.h"
#include "config/CemuConfig.h"
#include "config/LaunchSettings.h"
#include "config/NetworkSettings.h"
#include "input/InputManager.h"
#include "audio/IAudioAPI.h"
#include "audio/IAudioInputAPI.h"
#include "util/crypto/aes128.h"
#include "Cafe/HW/Espresso/PPCState.h"
#include "Common/ExceptionHandler/ExceptionHandler.h"
#include "NullRenderer.h"

// symbols that the wx build defines in src/main.cpp
std::atomic_bool g_isGPUInitFinished = false;
std::wstring executablePath;
void requireConsole() {}

void PPCTimer_init();

namespace
{
	std::atomic_bool s_processExited = false;

	class HeadlessSystem : public CafeSystem::SystemImplementation
	{
	public:
		void CafeRecreateCanvas() override {}
		void CafePPCProcessExit() override { s_processExited = true; }
	};
	HeadlessSystem s_system;

	bool CreateDirs(const fs::path& p)
	{
		std::error_code ec;
		if (fs::exists(p, ec))
			return true;
		return fs::create_directories(p, ec);
	}

	void CreateDefaultMLC(const fs::path& mlc)
	{
		const fs::path dirs[] = {
			mlc / "sys", mlc / "usr",
			mlc / "usr/title/00050000", mlc / "usr/title/0005000c", mlc / "usr/title/0005000e",
			mlc / "usr/save/00050010/1004a000/user/common/db",
			mlc / "usr/save/00050010/1004a100/user/common/db",
			mlc / "usr/save/00050010/1004a200/user/common/db",
			mlc / "sys/title/0005001b/1005c000/content",
		};
		for (auto& d : dirs)
			CreateDirs(d);
		auto langFile = mlc / "sys/title/0005001b/1005c000/content/language.txt";
		std::error_code ec;
		if (!fs::exists(langFile, ec))
		{
			std::ofstream f(langFile);
			for (const char* l : {"ja", "en", "fr", "de", "it", "es", "zh", "ko", "nl", "pt", "ru", "zh"})
				f << "\"" << l << "\"," << std::endl;
		}
	}

	bool PrepareTitle(const fs::path& launchPath)
	{
		TitleInfo launchTitle{launchPath};
		if (launchTitle.IsValid())
		{
			CafeTitleList::AddTitleFromPath(launchPath);
			TitleId baseTitleId;
			if (!CafeTitleList::FindBaseTitleId(launchTitle.GetAppTitleId(), baseTitleId))
			{
				fmt::print(stderr, "error: base title files not found for {}\n", _pathToUtf8(launchPath));
				return false;
			}
			auto r = CafeSystem::PrepareForegroundTitle(baseTitleId);
			if (r != CafeSystem::PREPARE_STATUS_CODE::SUCCESS)
			{
				fmt::print(stderr, "error: PrepareForegroundTitle failed ({})\n", (int)r);
				return false;
			}
			return true;
		}
		CafeTitleFileType fileType = DetermineCafeSystemFileType(launchPath);
		if (fileType == CafeTitleFileType::RPX || fileType == CafeTitleFileType::ELF)
		{
			auto r = CafeSystem::PrepareForegroundTitleFromStandaloneRPX(launchPath);
			if (r != CafeSystem::PREPARE_STATUS_CODE::SUCCESS)
			{
				fmt::print(stderr, "error: failed to load executable ({})\n", (int)r);
				return false;
			}
			return true;
		}
		if (launchTitle.GetInvalidReason() == TitleInfo::InvalidReason::NO_DISC_KEY)
			fmt::print(stderr, "error: could not decrypt title, keys.txt is missing the disc key\n");
		else if (launchTitle.GetInvalidReason() == TitleInfo::InvalidReason::NO_TITLE_TIK)
			fmt::print(stderr, "error: could not decrypt title, title.tik is missing\n");
		else
			fmt::print(stderr, "error: not a valid title or executable: {}\n", _pathToUtf8(launchPath));
		return false;
	}
}

int main(int argc, char* argv[])
{
	setvbuf(stdout, nullptr, _IOLBF, 0);
	auto parse_rc = LaunchSettings::HandleCommandline(argc, argv);
	if (parse_rc.has_value())
		return *parse_rc;

	const char* homeEnv = getenv("CEMU_HOME");
	fs::path home = fs::absolute(homeEnv && *homeEnv ? fs::path(homeEnv) : fs::path("cemu-home"));
	CreateDirs(home);
	fs::path exePath = fs::absolute(argv[0]);
	std::set<fs::path> failedWriteAccess;
	// data path (resources/, gameProfiles/) defaults to Cemu's bin/ folder when it is next to us, else CEMU_HOME
	const char* dataEnv = getenv("CEMU_DATA");
	fs::path dataPath = dataEnv && *dataEnv ? fs::path(dataEnv) : home;
	ActiveSettings::SetPaths(true, exePath, home, home, home, dataPath, failedWriteAccess);
	CreateDirs(ActiveSettings::GetConfigPath("controllerProfiles"));

	GetConfigHandle().SetFilename(ActiveSettings::GetConfigPath("settings.xml").generic_wstring());
	{
		std::error_code ec;
		if (fs::exists(ActiveSettings::GetConfigPath("settings.xml"), ec))
			GetConfigHandle().Load();
		else
			GetConfigHandle().Save();
	}
	NetworkConfig::LoadOnce();
	CreateDefaultMLC(ActiveSettings::GetMlcPath());
	ActiveSettings::Init();

	// equivalent of CemuCommonInit() in src/main.cpp, minus UI / GPU driver tweaks
	AES128_init();
	PPCTimer_init();
	ExceptionHandler_Init();
	IAudioAPI::InitializeStatic();
	IAudioInputAPI::InitializeStatic();
	GraphicPack2::LoadAll();
	InputManager::instance().load();
	CafeSystem::Initialize();
	CafeSystem::SetImplementation(&s_system);
	CafeTitleList::Initialize(ActiveSettings::GetUserDataPath("title_list_cache.xml"));
	CafeTitleList::SetMLCPath(ActiveSettings::GetMlcPath());
	CafeTitleList::Refresh();
	CafeSaveList::Initialize();
	CafeSaveList::SetMLCPath(ActiveSettings::GetMlcPath());
	CafeSaveList::Refresh();

	auto loadFile = LaunchSettings::GetLoadFile();
	if (!loadFile)
	{
		fmt::print(stderr, "usage: {} -g <game or rpx> [--force-interpreter] [--forward-console-logging]\n", argv[0]);
		return 2;
	}

	g_renderer = std::make_unique<NullRenderer>();

	if (!PrepareTitle(*loadFile))
		return 1;
	fmt::print("[headless] launching {} (title {:016x})\n", CafeSystem::GetForegroundTitleName(), CafeSystem::GetForegroundTitleId());
	CafeSystem::LaunchForegroundTitle();

	const char* secEnv = getenv("CEMU_RUN_SECONDS");
	double runSeconds = secEnv ? atof(secEnv) : 0.0;
	auto start = std::chrono::steady_clock::now();
	auto nextReport = start + std::chrono::seconds(5);
	auto* nullRenderer = static_cast<NullRenderer*>(g_renderer.get());
	while (!s_processExited)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		auto now = std::chrono::steady_clock::now();
		double elapsed = std::chrono::duration<double>(now - start).count();
		if (now >= nextReport)
		{
			nextReport = now + std::chrono::seconds(5);
			fmt::print("[headless] t={:.0f}s swaps={} draws={}\n", elapsed, nullRenderer->GetSwapCount(), nullRenderer->GetDrawCount());
		}
		if (runSeconds > 0.0 && elapsed >= runSeconds)
		{
			fmt::print("[headless] time limit reached ({}s), swaps={} draws={}\n", runSeconds, nullRenderer->GetSwapCount(), nullRenderer->GetDrawCount());
			fflush(stdout);
			_Exit(0);
		}
	}
	auto status = CafeSystem::GetForegroundTitleReturnStatus();
	fmt::print("[headless] title exited with status {}\n", status.value_or(-1));
	fflush(stdout);
	_Exit(status.value_or(0) & 0xFF);
}
