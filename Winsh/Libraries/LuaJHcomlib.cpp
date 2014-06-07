#include "stdafx.h"
#define LUACMLIB_BUILDING
#include "LibClass.h"
#include "LuaJHcomlib.h"
#include "..\LuaLibIF.h"
#include "..\resource.h"

int com_ix = LUA_NOREF; //Index table index
int com_msg = 0;		//Lua Message used for data ready notification
HWND com_hwnd = 0;		//Window to send the notifier message to

static int com_innercb(lua_State* L);

CommLua::CommLua(COMMSETTINGS* cs, DCB* dcb, COMMTIMEOUTS* cto) : JHCommBase()
{
	memcpy(&comsettings, cs, sizeof(COMMSETTINGS));
	memcpy(&devicecontrol, dcb, sizeof(DCB));
	comtimeouts = *cto;
	bytes = 0;
	cat = 0;
	csize = 0;
	max = 0;
	handler = 0;
}

//This is called by the base before openning the port.
void CommLua::OnOpen(COMMSETTINGS* cs, DCB* dcb, COMMTIMEOUTS* cto)
{
	memcpy(cs, &comsettings, sizeof(COMMSETTINGS));
	memcpy(dcb, &devicecontrol, sizeof(DCB));
	*cto = comtimeouts;
}

// Virtual override from base class, called when a byte is received.
bool CommLua::OnRxChar(BYTE ch)
{
	BOOL done = FALSE;
	inbuf[bytes] = ch; bytes++;
	if (csize > 0)
	{
		if (ch == compare[cat++])
		{
			if (cat >= csize) done = TRUE;
		}
		else
		{
			cat = 0;
			if ((csize > 1) && (ch == compare[0])) cat = 1;
		}
	}
	if (bytes >= max) done = TRUE;
	if (done)
	{
		// Process the string.
		cat = 0;
		SetEvent(rxevent);
		PostMessage(com_hwnd, WM_COMMAND, MAKEWPARAM(com_msg, this->comsettings.PortNumber), (LPARAM)bytes);
		return false;
	}
	return true;
}

// Virtual override from base class, called when all bytes queued have been transmitted.
void CommLua::OnTxDone()
{
	Lock();
	Unlock();
}
// Virtual override from base class, called when a communications error occurs.
void CommLua::OnError(int error)
{
	Lock();
	Unlock();
}

// Lua scripting object "Com".

static int luacmd = 0;

static int LuaComOpen(lua_State* L)
{
	WINSH_LUA(2)
	luaC_checkmethod(L);
	CommLua* cc = (CommLua*)lua_touserdata(L, 1);
	cc->Close();
	if (lua_gettop(L) > 1)
	{
		cc->comsettings.PortNumber = luaL_checkinteger(L, 2);
		if (cc->comsettings.PortNumber < 1) cc->comsettings.PortNumber = 1;
		if (cc->comsettings.PortNumber > 99) cc->comsettings.PortNumber = 99;
	}
	int r = cc->Open();
	switch (r) {
	case 0:
		lua_pushinteger(L, cc->comsettings.PortNumber);
		return 1;
	case -107:
		lua_pushnil(L);
		return 1;
	default:
		lua_pushboolean(L, FALSE);
		lua_pushinteger(L, r);
		return 2;
	}
}

static int LuaComClose(lua_State* L)
{
	WINSH_LUA(1)
	luaC_checkmethod(L);
	CommLua* cc = (CommLua*)lua_touserdata(L, 1);
	cc->Close();
	return 0;
}

static int LuaComWaitForInput(lua_State* L)
{
	WINSH_LUA(2)
	luaC_checkmethod(L);
	CommLua* cc = (CommLua*)lua_touserdata(L, 1);
	int tout = 20000;
	if (lua_type(L, 2) == LUA_TSTRING)
	{
		const char* str = lua_tostring(L, 2);
		cc->csize = lua_rawlen(L, 2);
		if (cc->csize > CP_BUF_SIZE) cc->csize = CP_BUF_SIZE;
		for (UINT i = 0; (i < cc->csize); i++) cc->compare[i] = str[i];
		cc->max = RX_BUF_SIZE;
	}
	else if (lua_type(L, 2) == LUA_TNUMBER)
	{
		cc->max = cc->bytes + lua_tointeger(L, 2);
		if (cc->max > RX_BUF_SIZE) cc->max = RX_BUF_SIZE;
		cc->csize = 0;
	}
	else
	{
		cc->max = 1;
		cc->csize = 0;
	}
	if (lua_isnumber(L, 3))
	{
		tout = lua_tointeger(L, 3);
	}
	cc->Receive();
	if (WaitForSingleObject(cc->rxevent, tout) == 0)
		lua_pushinteger(L, cc->bytes);
	else
		lua_pushnil(L);
	return 1;
}

static int LuaComSendCommand(lua_State* L)
{
	WINSH_LUA(2)
	luaC_checkmethod(L);
	CommLua* cc = (CommLua*)lua_touserdata(L, 1);
	size_t c = 0;
	const char* str = luaL_checklstring(L, 2, &c);
	if (c > 0)
	{
		LPBYTE buf = new BYTE[c+1];
		for (UINT i = 0; (i <= c); i++) buf[i] = str[i];
		cc->Send(buf, (int)c);
//		cc->CheckSend(true);
		delete buf;
	}
	lua_remove(L, 2);
	cc->bytes = 0;
	if (lua_isnoneornil(L, 2)){
		lua_pushboolean(L, TRUE);
		return 1;
	}
	return LuaComWaitForInput(L);
}

static int LuaComGetData(lua_State* L)
{
	WINSH_LUA(1)
	luaC_checkmethod(L);
	CommLua* cc = (CommLua*)lua_touserdata(L, 1);
	lua_pushlstring(L, (const char*)cc->inbuf, cc->bytes);
	return 1;
}

static int LuaComOnInput(lua_State* L)
{
	WINSH_LUA(4)
	luaC_checkmethod(L);
	if (lua_gettop(L) != 3) luaL_error(L, "com:oninput requires two parameters");
	if (!lua_isfunction(L, 3)) luaL_error(L, "com:oninput second parameter must be a function");
	CommLua* cc = (CommLua*)lua_touserdata(L, 1);
	if (lua_type(L, 2) == LUA_TSTRING)
	{
		const char* str = lua_tostring(L, 2);
		cc->csize = lua_rawlen(L, 2);
		if (cc->csize > CP_BUF_SIZE) cc->csize = CP_BUF_SIZE;
		for (UINT i = 0; (i < cc->csize); i++) cc->compare[i] = str[i];
		cc->max = RX_BUF_SIZE;
	}
	else if (lua_type(L, 2) == LUA_TNUMBER)
	{
		cc->max = cc->bytes + lua_tointeger(L, 2);
		if (cc->max > RX_BUF_SIZE) cc->max = RX_BUF_SIZE;
		cc->csize = 0;
	}
	else
	{
		cc->max = 1;
		cc->csize = 0;
	}
	H->GetRegistryTable(com_ix);
	lua_pushinteger(L, cc->comsettings.PortNumber);
	lua_pushvalue(L, 3);
	lua_pushvalue(L, 1);
	lua_pushcclosure(L, com_innercb, 2);
	lua_settable(L, -3);
	lua_pop(L, 1);
	return cc->bytes;
}

static int LuaComOnMessage(lua_State* L)
{
	WINSH_LUA(3)
	luaC_checkmethod(L);
	if (lua_gettop(L) != 3) luaL_error(L, "com:onmessage requires two parameters");
	if (!lua_isfunction(L, 3)) luaL_error(L, "com:onmessage second parameter must be a function");
	CommLua* cc = (CommLua*)lua_touserdata(L, 1);
	cc->bytes = 0;
	return LuaComOnInput(L);
}

static int LuaComWaitCTS(lua_State* L)
{
	WINSH_LUA(1)
	luaC_checkmethod(L);
	CommLua* cc = (CommLua*)lua_touserdata(L, 1);
	int tout = 20000;
	if (lua_isnumber(L, 2))
	{
		tout = lua_tointeger(L, 2);
	}
	while ((!cc->GetCTS()) && (tout > 0)) {tout -= 100; Sleep(100);}
	lua_pushboolean(L, cc->GetCTS());
	return 1;
}

static int LuaComWaitDSR(lua_State* L)
{
	WINSH_LUA(1)
	luaC_checkmethod(L);
	CommLua* cc = (CommLua*)lua_touserdata(L, 1);
	int tout = 0;
	if (lua_isnumber(L, 2))
	{
		tout = lua_tointeger(L, 2);
	}
	while ((!cc->GetDSR()) && (tout > 0)) {tout -= 100; Sleep(100);}
	lua_pushboolean(L, cc->GetDSR());
	return 1;
}

static int LuaComWaitRLSD(lua_State* L)
{
	WINSH_LUA(1)
	luaC_checkmethod(L);
	CommLua* cc = (CommLua*)lua_touserdata(L, 1);
	int tout = 0;
	if (lua_isnumber(L, 2))
	{
		tout = lua_tointeger(L, 2);
	}
	while ((!cc->GetRLSD()) && (tout > 0)) {tout -= 100; Sleep(100);}
	lua_pushboolean(L, cc->GetRLSD());
	return 1;
}

static int LuaComWaitRI(lua_State* L)
{
	WINSH_LUA(1)
	luaC_checkmethod(L);
	CommLua* cc = (CommLua*)lua_touserdata(L, 1);
	int tout = 0;
	if (lua_isnumber(L, 2))
	{
		tout = lua_tointeger(L, 2);
	}
	while ((!cc->GetRI()) && (tout > 0)) {tout -= 100; Sleep(100);}
	lua_pushboolean(L, cc->GetRI());
	return 1;
}

static int LuaComSetRTS(lua_State* L)
{
	WINSH_LUA(1)
	luaC_checkmethod(L);
	CommLua* cc = (CommLua*)lua_touserdata(L, 1);
	BOOL v = TRUE;
	if (lua_isboolean(L, 2)) v = lua_toboolean(L, 2);
	cc->SetRTS(v != 0);
	return 1;
}

static int LuaComSetDTR(lua_State* L)
{
	WINSH_LUA(1)
	luaC_checkmethod(L);
	CommLua* cc = (CommLua*)lua_touserdata(L, 1);
	BOOL v = TRUE;
	if (lua_isboolean(L, 2)) v = lua_toboolean(L, 2);
	cc->SetDTR(v != 0);
	return 1;
}

static int LuaComSetBreak(lua_State* L)
{
	WINSH_LUA(1)
	luaC_checkmethod(L);
	CommLua* cc = (CommLua*)lua_touserdata(L, 1);
	BOOL v = TRUE;
	if (lua_isboolean(L, 2)) v = lua_toboolean(L, 2);
	cc->SetBreak(v != 0);
	return 1;
}

int LuaComGc(lua_State* L)
{
	WINSH_LUA(1)
	luaC_checkmethod(L);
	CommLua* cc = (CommLua*)lua_touserdata(L, 1);
	cc->destruct();
	return 0;
}

static int com_Construct(lua_State* L)
{
	static const char* hsset [] = {"None", "XonXoff", "CtsRts", "DsrDtr", NULL};
	static const char* parset [] = {"None", "Odd", "Even", "Mark", "Space", NULL};
	WINSH_LUA(2)
	luaL_checktype(L, 1, LUA_TTABLE);
	COMMSETTINGS cs;
	DCB dcb;
	COMMTIMEOUTS cto;
	JHCommBase::InitSettings(&cs, &dcb, &cto);

	lua_getfield(L, 1, "port");
	if (lua_isnumber(L, -1)) cs.PortNumber = lua_tointeger(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 1, "baud");
	if (lua_isnumber(L, -1)) dcb.BaudRate = lua_tointeger(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 1, "handshake");
	if (lua_isstring(L, -1))
	{
		switch (luaL_checkoption(L, -1, "None", hsset))
		{
		case 1:
			JHCommBase::SetHandshake(&dcb, HandShake_XonXoff);
			break;
		case 2:
			JHCommBase::SetHandshake(&dcb, HandShake_CtsRts);
			break;
		case 3:
			JHCommBase::SetHandshake(&dcb, HandShake_DsrDtr);
			break;
		default:
			JHCommBase::SetHandshake(&dcb, HandShake_None);
			break;
		}
	}
	lua_pop(L, 1);

	lua_getfield(L, 1, "bytesize");
	if (lua_isnumber(L, -1)) dcb.ByteSize = lua_tointeger(L, -1);
	if (dcb.ByteSize < 4) dcb.ByteSize = 4;
	if (dcb.ByteSize > 8) dcb.ByteSize = 8;
	lua_pop(L, 1);

	lua_getfield(L, 1, "parity");
	if (lua_isstring(L, -1))
	{
		int v = luaL_checkoption(L, -1, "None", parset);
		dcb.fParity = (v != 0);
		dcb.Parity = v;
	}	
	lua_pop(L, 1);

	lua_getfield(L, 1, "stopbits");
	if (lua_isnumber(L, -1))
	{
		lua_Number x = lua_tonumber(L, -1);
		if (x < 1.25)
			dcb.StopBits = ONESTOPBIT;
		else if (x > 1.75)
			dcb.StopBits = TWOSTOPBITS;
		else
			dcb.StopBits = ONE5STOPBITS;
	}
	lua_pop(L, 1);

	CommLua c(&cs, &dcb, &cto);
	CommLua* cc = (CommLua*)lua_newuserdata(L, sizeof(c));
	memcpy(cc, &c, sizeof(c));

	cc->rxevent = CreateEvent(NULL, FALSE, FALSE, NULL);

	if (luaC_gettid(L,0) != 1) return luaL_error(L, "Bad Class");
	lua_setmetatable(L, -2);									//|T|
	return 1;
}

static int LuaComDelay(lua_State* L)
{
	WINSH_LUA(1)
	int tout = 20000;
	if (lua_isnumber(L, 1)) {tout = lua_tointeger(L, 1);}
	if (lua_isnumber(L, 2)) {tout = lua_tointeger(L, 2);}
	Sleep(tout);
	return 0;
}

static void com_Create(lua_State* L)
{
	static const struct luaL_Reg ml [] = {
		{"open", LuaComOpen},
		{"close", LuaComClose},
		{"sendcommand", LuaComSendCommand},
		{"waitforinput", LuaComWaitForInput},
		{"getdata", LuaComGetData},
		{"onmessage", LuaComOnMessage},
		{"oninput", LuaComOnInput},
		{"waitforCTS", LuaComWaitCTS},
		{"waitforDSR", LuaComWaitDSR},
		{"waitforRLSD", LuaComWaitRLSD},
		{"waitforRI", LuaComWaitRI},
		{"setRTS", LuaComSetRTS},
		{"setDTR", LuaComSetDTR},
		{"setBreak", LuaComSetBreak},
		{"delay", LuaComDelay},
		{"__gc", LuaComGc},
		{NULL, NULL}
	};
	luaC_newclass(L, com_Construct, ml);
}

static int com_innercb(lua_State* L)
{
	WINSH_LUA(2)
	lua_pushvalue(L, lua_upvalueindex(1));	//The user-supplied notifier function
	lua_pushvalue(L, lua_upvalueindex(2));	//The userdata based Com object
	int r = lua_pcall(L, 1, 0, 0);
	if (r != 0)
	{
		H->WriteError(CString("Com Object callback function:") + CString(lua_tostring(L, -1)));
		lua_pop(L, 1);
	}
	return 0;
}

static int com_Callback(lua_State* L)
{
	WINSH_LUA(1)
	int port = lua_tointeger(L, 2);
	int count = lua_tointeger(L, 3);
	lua_settop(L, 0);
	H->GetRegistryTable(com_ix);					//|T
	lua_pushinteger(L, port);						//|P|T
	lua_gettable(L, -2);							//|F|T
	if (lua_isfunction(L, -1))
		lua_call(L, 0, 0);
	else
		lua_pop(L, 1);
	lua_pushboolean(L, FALSE);						//|B|T
	lua_pushinteger(L, port);						//|P|B|T
	lua_settable(L, -3);							//|T
	lua_pop(L, 1);									//|
	return 0;
}

LUACMLIB_API int LUACMLIB_NGEN(luaopen_)(lua_State* L)
{
	WINSH_LUA(2)

	com_ix = LUA_NOREF;
	com_msg = 0;
	com_hwnd = H->GetHWND();

	// Table indexes Com objects by their com port number:
	com_ix = H->GetRegistryTable(com_ix, 0, "v");
	lua_pop(L, 1);

	// Allocate a notification message:
	com_msg = H->AllocLuaMessages(1);
	lua_pushcfunction(L, com_Callback);
	H->SetLuaMessageHandler(com_msg);
	lua_pop(L, 1);

	com_Create(L);

	return 1;
}
