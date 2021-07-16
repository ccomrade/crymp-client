#include "CryGame/StdAfx.h"
#include <stdio.h>
#include <stdint.h>
#include <string_view>

#include "CryCommon/CrySystem/ISystem.h"
#include "Library/External/nlohmann/json.hpp"
#include "Library/Format.h"
#include "Library/Util.h"

#include "ServerBrowser.h"
#include "Client.h"
#include "HTTPClient.h"

using json = nlohmann::json;

namespace
{
	bool GetBool(const json & object, const std::string_view & name)
	{
		auto it = object.find(name);
		if (it != object.end() && it->is_boolean())
			return it->get_ref<const bool&>();
		else
			return false;
	}

	int GetInt(const json & object, const std::string_view & name)
	{
		auto it = object.find(name);
		if (it != object.end())
		{
			if (it->is_number_integer())
				return static_cast<int>(it->get_ref<const int64_t&>());
			else if (it->is_number_unsigned())
				return static_cast<int>(it->get_ref<const uint64_t&>());
			else if (it->is_number_float())
				return static_cast<int>(it->get_ref<const double&>());
		}

		return 0;
	}

	std::string_view GetString(const json & object, const std::string_view & name)
	{
		auto it = object.find(name);
		if (it != object.end() && it->is_string())
			return it->get_ref<const std::string&>();
		else
			return std::string_view();
	}

	const char *GetCString(const json & object, const std::string_view & name)
	{
		auto it = object.find(name);
		if (it != object.end() && it->is_string())
			return it->get_ref<const std::string&>().c_str();
		else
			return "";
	}

	std::string IPToString(uint32_t ip)
	{
		const uint8_t byte0 = ip;
		const uint8_t byte1 = ip >> 8;
		const uint8_t byte2 = ip >> 16;
		const uint8_t byte3 = ip >> 24;

		return Format("%hhu.%hhu.%hhu.%hhu", byte0, byte1, byte2, byte3);
	}

	uint32_t IPFromString(const char *ip)
	{
		uint8_t byte0 = 0;
		uint8_t byte1 = 0;
		uint8_t byte2 = 0;
		uint8_t byte3 = 0;

		sscanf(ip, "%hhu.%hhu.%hhu.%hhu", &byte0, &byte1, &byte2, &byte3);

		return (byte3 << 24) | (byte2 << 16) | (byte1 << 8) | byte0;
	}

	const char *GetGameType(const json & serverInfo)
	{
		const std::string_view map = GetString(serverInfo, "map");

		if (Util::StartsWithNoCase("multiplayer/ia/", map))
			return "InstantAction";

		if (Util::StartsWithNoCase("multiplayer/ps/", map))
			return "PowerStruggle";

		return "";
	}

	int GetTimeLeft(const json & serverInfo)
	{
		int minutes = 0;
		int seconds = 0;

		sscanf(GetCString(serverInfo, "timel"), "%d:%d", &minutes, &seconds);

		return (minutes * 60) + seconds;
	}

	void SetBasicServerInfo(IServerListener *pListener, const std::string& master, const json & serverInfo, int serverID, bool isUpdate)
	{
		SBasicServerInfo info = {};

		info.m_hostName   =              GetCString(serverInfo, "name");
		info.m_mapName    =              GetCString(serverInfo, "mapnm");
		info.m_numPlayers =                  GetInt(serverInfo, "numpl");
		info.m_maxPlayers =                  GetInt(serverInfo, "maxpl");
		info.m_publicIP   = IPFromString(GetCString(serverInfo, "public_ip"));
		info.m_privateIP  = IPFromString(GetCString(serverInfo, "local_ip"));
		info.m_publicPort =                  GetInt(serverInfo, "public_port");
		info.m_hostPort   =                  GetInt(serverInfo, "local_port");
		info.m_official   =                  GetInt(serverInfo, "ranked") != 0;
		info.m_private    =               GetString(serverInfo, "pass") != "0";
		info.m_master     = master.c_str();

		const std::string version = "1.1.1." + std::to_string(GetInt(serverInfo, "ver"));
		info.m_gameVersion = version.c_str();

		info.m_gameType = GetGameType(serverInfo);

		if (GetBool(serverInfo, "gs"))
		{
			info.m_mapName      = GetCString(serverInfo, "mapdnm");
			info.m_anticheat    =    GetBool(serverInfo, "anticheat");
			info.m_dedicated    =    GetBool(serverInfo, "dedicated");
			info.m_dx10         =    GetBool(serverInfo, "dx10");
			info.m_friendlyfire =    GetBool(serverInfo, "friendlyfire");
			info.m_gamepadsonly =    GetBool(serverInfo, "gamepadsonly");
			info.m_voicecomm    =    GetBool(serverInfo, "voicecomm");
		}

		// not used
		info.m_country = "";

		// TODO: add optional SSM info?
		info.m_modName = "";
		info.m_modVersion = "";

		if (isUpdate)
			pListener->UpdateServer(serverID, &info);
		else
			pListener->NewServer(serverID, &info);

		// custom stuff
		pListener->UpdateValue(serverID, "connectable", GetInt(serverInfo, "available") ? "1" : "0");
	}
}

bool ServerBrowser::OnServerList(const std::string& master, HTTPClientResult & result)
{
	CryLog("[CryMP] Server list (%d): %s", result.code, result.response.c_str());

	try
	{
		const json serverList = json::parse(result.response);

		if (serverList.contains("error"))
		{
			CryLogAlways("$4[CryMP] Server list update error: %s", GetCString(serverList, "error"));
			return false;
		}

		for (const json & serverInfo : serverList)
		{
			const int serverID = m_servers.size();

			Server server;

			server.master = master;

			if (GetInt(serverInfo, "own"))
			{
				std::string_view ipString = GetString(serverInfo, "local_ip");

				if (ipString == "localhost")
					ipString = "127.0.0.1";

				server.ip = IPFromString(ipString.data());
				server.port = static_cast<uint16_t>(GetInt(serverInfo, "local_port"));
			}
			else
			{
				server.ip = IPFromString(GetCString(serverInfo, "public_ip"));
				server.port = static_cast<uint16_t>(GetInt(serverInfo, "public_port"));
			}

			m_servers.emplace_back(std::move(server));

			SetBasicServerInfo(m_pListener, server.master, serverInfo, serverID, false);
		}
	}
	catch (const json::exception & ex)
	{
		CryLogAlways("$4[CryMP] Server list parse error: %s", ex.what());
		return false;
	}

	return true;
}

bool ServerBrowser::OnServerInfo(const std::string& master, HTTPClientResult & result, int serverID)
{
	CryLog("[CryMP] Server info (%d): %s", result.code, result.response.c_str());

	try
	{
		const json serverInfo = json::parse(result.response);

		if (serverInfo.contains("error"))
		{
			CryLogAlways("$4[CryMP] Server update error: %s", GetCString(serverInfo, "error"));
			return false;
		}

		SetBasicServerInfo(m_pListener, master, serverInfo, serverID, true);

		m_pListener->UpdateValue(serverID, "hostname",              GetCString(serverInfo, "name"));
		m_pListener->UpdateValue(serverID, "mapname",               GetCString(serverInfo, "mapnm"));
		m_pListener->UpdateValue(serverID, "numplayers", std::to_string(GetInt(serverInfo, "numpl")).c_str());
		m_pListener->UpdateValue(serverID, "maxplayers", std::to_string(GetInt(serverInfo, "maxpl")).c_str());
		m_pListener->UpdateValue(serverID, "password",              GetCString(serverInfo, "pass"));
		m_pListener->UpdateValue(serverID, "official",                  GetInt(serverInfo, "ranked") ? "1" : "0");

		const std::string version = "1.1.1." + std::to_string(GetInt(serverInfo, "ver"));
		m_pListener->UpdateValue(serverID, "gamever", version.c_str());

		m_pListener->UpdateValue(serverID, "gametype", GetGameType(serverInfo));

		const int timeLeft = GetTimeLeft(serverInfo);
		m_pListener->UpdateValue(serverID, "timelimit", timeLeft ? "1" : "0");
		m_pListener->UpdateValue(serverID, "timeleft", std::to_string(timeLeft).c_str());

		if (GetBool(serverInfo, "gs"))
		{
			m_pListener->UpdateValue(serverID, "mapname",   GetCString(serverInfo, "mapdnm"));
			m_pListener->UpdateValue(serverID, "anticheat",    GetBool(serverInfo, "anticheat")    ? "1" : "0");
			m_pListener->UpdateValue(serverID, "dedicated",    GetBool(serverInfo, "dedicated")    ? "1" : "0");
			m_pListener->UpdateValue(serverID, "dx10",         GetBool(serverInfo, "dx10")         ? "1" : "0");
			m_pListener->UpdateValue(serverID, "friendlyfire", GetBool(serverInfo, "friendlyfire") ? "1" : "0");
			m_pListener->UpdateValue(serverID, "gamepadsonly", GetBool(serverInfo, "gamepadsonly") ? "1" : "0");
			m_pListener->UpdateValue(serverID, "voicecomm",    GetBool(serverInfo, "voicecomm")    ? "1" : "0");

			int playerID = 0;

			for (const json & player : serverInfo["players"])
			{
				const char *name = GetCString(player, "name");

				const std::string team   = std::to_string(GetInt(player, "team"));
				const std::string rank   = std::to_string(GetInt(player, "rank"));
				const std::string kills  = std::to_string(GetInt(player, "kills"));
				const std::string deaths = std::to_string(GetInt(player, "deaths"));

				m_pListener->UpdatePlayerValue(serverID, playerID, "player", name);
				m_pListener->UpdatePlayerValue(serverID, playerID, "team", team.c_str());
				m_pListener->UpdatePlayerValue(serverID, playerID, "rank", rank.c_str());
				m_pListener->UpdatePlayerValue(serverID, playerID, "kills", kills.c_str());
				m_pListener->UpdatePlayerValue(serverID, playerID, "deaths", deaths.c_str());

				playerID++;
			}
		}
	}
	catch (const json::exception & ex)
	{
		CryLogAlways("$4[CryMP] Server info parse error: %s", ex.what());
		return false;
	}

	return true;
}

ServerBrowser::ServerBrowser()
{
}

ServerBrowser::~ServerBrowser()
{
}

bool ServerBrowser::IsAvailable() const
{
	return true;
}

void ServerBrowser::Start(bool browseLAN)
{
}

void ServerBrowser::SetListener(IServerListener *pListener)
{
	m_pListener = pListener;
}

void ServerBrowser::Stop()
{
	m_servers.clear();
}

void ServerBrowser::Update()
{
	m_servers.clear();

	// TODO: read from config file or whatever
	auto& masters = gClient->GetMasters();
	for (auto& master : masters) {
		const std::string url = gClient->GetMasterServerAPI(master) + "/servers?all&detailed&json";
		gClient->GetHTTPClient()->GET(url, [this, master](HTTPClientResult& result)
			{
				if (m_pListener)
				{
					if (result.error)
					{
						CryLogAlways("$4[CryMP] Server list update failed: %s", result.error.what());
					}
					else
					{
						OnServerList(master, result);
					}

					m_pListener->UpdateComplete(false);
				}
			});
	}
}

void ServerBrowser::UpdateServerInfo(int id)
{
	if (id < 0 || id >= m_servers.size())
		return;

	const std::string ip = IPToString(m_servers[id].ip);
	const std::string port = std::to_string(m_servers[id].port);
	const std::string master = m_servers[id].master;

	const std::string url = gClient->GetMasterServerAPI(master) + "/server?ip=" + ip + "&port=" + port + "&json";

	gClient->GetHTTPClient()->GET(url, [id, master, this](HTTPClientResult & result)
	{
		if (m_pListener)
		{
			if (result.error)
			{
				CryLogAlways("$4[CryMP] Server update failed: %s", result.error.what());
			}
			else
			{
				if (OnServerInfo(master, result, id))
					m_pListener->ServerUpdateComplete(id);
				else
					m_pListener->ServerUpdateFailed(id);
			}
		}
	});
}

void ServerBrowser::BrowseForServer(unsigned int ip, unsigned short port)
{
}

void ServerBrowser::BrowseForServer(const char *address, unsigned short port)
{
}

void ServerBrowser::SendNatCookie(unsigned int ip, unsigned short port, int cookie)
{
}

void ServerBrowser::CheckDirectConnect(int id, unsigned short port)
{
	if (!m_pListener || id < 0 || id >= m_servers.size())
		return;
	gEnv->pConsole->GetCVar("cl_masteraddr")->Set(m_servers[id].master.c_str());
	m_pListener->ServerDirectConnect(false, m_servers[id].ip, m_servers[id].port);
}

int ServerBrowser::GetServerCount()
{
	return m_servers.size();
}

int ServerBrowser::GetPendingQueryCount()
{
	return 0;
}
