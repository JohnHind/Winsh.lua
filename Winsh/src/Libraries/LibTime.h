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
// a larger project, also globally edit the 'LUATMLIB_' prefix here and in the cpp file to
// a unique prefix.
#define LUATMLIB_NAME "time"
#define LUATMLIB_NGEN(p) p##time
// =========================================================================================

// If building or consuming the library as a DLL, uncomment this block:=====================
//#if defined(LUATMLIB_BUILDING)
//#define LUATMLIB_API extern "C" __declspec(dllexport)
//#define LUATMLIB_DLL
//#else
//#define LUATMLIB_API extern "C" __declspec(dllimport)
//#endif
// =========================================================================================

// If building or consuming the library as a static-link code library, uncomment this block:
//#define LUATMLIB_API extern
// =========================================================================================

// If compiling the library into an EXE file, uncomment this block:=========================
#define LUATMLIB_API
// =========================================================================================

enum TudTyp{
	tud_abs = 0,
	tud_abs_min = 1,
	tud_abs_hour = 2,
	tud_abs_day = 3,
	tud_abs_month = 4,
	tud_abs_year = 5,
	tud_tod = 6,
	tud_dur = 7
};

struct timeud {
	DOUBLE val;
	TudTyp typ;
	int alm;
};

LUATMLIB_API int LUATMLIB_NGEN(luaopen_)(lua_State* L);

// Add any additional library functions for use from "C" code below here (with LUALIB_API)

// Reset a Time object at the top of the stack to the time given by 'ft'.
LUATMLIB_API BOOL luaTM_TimeFromFiletime(lua_State* L, FILETIME ft);

// Set the structure pointed to by 'pft' to the value of the Time object at 'inx'.
LUATMLIB_API BOOL luaTM_FiletimeFromTime(lua_State* L, int inx, FILETIME* pft);
