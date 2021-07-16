#include <stdlib.h>  // atoi

#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CryAction/IGameFramework.h"
#include "CryGame/Game.h"
#include "CryGame/GameCVars.h"
#include "CryGame/Menus/FlashMenuObject.h"
#include "CryGame/Menus/MPHub.h"
#include "Library/External/nlohmann/json.hpp"
#include "Library/Util.h"

#include "ServerConnector.h"
#include "Client.h"
#include "HTTPClient.h"
#include "MapDownloader.h"
#include "FileCache.h"
#include "ServerPAK.h"

using json = nlohmann::json;

namespace
{
	std::string_view GetString(const json & object, const std::string_view & name)
	{
		auto it = object.find(name);
		if (it != object.end() && it->is_string())
			return it->get_ref<const std::string&>();
		else
			return std::string_view();
	}
}

bool ServerConnector::ParseServerInfo(HTTPClientResult & result)
{
	if (result.error)
	{
		CryLogAlways("$4[CryMP] Server check failed: %s", result.error.what());
		return false;
	}

	CryLog("[CryMP] Server info (%d): %s", result.code, result.response.c_str());

	try
	{
		const json serverInfo = json::parse(result.response);

		if (serverInfo.contains("error"))
		{
			CryLogAlways("$4[CryMP] Server check error: %s", GetString(serverInfo, "error").data());
			return false;
		}

		m_server.name   = GetString(serverInfo, "name");
		m_server.map    = GetString(serverInfo, "map");
		m_server.mapURL = GetString(serverInfo, "mapdl");
		m_server.pakURL = GetString(serverInfo, "pak");
	}
	catch (const json::exception & ex)
	{
		CryLogAlways("$4[CryMP] Server info parse error: %s", ex.what());
		return false;
	}

	if (!m_server.mapURL.empty() && !Util::StartsWith("http", m_server.mapURL))
		m_server.mapURL = "http://" + m_server.mapURL;

	if (!m_server.pakURL.empty() && !Util::StartsWith("http", m_server.pakURL))
		m_server.pakURL = "http://" + m_server.pakURL;

	return true;
}

void ServerConnector::SetLoadingDialogText(const char *text)
{
	CMPHub *pMPHub = SAFE_MENU_FUNC_RET(GetMPHub());
	if (pMPHub)
	{
		pMPHub->SetLoadingDlgText(text, false);
	}
}

void ServerConnector::SetLoadingDialogText(const char *label, const char *param)
{
	CMPHub *pMPHub = SAFE_MENU_FUNC_RET(GetMPHub());
	if (pMPHub)
	{
		pMPHub->SetLoadingDlgText(label, param);
	}
}

void ServerConnector::ShowErrorBox(const char *text)
{
	CMPHub *pMPHub = SAFE_MENU_FUNC_RET(GetMPHub());
	if (pMPHub)
	{
		pMPHub->CloseLoadingDlg();
		pMPHub->ShowError(text);
	}
}

void ServerConnector::ResetCVars()
{
	SCVars *pGameCVars = g_pGame->GetCVars();

	pGameCVars->cl_crymp = 0;
	pGameCVars->cl_circleJump = 0.0f;
	pGameCVars->cl_wallJump = 1.0f;
	pGameCVars->cl_flyMode = 0;
}

void ServerConnector::Step1_RequestServerInfo()
{
	CryLogAlways("$3[CryMP] Checking server at $6%s:%u$3", m_server.host.c_str(), m_server.port);

	const std::string & ip = m_server.host;
	const std::string port = std::to_string(m_server.port);

	const std::string url = gClient->GetMasterServerAPI(m_server.master) + "/server?ip=" + ip + "&port=" + port + "&json";

	gClient->GetHTTPClient()->GET(url, [contractID = m_contractID, this](HTTPClientResult & result)
	{
		if (contractID == m_contractID)
		{
			if (ParseServerInfo(result))
			{
				// next step
				Step2_DownloadMap();
			}
			else
			{
				// don't give up
				Step4_TryConnect();
			}
		}
	});
}

void ServerConnector::Step2_DownloadMap()
{
	MapDownloaderRequest request;
	request.map = m_server.map;
	request.mapURL = m_server.mapURL;

	request.onProgress = [contractID = m_contractID, this](const std::string & message) -> bool
	{
		if (contractID == m_contractID)
		{
			SetLoadingDialogText(message.c_str());

			// continue download
			return true;
		}
		else
		{
			// cancel download
			return false;
		}
	};

	request.onComplete = [contractID = m_contractID, this](MapDownloaderResult & result)
	{
		if (contractID == m_contractID)
		{
			if (result.success)
			{
				// next step
				Step3_DownloadPAK();
			}
			else
			{
				ShowErrorBox("Map download failed.");
			}
		}
	};

	gClient->GetMapDownloader()->Request(std::move(request));
}

void ServerConnector::Step3_DownloadPAK()
{
	if (m_server.pakURL.empty())
	{
		// next step
		Step4_TryConnect();
	}
	else
	{
		CryLogAlways("$3[CryMP] Obtaining server PAK...");

		FileCacheRequest request;
		request.fileURL = m_server.pakURL;
		request.fileType = "PAK";

		request.onProgress = [contractID = m_contractID, this](const std::string & message) -> bool
		{
			if (contractID == m_contractID)
			{
				SetLoadingDialogText(message.c_str());

				// continue download
				return true;
			}
			else
			{
				// cancel download
				return false;
			}
		};

		request.onComplete = [contractID = m_contractID, this](FileCacheResult & result)
		{
			if (contractID == m_contractID)
			{
				if (result.success)
				{
					if (gClient->GetServerPAK()->Load(result.filePath.string()))
					{
						// next step
						Step4_TryConnect();
					}
					else
					{
						ShowErrorBox("Server PAK load failed.");
					}
				}
				else
				{
					ShowErrorBox("Server PAK download failed.");
				}
			}
		};

		gClient->GetFileCache()->Request(std::move(request));
	}
}

void ServerConnector::Step4_TryConnect()
{
	const std::string endpoint = m_server.host + ':' + std::to_string(m_server.port);

	if (m_server.name.empty())
	{
		CryLogAlways("$3[CryMP] Joining unknown server at $6%s$3", endpoint.c_str());

		SetLoadingDialogText("@ui_connecting_to", endpoint.c_str());
	}
	else
	{
		CryLogAlways("$3[CryMP] Joining $6%s$3 at $6%s$3", m_server.name.c_str(), endpoint.c_str());

		SetLoadingDialogText("@ui_connecting_to", m_server.name.c_str());
	}

	ResetCVars();

	SGameStartParams params;
	params.flags = eGSF_Client | eGSF_NoDelayedStart | eGSF_NoQueries | eGSF_ImmersiveMultiplayer;
	params.hostname = m_server.host.c_str();
	params.port = m_server.port;

	gClient->GetGameFramework()->StartGameContext(&params);
}

ServerConnector::ServerConnector()
{
}

ServerConnector::~ServerConnector()
{
}

void ServerConnector::Connect(const std::string& master, const std::string_view & host, unsigned int port)
{
	m_contractID++;

	m_server.clear();
	m_server.host = host;
	m_server.port = port;
	m_server.master = master;

	// IP:PORT host workaround
	const size_t colonPos = host.find(':');
	if (colonPos != std::string::npos)
	{
		m_server.port = atoi(&host[colonPos + 1]);
		m_server.host.resize(colonPos);
	}

	Step1_RequestServerInfo();
}

void ServerConnector::Disconnect()
{
	m_contractID++;

	gClient->GetServerPAK()->Unload();
}
