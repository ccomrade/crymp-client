#pragma once

#include <array>

#include "CryCommon/CryScriptSystem/IScriptSystem.h"

struct IEntity;

enum EScriptCallback
{
	SCRIPT_CALLBACK_ON_UPDATE,
	SCRIPT_CALLBACK_ON_DISCONNECT,
	SCRIPT_CALLBACK_ON_SPAWN,
	SCRIPT_CALLBACK_ON_MASTER_RESOLVED,

	// must be last
	SCRIPT_CALLBACK_COUNT
};

class ScriptCallbacks
{
	IScriptSystem *m_pSS = nullptr;
	std::array<HSCRIPTFUNCTION, SCRIPT_CALLBACK_COUNT> m_handlers = {};

	template<class... Params>
	void Call(EScriptCallback callback, const Params &... params)
	{
		HSCRIPTFUNCTION handler = m_handlers[callback];

		if (handler && m_pSS->BeginCall(handler))
		{
			(m_pSS->PushFuncParam(params), ...);
			m_pSS->EndCall();
		}
	}

public:
	ScriptCallbacks();
	~ScriptCallbacks();

	bool SetHandler(EScriptCallback callback, HSCRIPTFUNCTION handler);

	void OnUpdate(float deltaTime);
	void OnDisconnect(int reason, const char *message);
	void OnSpawn(IEntity *pEntity);
	void OnMasterResolved();
};
