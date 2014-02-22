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
// a larger project, also globally edit the 'LUATKLIB_' prefix here and in the cpp file to
// a unique prefix.
#define LUATKLIB_NAME "task"
#define LUATKLIB_NGEN(p) p##task
// =========================================================================================

// If building or consuming the library as a DLL, uncomment this block:=====================
//#if defined(LUATKLIB_BUILDING)
//#define LUATKLIB_API extern "C" __declspec(dllexport)
//#define LUATKLIB_DLL
//#else
//#define LUATKLIB_API extern "C" __declspec(dllimport)
//#endif
// =========================================================================================

// If building or consuming the library as a static-link code library, uncomment this block:
//#define LUATKLIB_API extern
// =========================================================================================

// If compiling the library into an EXE file, uncomment this block:=========================
#define LUATKLIB_API
// =========================================================================================

LUATKLIB_API int LUATKLIB_NGEN(luaopen_)(lua_State* L);

// Add any additional library functions for use from "C" code below here (with LUALIB_API)
