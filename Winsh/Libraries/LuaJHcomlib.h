#pragma once
//#include "windows.h"
//
//extern "C" {
//#include ".\..\lua\lua.h"
//#include ".\..\lua\lauxlib.h"
//#include ".\..\lua\lualib.h"
//}
#include "CommBaseC.h"

// The Library Name (for Lua 'require') should be defined below:============================
#define LUACMLIB_NAME "com"
#define LUACMLIB_NGEN(p) p##com
// =========================================================================================

// If building or consuming the library as a DLL, uncomment this block:=====================
//#if defined(LUATLIB_BUILDING)
//#define LUATLIB_API extern "C" __declspec(dllexport)
//#else
//#define LUATLIB_API extern "C" __declspec(dllimport)
//#endif
// =========================================================================================

// If building or consuming the library as a static-link code library, uncomment this block:
//#define LUATLIB_API extern
// =========================================================================================

// If compiling the library into an EXE file, uncomment this block:=========================
#define LUACMLIB_API
// =========================================================================================

#define RX_BUF_SIZE 2048
#define CP_BUF_SIZE 64

class CommLua: public JHCommBase
{
public:
	CommLua(COMMSETTINGS* cs, DCB* dcb, COMMTIMEOUTS* cto);

protected:
	virtual void OnOpen(COMMSETTINGS* cs, DCB* dcb, COMMTIMEOUTS* cto);
	virtual bool OnRxChar(BYTE ch);
	virtual void OnTxDone();
	virtual void OnError(int error);

public:
	BYTE inbuf[RX_BUF_SIZE];
	UINT bytes;

//private:
	COMMSETTINGS comsettings;
	DCB devicecontrol;
	COMMTIMEOUTS comtimeouts;
	BYTE compare[CP_BUF_SIZE];
	UINT cat;
	UINT csize;
	UINT max;
	HANDLE rxevent;
	UINT handler;
//	CLua* lua;
};


LUACMLIB_API int LUACMLIB_NGEN(luaopen_)(lua_State* L);

//LUATLIB_API void luaT_copytable(lua_State* L, int source, int dest = 0, int depth = 0);
