#pragma once

#include <string>

class ServerPAK
{
	std::string m_path;

public:
	ServerPAK();
	~ServerPAK();

	bool IsZipFile(const std::string & path);
	bool Load(const std::string & path);
	bool Unload();
	void OnDisconnect(int reason, const char* message);
	void ReloadEntityScripts();
};
