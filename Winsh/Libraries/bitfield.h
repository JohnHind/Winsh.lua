#pragma once
//#include "windows.h"

//extern "C" {
//#include ".\..\lua\lua.h"
//#include ".\..\lua\lauxlib.h"
//#include ".\..\lua\lualib.h"
//}

// If building or consuming the library as a DLL, uncomment this block:=====================
//#if defined(LUACLLIB_BUILDING)
//#define LUACLLIB_API extern "C" __declspec(dllexport)
//#else
//#define LUACLLIB_API extern "C" __declspec(dllimport)
//#endif
// =========================================================================================

// If building or consuming the library as a static-link code library, uncomment this block:
//#define LUACLLIB_API extern
// =========================================================================================

// If compiling the library into an EXE file, uncomment this block:=========================
#define LUABFLIB_API
// =========================================================================================

LUABFLIB_API int luaopen_bitfield(lua_State* L);

// **********************
// 'C' Interface.
// **********************
// Add any additional library functions for use from "C" code below here (with LUAxxLIB_API)
