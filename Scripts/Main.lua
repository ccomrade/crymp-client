System.LogAlways("$5[CryMP] Client loaded")

ALLOW_EXPERIMENTAL = true

function InitializeClient()
	local masters = nil
	local _L = {}
	local logAlways = System.LogAlways
	local EXPORT = false
	local EXPORTED = {}
	local RED, GREEN, YELLOW = "$4", "$3", "$6"
	local DEBUG_MODE = false
	local LAST_ACTOR = nil
	local LAST_RULES = nil
	local ACTIVE_RPC = nil
	local RPC_STATE  = true

	local pendingMasterResolves = {}
	local activeProfile = {
		static = {
			logged = false,
			name = nil,
			password = nil,
			id = nil,
			token = nil,
			display = nil,
			master = "crymp.net"
		},
		real = {
			logged = false,
			name = nil,
			password = nil,
			id = nil,
			token = nil,
			display = nil,
			master = "crymp.net"
		}
	};

	local localState = {};
	local addCCommand = System.AddCCommand;
	local getCVar = System.GetCVar;

	local function OnMasterResolved()
		masters = CPPAPI.GetMasters()
		printf("$5[CryMP] Resolved masters:", #masters)
		for i, master in pairs(masters) do
			printf("$5[CryMP] - %s", master)
		end
		for i, resolve in pairs(pendingMasterResolves) do
			-- first master gets to be the default master
			resolve(masters[1])
		end
		pendingMasterResolves = {}
	end

	local function _pcall(func, ...)
		local status,err=pcall(func,...);
		if not status then
			_L.System.Log("$6Lua Error: " .. err);
		end
		return err;
	end

	local function lerp(a, b, t)
		if type(a) == "table" and type(b) == "table" then
			if a.x and a.y and b.x and b.y then
				if a.z and b.z then return lerp3(a, b, t) end
				return lerp2(a, b, t)
			end
		end
		t = clamp(t, 0, 1)
		return a + t*(b-a)
	end

	local function _lerp(a, b, t)
		return a + t*(b-a)
	end

	local function lerp2(a, b, t)
		t = clamp(t, 0, 1)
		return { x = _lerp(a.x, b.x, t); y = _lerp(a.y, b.y, t); };
	end

	local function lerp3(a, b, t)
		t = clamp(t, 0, 1)
		return { x = _lerp(a.x, b.x, t); y = _lerp(a.y, b.y, t); z = _lerp(a.z, b.z, t); };
	end

	local function clamp(a, b, t)
		if a < b then return b end
		if a > t then return t end
		return a
	end

	local function printf(fmt, ...)
		if (#{...}) == 0 then return logAlways(fmt) end
		logAlways(fmt:format(...))
	end

	local function FixFlaws()
		local ALLOWED_CVARS = {
			"g_playerHealthValue", "g_blood", "g_ragdollMinTime", "cl_hitShake", "cl_hitBlur",
			"g_ragdollPollTime", "g_ragdollDistance", "g_ragdollUnseenTime",
			"cl_motionBlur", "cl_sprintBlur", "cl_fov", "g_tk_punish", "g_tk_punish_limit",
			"g_pp_scale_income", "g_pp_scale_price", "g_energy_scale_price", "g_energy_scale_income",
			"g_suitArmorHealthValue", "g_PlayerFallAndPlay", "g_difficultyLevel", "g_godMode",
			"g_playerRespawns", "g_fallAndPlayThreshold", "g_difficultyHintSystem",
			"g_difficultyRadius", "g_difficultyRadiusThreshold", "g_difficultyRadiusThreshold",
			"g_difficultySaveThreshold", "g_punishFriendlyDeaths", "g_enableloadingscreen",
			"g_enableitems"
		}

		local BANNED_CVARS = {
			"sv_password"
		}

		local ALLOWED_CVARS_DIC = {}
		local BANNED_CVARS_DIC = {}
		for i,v in pairs(ALLOWED_CVARS) do
			ALLOWED_CVARS_DIC[v] = true
		end
		for i,v in pairs(BANNED_CVARS) do
			BANNED_CVARS_DIC[v] = true
		end
		System.AddCCommand = function(name, ...)
			if name == "login" or name == "simple_login" or name == "auth_login" then
				System.LogAlways("$4[error] Cannot override these!")
				return nil
			end
			return addCCommand(name, ...)
		end
		System.GetCVar = function(cvar)
			if BANNED_CVARS_DIC[cvar] then
				return nil
			end
			return getCVar(cvar)
		end
	end

	local function RemoveFlaws()
		io = nil
		for i, v in pairs(os) do
			if not (i == "clock" or i == "time") then
				os[i] = nil
			end
		end
		System.ScanDirectory = nil
		FixFlaws()
	end

	local function RestoreEnvironment()
		for i, v in pairs(EXPORTED) do
			_G[i] = v
		end
		for i, v in pairs(_L) do
			if type(_G[i]) ~= "table" then
				printf(RED .. "[sfwcl] Server changed global " .. YELLOW .. i .. RED.. ", reverting back!")
				_G[i] = {}
				v = _G[i]
			end
			for j, w in pairs(v) do
				if _G[i][j] ~= w then
					printf(RED .. "[sfwcl] Server changed global function " .. YELLOW .. i .. "." .. j .. RED .. ", reverting back!")
				end
				_G[i][j] = w
			end
		end
		RemoveFlaws()
	end

	local function ResetState()
		RestoreEnvironment()
		localState = {
			ACTIVE_ANIMATIONS = {},
			ACTIVE_EFFECTS = {},
			STATIC_ID = activeProfile.static.id or "Unknown",
			STATIC_HASH = activeProfile.static.token or "Unknown",
			KEY_BINDINGS = {},
			SPAWNED_FOG_VOLUMES = {},
			SPAWNED_LIGHT_ENTITIES = {},
			HOOKS = {
				Counter = 0,
				OnUpdate = {},
				OnSpawn = {}
			}
		}
	end

	ResetState()

	local function CallHooks(name, ...)
		for i, v in pairs(localState.HOOKS[name]) do
			if type(v) == "function" then
				local ok, err = pcall(v, dt)
				if not ok then
					_L.System.LogAlways("$4 [hook] Error during "..name.." hook (id=" .. tostring(i) .. "): " .. tostring(err))
				end
			end
		end
	end

	local function UpdateWorld(localState, dt)
		for i,params in pairs(localState.ACTIVE_ANIMATIONS) do
			if params then
				local ent = params.entity;
				if not ent then params.entity = _L.System.GetEntityByName(params.name); ent = params.entity; end
				if ent then
					local dur = _time - params.start;

					if params.pos then
						ent:SetWorldPos(lerp(params.pos.from, params.pos.to, dur / params.duration))
					end

					if params.scale then
						ent:SetScale(lerp(params.scale.from, params.scale.to, dur / params.duration))
					end

					if dur >= params.duration then
						localState.ACTIVE_ANIMATIONS[i] = nil
					end
				end
			end
		end

		for i,v in pairs(localState.ACTIVE_EFFECTS) do
			if v~=nil then
				_L.System.SetScreenFx(i, v)
			end
		end

		CallHooks("OnUpdate", dt)
	end

	local function PerformKeyBind(key)
		if localState.KEY_BINDINGS[key] then
			g_gameRules.game:SendChatMessage(ChatToTarget, g_localActor.id, g_localActor.id, localState.KEY_BINDINGS[key]);
		end
	end

	local function CreateBinding(key, action)
		if not localState.KEY_BINDINGS[key] then
			_L.CPPAPI.AddCCommand("bind_action_"..key, function()
				PerformKeyBind(key)
			end)
			_L.System.ExecuteCommand("bind " .. key .. " bind_action_"..key)
		end
		localState.KEY_BINDINGS[key] = action
	end

	local function AddHook(hookType, name, fn)
		localState.HOOKS.Counter = localState.HOOKS.Counter + 1
		if type(name) == "function" then
			fn = name
			name = tostring(localState.HOOKS.Counter)
		end
		localState.HOOKS[hookType] = localState.HOOKS[hookType] or {}
		localState.HOOKS[hookType][name] = fn
		return name
	end

	local function RemoveHook(hookType, name)
		localState.HOOKS[hookType] = localState.HOOKS[hookType] or {}
		localState.HOOKS[hookType][name] = nil
	end

	local function GetProfile()
		if activeProfile.real.logged then
			return activeProfile.real
		elseif activeProfile.static.logged then
			return activeProfile.static
		else
			return nil
		end
	end

	local function Promise(callback)
		local Ancestor = {
			_PrematureResolveFlag = false,
			_PrematureRejectFlag = false,
			_PrematureResolve = nil,
			_PrematureReject = nil,
			_Handler = nil,
			_Catch = nil,
			_Promise = false,
			Then = function(self, handler)
				self._Handler = handler
				if self._PrematureResolveFlag then
					self:CallThen(self._PrematureResolve)
				end
				return self
			end,
			Catch = function(self, handler)
				self._Catch = handler
				if self._PrematureRejectFlag then
					self:CallCatch(self._PrematureReject)
				end
				return self
			end,
			CallThen = function(self, response)
				if self._Handler ~= nil then
					self._Handler(response)
					self._Handler = nil
				else
					self._PrematureResolveFlag = true
					self._PrematureResolve = response
				end
			end,
			CallCatch = function(self, response)
				if self._Catch ~= nil then
					self._Catch(response)
					self._Catch = nil
				else
					self._PrematureRejectFlag = true
					self._PrematureReject = response
				end
			end,
		}
		local Resolver = function(response)
			Ancestor:CallThen(response)
		end
		local Rejector = function(response)
			Ancestor:CallCatch(response)
		end
		callback(Resolver, Rejector)
		return Ancestor
	end

	local function Request(params)
		return Promise(function(resolve, reject)
			CPPAPI.Request(params, function(error, response, code)
				if error then return reject(error) end
				return resolve(response)
			end)
		end)
	end

	local function GET(url)
		return Request({
			url = url,
			method = "GET"
		})
	end

	local function POST(url, data, contentType)
		contentType = contentType or "application/x-www-form-urlencoded; charset=utf-8"
		local body = ""
		if type(data) == "table" then
			local kv = {}
			for i, v in pairs(data) do
				table.insert(kv, tostring(i) .. "=" .. CPPAPI.URLEncode(v))
			end
			body = table.concat(kv, "&")
		elseif type(data) == "string" then
			body = data
		end
		return Request({
			url = url,
			body = body,
			method = "POST",
			headers = {
				["Content-type"] = contentType
			}
		})
	end

	local function GetMaster()
		return Promise(function(resolve)
			if masters ~= nil then
				return resolve(masters[1])
			end
			table.insert(pendingMasterResolves, resolve)
		end)
	end

	local function GetStaticID()
		return Promise(function(resolve, reject)
			GetMaster()
			:Then(function(defaultMaster)
				local endpoint = "https://" .. defaultMaster .. "/api"
				GET(endpoint .. "/id")
				:Then(function(response)
					local id, token = string.match(response,"([0-9]+)/([0-9a-fA-F]+)")
					if id and token then
						return resolve({id = id, token = token, name = "::tr:" .. id, password = token, logged = true, master = defaultMaster})
					end
					reject("Couldn't obtain static profile ID")
				end)
				:Catch(function(error)
					reject("Couldn't obtain static profile ID, error: " .. tostring(error))
				end)
			end)
		end)
	end

	local function ObtainStaticID()
		GetStaticID()
		:Then(function(profile)
			printf(GREEN .. "Successfully obtained static profile: %s", profile.id)
			activeProfile.static = profile
			localState.STATIC_ID = profile.id
			localState.STATIC_HASH = profile.token
		end)
		:Catch(function(error)
			printf(RED .. error)
		end)
	end

	local function Login(name, password, profileType)
		profileType = profileType or "real"
		return Promise(function(resolve, reject)
			GetMaster():Then(function(defaultMaster)
				local endpoint = "https://" .. defaultMaster .. "/api"
				local authHost = defaultMaster
				local authEndpoint = endpoint
				local prefix, postfix = name:match("([^@]+)@(.+)")
				if prefix and postfix then
					name = prefix
					authHost = postfix
					authEndpoint = "https://" .. postfix .. "/api"
				else
					authHost = defaultMaster
				end
				POST(authEndpoint .. "/login", {
					a = name,
					b = password
				})
				:Then(function(response)
					if response == "FAIL" then
						reject("Invalid credentials")
					else
						local id, token, display = response:match("([0-9]+),([a-zA-Z0-9_]+),(.*)")
						activeProfile[profileType] = {
							logged = true,
							name = name,
							password = password,
							id = id,
							token = token,
							display = display,
							master = authHost
						}
						resolve(activeProfile[profileType])
					end
				end)
				:Catch(reject)
			end)
		end)
	end

	local function RefreshSession()
		return Promise(function(resolve, reject)
			local profile = GetProfile()
			if not profile then return reject("Couldn't find any profile") end
			local profileName = profile.name
			if profile.master ~= "crymp.net" then
				profileName = profileName .. "@" .. profile.master
			end
			Login(profileName, profile.password)
			:Then(function(session)
				resolve(session)
			end)
			:Catch(function(error)
				reject(error)
			end)
		end)
	end

	local function LoginCCommandHandler(name, password)
		Login(name, password)
		:Then(function(response)
			printf(GREEN .. "Successfully logged in, profile ID: %s", activeProfile.real.id)
		end)
		:Catch(function(error)
			printf(RED .. "Log-in failed")
		end)
	end

	local function GetValidateCommand(profile)
		local id = profile.id
		if profile.master ~= "crymp.net" then
			id = id .. "@" .. profile.master
		end
		local command = string.format("!validate %s %s %s", id, profile.token, profile.display or "Nomad")
		return command
	end

	local function Authenticate()
		return Promise(function(resolve, reject)
			if not g_localActor then
				printf(RED .. "You are not in-game")
				return resolve(false)
			end
			local profile = GetProfile()
			if profile then
				RefreshSession()
				:Then(function(session)
					profile = GetProfile()
					local command = GetValidateCommand(profile)
					g_gameRules.game:SendChatMessage(ChatToTarget, g_localActor.id, g_localActor.id, command)
					resolve(true)
				end)
				:Catch(function(error)
					printf(RED .. "Failed to reactivate session, using old profile")
					local command = GetValidateCommand(profile)
					g_gameRules.game:SendChatMessage(ChatToTarget, g_localActor.id, g_localActor.id, command)
					resolve(true)
				end)
			else
				printf(RED .. "Cannot authenticate due to missing profile")
				return resolve(false)
			end
		end)
	end

	local function HookedStartWorking(self, entityId, workName)
		if workName:sub(1, 1) == "@" and ACTIVE_RPC ~= nil then
			if ALLOW_EXPERIMENTAL then
				_L.System.Log("Received RPC call: " .. workName)
			end
			local what = json.decode(workName:sub(2))
			if what and what.method then
				if ALLOW_EXPERIMENTAL then
					_L.System.Log("Resolved RPC method: " .. what.method)
				end
				local method = ACTIVE_RPC[what.method]
				if RPC_STATE then
					if what.class then
						method = ACTIVE_RPC[what.class].method
						if method then
							_pcall(method, ACTIVE_RPC[what.class], what.params, what.id)
						end
					elseif method then
						_pcall(method, what.params, what.id)
					end
				end
			end
		else
			self.work_type=workName;
			self.work_name="@ui_work_"..workName;
			HUD.SetProgressBar(true, 0, self.work_name);
		end
	end

	local function OnUpdate(dt)
		if g_gameRules ~= nil then
			if g_gameRules ~= LAST_RULES then
				LAST_RULES = g_gameRules
				LAST_ACTOR = nil
			end

			if g_localActor ~= LAST_ACTOR then
				LAST_ACTOR = g_localActor
				Authenticate()
			end

			if g_gameRules.Client.ClStartWorking ~= HookedStartWorking then
				g_gameRules.Client.ClStartWorking = HookedStartWorking
			end

			UpdateWorld(localState, dt)
		end
	end

	local function OnDisconnect(reason, message)
		printf("Disconnect: %d %s", reason, message)
		ResetState()
	end

	local function OnSpawn(entityId)
		local entity = _L.System.GetEntity(entityId)
		if entity then
			--printf("Spawned entity %s of class %s", entity:GetName(), entity.class)
		end
	end

	CPPAPI.SetCallback(SCRIPT_CALLBACK_ON_UPDATE, OnUpdate)
	CPPAPI.SetCallback(SCRIPT_CALLBACK_ON_DISCONNECT, OnDisconnect)
	CPPAPI.SetCallback(SCRIPT_CALLBACK_ON_SPAWN, OnSpawn)
	CPPAPI.SetCallback(SCRIPT_CALLBACK_ON_MASTER_RESOLVED, OnMasterResolved)

	CPPAPI.AddCCommand("secu_login", LoginCCommandHandler)
	CPPAPI.AddCCommand("simple_login", LoginCCommandHandler)
	CPPAPI.AddCCommand("login", LoginCCommandHandler)
	CPPAPI.AddCCommand("static_id", ObtainStaticID)
	CPPAPI.AddCCommand("sfwcl_refresh", function()
		RefreshSession()
		:Then(function(session)
			printf(GREEN .. "Successfully refreshed session: %s", session.id)
		end)
		:Catch(function(error)
			printf(RED .. "Failed to refresh session: " .. tostring(error))
		end)
	end)
	CPPAPI.AddCCommand("auth_login", function()
		Authenticate()
	end)
	CPPAPI.AddCCommand("sfwcl_debug", function()
		DEBUG_MODE = not DEBUG_MODE
		if DEBUG_MODE then
			printf("$3Debug mode active")
		else
			printf("$4Debug mode disabled")
		end
	end)

	ObtainStaticID()
	RemoveFlaws()

	ACTIVE_RPC = CreateRPC(localState, _L)
	EXPORTED.RPC = ACTIVE_RPC
	EXPORTED.ActiveAnims = localState.ACTIVE_ANIMATIONS
	EXPORTED._pcall = _pcall
	EXPORTED.lerp = lerp
	EXPORTED.lerp2 = lerp2
	EXPORTED.lerp3 = lerp3
	EXPORTED.clamp = clamp
	EXPORTED.printf = printf
	EXPORTED.AddHook = AddHook
	EXPORTED.RemoveHook = RemoveHook
	EXPORTED.CreateBinding = CreateBinding

	if EXPORT then
		EXPORTED.Request = Request
		EXPORTED.Promise = Promise
		EXPORTED.GET = GET
		EXPORTED.POST = POST
	end

	for i,v in pairs(EXPORTED) do
		_G[i] = v
	end

	for i,v in pairs(_G) do
		if type(v) == "table" and i ~= "_G" then
			_L[i] = {}
			for j, w in pairs(v) do
				if type(w) == "function" then
					_L[i][j] = w
				end
			end
		end
	end
	return true
end

local err, res = pcall(InitializeClient)
if err then
	System.LogAlways("$4[CryMP] Failed to InitializeClient: %s", tostring(err))
end