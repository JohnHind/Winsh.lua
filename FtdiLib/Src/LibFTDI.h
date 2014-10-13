#pragma once
#include "windows.h"
//#include <atlbase.h>
//#include ".\..\WTL\atlapp.h"
//#include ".\..\WTL\atlmisc.h"
//
extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

// To create a unique library from this template, change the text 'lib' in the two lines====
// in this section to the name of the library for the 'require' statement. If building into
// a larger project, also globally edit the 'LUAFTLIB_' prefix here and in the cpp file to
// a unique prefix.
#define LUAFTLIB_NAME "FtdiLink"
#define LUAFTLIB_NGEN(p) p##FtdiLink
// =========================================================================================

// If building or consuming the library as a DLL, uncomment this block:=====================
#if defined(LUAFTLIB_BUILDING)
#define LUAFTLIB_API extern "C" __declspec(dllexport)
#define LUAFTLIB_DLL
#else
#define LUAFTLIB_API extern "C" __declspec(dllimport)
#endif
// =========================================================================================

// If building or consuming the library as a static-link code library, uncomment this block:
//#define LUAFTLIB_API extern
// =========================================================================================

// If compiling the library into an EXE file, uncomment this block:=========================
//#define LUAFTLIB_API
// =========================================================================================

#pragma comment(lib, "ftd2xx.lib")

LUAFTLIB_API int LUAFTLIB_NGEN(luaopen_)(lua_State* L);

// Add any additional library functions for use from "C" code below here (with LUALIB_API)
