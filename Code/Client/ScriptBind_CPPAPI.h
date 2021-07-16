#pragma once

#include "CryCommon/CryScriptSystem/IScriptSystem.h"

class ScriptBind_CPPAPI : public CScriptableBase
{
public:
	ScriptBind_CPPAPI();
	~ScriptBind_CPPAPI();

	int AddCCommand(IFunctionHandler *pH, const char *name, HSCRIPTFUNCTION handler);
	int ApplyMaskAll(IFunctionHandler *pH, int mask, bool apply);
	int ApplyMaskOne(IFunctionHandler *pH, ScriptHandle entity, int mask, bool apply);
	int FSetCVar(IFunctionHandler *pH, const char *cvar, const char *value);
	int GetLocaleInformation(IFunctionHandler *pH);
	int GetMapName(IFunctionHandler *pH);
	int MakeUUID(IFunctionHandler *pH, const char *salt);
	int Random(IFunctionHandler *pH);
	int Request(IFunctionHandler *pH, SmartScriptTable params, HSCRIPTFUNCTION callback);
	int SetCallback(IFunctionHandler *pH, int callback, HSCRIPTFUNCTION handler);
	int SHA256(IFunctionHandler *pH, const char *text);
	int URLEncode(IFunctionHandler *pH, const char *text);
	int GetMasters(IFunctionHandler *pH);
};
