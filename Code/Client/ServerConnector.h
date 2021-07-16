#pragma once

#include <string>
#include <string_view>

struct HTTPClientResult;

class ServerConnector
{
	struct ServerInfo
	{
		std::string host;
		std::string master;
		unsigned int port = 0;

		std::string name;
		std::string map;
		std::string mapURL;
		std::string pakURL;

		void clear()
		{
			host.clear();
			master.clear();
			port = 0;

			name.clear();
			map.clear();
			mapURL.clear();
			pakURL.clear();
		}
	};

	ServerInfo m_server;
	unsigned int m_contractID = 0;

	bool ParseServerInfo(HTTPClientResult & result);
	void SetLoadingDialogText(const char *text);
	void SetLoadingDialogText(const char *label, const char *param);  // for localized stuff
	void ShowErrorBox(const char *text);
	void ResetCVars();

	void Step1_RequestServerInfo();
	void Step2_DownloadMap();
	void Step3_DownloadPAK();
	void Step4_TryConnect();

public:
	ServerConnector();
	~ServerConnector();

	void Connect(const std::string& master, const std::string_view & host, unsigned int port);
	void Disconnect();
};
