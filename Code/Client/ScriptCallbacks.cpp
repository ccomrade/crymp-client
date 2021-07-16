#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CryEntitySystem/IEntity.h"

#include "ScriptCallbacks.h"

ScriptCallbacks::ScriptCallbacks()
{
	m_pSS = gEnv->pScriptSystem;
}

ScriptCallbacks::~ScriptCallbacks()
{
}

bool ScriptCallbacks::SetHandler(EScriptCallback callback, HSCRIPTFUNCTION handler)
{
	if (callback < 0 || callback >= SCRIPT_CALLBACK_COUNT)
	{
		// invalid callback
		return false;
	}

	if (m_handlers[callback])
	{
		// handlers cannot be replaced
		return false;
	}

	m_handlers[callback] = handler;

	return true;
}

void ScriptCallbacks::OnUpdate(float deltaTime)
{
	Call(SCRIPT_CALLBACK_ON_UPDATE, deltaTime);
}

void ScriptCallbacks::OnDisconnect(int reason, const char *message)
{
	Call(SCRIPT_CALLBACK_ON_DISCONNECT, reason, message);
}

void ScriptCallbacks::OnSpawn(IEntity *pEntity)
{
	ScriptHandle entityId;
	entityId.n = pEntity->GetId();

	Call(SCRIPT_CALLBACK_ON_SPAWN, entityId);
}

void ScriptCallbacks::OnMasterResolved() {
	Call(SCRIPT_CALLBACK_ON_MASTER_RESOLVED, true);
}
