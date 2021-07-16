#pragma once

#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "CryCommon/CryAction/IGameFramework.h"
#include "CryCommon/CryAction/ILevelSystem.h"
#include "CryCommon/CryEntitySystem/IEntitySystem.h"

struct IConsoleCmdArgs;

class Executor;
class HTTPClient;
class FileDownloader;
class FileRedirector;
class FileCache;
class MapDownloader;
class GSMasterHook;
class ScriptCommands;
class ScriptCallbacks;
class ScriptBind_CPPAPI;
class ServerBrowser;
class ServerConnector;
class ServerPAK;

class Client : public IGameFrameworkListener, public ILevelSystemListener, public IEntitySystemSink
{
	IGameFramework *m_pGameFramework = nullptr;

	std::string_view m_scriptMain;
	std::string_view m_scriptGameRules;
	std::string_view m_scriptJSON;
	std::string_view m_scriptRPC;

	std::unique_ptr<Executor> m_pExecutor;
	std::unique_ptr<HTTPClient> m_pHTTPClient;
	std::unique_ptr<FileDownloader> m_pFileDownloader;
	std::unique_ptr<FileRedirector> m_pFileRedirector;
	std::unique_ptr<FileCache> m_pFileCache;
	std::unique_ptr<MapDownloader> m_pMapDownloader;
	std::unique_ptr<GSMasterHook> m_pGSMasterHook;
	std::unique_ptr<ScriptCommands> m_pScriptCommands;
	std::unique_ptr<ScriptCallbacks> m_pScriptCallbacks;
	std::unique_ptr<ScriptBind_CPPAPI> m_pScriptBind_CPPAPI;
	std::unique_ptr<ServerBrowser> m_pServerBrowser;
	std::unique_ptr<ServerConnector> m_pServerConnector;
	std::unique_ptr<ServerPAK> m_pServerPAK;

	std::minstd_rand m_randomEngine;

	std::vector<std::string> m_masters;

	static void OnConnectCmd(IConsoleCmdArgs *pArgs);
	static void OnDisconnectCmd(IConsoleCmdArgs *pArgs);

public:
	Client();
	~Client();

	void Init(IGameFramework *pGameFramework);

	std::string GetMasterServerAPI(const std::string& master = "");
	std::string GetHWID(const std::string_view & salt);

	// IGameFrameworkListener
	void OnPostUpdate(float deltaTime) override;
	void OnSaveGame(ISaveGame *pSaveGame) override;
	void OnLoadGame(ILoadGame *pLoadGame) override;
	void OnLevelEnd(const char *nextLevel) override;
	void OnActionEvent(const SActionEvent & event) override;

	// ILevelSystemListener
	void OnLevelNotFound(const char *levelName) override;
	void OnLoadingStart(ILevelInfo *pLevel) override;
	void OnLoadingComplete(ILevel *pLevel) override;
	void OnLoadingError(ILevelInfo *pLevel, const char *error) override;
	void OnLoadingProgress(ILevelInfo *pLevel, int progressAmount) override;

	// IEntitySystemSink
	bool OnBeforeSpawn(SEntitySpawnParams & params) override;
	void OnSpawn(IEntity *pEntity, SEntitySpawnParams & params) override;
	bool OnRemove(IEntity *pEntity) override;
	void OnEvent(IEntity *pEntity, SEntityEvent & event) override;

	IGameFramework *GetGameFramework()
	{
		return m_pGameFramework;
	}

	Executor *GetExecutor()
	{
		return m_pExecutor.get();
	}

	HTTPClient *GetHTTPClient()
	{
		return m_pHTTPClient.get();
	}

	FileDownloader *GetFileDownloader()
	{
		return m_pFileDownloader.get();
	}

	FileRedirector *GetFileRedirector()
	{
		return m_pFileRedirector.get();
	}

	FileCache *GetFileCache()
	{
		return m_pFileCache.get();
	}

	MapDownloader *GetMapDownloader()
	{
		return m_pMapDownloader.get();
	}

	GSMasterHook *GetGSMasterHook()
	{
		return m_pGSMasterHook.get();
	}

	ScriptCommands *GetScriptCommands()
	{
		return m_pScriptCommands.get();
	}

	ScriptCallbacks *GetScriptCallbacks()
	{
		return m_pScriptCallbacks.get();
	}

	ScriptBind_CPPAPI *GetScriptBind_CPPAPI()
	{
		return m_pScriptBind_CPPAPI.get();
	}

	ServerBrowser *GetServerBrowser()
	{
		return m_pServerBrowser.get();
	}

	ServerConnector *GetServerConnector()
	{
		return m_pServerConnector.get();
	}

	ServerPAK *GetServerPAK()
	{
		return m_pServerPAK.get();
	}

	const std::vector<std::string>& GetMasters() const {
		return m_masters;
	}

	// ints
	template<class T>
	typename std::enable_if<std::is_integral<T>::value, T>::type GetRandomNumber(T min, T max)
	{
		// uniform distribution is stateless
		return std::uniform_int_distribution<T>(min, max)(m_randomEngine);  // [min, max]
	}

	// floats
	template<class T>
	typename std::enable_if<std::is_floating_point<T>::value, T>::type GetRandomNumber(T min, T max)
	{
		// uniform distribution is stateless
		return std::uniform_real_distribution<T>(min, max)(m_randomEngine);  // [min, max)
	}
};

///////////////////////
inline Client *gClient;
///////////////////////
