#pragma once
//#include "windows.h"
//
//extern "C" {
//#include ".\..\lua\lua.h"
//#include ".\..\lua\lauxlib.h"
//#include ".\..\lua\lualib.h"
//}

// The Library Name (for Lua 'require') should be defined below:============================
#define LUACMLIB_NAME "ComLink"
#define LUACMLIB_NGEN(p) p##ComLink
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


LUACMLIB_API int LUACMLIB_NGEN(luaopen_)(lua_State* L);
