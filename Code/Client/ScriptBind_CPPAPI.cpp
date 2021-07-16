#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CrySystem/IConsole.h"
#include "CryCommon/CryScriptSystem/IScriptSystem.h"
#include "Library/Util.h"
#include "Library/WinAPI.h"

#include "ScriptBind_CPPAPI.h"
#include "Client.h"
#include "HTTPClient.h"
#include "ScriptCommands.h"
#include "ScriptCallbacks.h"

ScriptBind_CPPAPI::ScriptBind_CPPAPI()
{
	Init(gEnv->pScriptSystem, gEnv->pSystem);

	SetGlobalName("CPPAPI");

#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &ScriptBind_CPPAPI::

	SCRIPT_REG_GLOBAL(SCRIPT_CALLBACK_ON_UPDATE);
	SCRIPT_REG_GLOBAL(SCRIPT_CALLBACK_ON_DISCONNECT);
	SCRIPT_REG_GLOBAL(SCRIPT_CALLBACK_ON_SPAWN);
	SCRIPT_REG_GLOBAL(SCRIPT_CALLBACK_ON_MASTER_RESOLVED);

	SCRIPT_REG_TEMPLFUNC(AddCCommand, "name, handler");
	SCRIPT_REG_TEMPLFUNC(ApplyMaskAll, "mask, apply");
	SCRIPT_REG_TEMPLFUNC(ApplyMaskOne, "entity, mask, apply");
	SCRIPT_REG_TEMPLFUNC(FSetCVar, "cvar, value");
	SCRIPT_REG_TEMPLFUNC(GetLocaleInformation, "");
	SCRIPT_REG_TEMPLFUNC(GetMapName, "");
	SCRIPT_REG_TEMPLFUNC(MakeUUID, "salt");
	SCRIPT_REG_TEMPLFUNC(Random, "");
	SCRIPT_REG_TEMPLFUNC(Request, "params, callback");
	SCRIPT_REG_TEMPLFUNC(SetCallback, "callback, handler");
	SCRIPT_REG_TEMPLFUNC(SHA256, "text");
	SCRIPT_REG_TEMPLFUNC(URLEncode, "text");
	SCRIPT_REG_TEMPLFUNC(GetMasters, "");
}

ScriptBind_CPPAPI::~ScriptBind_CPPAPI()
{
}

int ScriptBind_CPPAPI::AddCCommand(IFunctionHandler *pH, const char *name, HSCRIPTFUNCTION handler)
{
	bool success = gClient->GetScriptCommands()->AddCommand(name, handler);

	return pH->EndFunction(success);
}

int ScriptBind_CPPAPI::ApplyMaskAll(IFunctionHandler *pH, int mask, bool apply)
{
	// TODO: misterSD needs this
	CryLogWarningAlways("CPPAPI.ApplyMaskAll is not implemented!");
	return pH->EndFunction();
}

int ScriptBind_CPPAPI::ApplyMaskOne(IFunctionHandler *pH, ScriptHandle entity, int mask, bool apply)
{
	// TODO: misterSD needs this
	CryLogWarningAlways("CPPAPI.ApplyMaskOne is not implemented!");
	return pH->EndFunction();
}

int ScriptBind_CPPAPI::FSetCVar(IFunctionHandler *pH, const char *cvar, const char *value)
{
	bool success = false;

	if (ICVar *pCvar = gEnv->pConsole->GetCVar(cvar))
	{
		pCvar->ForceSet(value);
		success = true;
	}

	return pH->EndFunction(success);
}

int ScriptBind_CPPAPI::GetLocaleInformation(IFunctionHandler *pH)
{
	return pH->EndFunction(WinAPI::GetLocale().c_str(), WinAPI::GetTimeZoneBias());
}

int ScriptBind_CPPAPI::GetMapName(IFunctionHandler *pH)
{
	return pH->EndFunction(gClient->GetGameFramework()->GetLevelName());
}

int ScriptBind_CPPAPI::MakeUUID(IFunctionHandler *pH, const char *salt)
{
	return pH->EndFunction(gClient->GetHWID(salt).c_str());
}

int ScriptBind_CPPAPI::Random(IFunctionHandler *pH)
{
	return pH->EndFunction(gClient->GetRandomNumber<float>(0, 1));
}

int ScriptBind_CPPAPI::Request(IFunctionHandler *pH, SmartScriptTable params, HSCRIPTFUNCTION callback)
{
	HTTPClientRequest request;

	const char *url;
	if (params->GetValue("url", url))
		request.url = url;
	else
		return pH->EndFunction(false, "url not provided");

	const char *method;
	if (params->GetValue("method", method))
		request.method = method;

	const char *body;
	if (params->GetValue("body", body))
		request.data = body;

	int timeout;
	if (params->GetValue("timeout", timeout))
		request.timeout = timeout;

	SmartScriptTable headersTable;
	if (params->GetValue("headers", headersTable))
	{
		auto it = headersTable->BeginIteration();
		while (headersTable->MoveNext(it))
		{
			if (it.value.GetVarType() == ScriptVarType::svtString)
			{
				request.headers[it.sKey] = it.value.str;
			}
		}
		headersTable->EndIteration(it);
	}

	request.callback = [callback, this](HTTPClientResult & result)
	{
		if (m_pSS->BeginCall(callback))
		{
			if (result.error)
				m_pSS->PushFuncParam(result.error.what());
			else
				m_pSS->PushFuncParam(false);

			m_pSS->PushFuncParam(result.response.c_str());
			m_pSS->PushFuncParam(result.code);
			m_pSS->EndCall();
		}
	};

	gClient->GetHTTPClient()->Request(std::move(request));

	return pH->EndFunction(true);
}

int ScriptBind_CPPAPI::SetCallback(IFunctionHandler *pH, int callback, HSCRIPTFUNCTION handler)
{
	bool success = gClient->GetScriptCallbacks()->SetHandler(static_cast<EScriptCallback>(callback), handler);

	return pH->EndFunction(success);
}

int ScriptBind_CPPAPI::SHA256(IFunctionHandler *pH, const char *text)
{
	return pH->EndFunction(Util::SHA256(text).c_str());
}

int ScriptBind_CPPAPI::URLEncode(IFunctionHandler *pH, const char *text)
{
	return pH->EndFunction(HTTP::URLEncode(text).c_str());
}

int ScriptBind_CPPAPI::GetMasters(IFunctionHandler* pH)
{
	auto masters = gClient->GetMasters();
	SmartScriptTable table;
	if (!table.Create(m_pSS)) return pH->EndFunction(false);
	for (auto& master : masters) {
		table->PushBack(master.c_str());
	}
	return pH->EndFunction(table);
}