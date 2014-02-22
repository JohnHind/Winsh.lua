#pragma once
//#include "windows.h"
//#include <atlbase.h>
//#include ".\..\WTL\atlapp.h"
//#include ".\..\WTL\atlmisc.h"
//
//extern "C" {
//#include ".\..\lua\lua.h"
//#include ".\..\lua\lauxlib.h"
//#include ".\..\lua\lualib.h"
//}

// To create a unique library from this template, change the text 'lib' in the two lines====
// in this section to the name of the library for the 'require' statement. If building into
// a larger project, also globally edit the 'LUAXLIB_' prefix here and in the cpp file to
// a unique prefix.
#define LUAGCLIB_NAME "winsh"
#define LUAGCLIB_NGEN(p) p##winsh
// =========================================================================================

// If building or consuming the library as a DLL, uncomment this block:=====================
//#if defined(LUAGCLIB_BUILDING)
//#define LUAGCLIB_API extern "C" __declspec(dllexport)
//#define LUAGCLIB_DLL
//#else
//#define LUAGCLIB_API extern "C" __declspec(dllimport)
//#endif
// =========================================================================================

// If building or consuming the library as a static-link code library, uncomment this block:
//#define LUAGCLIB_API extern
// =========================================================================================

// If compiling the library into an EXE file, uncomment this block:=========================
#define LUAGCLIB_API
// =========================================================================================

LUAGCLIB_API int LUAGCLIB_NGEN(luaopen_)(lua_State* L);

// Add any additional library functions for use from "C" code below here (with LUALIB_API)
