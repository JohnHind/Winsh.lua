/*
Reference: http://www.songho.ca/misc/timer/timer.html
Library returns a single class - Timer. When executed this Class function returns a high resolution
performance timer object implemented as a closure object, the enclosed function taking the following
parameters:

et = timer()  - With no parameters, returns elapsed seconds since creation or reset.
timer(offset) - With a number parameter, resets the timer applying an offset (use 0 for no offset).
rs = timer("resolution") - Returns the resolution of the timer in seconds (should be a few microseconds).
*/

#include "stdafx.h"
#define LUATRLIB_BUILDING
#include "LibClass.h"
#include "LibTimer.h"
#include "..\LuaLibIF.h"
#include "..\resource.h"
#include <math.h>

#ifdef LUATRLIB_DLL
HMODULE hM;
LUATRLIB_API BOOL APIENTRY DllMain( HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved )
{
	hM = hModule;
    return TRUE;
}
#endif

// Timer Class.
// ============

static int timer_Func(lua_State* L)
{
	const char* const ke[] = {"tick", NULL};
	LARGE_INTEGER tps, ct;
	lua_Number rv;
	lua_settop(L, 1);
	timerud* ud = (timerud*)lua_touserdata(L, lua_upvalueindex(3));
	QueryPerformanceFrequency(&tps);
	QueryPerformanceCounter(&ct);
	switch (lua_type(L, 1)) {
	case LUA_TNIL:
		ct.QuadPart = ct.QuadPart - ud->tim.QuadPart;
		rv = (lua_Number)ct.QuadPart / (lua_Number)tps.QuadPart;
		lua_pushnumber(L, rv);
		return 1;
	case LUA_TNUMBER:
		rv = lua_tonumber(L, 1);
		rv = (rv * (lua_Number)tps.QuadPart);
		tps.HighPart = 0; tps.LowPart = (DWORD)rv;
		ud->tim.QuadPart = ct.QuadPart + tps.QuadPart;
		return 0;
	case LUA_TSTRING:
		luaL_checkoption(L, 1, NULL, ke);
		rv = (lua_Number)tps.QuadPart;
		rv = 1 / rv;
		lua_pushnumber(L, rv);
		return 1;
	default:
		return luaL_argerror(L, 1, "Invalid Type");
	}
}

static int timer_Construct(lua_State* L)
{
	WINSH_LUA(3)
	lua_pushstring(L, "_TID_O");
	lua_pushstring(L, "ClassTimer");
	timerud* ud = (timerud*)lua_newuserdata(L, sizeof(timerud));
	QueryPerformanceCounter(&(ud->tim));
	lua_pushcclosure(L, timer_Func, 3);
	return 1;
}

void timer_Create(lua_State* L)
{
	lua_pushstring(L, "_TID");
	lua_pushstring(L, "ClassTimer");
	lua_pushcclosure(L, timer_Construct, 2);
}

// =================================================================================================

LUATRLIB_API int LUATRLIB_NGEN(luaopen_)(lua_State* L)
{
	WINSH_LUA(6)
	H->Require(CString("class"));

	timer_Create(L);

	return 1; // Return the Timer class NOT a library table!
}
