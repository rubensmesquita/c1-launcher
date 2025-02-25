#include "Library/CrashLogger.h"
#include "Library/Path.h"
#include "Library/WinAPI.h"

#include "../Patch.h"

#include "HeadlessServerLauncher.h"

#include "Project.h"

HeadlessServerLauncher::HeadlessServerLauncher() : m_rootFolder(GetRootFolder()), m_log(m_executor, m_rootFolder)
{
}

HeadlessServerLauncher::~HeadlessServerLauncher()
{
}

int HeadlessServerLauncher::Run()
{
	try
	{
		LogSystem::StdErr("%s", PROJECT_VERSION_DETAILS);
		LogSystem::StdErr("Command line: [%s]", WinAPI::CmdLine::GetOnlyArgs());
		LogSystem::StdErr("Root folder: \"%s\"", m_rootFolder.c_str());

		m_params.hInstance = WinAPI::EXE::Get();
		m_params.logFileName = LogSystem::GetDefaultFileName();
		m_params.isDedicatedServer = true;
		m_params.pLog = &m_log;
		m_params.pValidator = &m_validator;
		m_params.pUserCallback = this;

		SetParamsCmdLine(WinAPI::CmdLine::Get());

		CrashLogger::SetSink(m_log);

		LoadEngine();
		PatchEngine();

		LogSystem::StdErr("Log file: %s", m_params.logFileName);

		if (!m_log.SetFileName(m_params.logFileName))
		{
			return 1;
		}

		LogSystem::StdErr("Starting CryEngine...");
		StartEngine(m_CryGame);

		LogSystem::StdErr("Ready");

		return UpdateLoop();
	}
	catch (const std::runtime_error& error)
	{
		LogSystem::StdErr("%s", error.what());
		return 1;
	}
}

bool HeadlessServerLauncher::OnError(const char* error)
{
	return true;
}

void HeadlessServerLauncher::OnSaveDocument()
{
}

void HeadlessServerLauncher::OnProcessSwitch()
{
}

void HeadlessServerLauncher::OnInitProgress(const char* message)
{
}

void HeadlessServerLauncher::OnInit(ISystem* pSystem)
{
	gEnv = pSystem->GetGlobalEnvironment();
}

void HeadlessServerLauncher::OnShutdown()
{
}

void HeadlessServerLauncher::OnUpdate()
{
	m_executor.ExecuteMainThreadTasks();
}

void HeadlessServerLauncher::GetMemoryUsage(ICrySizer* pSizer)
{
}

void HeadlessServerLauncher::LoadEngine()
{
	m_CrySystem.Load("CrySystem.dll", DLL::NO_UNLOAD);  // unloading Crysis DLLs is not safe

	m_gameBuild = WinAPI::GetCrysisGameBuild(m_CrySystem.GetHandle());
	if (m_gameBuild < 0)
	{
		throw WinAPI::CurrentError("Failed to get the game version!");
	}

	LogSystem::StdErr("Game build: %d", m_gameBuild);

	VerifyGameBuild();

	m_CryGame.Load("CryGame.dll", DLL::NO_UNLOAD);
	m_CryAction.Load("CryAction.dll", DLL::NO_UNLOAD);
	m_CryNetwork.Load("CryNetwork.dll", DLL::NO_UNLOAD);
	m_CryRenderNULL.Load("CryRenderNULL.dll", DLL::NO_UNLOAD);
}

void HeadlessServerLauncher::PatchEngine()
{
	if (m_CryAction.IsLoaded())
	{
		void* pCryAction = m_CryAction.GetHandle();

		Patch::CryAction::DisableGameplayStats(pCryAction, m_gameBuild);
	}

	if (m_CryNetwork.IsLoaded())
	{
		void* pCryNetwork = m_CryNetwork.GetHandle();

		Patch::CryNetwork::EnablePreordered(pCryNetwork, m_gameBuild);
		Patch::CryNetwork::AllowSameCDKeys(pCryNetwork, m_gameBuild);
		Patch::CryNetwork::FixInternetConnect(pCryNetwork, m_gameBuild);
		Patch::CryNetwork::DisableServerProfile(pCryNetwork, m_gameBuild);
	}

	if (m_CrySystem.IsLoaded())
	{
		void* pCrySystem = m_CrySystem.GetHandle();

		if (WinAPI::CPU::IsAMD() && !WinAPI::CPU::Has3DNow())
		{
			Patch::CrySystem::Disable3DNow(pCrySystem, m_gameBuild);
		}

		Patch::CrySystem::UnhandledExceptions(pCrySystem, m_gameBuild);
	}

	if (m_CryRenderNULL.IsLoaded())
	{
		void* pCryRenderNULL = m_CryRenderNULL.GetHandle();

		Patch::CryRenderNULL::DisableDebugRenderer(pCryRenderNULL, m_gameBuild);
	}
}

std::string HeadlessServerLauncher::GetRootFolder()
{
	std::string rootFolder = WinAPI::CmdLine::GetArgValue("-root");

	if (rootFolder.empty())
	{
		// Bin32 or Bin64 folder
		rootFolder = Path::DirName(WinAPI::EXE::GetPath());

		const std::string binFolder = Path::BaseName(rootFolder);

		if (Path::IsNameEqual(binFolder, "Bin32") || Path::IsNameEqual(binFolder, "Bin64"))
		{
			// Crysis main folder
			rootFolder = Path::DirName(rootFolder);
		}
	}

	return rootFolder;
}
