#include "stdafx.h"
#include <conio.h>
#define LUAGCLIB_BUILDING
#include "LibClass.h"
#include "LibGC.h"
#include "..\JHCPathString.h"
#include "..\LuaLibIF.h"
#include "..\resource.h"

WORD GcFirstMenuID = 0;				//The first Menu ID allocated when winsh.taskicon first called
WORD GcLastMenuID = 0;				//The last Menu ID in the block allocated as above (0 means not done yet)
WORD GcNRCid = 0;					//The ID of the Notify Icon Right-click message
WORD GcNLCid = 0;					//The ID of the Notify Icon left-click message
int GcRefRMenuTable = LUA_NOREF;	//The Registry Table key for the table used to define the Right task icon menu
int GcRefLMenuTable = LUA_NOREF;    //The Registry Table key for the table used to define the Left task icon menu
bool m_leftMenu = false;			//Is a menu enabled for notify icon left click?
bool m_rightMenu = false;           //Is a menu enabled for notify icon right click?
int GcNextKeyID = 1;				//The next HotKey ID available for allocation
int GcCmdCount = 0;					//The number of times winsh.commandline has been used from Lua
int GcRepAct = 0;					//Message code for first of two timer messages for setreport.

#ifdef LUAGCLIB_DLL
HMODULE hM;
LUAGCLIB_API BOOL APIENTRY DllMain( HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved )
{
	hM = hModule;
    return TRUE;
}
#endif

#pragma region Menu Support Functions

// Need forward declaration because this function is used iteratively
int gc_DoPrepMenu(lua_State* L, CMenuHandle m, int mn, WORD* id);

void gc_PrepMenu(lua_State* L, int& ref, int mn)
{
	WINSH_LUA(1)
	lua_settop(L, 3);
	UINT mm = lua_tointeger(L, 3);
	CMenuHandle m((HMENU)mm);
	m.DeleteMenu(ID_NOTIFY_EXIT, MF_BYCOMMAND);
	WORD id = GcFirstMenuID;
	ref = H->GetRegistryTable(ref);
	gc_DoPrepMenu(L, m, mn, &id);
	lua_pop(L, 1);
}

int gc_PrepRmenu(lua_State* L)
{
	if (!m_rightMenu) return 0;
	gc_PrepMenu(L, GcRefRMenuTable, 0);
	return 0;
}

int gc_PrepLmenu(lua_State* L)
{
	if (!m_leftMenu) return 0;
	gc_PrepMenu(L, GcRefLMenuTable, 0);
	return 0;
}

int gc_DoPrepMenu(lua_State* L, CMenuHandle m, int mn, WORD* id)
{												//|menutab|...
	WINSH_LUA(6)
	bool bk = false;
	int gt = FALSE;
	int ty = 0;
	int da = FALSE;
	int ck = FALSE;
	int ix = 1;

	lua_getfield(L, -1, "onshow");
	if (luaX_iscallable(L, -1)) {
		lua_pushvalue(L, -2);
		H->ExecChunk(1, _T("MenuOnShow"));
	} else lua_pop(L, 1);

	while (true)
	{											//|menutab|...
		lua_rawgeti(L, -1, ix++);				//|menuitem|menutab|...
		if (lua_isnil(L, -1)) {lua_pop(L, 1); break;}
		if (lua_isstring(L, -1))
		{										//|menuitem|menutab|...
			switch (*luaL_optstring(L, -1, ""))
			{
			case '-':		//Insert horizontal seperator
				H->Menu(mn)->AppendCCMenu(m, MF_SEPARATOR);
				break;
			case '|':		//Next item starts a new column
				bk = true;
				break;
			default:		//Otherwise it is a menu heading
				CString tx(luaL_optstring(L, -1, ""));
				UINT f = MF_STRING | MF_DISABLED;
				if (bk) f |= MF_MENUBARBREAK; bk = false;
				//Look ahead to the next item: if it's a column break, heading might be a sidebar:
				UINT ex = 1;
				lua_rawgeti(L, -2, ix);
				if (lua_isstring(L, -1)) if (*luaL_optstring(L, -1, "") == '|') ex = 2;
				lua_pop(L, 1);
				//Look behind to the previous item: if it exists and is not a column break, not a sidebar:
				if (ix > 2)
				{
					lua_rawgeti(L, -2, (ix - 2));
					if (lua_isstring(L, -1)) {if (*luaL_optstring(L, -1, "") != '|') ex = 1;} else {ex = 1;}
					lua_pop(L, 1);
				}
				H->Menu(mn)->AppendCCMenu(m, f, 0, tx, NULL, ex);
				break;
			}
			lua_pop(L, 1);						//|menutab|...
		}
		else if (luaC_isclass(L, -1, "winsh.Menu"))
		{										//|submenutab|menutab|...
			lua_getfield(L, -1, "title");		//|text|submenutab|menutab|...
			CString n(lua_tostring(L, -1));
			lua_pop(L, 1);						//|submenutab|menutab|...
			lua_getfield(L, -1, "disabled");	//|da|submenutab|menutab|...
			da = lua_toboolean(L, -1);
			lua_pop(L, 1);						//|submenutab|menutab|...
			HICON h = NULL;
			CMenuHandle sm; sm.CreatePopupMenu();
			UINT f = MF_POPUP; f |= (da? MF_GRAYED : MF_ENABLED);
			if (bk) f |= MF_MENUBARBREAK; bk = false;
			H->Menu(mn)->AppendCCMenu(m, f, (UINT_PTR)sm.m_hMenu, n, h);
			if (!da) gt = gc_DoPrepMenu(L, sm, mn, id);	//Recursively call this routine to prepare the sub-menu
			lua_pop(L, 1);						//|menutab|...
		}
		else if (luaC_isclass(L, -1, "winsh.Command"))
		{										//|command|menutab|...
			CString n = "sub menu";
			lua_getfield(L, -1, "title");		//|text|command|menutab|...
			if (lua_isstring(L, -1)) n = CString(lua_tostring(L, -1));
			lua_pop(L, 1);						//|command|menutab|...
			lua_getfield(L, -1, "disabled");	//|da|command|menutab|...
			da = lua_toboolean(L, -1);
			lua_pop(L, 1);						//|command|menutab|...
			HICON h = NULL;
			lua_getfield(L, -1, "icon");
			if (lua_isstring(L, -1))
				{h = H->LoadIcon(CString(lua_tostring(L, -1)), 16, 16);gt = TRUE;}
			else if (lua_isuserdata(L, -1))
				{h = CopyIcon(*(HICON*)lua_touserdata(L, -1));gt = TRUE;}
			lua_pop(L, 1);						//|command|menutab|...
			lua_getfield(L, -1, "value");		//|value|command|menutab|...
			if (!lua_isnil(L, -1)) {ty = 2;gt = TRUE;} else ty = 0;
			ck = lua_toboolean(L, -1);
			lua_pop(L, 1);						//|command|menutab|...
			lua_getfield(L, -1, "default");		//|df|command|menutab|...
			BOOL df = lua_toboolean(L, -1);
			lua_pop(L, 1);						//|command|menutab|...
			WORD idc = GcLastMenuID + 1;
			if (*id > GcLastMenuID)
				return luaL_error(L, "ERROR: Too many menu items in Menu Table. Increase using last parameter on winsh.taskicon.");
			idc = *id; (*id)++;
			H->SetLuaMessageHandler(idc);
			UINT f = MF_STRING;
			if (bk) f |= MF_MENUBARBREAK; bk = false;
			if (ty == 2) f |= (ck? MF_CHECKED : MF_UNCHECKED);
			f |= (da? MF_GRAYED : MF_ENABLED);
			if (df) f |= MF_DEFAULT;
			H->Menu(mn)->AppendCCMenu(m, f, idc, n, h);
			lua_pop(L, 1);						//|menutab|...
		}
		else {lua_pop(L, 1);}					//|menutab|...
	}
	lua_getfield(L, -1, "gutter");
	if (!lua_isnil(L, -1)) gt = lua_toboolean(L, -1);
	lua_pop(L, 1);
	H->Menu(mn)->SetCCMenuGutter(gt!=0);
	return gt;
}

#pragma endregion

#pragma region Miscellaneous Support Functions

CString gc_FindScriptFile(lua_State* L, CString& n)
{
	WINSH_LUA(1)
	CString p;
	CString e = H->GetLuaExt();
	LPTSTR ex = PathFindExtension(n);
	if (*ex == NULL)
	{
		p = n + e;
	}
	else
	{
		if (CString(ex) != e) return CString("");
		p = n;
	}
	if (PathFileExists(p)) return p;
	if (!PathIsRelative(p)) return CString("");
	CString p1 = H->GetExePath() + p;
	if (PathFileExists(p1)) return p1;
	p1 = H->GetExePath() + H->GetExeName() + e + CString("\\") + p;
	if (PathFileExists(p1)) return p1;
	return CString("");
}

BOOL gc_FindScriptResource(lua_State* L, CString& n)
{
	WINSH_LUA(1)
	if ((n == H->GetInitName()) || ((n.FindOneOf(TEXT(" .!@£$%^&*()_+=-;:<>,.//\?{}[]"))) >= 0))
		return FALSE;
	if (FindResource(_Module.GetResourceInstance(), n, H->GetResType()) == NULL)
		return FALSE;
	else
		return TRUE;
}

static int gc_LoadEnvironment(lua_State* L)
{
	WINSH_LUA(1)
	CString x = CString();

	DWORD n = 1024;
	if (GetComputerNameEx(ComputerNamePhysicalDnsHostname, x.GetBuffer(1024), &n))
	{
		x.ReleaseBuffer();
	}
	else
	{
		x.ReleaseBuffer();
		x = CString("");
	}
	SetEnvironmentVariable(TEXT("G_COMPUTERNAME"), x);

	n = 1024;
	if (GetUserName(x.GetBuffer(1024), &n))
	{
		x.ReleaseBuffer();
	}
	else
	{
		x.ReleaseBuffer();
		x = CString("");
	}
	SetEnvironmentVariable(TEXT("G_USERNAME"), x);

	CString x1 = CString(H->GetExePath());
	x = CString(x1);
	int p = x.Find(':');
	if (p != 1)
	{
		p = x.Find('\\', 2);
		x = x.Left(p);
	}
	else
	{
		x = x.Left(p + 1);
		x.MakeUpper();
	}
	SetEnvironmentVariable(TEXT("G_EXEDEV"), x);

	x = CString(x1);
	int q = x.ReverseFind('\\');
	if (q > (p + 1))
		x = x.Mid(p+1, q - p);
	else
		x = CString("\\");
	SetEnvironmentVariable(TEXT("G_EXEPATH"), x);

	x = H->GetExeName();
	SetEnvironmentVariable(TEXT("G_EXENAME"), x);

	x = H->GetLuaExt();
	SetEnvironmentVariable(TEXT("G_LUAEXT"), x);

	return 0;
}

LRESULT gc_msgprocpl(UINT id, CMsgTrap* t, CWindow* w, UINT& msg, WPARAM& wp, LPARAM& lp, BOOL& h)
{
	w->PostMessage(WM_COMMAND, MAKEWPARAM(id, msg), lp);
	return 0;
}

LRESULT gc_msgprocpw(UINT id, CMsgTrap* t, CWindow* w, UINT& msg, WPARAM& wp, LPARAM& lp, BOOL& h)
{
	w->PostMessage(WM_COMMAND, MAKEWPARAM(id, msg), wp);
	return 0;
}

LRESULT gc_msgprocsl(UINT id, CMsgTrap* t, CWindow* w, UINT& msg, WPARAM& wp, LPARAM& lp, BOOL& h)
{
	return t->OnMsgTrap(id, msg, lp, NULL);
}

LRESULT gc_msgprocsw(UINT id, CMsgTrap* t, CWindow* w, UINT& msg, WPARAM& wp, LPARAM& lp, BOOL& h)
{
	return t->OnMsgTrap(id, msg, wp, NULL);
}

#pragma endregion

#pragma region Lua Functions

//P1 (string; def:"Winsh.lua") New title string for windows etc.
//P2 (string; opt) Name of new icon for windows.
static int gc_setidentity(lua_State* L)
{
	WINSH_LUA(2)
	lua_settop(L, 2);
	CString title(luaL_checkstring(L, 1));
	H->SetAppName(title);
	if (lua_type(L, 2) == LUA_TSTRING)
	{
		if (!H->SetAppIcon(CString(luaL_checkstring(L, 2))))
			return luaL_error(L, "winsh.setidentity: Icon '%s' not found", lua_tostring(L, 2));
	}
	return 0;
}

static UINT gc_packset(lua_State* L, int ix, const char* set[])
{
	int t = lua_gettop(L);
	UINT x = 0; int p;
	if (lua_type(L, ix) == LUA_TSTRING)
	{
		p = luaL_checkoption(L, ix, NULL, set) - 1;
		x |= 1<<p;
	}
	else if (lua_type(L, ix) == LUA_TTABLE)
	{
		lua_pushnil(L);
		while (lua_next(L, ix) != 0)
		{
			if (lua_toboolean(L, -1))
			{
				lua_pop(L, 1);
				p = luaL_checkoption(L, t + 1, NULL, set) - 1;
				x |= 1<<p;
			}
			else
			{
				lua_pop(L, 1);
			}
		}
	}
	else
	{
		const char *msg = lua_pushfstring(L, "%s expected, got %s", "string or table", luaL_typename(L, ix));
		luaL_argerror(L, ix, msg);
	}
	lua_settop(L, t);
	return x;
}

// P1 (string or table) Report destination, one 'modeset' key or a set containing these keys.
// P2 (string or table; opt) Error destination, one 'modeset' key or a set containing these keys, if absent P1 is used.
static int gc_outputmode(lua_State* L)
{
    static const char* modeset [] = {"void", "gui", "report", "stdio", "file", "debugger", NULL};
	WINSH_LUA(2)
	lua_settop(L, 2);
	UINT rf = gc_packset(L, 1, modeset);
	UINT ef = 0;
	if ((lua_type(L, 2) == LUA_TSTRING) || (lua_type(L, 2) == LUA_TTABLE))
		ef = gc_packset(L, 2, modeset);
	else
		ef = rf;
	H->SetReportMode(rf, ef);
	return 0;
}

static int gc_setfile(lua_State* L)
{
	static const char* options [] = {"clear", "unique", "build", NULL};
	WINSH_LUA(3)
	UINT opt;
	CString fn(luaL_optstring(L, 1, ""));
	if (lua_isstring(L, 2))
	    opt = luaX_checkoptions(L, options, 2, T);
	else
		opt = (lua_toboolean(L, 2))? 1 : 0;
	lua_pushboolean(L, H->SetFile(fn, opt));
	return 1;
}

static int gc_sienum(lua_State* L)
{
	WINSH_LUA(1)
	LPCTSTR l = H->GetLine(FALSE);
	if (l == NULL)
		lua_pushnil(L);
	else
		luaX_pushstring(L, CString(l));
	return 1;
}

//P1, P2: Returns the values required to iterate lines from a file or stdin in generic for loop.
static int gc_file(lua_State* L)
{
	WINSH_LUA(2)
	H->GetLine(TRUE);
	lua_pushcfunction(L, gc_sienum);
	lua_pushnil(L);
	return 2;
}

// P1 (string, 'actions') The action to be applied to the report window.
// P2 (number or string, 'positions', optional) The Width position or Left position of the window, or milliseconds to delay action.
// P3 (number, optional) The Height or Top position of the window.
static int gc_setreport(lua_State* L)
{
    static const char* actions [] = {"clear", "hide", "resize", "position", "bottomright", "delete", "reset", "running", "paused", "error", "done", "overlay", "minimise", NULL};
    static const char* positions [] = {"center", "centertop", "righttop", "rightcenter", "rightbottom", "centerbottom", "leftbottom", "leftcenter", "lefttop", NULL};
	WINSH_LUA(1)
	int ac = luaL_checkoption(L, 1, "clear", actions);
	int px = 0;
	int py = 0;
	CString icon;
	switch (ac) {
	case 3:
	case 4:
		if (lua_isnumber(L, 2)) {
		  px = lua_tointeger(L, 2);
		  py = luaL_optinteger(L, 3, 0);
		  ac = 3;
		} else {
		  px = luaL_checkoption(L, 2, "center", positions);
		  ac = 4;
		}
	    break;
	case 11:
		icon = CString(luaL_optstring(L, 2, ""));
		H->SetOverlay(icon);
		return 0;
	default:
		px = luaL_optinteger(L, 2, -1);
		py = luaL_optinteger(L, 3, -1);
		break;
	}
	if ((ac <= 1) && (px >= 100))
	{
		H->SetTimerMessage(GcRepAct + ac, px);
	}
	else
	{
		H->SetReportWindow(ac, px, py);
	}
	return 0;
}

static int gc_RepAct(lua_State* L)
{
	WINSH_LUA(1)
	int p1 = luaL_optinteger(L, 1, 0);
	H->SetTimerMessage(p1, 0);
	H->SetReportWindow(p1 - GcRepAct, 0, 0);
	return 0;
}

// R1 (string) Returns the last error message (for use in GT_ERROR message handler)
static int gc_lasterror(lua_State* L)
{
	WINSH_LUA(1)
	lua_settop(L, 0);
	luaX_pushstring(L, H->GetLastError());
	return 1;
}

// P1 (number) The Windows or Winsh message number which triggers the function.
// P2 (function) The Lua function which will be run on the message.
// P3 (optional number) If supplied, the message wParam value must also match this.
// P4 (optional string, 'types') Specifies the message handler function to be used.
static int gc_onevent(lua_State* L)
{
    static const char* types [] = {"postl", "postw", "sendl", "sendw", NULL};
	WINSH_LUA(4)
	lua_settop(L, 4);
	WORD msg = luaL_checkinteger(L, 1);
	if (!luaX_iscallable(L, 2)) luaL_argerror(L, 2, "Event Binding must be a callable object.");
	int hf = 0; WPARAM wp = 0;
	if (lua_type(L, 3) == LUA_TSTRING)
	{
		hf = luaL_checkoption(L, 3, (msg < 1024)? "postl" : "sendl", types);
		wp = 0;
	}
	else
	{
		hf = luaL_checkoption(L, 4, (msg < 1024)? "postl" : "sendl", types);
		wp = luaL_optinteger(L, 3, 0);
	}
	lua_settop(L, 2);
	msgtraphandler* mp;
	switch (hf)
	{
	case 1:
		mp = gc_msgprocpw;
		break;
	case 2:
		mp = gc_msgprocsl;
		break;
	case 3:
		mp = gc_msgprocsw;
		break;
	default:
		mp = gc_msgprocpl;
		break;
	}
	int k = H->CaptureMessage(msg, mp);
	H->SetLuaMessageHandler(k);
	return 0;
}

static int gc_paramstring(lua_State* L)
{
	WINSH_LUA(2)
	CString pm = CString("");
	for (int i = 1; (i <= lua_gettop(L)); i++)
	{
		switch (lua_type(L, i))
		{
		case LUA_TNIL:
			pm += CString(" nil");
			break;
		case LUA_TBOOLEAN:
			if (lua_toboolean(L, i) != 0)
				pm += CString(" true");
			else
				pm += CString(" false");
			break;
		case LUA_TNUMBER:
			pm += CString(" ");
			pm += CString(lua_tostring(L, i));
			break;
		case LUA_TSTRING:
			{
				// This complex quoting is necessary to get round the arcane escape system in CommandLineToArgvW
				// used to parse command lines in Windows. The outer quote marks get stripped by CommandLineToArgvW
				// while the inner escaped ones are passed through as literal quote marks used by Winsh to distinguish
				// string from other types (the inner quotes are stripped by Winsh). This also avoids a problem if
				// the final character in the string is a backslash because backslash followed by quote is treated
				// arcanely (and insanely!) by CommandLineToArgvW. Quote marks within the string are escaped with an
				// added backslash.
				CString s = CString(lua_tostring(L, i));
				s.Replace(TEXT("\""), TEXT("\\\""));
				pm += CString(" \"\\\"");
				pm += s;
				pm += CString("\\\"\"");
			}
			break;
		default:
			return luaL_error(L, "Invalid parameter for conversion, must be NIL, BOOLEAN, NUMBER or STRING.");
			break;
		}
	}
	luaX_pushstring(L, pm);
	return 1;
}

// P1: String. Command-line to be run on the spawned Winsh process.
// Pn: String, 'keys'(Opt). Any number of string keys.
// R1: Boolean or Number. If the process could not be started for any reason,
//     returns boolean false. If the process was run with the "sync" option,
//     returns the process exit number from the process, otherwise boolean
//     true (the process may still be running asynchronously).
static int gc_spawn(lua_State* L)
{
    static const char* keys [] = {"admin", "sync", NULL};
	WINSH_LUA(2)
	CString sn = CString(luaL_optstring(L, 1, ""));
	sn.TrimLeft(); sn.TrimRight();

	// Process the keys:
	BOOL admin = FALSE;
	BOOL sync = FALSE;
	for (int i = lua_gettop(L); (i > 1); i--)
	{
		switch(luaL_checkoption(L, i, NULL, keys))
		{
		case 0:
			if (LOBYTE(GetVersion()) >= 6) admin = TRUE;
			break;
		case 1:
			sync = TRUE;
		}
	}
	CString fl = H->GetExePath() + H->GetExeName() + CString(".EXE");

	SHELLEXECUTEINFO shex;
    memset(&shex, 0, sizeof(shex));
    shex.cbSize = sizeof(SHELLEXECUTEINFO);
    shex.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_DOENVSUBST;
	if (sync) shex.fMask |= SEE_MASK_NOCLOSEPROCESS;
	if (admin) shex.lpVerb = _T("runas");
    shex.lpFile = fl;
	shex.lpParameters = sn;
    shex.lpDirectory = H->GetExePath();
    shex.nShow = SW_NORMAL;
	if (::ShellExecuteEx(&shex))
	{
		if (sync && (shex.hProcess != NULL))
		{
			WaitForSingleObject(shex.hProcess, INFINITE);
			DWORD r = 0;
			GetExitCodeProcess(shex.hProcess, &r);
			CloseHandle(shex.hProcess);
			lua_pushinteger(L, r);
		}
		else
		{
			lua_pushboolean(L, TRUE);
		}
	}
	else
	{
		lua_pushboolean(L, FALSE);
	}
	return 1;
}

// P1: String, 'scopes' (Def: 'desktop'). The scope of the exclusive group.
// P2: String (Opt). If specified, instances are only unique if this string matches.
// P3: String (Opt). If specified, is sent to the primary instance as a command-line.
// R1: Boolean. True if this is the first instance, false if another instance was found.
static int gc_mutex(lua_State* L)
{
    static const char* scopes [] = {"system", "desktop", "session", "user", NULL};
	WINSH_LUA(1)
	lua_settop(L, 3);
	int scope = luaL_checkoption(L, 1, "desktop", scopes);
	CString uid(luaL_optstring(L, 2, ""));
	CString cmd(luaL_optstring(L, 3, ""));
	lua_pushboolean(L, H->Mutex(uid, scope, cmd));	
	return 1;
}

//P1 (String, Optional): If supplied, should be a string containing embedded environment variables
//   prefixed and suffixed by %. A string is returned with the variables replaced by their values.
//   If no parameter is supplied, a table is returned containing all the environment variables.
static int gc_environment(lua_State* L)
{
	WINSH_LUA(2)
	if (lua_isstring(L, 1))
	{
		CPathString s(lua_tostring(L, 1));
		s.ExpandEnvironment();
		luaX_pushstring(L, s);
	}
	else
	{
		LPWCH penv = GetEnvironmentStrings();
		if (penv == NULL) return 0;
		lua_newtable(L);
		LPWCH ps = penv; LPWCH pe = penv; int q = 0;
		CString k; CString v;
		while (*(pe + 1) != 0)
		{
			while (*pe != 0) pe++;
			k = CString(ps);
			q = k.ReverseFind('=');
			if (q > 0)
			{
				v = k.Mid(q + 1);
				k = k.Left(q);
				luaX_pushstring(L, k);
				luaX_pushstring(L, v);
				lua_settable(L, -3);
			}
			ps = pe + 1;
			pe = ps;
		}
		FreeEnvironmentStrings(penv);
	}
	return 1;
}
// P1 - Icon name.
// P2 - Right Menu Object, or false.
// P3 - Left Menu Object, or false, or true.
// P4 - Number of menu commands to reserve (defaults to 10).
static int gc_taskicon(lua_State* L)
{
	WINSH_LUA(1)
	HICON h = NULL;
	int nCmd = 10;

	if (!lua_toboolean(L, 1)) {
		H->SetTaskIcon((HICON)INVALID_HANDLE_VALUE, false, false);
		return 0;
	}
	if (lua_isuserdata(L, 1)) {
		h = CopyIcon(*(HICON*)lua_touserdata(L, 1));
	} else if (lua_isstring(L, 1)){
		h = H->LoadIcon(CString(lua_tostring(L, 1)), 0, 0);
	}
	if (h == NULL) return luaL_argerror(L, 1, "Must be Icon object or valid icon resource name");
	m_leftMenu = false;
	m_rightMenu = false;
	nCmd = luaL_optinteger(L, 4, 10);
	if (nCmd < 10) nCmd = 10;
	if ((GcLastMenuID - GcFirstMenuID) < nCmd) {
		if (GcFirstMenuID != 0) {
			lua_pushnil(L);
			for (WORD i = GcFirstMenuID; (i <= GcLastMenuID); i++) H->SetLuaMessageHandler(i);
			H->SetLuaMessageHandler(GcNRCid);
			H->SetLuaMessageHandler(GcNLCid);
			lua_pop(L, 1);
		}
		GcFirstMenuID = H->AllocLuaMessages(nCmd);
		GcLastMenuID = GcFirstMenuID + nCmd;
		GcNRCid = H->CaptureMessage(GT_NOTIFY_RCLICK, gc_msgprocsl);
		lua_pushcfunction(L, gc_PrepRmenu);
		H->SetLuaMessageHandler(GcNRCid);
		lua_pop(L, 1);
		GcNLCid = H->CaptureMessage(GT_NOTIFY_LCLICK, gc_msgprocsl);
		lua_pushcfunction(L, gc_PrepLmenu);
		H->SetLuaMessageHandler(GcNLCid);
		lua_pop(L, 1);
	}
	lua_settop(L, 3);
	if (luaC_isclass(L, 2, "winsh.Menu")) {
		m_rightMenu = true;
		if (GcRefRMenuTable == LUA_NOREF) {
			GcRefRMenuTable = H->GetRegistryTable(GcRefRMenuTable, 2);
			lua_pop(L, 1);
		} else {
			lua_pushinteger(L, GcRefRMenuTable);
			lua_pushvalue(L, 2);
			lua_settable(L, LUA_REGISTRYINDEX);
		} if (lua_isboolean(L, 3) && lua_toboolean(L, 3)) {
			lua_settop(L, 2);
			lua_pushvalue(L, 2);
		}
	} else {
		if ((lua_type(L, 2) != LUA_TBOOLEAN) || (lua_toboolean(L, 2)))
			return luaL_argerror(L, 2, "Must be Menu object or 'false'");
	}
	if (luaC_isclass(L, 3, "winsh.Menu")) {
		m_leftMenu = true;
		if (GcRefLMenuTable == LUA_NOREF) {
			GcRefLMenuTable = H->GetRegistryTable(GcRefLMenuTable, 3);
			lua_pop(L, 1);
		} else {
			lua_pushinteger(L, GcRefLMenuTable);
			lua_pushvalue(L, 3);
			lua_settable(L, LUA_REGISTRYINDEX);
		}
	} else {
		if ((!lua_isnoneornil(L, 3)) && (lua_type(L, 3) != LUA_TBOOLEAN))
			return luaL_argerror(L, 3, "Must be Menu object or Boolean");
	}
	lua_settop(L, 1);
	H->SetTaskIcon(h, m_leftMenu, m_rightMenu);
	return 0;
}

//P1 - (String, "") Title text for balloon.
//P2 - (String, "") Body text for balloon.
//P3 - (String, 'icons', "none") The icon to display in the balloon.
//P4 - (Number, 0) Timeout in milliseconds, 0 means infinite.
static int gc_balloon(lua_State* L)
{
    static const char* icons [] = {"none", "info", "exclamation", "stop", "winsh", NULL};
	WINSH_LUA(1)
	CString mt(luaL_optstring(L, 1, ""));
	CString ms(luaL_optstring(L, 2, ""));
	int ic = luaL_checkoption(L, 3, "none", icons);
	UINT to = luaL_optlong(L, 4, 0);
	H->Balloon(mt, ms, to, (DWORD)ic);
	return 0;
}

// P1		: String - The message to display in the message box.
// P2(opt)	: String, 'icons' - The icon to show in the message box, default 'none'.
// P3(opt)	: String, 'buttons' - The set of buttons available in the message box, default 'ok'.
// P4(opt)  : Number - Which of the buttons, counting from 1 at left, is the default button, default 1.
// R1		: String - The text label of the button which the user pressed.
static int gc_messagebox(lua_State* L)
{
    static const char* icons [] = {"none", "stop", "question", "exclamation", "asterisk", NULL};
    static const char* buttons [] = {"ok", "ok+cancel", "abort+retry+ignore", "yes+no+cancel", "yes+no",
	    "retry+cancel", "cancel+tryagain+continue", NULL};
	WINSH_LUA(1)
	CString ms(luaL_checkstring(L, 1));
	int ic = 16 * luaL_checkoption(L, 2, "none", icons);
	ic += luaL_checkoption(L, 3, "ok", buttons);
	int df = luaL_optinteger(L, 4, 1); df = (df < 1)? 1 : df; df = (df > 4)? 4 : df;
	ic += 256 * (df - 1);

	int r = ::MessageBox(NULL, ms, H->GetAppName(), ic | MB_TASKMODAL | MB_SETFOREGROUND);

	switch (r)
	{
	case IDABORT:
		lua_pushstring(L, "abort");
		break;
	case IDCONTINUE:
		lua_pushstring(L, "continue");
		break;
	case IDIGNORE:
		lua_pushstring(L, "ignore");
		break;
	case IDNO:
		lua_pushstring(L, "no");
		break;
	case IDOK:
		lua_pushstring(L, "ok");
		break;
	case IDRETRY:
		lua_pushstring(L, "retry");
		break;
	case IDTRYAGAIN:
		lua_pushstring(L, "tryagain");
		break;
	case IDYES:
		lua_pushstring(L, "yes");
		break;
	default:
		lua_pushstring(L, "cancel");
		break;
	}
	return 1;
}

//P1 (Function/Table/Userdata) The object to be executed when the hotkey is pressed.
//P2 (Number) Virtual Key Code (see VK table in Lua annex) or ASCII code for '0'..'9' or 'A'..'Z'.
//Px (String, 'mods') Up to 4 strings representing key modifiers.
static int gc_hotkey(lua_State* L)
{
	static const char* mods [] = {"alt", "ctrl", "shift", "win", NULL};
	WINSH_LUA(2)
	int n = 10;
	UINT k = 0; UINT m = 0; INT b = -1;
	if (!luaX_iscallable(L, 1)) luaL_error(L, "Hotkey parameter must be a callable object.");
	for (int i = lua_gettop(L); (i > 2); i--)
	{
		switch(luaL_checkoption(L, i, NULL, mods))
		{
		case 0:
			m |= MOD_ALT;
			break;
		case 1:
			m |= MOD_CONTROL;
			break;
		case 2:
			m |= MOD_SHIFT;
			break;
		case 3:
			m |= MOD_WIN;
			break;
		}
	}
	lua_settop(L, 2);
	k = lua_tointeger(L, 2);
	if ((k < 1) || (k > 255)) return luaL_error(L, "Virtual Key Code must be 1..255");
	lua_settop(L, 1);						//|binding

	if (::RegisterHotKey(H->GetHWND(), GcNextKeyID, m, k))
	{
		int k = H->CaptureMessage(WM_HOTKEY, gc_msgprocpl, GcNextKeyID++);
		H->SetLuaMessageHandler(k);
		lua_settop(L, 0);
		lua_pushboolean(L, TRUE);
		return 1;
	}

	lua_settop(L, 0);
	lua_pushboolean(L, FALSE);
	return 1;
}

// P1 (String, Opt) Overrides the default "Press any key to continue" prompt.
// P2 (String, Opt) The set of keyboard keys that should be accepted, if missing any key
//    is accepted.
// R1 (Number, String) If P2 is specified returns the position of the key in the set.
//    otherwise, returns a single string containing the character that was pressed.
static int gc_waitkey(lua_State* L)
{
	WINSH_LUA(1)
	CString p = CString(luaL_optstring(L, 1, "Press any key to continue"));
	CString t = CString(luaL_optstring(L, 2, ""));
	t.MakeLower();
	TCHAR x = 0; int n = -1;
	H->WriteMessage(p, FALSE);
	while (n < 0)
	{
		x = H->ReadConsole();
		if ((t.GetLength() < 1) || (x == 0)) break;
		CString xx(x); xx.MakeLower();
		n = t.Find(CString(xx)) + 1;
	}
	H->WriteMessage(CString(""));
	if (n >= 0)
		lua_pushinteger(L, n);
	else
		luaX_pushstring(L, CString(x));
	return 1;
}

/* R1: Boolean, true if there is outstanding data in the keyboard buffer.
** NB: _kbhit() is POSIX and in Windows, but it is not present in Linux or ANSI C.
*/
static int gc_kbhit(lua_State* L) {
 	lua_pushboolean(L, _kbhit());
 	return 1;
}

// P1 (string): Resource or File name.
// R1 (function): If the script is loaded and compiled, returns the chunk as a function,
//    otherwise returns nil and an error message (R2 - String).
static int gc_loadscript(lua_State* L)
{
	WINSH_LUA(2)
	CString n = CString(luaL_checkstring(L, 1));
	n.MakeUpper();
	int r = 999;
	if (gc_FindScriptResource(L, n))
	{
		r = H->LoadScriptResource(n);
	}
	else
	{
		n = gc_FindScriptFile(L, n);
		if (n.GetLength() > 0)
		{
			r = H->LoadScriptFile(n);
		}
	}
	if (r == 0) return 1;
	if (r == 999)
	{
		lua_pushboolean(L, FALSE);
		luaX_pushstring(L, CString("winsh.loadscript: script file or resource not found"));
		return 2;
	}
	lua_pushnil(L);
	lua_insert(L, -2);
	return 2;
}

#pragma endregion

#pragma region Lua Library Publication

LUAGCLIB_API int LUAGCLIB_NGEN(luaopen_)(lua_State* L)
{
	static const luaL_Reg fl [] = {
		{"setidentity", gc_setidentity},
		{"outputmode", gc_outputmode},
		{"setreport", gc_setreport},
		{"setfile", gc_setfile},
		{"file", gc_file},
		{"spawn", gc_spawn},
		{"mutex", gc_mutex},
		{"ps", gc_paramstring},
		{"taskicon", gc_taskicon},
		{"balloon", gc_balloon},
		{"hotkey", gc_hotkey},
		{"messagebox", gc_messagebox},
		{"waitkey", gc_waitkey},
		{"kbhit", gc_kbhit},
		{"environment", gc_environment},
		{"onevent", gc_onevent},
		{"loadscript", gc_loadscript},
		{"lasterror", gc_lasterror},
		{NULL, NULL}
	};

	WINSH_LUA(2)
	H->Require(CString("class"));

	// In case we are resetting, reset globals and remove resources from previous run:
	H->SetTaskIcon((HICON)INVALID_HANDLE_VALUE, false, false);
	GcFirstMenuID = GcLastMenuID = GcNRCid = GcNLCid = 0;
	GcRefRMenuTable = GcRefLMenuTable = LUA_NOREF;
	m_leftMenu = m_rightMenu = false;
	GcCmdCount = 0;
	for (int i = 1; (i < GcNextKeyID); i++) ::UnregisterHotKey(H->GetHWND(), i);
	GcNextKeyID = 1;
	GcRepAct = 0;

	// Create timer messages for delayed report window actions:
	GcRepAct = H->AllocLuaMessages(2);
	lua_pushcfunction(L, gc_RepAct);
	H->SetLuaMessageHandler(GcRepAct);
	H->SetLuaMessageHandler(GcRepAct + 1);
	lua_pop(L, 1);

	if (fl[0].name == NULL)
		lua_pushboolean(L, TRUE);
	else
	{
		lua_createtable(L, 0, sizeof(fl)/sizeof(fl[0]) - 1);
		luaL_setfuncs(L, fl, 0);
		// Load the "special" environment variables:
		gc_LoadEnvironment(L);
		// Load and execute the Lua part of the library:
		H->LoadScriptResource(CString("LibGC"));
		lua_pushvalue(L, -2);
		H->ExecChunk(1, CString("LibGC-LuaPart"));
	}
	return 1;
}

#pragma endregion
