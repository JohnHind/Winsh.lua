#include "stdafx.h"
#define LUATKLIB_BUILDING
#include "LibClass.h"
#include "LibTask.h"
#include "psapi.h"
#include "..\LuaLibIF.h"
#include "..\JHCPathString.h"
#include "..\resource.h"

#ifdef LUATKLIB_DLL
HMODULE hM;
LUATKLIB_API BOOL APIENTRY DllMain( HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved )
{
	hM = hModule;
    return TRUE;
}
#endif

#ifdef UNICODE
typedef BOOL WINAPI QFPIN (HANDLE, DWORD, LPWSTR, PDWORD);
typedef BOOL WINAPI GMBN (HANDLE, HMODULE, LPWSTR, DWORD);
#else
typedef BOOL WINAPI QFPIN (HANDLE, DWORD, LPSTR, PDWORD);
typedef BOOL WINAPI GMBN (HANDLE, HMODULE, LPSTR DWORD);
#endif
typedef BOOL WINAPI ACFL (HWND);
QFPIN* PQFPIN;
GMBN* PGMBN;
ACFL* PACFL;

/*
// http://stackoverflow.com/questions/3382384/how-to-get-applications-from-windows-task-manager-applications-tab-their-loc
BOOL IsTaskWindow(HWND hwnd)
{
	if (!IsWindow(hwnd)) return FALSE;
	if (!IsWindowVisible(hwnd)) return FALSE;
	if (GetParent(hwnd) != NULL) return FALSE;
	if (GetWindow(hwnd, GW_OWNER) != NULL) return FALSE;
	if ((GetWindowLong(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) != 0) return FALSE;
	return TRUE;
}
*/

// http://vb.mvps.org/samples/AltTab/
BOOL IsTaskWindow(HWND hwnd)
{
	if (!IsWindow(hwnd)) return FALSE;
	if (!IsWindowVisible(hwnd)) return FALSE;
	if (GetParent(hwnd) != NULL) return FALSE;
	HWND owner(GetWindow(hwnd, GW_OWNER));
	if ((!IsWindow(owner)) || ((GetWindowLong(owner, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) != 0))
	{
		if ((GetWindowLong(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) != 0) return FALSE;
	}
	return TRUE;
}

/*
// JH - This is my own method - seems logical
BOOL IsTaskWindow(HWND hwnd)
{
	if (!IsWindow(hwnd)) return FALSE;
	if ((GetWindowLong(hwnd, GWL_STYLE) & (WS_SYSMENU|WS_VISIBLE)) == (WS_SYSMENU|WS_VISIBLE)) return TRUE;
	return FALSE;
}
*/

#pragma region Application Monitoring Thread
// The monitor thread uses a Lua table in the registry at 'task_appix'. This table contains
// an entry for each Application object with an existing process. The key is the PID of the
// process and the value is the Application object. The thread detects when any of the
// processes in the table terminates and updates the associated Application object including
// triggering any termination function.

static BOOL task_AppCheck(lua_State* L, BOOL wind);

CRITICAL_SECTION task_cs;
HANDLE task_reset = INVALID_HANDLE_VALUE;
HANDLE task_handle = INVALID_HANDLE_VALUE;
DWORD task_threadid = 0;
int task_watchcount = 0;
HANDLE task_watches[MAXIMUM_WAIT_OBJECTS];
int task_messageid = 0;
HWND task_mhwnd = 0;
int task_appix = LUA_NOREF;

static DWORD WINAPI task_ThreadProc(LPVOID lpPara);

void task_StartMonitorThread(int message, HWND hwnd)
{
	InitializeCriticalSection(&task_cs);
	task_watchcount = 0;
	task_messageid = message;
	task_mhwnd = hwnd;
	task_reset = CreateEvent(NULL, FALSE, FALSE, NULL);
	task_handle = CreateThread(NULL, 0, task_ThreadProc, NULL, 0, &task_threadid);
}

void task_StopMonitorThread()
{
	if (task_threadid != 0)
	{
		EnterCriticalSection(&task_cs);
		task_watchcount = -1;
		LeaveCriticalSection(&task_cs);
		SetEvent(task_reset);
		WaitForSingleObject(task_handle, 10000);
		CloseHandle(task_handle);
		task_threadid = 0;
	}
	if (task_reset != INVALID_HANDLE_VALUE)
	{
		CloseHandle(task_reset);
		task_reset = INVALID_HANDLE_VALUE;
		DeleteCriticalSection(&task_cs);
	}
}
	
void task_ResetWatches(lua_State* L)
{
	WINSH_LUA(4)
	lua_rawgeti(L, LUA_REGISTRYINDEX, task_appix);	//|AI|..
	int i = 0; BOOL overflow = FALSE; HANDLE h; DWORD f;
	lua_pushnil(L);									//|K|AI|..
	EnterCriticalSection(&task_cs);
	while ((lua_next(L, -2) != 0) && (!overflow))
	{												//|A|PID|AI|..
		if (i >= MAXIMUM_WAIT_OBJECTS)
		{
			if (!overflow) luaL_error(L, "Application Object: Warning - too many processes to watch.");
			overflow = true;
		}
		lua_getfield(L, -1, "_ph");					//|H|A|PID|AI|..
		h = (HANDLE)lua_tointeger(L, -1);
		lua_pop(L, 2);								//|PID|AI|..
		if (GetHandleInformation(h, &f))
		{	
			if (!overflow) task_watches[i++] = h;
		}
		else
		{	//Precaution in case expired handle was not removed by normal mechanism.
			lua_pushvalue(L, -1);					//|PID|PID|AI|..
			lua_pushnil(L);							//|N|PID|PID|AI|..
			lua_settable(L, -4);					//|PID|AI|..
		}
	}
	task_watchcount = i;
	LeaveCriticalSection(&task_cs);
	lua_settop(L, T);
	SetEvent(task_reset);
}

// The Application object or 'nil' is at stack top.
void task_RegisterWatch(lua_State* L, UINT pid)
{
	WINSH_LUA(4)
	lua_rawgeti(L, LUA_REGISTRYINDEX, task_appix);	//|AI|A
	lua_pushinteger(L, (lua_Integer)pid);			//|PID|AI|A
	lua_pushvalue(L, -3);							//|A|PID|AI|A
	lua_settable(L, -3);							//|AI|A
	lua_settop(L, T);
	task_ResetWatches(L);
}

static int task_OnMessage(lua_State* L)
{
	WINSH_LUA(3)
	HANDLE ph = (HANDLE)lua_tointeger(L, 3);
	UINT pid = GetProcessId(ph);
	lua_settop(L, 0);
	lua_rawgeti(L, LUA_REGISTRYINDEX, task_appix);	//|AI|
	lua_pushinteger(L, (lua_Integer)pid);			//|PID|AI|
	lua_gettable(L, -2);							//|A|AI|
	lua_remove(L, 1);								//|A|

	if (lua_istable(L, -1)) task_AppCheck(L, TRUE);
	lua_settop(L, 0);

	lua_rawgeti(L, LUA_REGISTRYINDEX, task_appix);	//|AI|
	lua_pushinteger(L, (lua_Integer)pid);			//|PID|AI|
	lua_pushnil(L);									//|N|PID|AI|
	lua_settable(L, -3);							//|AI|
	lua_settop(L, 0);								//|
	task_ResetWatches(L);
	return 0;
}

DWORD WINAPI task_ThreadProc(LPVOID lpPara)
{
	int r = 0;
	int watchcount = 0;
	HANDLE watches[MAXIMUM_WAIT_OBJECTS];
	watches[0] = task_reset;
	while (watchcount >= 0)
	{
		r = WaitForMultipleObjects(watchcount + 1, watches, FALSE, INFINITE);
		if (r == WAIT_FAILED)
		{
			int err = GetLastError();
		}
		if ((r > 0) && (r <= watchcount))
			PostMessage(task_mhwnd, WM_COMMAND, task_messageid, (LPARAM)(watches[r]));
		EnterCriticalSection(&(task_cs));
		if (r == 0)
		{
			watchcount = task_watchcount;
			if (watchcount >= MAXIMUM_WAIT_OBJECTS)
				watchcount = MAXIMUM_WAIT_OBJECTS - 1;
			if (watchcount > 0)
				for (int i = 0; (i < watchcount); i++) watches[i+1] = task_watches[i];
		}
		else
		{
			watchcount = 0;
		}
		LeaveCriticalSection(&(task_cs));
	}
	return 0;
}

#pragma endregion

#pragma region Lua scripting object "Application".

// Represents a Windows Application providing methods to start and stop it and to interact with
// it when it is running.
	
// Fill in process info for an open process in Application object at stack index 1.
HANDLE task_SetProcessInfo(lua_State* L, UINT pid)
{
	HANDLE ph; DWORD sz; CString x;
	if (pid > 0) {
		lua_pushinteger(L, pid);
		lua_setfield(L, 1, "_pid");
		ph = OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ|PROCESS_TERMINATE|SYNCHRONIZE, FALSE, pid);
		lua_pushinteger(L, (lua_Integer)ph);
		lua_setfield(L, 1, "_ph");
		sz = MAX_PATH;
		if (PQFPIN != NULL)
		{
			(PQFPIN)(ph, 0, x.GetBuffer(sz), &sz); //QueryFullProcessImageName
			luaX_pushstring(L, x);
			lua_setfield(L, 1, "ImagePath");
		}
		lua_getfield(L, 1, "Command");
		if (!lua_isstring(L, 1)) {
			luaX_pushstring(L, x);
			lua_setfield(L, 1, "Command");
		}
		lua_pop(L, 1);
		if (PGMBN != NULL)
		{
			(PGMBN)(ph, 0, x.GetBuffer(sz), sz); //GetModuleBaseName
			luaX_pushstring(L, x);
			lua_setfield(L, 1, "BaseName");
		}
	} else {
		lua_getfield(L, 1, "_ph");
		if (lua_isnumber(L, -1)) CloseHandle((HANDLE)lua_tointeger(L, -1));
		lua_pop(L, 1);
		lua_pushnil(L); lua_setfield(L, 1, "_pid");
		lua_pushnil(L); lua_setfield(L, 1, "_ph");
		lua_pushnil(L); lua_setfield(L, 1, "ImagePath");
		lua_pushnil(L); lua_setfield(L, 1, "BaseName");
		ph = NULL;
	}
	return ph;
}

// Used by task_AppCheck to iterate windows belonging to the process.
static BOOL CALLBACK task_EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
	lua_State* L = (lua_State*)lParam;
	CWindow w(hwnd);
	if (lua_tointeger(L, -2) != w.GetWindowProcessID()) return TRUE;
	if (!IsTaskWindow(w)) return TRUE;
	// Do we already know about it?
	lua_getfield(L, 1, "_wix");
	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);
		lua_newtable(L);
		lua_pushvalue(L, -1);
		lua_setfield(L, 1, "_wix");
	}
	lua_rawgeti(L, -1, (lua_Integer)hwnd);
	if (lua_isnumber(L, -1)) {lua_pop(L, 2); return TRUE;}
	lua_pop(L, 1);
	// New window: store forward and reverse entry:
	int c = lua_tointeger(L, -2);
	lua_pushinteger(L, (lua_Integer)hwnd);
	lua_rawseti(L, 1, c);
	lua_pushinteger(L, c);
	lua_rawseti(L, -2, (lua_Integer)hwnd);
	lua_pop(L, 2);
	lua_pushinteger(L, ++c);
	return TRUE;
}

int task_WeedWindows(lua_State* L, BOOL clear)
{
	lua_checkstack(L, 3);
	int k = 1; HWND h; int c = 0;
	lua_getfield(L, 1, "_wix");
	if (!lua_istable(L, -1)) {lua_pop(L,1); return 1;}
	do {
		lua_rawgeti(L, 1, k);
		if (!lua_isnil(L, -1)) {
			h = (HWND)lua_tointeger(L, -1);
			if ((clear) || (!IsWindow(h))) {
				if (clear) {lua_pushnil(L);} else {lua_pushinteger(L, 0);}
				lua_rawseti(L, 1, k);
				lua_pushnil(L);
				lua_rawseti(L, -3, (lua_Integer)h);
			}
			k++;
		} else {
			c = k; k = 0;
		}
		lua_pop(L, 1);
	} while (k > 0);
	lua_pop(L, 1);
	return c;
}

int task_FindWindow(lua_State* L, int inc)
{
	int c;
	lua_getfield(L, 1, "_cw"); c = (lua_isnumber(L, -1))? lua_tointeger(L, -1) : 1; lua_pop(L, 1);
	int p = c; int d = inc; int op;
	if (p < 1) p = 1;
	if (d < -1) {d = 1; p = 0;} // looking for first
	if (d > 1) {d = 1;} // looking for next (otherwise: d = -1 previous or d = 0 current)
	do {
		op = p;
		p += d;
		if (p < 1) {break;} // decremented past minimum
		lua_rawgeti(L, 1, p);
		if (lua_isnumber(L, -1)) {
			if (lua_tointeger(L, -1) != 0) {
				lua_pop(L, 1);
				if (p != c) {lua_pushinteger(L, p); lua_setfield(L, 1, "_cw");}
				return p;
			}
		} else {
			if ((d == 1) && (p > 2)) {p = 0;} else {lua_pop(L, 1); break;}
		}
		lua_pop(L, 1);
	} while ((p != op) && (p != c)); 
	return 0;
}

// Check object at stack location 1. Error if it is not an application object. Otherwise check
// its process status is valid aligning it and returning false if there is no active process.
// If 'wind' is true, also re-creates the window list for the process.
static BOOL task_AppCheck(lua_State* L, BOOL wind)
{
	WINSH_LUA(2)
	luaL_checktype(L, 1, LUA_TTABLE);
	DWORD pid;
	BOOL r = TRUE;
	int c;

	lua_getfield(L, 1, "_ph");
	HANDLE hh = (HANDLE)luaL_optinteger(L, -1, 0);
	lua_pop(L, 1);
	if (hh == 0) r = FALSE;
	if (r)
	{   // Process was started, check it still exists:
		DWORD excode;
		if (!GetExitCodeProcess(hh, &excode)) r = FALSE;
		if ((r) && (excode != STILL_ACTIVE))
		{	// Process has terminated:
			lua_getfield(L, 1, "_pid");
			pid = (UINT)lua_tointeger(L, -1);
			lua_pushinteger(L, (lua_Integer)excode);
			lua_setfield(L, 1, "ExitCode");
			lua_pushnil(L);
			task_RegisterWatch(L, pid);
			lua_pop(L, 1);
			task_SetProcessInfo(L, 0);
			lua_getfield(L, 1, "OnClose");
			if (luaX_iscallable(L, -1)) H->ExecChunk(0, CString("Application:OnClose"));
			lua_settop(L, 1);
			task_WeedWindows(L, TRUE);
			r = FALSE;
		}
	}
	if ((r) && (wind))
	{	// If we have a process and want to refresh the window list, do that:
		c = task_WeedWindows(L, FALSE);
		lua_getfield(L, 1, "_pid");
		lua_pushinteger(L, c);
		EnumWindows(task_EnumWindowsProc, (LPARAM)L);
		lua_pop(L, 2);
		lua_getfield(L, 1, "_cw"); c = (lua_isnumber(L, -1))? lua_tointeger(L, -1) : 1; lua_pop(L, 1);
		c = task_FindWindow(L, 0);
		lua_pushinteger(L, c); lua_setfield(L, 1, "_cw");
	}
	return r;
}

// Lua object method (app:open).
static int task_ApplicationOpen(lua_State* L)
{
	static const char* as [] = {"sync","noconsole","comspec","minimized","maximized","hidden",NULL};
	WINSH_LUA(4)
	// Ensure it is not already open:
	if (task_AppCheck(L, FALSE)) {lua_settop(L, 0); lua_pushboolean(L, FALSE); return 1;};
	// Process the keys:
	BOOL sync = FALSE;
	BOOL noconsole = FALSE;
	BOOL comspec = FALSE;
	WORD st = SW_SHOWNORMAL;
	for (int i = lua_gettop(L); (i > 1); i--)
	{
		switch(luaL_checkoption(L, i, NULL, as))
		{
		case 0: sync = TRUE; break;
		case 1: noconsole = TRUE; break;
		case 2: comspec = TRUE; break;
		case 3: st = SW_MINIMIZE; break;
		case 4: st = SW_MAXIMIZE; break;
		case 5: st = SW_HIDE; break;
		}
	}
	lua_settop(L, 1);

	// Clear the exit code from any previous run:
	DWORD res = -1;
	lua_pushnil(L);
	lua_setfield(L, 1, "ExitCode");

	// Get the command line to be executed:
	lua_getfield(L, 1, "Command");
	CString pm(luaL_optstring(L, -1, ""));
	lua_pop(L, 1);

	// Get the directory to set as default for new process, use current directory if not specified:
	lua_getfield(L, 1, "Directory");
	CString dr(luaL_optstring(L, -1, ""));
	lua_pop(L, 1);
	if (dr.GetLength() < 1)
	{
		int n = GetCurrentDirectory(0, NULL);
		GetCurrentDirectory(n, dr.GetBuffer(n));
		dr.ReleaseBuffer();
	}

	// Process the environment table producing a sorted environment block for the new process:
	LPWSTR env = NULL;
	lua_getfield(L, 1, "Environment");				//|E|
	if (lua_istable(L, -1) != 0)
	{
		CString e;
		luaC_newobject(L, 0, "class.List");			    //|L|E|
		int c = 1; int ss = 0;
		lua_pushnil(L);						//|K|L|E|
		while (lua_next(L, -3))				//|V|K|L|E|
		{
			if (lua_isstring(L, -1) && lua_isstring(L, -2))
			{
				ss += lua_rawlen(L, -1);
				ss += lua_rawlen(L, -2);
				ss += 2;
				lua_pop(L, 1);				//|K|L|E|
				lua_pushvalue(L, -1);		//|K|K|L|E|
				lua_rawseti(L, -3, c++);	//|K|L|E|
			}
		}									//|L|E|
		luaC_sortlist(L, -1);
		env = (LPWSTR)malloc((2 * ss) + 2);
		LPWSTR p = env;
		for (int i = 1; (i < c); i++)
		{
			lua_rawgeti(L, -1, i);			//|K|L|E|
			e = CString(lua_tostring(L, -1));
			lua_rawget(L, -3);				//|V|L|E|
			e += CString('=') += CString(lua_tostring(L, -1));
			for (int i = 0; (i < e.GetLength()); i++) {*p = e.GetAt(i); p++;}
			*p = 0; p++;
			lua_pop(L, 1);					//|L|E|
		}
		*p = 0;
		lua_pop(L, 1);						//|E|
	}
	lua_pop(L, 1);

	STARTUPINFO si;
	ZeroMemory(&si, sizeof(STARTUPINFO));
	si.cb = sizeof(STARTUPINFO);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = st;
	lua_getfield(L, 1, "WindowFrame");
	if (lua_istable(L, -1)) {
		lua_getfield(L, -1, "_l"); si.dwX = luaL_optlong(L, -1, 0); lua_pop(L, 1);
		lua_getfield(L, -1, "_t"); si.dwY = luaL_optlong(L, -1, 0); lua_pop(L, 1);
		si.dwFlags |= STARTF_USEPOSITION;
		long w, h;
		lua_getfield(L, -1, "_w"); w = luaL_optlong(L, -1, 0); lua_pop(L, 1);
		lua_getfield(L, -1, "_h"); h = luaL_optlong(L, -1, 0); lua_pop(L, 1);
		if ((w > 0) && (h > 0)) {
			si.dwXSize = w;
			si.dwYSize = h;
			si.dwFlags |= STARTF_USESIZE;
		}
	}

	lua_settop(L, 1);

	// Prepare the command interpreter (if any):
	LPCTSTR cmd = NULL;
	CString cc("");
	if (comspec)
	{
		GetEnvironmentVariable(TEXT("ComSpec"), cc.GetBuffer(MAX_PATH), MAX_PATH);
		cc.ReleaseBuffer();
		if (noconsole)
			pm = CString("/C ") + pm;
		else
			pm = CString("/K ") + pm;
		cmd = (LPCTSTR)cc;
	}

	// Prepare the remaining parameter memory for CreateProcess:
	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
	LPTSTR x = pm.GetBuffer(pm.GetLength() + 2);

	DWORD flags = 0;
	if (env != NULL) flags |= CREATE_UNICODE_ENVIRONMENT;
	if (noconsole) flags |= CREATE_NO_WINDOW;

	// Try to start the process:
	if (CreateProcess(cmd, x, NULL, NULL, TRUE, flags, env, dr, &si, &pi))
	{
		WaitForInputIdle(pi.hProcess, INFINITE);
		CloseHandle(pi.hThread); 
		CloseHandle(pi.hProcess);
		if (env != NULL) free(env);
		pm.ReleaseBuffer();
		HANDLE ph = task_SetProcessInfo(L, pi.dwProcessId);
		if (sync)
		{
			// Wait for the process to terminate and get its exit code:
			WaitForSingleObject(ph, INFINITE);
			GetExitCodeProcess(ph, &res);
		}
		else
		{
			// Add the application to the list of apps to be watched:
			lua_pushvalue(L, 1);
			task_RegisterWatch(L, pi.dwProcessId);
			lua_pop(L, 1);
		}
	}
	else
	{
		// If the app could not be run, return nil and the error code:
		DWORD err = GetLastError();
		lua_settop(L, 0);
		lua_pushnil(L);
		lua_pushinteger(L, err);
		if (env != NULL) free(env);
		pm.ReleaseBuffer();
		return 2;
	}

	// If the app was started, get its window information and return boolean true if it
	// is still running. If it was run synchronously, return the process exitcode. If
	// it was run asynchronously but has already terminated or the main window could not
	// be found, return boolean false.
	lua_settop(L, 1);
	if (task_AppCheck(L, TRUE))
		lua_pushboolean(L, TRUE);
	else
		if (res < 0)
			lua_pushboolean(L, FALSE);
		else
			lua_pushnumber(L, (lua_Number)res);
	return 1;
}

// P1: Window index to select (number), or string key.
// R1: The (new) 'current window' index.
static int task_Window(lua_State* L)
{
	static const char* sl [] = {"next", "current", "previous", "first", "cbowner", NULL};
	int cm = -1; int wk = 0;
	WINSH_LUA(3)
	if (!task_AppCheck(L, TRUE)) return 0;
	lua_settop(L, 2);
	if (lua_isnumber(L, 2)) {
		wk = lua_tointeger(L, 2);
	} else {
		cm = luaL_checkoption(L, 2, "current", sl);
	}
	switch (cm) {
	case 0: //next
		wk = task_FindWindow(L, 1);
		break;
	case 1: //current
		wk = task_FindWindow(L, 0);
		break;
	case 2: //previous
		wk = task_FindWindow(L, -1);
		break;
	case 3: //first
		wk = task_FindWindow(L, -2);
		break;
	case 4: //cbowner
		lua_getfield(L, 1, "_wix");
		lua_pushinteger(L,(lua_Integer)GetClipboardOwner());
		lua_rawget(L, -2);
		if (lua_isnumber(L, -1)) {
			wk = lua_tointeger(L, -1);
		} else {
			wk = 0;
		}
		lua_pop(L, 2);
		break;
	default: //numeric selector
		lua_rawgeti(L, 1, wk);
		if (lua_isnumber(L, -1)) {
			lua_pop(L, 1);
			lua_pushinteger(L, wk);
			lua_setfield(L, 1, "_cw");
			wk = task_FindWindow(L, 0);
		} else {
			lua_pop(L, 1);
			wk = 0;
		}
		break;
	}
	if (wk < 1) lua_pushnil(L); else lua_pushunsigned(L, wk);
	return 1;
}

static int task_ApplicationWindowTitle(lua_State* L)
{
	WINSH_LUA(3)
	if (!task_AppCheck(L, TRUE)) return 0;
	lua_getfield(L, 1, "_cw");
	lua_rawget(L, 1);
	CWindow c((HWND)lua_tointeger(L, -1));
	lua_pop(L, 1);
	if (c.IsWindow()) {
		lua_pop(L, 1);
		CString x("");
		int l = c.GetWindowTextLength();
		if (l > 0)
		{
			l++;
			c.GetWindowText(x.GetBuffer(l), l);
			x.ReleaseBuffer();
		}
		luaX_pushstring(L, x);
		return 1;
	} else {
		lua_pushnil(L);
		return 1;
	}
}

int task_ApplicationWindowFrame(lua_State* L)
{
	WINSH_LUA(5)
	if (!task_AppCheck(L, TRUE)) return 0;
	lua_getfield(L, 1, "_cw");
	lua_rawget(L, 1);
	CWindow c((HWND)lua_tointeger(L, -1));
	lua_pop(L, 1);
	if (c.IsWindow()) {
		if (lua_istable(L, 2)) {
			UINT fg = SWP_NOACTIVATE|SWP_NOOWNERZORDER|SWP_NOZORDER;
			long w, h, l, t;
			lua_getfield(L, -1, "_w"); w = luaL_optlong(L, -1, 0); lua_pop(L, 1);
			lua_getfield(L, -1, "_h"); h = luaL_optlong(L, -1, 0); lua_pop(L, 1);
			lua_getfield(L, -1, "_l"); l = luaL_optlong(L, -1, 0); lua_pop(L, 1);
			lua_getfield(L, -1, "_t"); t = luaL_optlong(L, -1, 0); lua_pop(L, 1);
			if ((w < 1) || (h < 1)) fg |= SWP_NOSIZE;
			c.SetWindowPos(NULL, l, t, w, h, fg);
		}
		CRect r;
		WINDOWPLACEMENT wp;
		wp.length = sizeof(WINDOWPLACEMENT);
		if (c.GetWindowPlacement(&wp))
		if ((wp.showCmd == SW_SHOWNORMAL) && (c.GetWindowRect(r)))
		{
			lua_pushinteger(L, r.left);
			lua_pushinteger(L, r.top);
			lua_pushinteger(L, r.Width());
			lua_pushinteger(L, r.Height());
			luaC_newobject(L, 4, "task.Frame");
		}
		else
		{
			lua_pushnil(L);
		} 
	} else {
		lua_pushnil(L);
	}
	return 1;
}

int task_ApplicationWindowState(lua_State* L)
{
	static const char* ws [] = {"normal", "minimized", "maximized", "hidden", NULL};
	WINSH_LUA(3)
	if (!task_AppCheck(L, TRUE)) return 0;
	lua_getfield(L, 1, "_cw");
	lua_rawget(L, 1);
	CWindow c((HWND)lua_tointeger(L, -1));
	lua_pop(L, 1);
	if (c.IsWindow()) {
		if (lua_isstring(L, 2)) {
			switch (luaL_checkoption(L, 2, "normal", ws))
			{
			case 1:
				c.ShowWindow(SW_MINIMIZE);
				break;
			case 2:
				c.ShowWindow(SW_MAXIMIZE);
				break;
			case 3:
				c.ShowWindow(SW_HIDE);
				break;
			default:
				c.ShowWindow(SW_NORMAL);
				break;
			}
		}
		WINDOWPLACEMENT wp;
		wp.length = sizeof(WINDOWPLACEMENT);
		wp.showCmd = SW_SHOWNORMAL;
		if (c.GetWindowPlacement(&wp))
		{
			switch (wp.showCmd)
			{
			case SW_SHOWMAXIMIZED:
				lua_pushstring(L, "maximized");
				break;
			case SW_SHOWMINIMIZED:
				lua_pushstring(L, "minimized");
				break;
			default:
				{
					if (c.IsWindowVisible())
						lua_pushstring(L, "normal");
					else
						lua_pushstring(L, "hidden");
				}
				break;
			}
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

// P0: (optional) String key "app" or "force" - if missing current window is closed.
// R1: Boolean - true if the application was closed.
static int task_ApplicationClose(lua_State* L)
{
	WINSH_LUA(2)
	static const char* fk [] = {"app","force",NULL};
	int nw = -1; CWindow c;
	if (!task_AppCheck(L, TRUE)) return 0;
	lua_settop(L, 2);
	if (lua_isstring(L, 2)) {
		nw = luaL_checkoption(L, 2, NULL, fk);
		lua_getfield(L, 1, "_wix");
		lua_pushnil(L);
		while (lua_next(L, 3)) {
			lua_pop(L, 1);
			c = CWindow((HWND)lua_tointeger(L, -1));
			c.PostMessage(WM_CLOSE);
		}
		lua_pop(L, 1);
		if (nw == 1) {
			lua_getfield(L, 1, "_ph");
			TerminateProcess((HANDLE)lua_tointeger(L, -1), 0);
			lua_pop(L, 1);
		}
	} else {
		lua_getfield(L, 1, "_cw");
		lua_rawget(L, 1);
		c = CWindow((HWND)lua_tointeger(L, -1));
		lua_pop(L, 1);
		if (c.IsWindow()) {c.PostMessage(WM_CLOSE);}
	}
	lua_pushboolean(L, !task_AppCheck(L, TRUE));
	return 1;
}

BOOL task_MatchWindow(HWND sample, UINT pid, HWND targ)
{
	DWORD spid; HWND h;
	if (!IsWindow(sample)) return FALSE;
	GetWindowThreadProcessId(sample, &spid);
	//ATLTRACE2("MatchWindow: %i==%i,%i==%i\n", pid, spid, sample, targ);
	if (spid != pid) return FALSE;
	if (!IsWindow(targ)) return TRUE;
	h = sample;
	while (IsWindow(h)) {
		if (h == targ) return TRUE;
		h = GetParent(h);
	    //ATLTRACE2("MatchWindow Parent: %i==%i\n", targ, h);
	}
	return FALSE;
}

static int task_ApplicationSendKeys(lua_State* L)
{
	DWORD pid; HWND w1, w2, w3, w4; INPUT ips[4]; int c; BOOL m; const char* s; size_t sz; SHORT x;
	WINSH_LUA(1)
	luaC_checkmethod(L);
	if (!task_AppCheck(L, TRUE)) {lua_pushboolean(L, FALSE); return 1;}
	lua_getfield(L, 1, "_cw");
	lua_rawget(L, 1);
	w1 = (HWND)lua_tointeger(L, -1);
	lua_pop(L, 1);
	lua_getfield(L, 1, "_pid"); pid = (lua_isnumber(L, -1))? lua_tointeger(L, -1) : 0; lua_pop(L, 1);
	if (pid == 0) {lua_pushboolean(L, FALSE); return 1;}
	//ATLTRACE2("SendKeys: %i,%i\n", pid, w1);
	for (int i = 0; (i < 4); i++) {
		ips[i].type = INPUT_KEYBOARD;
		ips[i].ki.time = ips[i].ki.dwFlags = ips[i].ki.dwExtraInfo = ips[i].ki.wScan = 0;
	}
	ips[2].ki.dwFlags = ips[3].ki.dwFlags = KEYEVENTF_KEYUP;
	ips[0].ki.wVk = ips[3].ki.wVk = VK_MENU;
	ips[1].ki.wVk = ips[2].ki.wVk = VK_ESCAPE;
	w2 = w3 = w4 = 0;
	do {
		w4 = w2; // remember last hwnd to check it changes.
		if (w3 == 0) w3 = w2; // remember first hwnd to check for wrap-round.
		w2 = GetForegroundWindow();
		m = task_MatchWindow(w2, pid, w1); if (m) {break;}
		if (SendInput(4, ips, sizeof(INPUT)) != 4) {break;}
		Sleep(100);
	} while ((w2 != w3) && (w2 != w4));
	//ATLTRACE2("=====\n");
	if (!m) {lua_pushboolean(L, FALSE); return 1;}

	for (int i = 2; (i <= T); i++) {
		if (lua_type(L, i) == LUA_TSTRING) {
			s = lua_tolstring(L, i, &sz);
			for (UINT j = 0; (j < sz); j++) {
				ips[0].ki.wVk = VK_SPACE; m = false;
				x = VkKeyScan(s[j]);
				m = (HIBYTE(x) == 1);
				ips[0].ki.wVk = LOBYTE(x);
				if (m) {
					ips[2].ki.wVk = ips[1].ki.wVk = ips[0].ki.wVk;		
					ips[0].ki.wVk = ips[3].ki.wVk = VK_SHIFT;
					ips[0].ki.dwFlags = ips[1].ki.dwFlags = 0;
					ips[2].ki.dwFlags = ips[3].ki.dwFlags = KEYEVENTF_KEYUP;
					if (!task_MatchWindow(GetForegroundWindow(), pid, w1)) {lua_pushboolean(L, FALSE); return 1;}
					SendInput(4, ips, sizeof(INPUT));
				} else {
					ips[1].ki.wVk = ips[0].ki.wVk;
					ips[0].ki.dwFlags = 0;
					ips[1].ki.dwFlags = KEYEVENTF_KEYUP;
					if (!task_MatchWindow(GetForegroundWindow(), pid, w1)) {lua_pushboolean(L, FALSE); return 1;}
					SendInput(2, ips, sizeof(INPUT));
				}
			}
		} else if (lua_type(L, i) == LUA_TNUMBER) {
			c = lua_tointeger(L, i);
			if ((c < 0) || (c > 255)) {
				Sleep(250);
			} else {
				if (!lua_istable(L, T+1)) {lua_newtable(L);}
				lua_rawgeti(L, T+1, c);
				if (lua_toboolean(L, -1)) {
					lua_pop(L, 1);
					lua_pushboolean(L, FALSE);
					ips[0].ki.dwFlags = KEYEVENTF_KEYUP;
				} else {
					lua_pop(L, 1);
					lua_pushboolean(L, TRUE);
					ips[0].ki.dwFlags = 0;
				}
				ips[0].ki.wVk = c;
				if (!task_MatchWindow(GetForegroundWindow(), pid, w1)) {lua_pushboolean(L, FALSE); return 1;}
				SendInput(1, ips, sizeof(INPUT));
				lua_rawseti(L, T+1, c);
			}
		}
	}
	if (lua_istable(L, T+1)) {
		lua_pushnil(L);
		while (lua_next(L, T+1)) {
			if (lua_toboolean(L, -1)) {
				ips[0].ki.wVk = lua_tointeger(L, -2);
				ips[0].ki.dwFlags = KEYEVENTF_KEYUP;
				if (!task_MatchWindow(GetForegroundWindow(), pid, w1)) {lua_pushboolean(L, FALSE); return 1;}
				SendInput(1, ips, sizeof(INPUT));
			}
			lua_pop(L, 1);
		}
	}
	lua_pushboolean(L, TRUE);
	return 1;
}

// '__tostring' metamethod returns a string characterising the object.
static int task_ApplicationToString(lua_State* L)
{
	WINSH_LUA(1)
	lua_getfield(L, 1, "Command");
	if (lua_isstring(L, -1)) return 1;
	lua_pop(L, 1);
	lua_pushstring(L, "Application");
	return 1;
}

static int app_Construct(lua_State* L)
{
	WINSH_LUA(6)
	if (lua_isnumber(L, 1))
	{	// Application object for a task which is already executing:
		UINT pid = (UINT)lua_tointeger(L, 1);
		lua_settop(L, 0);
		lua_rawgeti(L, LUA_REGISTRYINDEX, task_appix);	//|AI
		lua_rawgeti(L, -1, pid);						//|T|AI
		if (lua_istable(L, -1)) { // Return existing application object for this task:
			lua_remove(L, 1);							//|T|
			task_AppCheck(L, TRUE);
			return 1;
		} else { // Create new application object for this task:
			lua_settop(L, 0);
			lua_newtable(L);							//|T|
			task_SetProcessInfo(L, pid);
			task_RegisterWatch(L, pid);
			task_AppCheck(L, TRUE);
		}
	}
	else
	{	// Application object for a task to be started later:
		lua_settop(L, 2);
		lua_newtable(L);									//|T|P2|P1
		CString cl1(luaL_optstring(L, 1, "%ComSpec%"));
		CString cl2("");
		ExpandEnvironmentStrings(cl1.GetBuffer(cl1.GetLength()), cl2.GetBuffer(32767), 32767);
		cl1.ReleaseBuffer(); cl2.ReleaseBuffer();
		luaX_pushstring(L, cl2);							//|C|T|P2|P1
		lua_setfield(L, -2, "Command");					    //|T|P2|P1
		cl1.Empty(); cl2.Empty();
		if (lua_isstring(L, 2))
		{
			cl1 = CString(lua_tostring(L, 2));
			ExpandEnvironmentStrings(cl1.GetBuffer(cl1.GetLength()), cl2.GetBuffer(32767), 32767);
			cl1.ReleaseBuffer(); cl2.ReleaseBuffer();
			luaX_pushstring(L, cl2);						//|D|T|P2|P1
			lua_setfield(L, -2, "Directory");				//|T|P2|P1
		}
		lua_remove(L, 1);
		lua_remove(L, 1);									//|T|
	}
	if (luaC_gettid(L,0) != 1) return luaL_error(L, "Bad Class");
	lua_setmetatable(L, -2);									//|T|
	return 1;
}


void application_Create(lua_State* L)
{
	static const struct luaL_Reg ml [] = {
		{"open", task_ApplicationOpen},
		{"close", task_ApplicationClose},
		{"window", task_Window},
		{"windowtitle", task_ApplicationWindowTitle},
		{"windowstate", task_ApplicationWindowState},
		{"windowframe", task_ApplicationWindowFrame},
		{"sendkeys", task_ApplicationSendKeys},
		{"__tostring", task_ApplicationToString},
		{NULL, NULL}
	};
	luaC_newclass(L, app_Construct, ml);
};

#pragma endregion

#pragma region "Task" library functions

// Monitor Enumerator
BOOL CALLBACK task_MonitorEnum(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
	lua_State* L = (lua_State*)dwData;
	WINSH_LUA(7)
	MONITORINFO mi;
	mi.cbSize = sizeof(MONITORINFO);
	GetMonitorInfo(hMonitor, &mi);

	if (lua_toboolean(L, 2))										//|C|F|T
	{
		lua_pushinteger(L, mi.rcMonitor.left);						//|L|C|F|T
		lua_pushinteger(L, mi.rcMonitor.top);						//|T|L|C|F|T
		lua_pushinteger(L, mi.rcMonitor.right - mi.rcMonitor.left);	//|L|T|L|C|F|T
		lua_pushinteger(L, mi.rcMonitor.bottom - mi.rcMonitor.top);	//|T|L|T|L|C|F|T
	}
	else
	{
		lua_pushinteger(L, mi.rcWork.left);							//|L|C|F|T
		lua_pushinteger(L, mi.rcWork.top);							//|T|L|C|F|T
		lua_pushinteger(L, mi.rcWork.right - mi.rcWork.left);		//|L|T|L|C|F|T
		lua_pushinteger(L, mi.rcWork.bottom - mi.rcWork.top);		//|T|L|T|L|C|F|T
	}
	luaC_newobject(L, 4, "task.Frame");									//|O|C|F|T

	if ((mi.dwFlags & MONITORINFOF_PRIMARY) != 0)
	{
		lua_rawseti(L, 1, 1);										//|C|F|T
	}
	else
	{
		int p = lua_tointeger(L, 3);
		lua_rawseti(L, 1, p);										//|C|F|T
		lua_pop(L, 1);												//|F|T
		lua_pushinteger(L, p + 1);									//|C|F|T
	}
	return TRUE;
}

static int task_Monitors(lua_State* L)
{
	WINSH_LUA(1)
	BOOL full = lua_toboolean(L, 1);
	lua_settop(L, 0);
	luaC_newobject(L, 0, "class.List");
	lua_pushboolean(L, full);
	lua_pushinteger(L, 2);		// ix = 1 reserved for primary monitor.
	EnumDisplayMonitors(NULL, NULL, task_MonitorEnum, (LPARAM)L);
	lua_pushvalue(L, 1);
	return 1;
}

// Returns a frame object representing the overall bounds of the Virtual Desktop.
// This is the minimum rectangle enclosing the total areas of all the monitors.
// Note that it may include areas which do not show on any monitor. On a single
// monitor system, it is the same as the full (not the working) area of the monitor.
static int task_VirtualScreen(lua_State* L)
{
	WINSH_LUA(1)
	lua_settop(L, 0);
	lua_pushinteger(L, GetSystemMetrics(SM_XVIRTUALSCREEN));
	lua_pushinteger(L, GetSystemMetrics(SM_YVIRTUALSCREEN));
	lua_pushinteger(L, GetSystemMetrics(SM_CXVIRTUALSCREEN));
	lua_pushinteger(L, GetSystemMetrics(SM_CYVIRTUALSCREEN));
	luaC_newobject(L, 4, "task.Frame");
	return 1;
}

static BOOL CALLBACK task_EnumWindowsProc2(HWND hwnd, LPARAM lParam)
{
	lua_State* L = (lua_State*)lParam;
	WINSH_LUA(2)
	CWindow w(hwnd);
	if (!IsTaskWindow(w)) return TRUE;
	if (lua_type(L, -2) == LUA_TTABLE)
	{
		int r = w.GetWindowTextLength();
		if (r > 0)
		{
			int k = lua_tointeger(L, -1);
			DWORD id = w.GetWindowProcessID();
			CString n;
			w.GetWindowText(n.GetBuffer(r+1),r+1);
			n.ReleaseBuffer();
			luaX_pushstring(L, n);
			lua_rawseti(L, -3, k);
			lua_pop(L, 1);
			lua_pushinteger(L, ++k);
		}
	}
	else
	{
		int ty = lua_tointeger(L, -1);
		if (ty == 3) {
			lua_pushvalue(L, -2);
			lua_pushinteger(L, 1);
			lua_arith(L, LUA_OPSUB);
			lua_pushinteger(L, 1);
			if (lua_compare(L, -2, -1, LUA_OPLT)) {
				lua_pop(L, 3);
				lua_pushinteger(L, w.GetWindowProcessID());
				lua_pushboolean(L, TRUE);
				return FALSE;
			}
			lua_pop(L, 1);
			lua_replace(L, -3);
		} else {
			CString c(lua_tostring(L, -2));
			c.MakeUpper();
			int r = w.GetWindowTextLength();
			if (r >= c.GetLength())
			{
				CString n;
				w.GetWindowText(n.GetBuffer(r+1),r+1);
				n.ReleaseBuffer();
				n.MakeUpper();
				if (((ty == 0) && (n.Left(c.GetLength()) == c)) ||
				    ((ty == 1) && (n.Right(c.GetLength()) == c)) ||
				    ((ty == 2) && (n == c)))
				{
					lua_pop(L, 1);
					lua_pushinteger(L, w.GetWindowProcessID());
					lua_pushboolean(L, TRUE);
					return FALSE;
				}
			}
		}
	}
	return TRUE;
}

// R1: List of title texts of top-level windows in z-order
static int task_Applications(lua_State* L)
{
	WINSH_LUA(2)
	luaC_newobject(L, 0, "class.List");
	lua_pushinteger(L, 1);
	EnumWindows(task_EnumWindowsProc2, (LPARAM)L);
	lua_pop(L, 1);
	return 1;
}

// P1: String Key.
// P2: Optional parameter, number or string depends on key.
// R1: Process ID (number) or nil.
static int task_Application(lua_State* L)
{
	WINSH_LUA(2)
	static const char* k [] = {"prefix","suffix","title","order","active","cbowner",NULL};
	HWND h; DWORD p;
	lua_settop(L, 2);
	int x = luaL_checkoption(L, 1, "active", k);
	switch (x) {
	case 0:
	case 1:
	case 2:
		luaL_checkstring(L, 2);
		lua_pushinteger(L, x);
		EnumWindows(task_EnumWindowsProc2, (LPARAM)L);
		break;
	case 3:
		luaL_checknumber(L, 2);
		lua_pushinteger(L, x);
		EnumWindows(task_EnumWindowsProc2, (LPARAM)L);
		break;
	case 5:
		h = GetClipboardOwner();
		if (IsWindow(h)) {
			GetWindowThreadProcessId(h, &p);
			lua_pushinteger(L, (lua_Integer)p);
			lua_pushboolean(L, TRUE);
		}
		break;
	default:
		h = GetActiveWindow();
		if (IsWindow(h)) {
			GetWindowThreadProcessId(h, &p);
			lua_pushinteger(L, (lua_Integer)p);
			lua_pushboolean(L, TRUE);
		}
		break;
	}
	if (lua_type(L, -1) == LUA_TBOOLEAN) {lua_pop(L, 1); return 1;}
	return 0;
}

#define CLIP_FORMATS (8)	// Number of supported clipboard formats
#define CC_TEXT (0)			// Index numbers of supported formats
#define CC_UNICODE (1)
#define CC_HDROP (2)
#define CC_RICHTEXT (3)
#define CC_HTMLURL (4)
#define CC_HTML (5)
#define CC_HTMLMAX (6)
#define CC_HTMLMIN (7)      // ----
// Lua names for supported formats:
const char* clip_types [] = {"text","unicode","filelist","richtext","htmlurl","html","htmlmax","htmlmin", NULL};
// Windows API codes for supported formats (0 values are set programatically):
UINT clip_codes [] = {CF_TEXT,CF_UNICODETEXT,CF_HDROP,0,0,0,0,0}; // 0 values are filled in during initialisation.

// Reads an ANSI clipboard format API code 'typ' and pushes as string onto Lua stack
// Returns TRUE on success.
bool clip_getansi(lua_State* L, int typ) {
	HGLOBAL hglb; const char* txt; size_t sz; bool r = false;
	if (OpenClipboard(NULL)) {
		hglb = GetClipboardData(typ);
		if (hglb != NULL) {
			sz = GlobalSize(hglb);
			txt = (const char*)GlobalLock(hglb);
			for (size_t i = 0; (i < sz); i++) if (txt[i] == 0) sz = i;
			if (txt != NULL) {
				lua_pushlstring(L, txt, sz);
				r = true;
				GlobalUnlock(hglb);
			}
		}
		CloseClipboard();
	}
	return r;
}

bool clip_getunicode(lua_State*L) {
	HGLOBAL hglb; const char* txt; size_t sz; bool r = false;
	if (OpenClipboard(NULL)) {
		hglb = GetClipboardData(CF_UNICODETEXT);
		if (hglb != NULL) {
			sz = GlobalSize(hglb);
			txt = (const char*)GlobalLock(hglb);
			for (size_t i = 0; (i < sz); i++) if ((txt[i] == 0) && (txt[i+1] == 0)) sz = i;
			if (txt != NULL) {
				lua_pushlstring(L, txt, sz);
				r = true;
				GlobalUnlock(hglb);
			}
		}
		CloseClipboard();
	}
	return r;
}

// Reads a CF_HDROP (Filelist) format from the clipboard and unpacks the filenames into
// a Lua table on the stack (list format). Returns TRUE on success.
bool clip_getfilelist(lua_State* L) {
	HGLOBAL hglb; HDROP hDrop; bool v = false;
	TCHAR m_buff[MAX_PATH+5]; UINT nf = 0;
	if (OpenClipboard(NULL)) {
		hglb = GetClipboardData(CF_HDROP);
		if (hglb != NULL) {
			hDrop = (HDROP)GlobalLock(hglb);
			if (hDrop != NULL) {
				nf = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);
				lua_createtable(L, nf, 0);
				v = true;
				for (UINT i = 0; (i < nf); i++) {
					DragQueryFile(hDrop, i, m_buff, MAX_PATH);
					luaX_pushstring(L, m_buff);
					lua_rawseti(L, -2, i+1);
				}
				GlobalUnlock(hglb);
			}
		}
		CloseClipboard();
	}
	return v;
}

CString clip_getvalue(CString s, CString t) {
	int ps, pe, ln;
	ln = t.GetLength();
	ps = s.Find(t, 0);
	if (ps < 0) return CString("");
	pe = s.Find(_T("\n"),ps);
	if (pe < 0) pe = s.GetLength();
	return s.Mid(ps + ln, pe - (ps + ln));
}

bool gethtml(lua_State* L, int ty) {
	if (!clip_getansi(L, clip_codes[ty])) return false;
	int ps, pe; CString rx;
	CString tx(lua_tostring(L, -1));
	lua_pop(L, 1);
	switch (ty) {
	case CC_HTMLURL:
		rx = clip_getvalue(tx, CString("SourceURL:"));
		break;
	case CC_HTMLMIN:
		luaX_pushstring(L, clip_getvalue(tx, CString("StartFragment:")));
		ps = lua_tointeger(L, -1);
		lua_pop(L, 1);
		luaX_pushstring(L, clip_getvalue(tx, CString("EndFragment:")));
		pe = lua_tointeger(L, -1);
		lua_pop(L, 1);
		rx = tx.Mid(ps, pe - ps);
		break;
	default:
		luaX_pushstring(L, clip_getvalue(tx, CString("StartHTML:")));
		ps = lua_tointeger(L, -1);
		lua_pop(L, 1);
		luaX_pushstring(L, clip_getvalue(tx, CString("EndHTML:")));
		pe = lua_tointeger(L, -1);
		lua_pop(L, 1);
		rx = tx.Mid(ps, pe - ps);
		break;
	}
	luaX_pushstring(L, rx);
	return true;
}

// Pn : A list of format name strings in priority order, if absent defaults to "text".
// R1 : The name of the highest priority format available.
// R2 : The value of the format, Table for filelist, otherwise String.
static int task_GetClipboard(lua_State* L) {
	UINT pl[CLIP_FORMATS]; UINT nc = lua_gettop(L); int fmt; bool r = false; int htyp = 0;
	if (nc > CLIP_FORMATS) nc = CLIP_FORMATS;
	for (int i = 0; (i < (int)nc); i++) {
		fmt = luaL_checkoption(L, i+1, "", clip_types);
		if ((htyp == 0) && (fmt >= CC_HTMLURL)) htyp = fmt;
		pl[i] = clip_codes[fmt];
	}
	if (nc < 1) {nc = 1; pl[0] = CF_TEXT;}
	lua_settop(L, 0);
	fmt = GetPriorityClipboardFormat(pl, nc);
	if (fmt < 1) {lua_pushnil(L); return 1;}
	for (int i = 0; (i < CLIP_FORMATS); i++) if ((UINT)fmt == clip_codes[i]) {fmt = i; break;}
	if (fmt >= CC_HTMLURL) fmt = htyp;
	switch (fmt) {
	case CC_UNICODE:
		r = clip_getunicode(L);
		break;
	case CC_HDROP:
		r = clip_getfilelist(L);
		break;
	case CC_HTMLURL:
	case CC_HTML:
	case CC_HTMLMAX:
	case CC_HTMLMIN:
		return r = gethtml(L, fmt);
		break;
	default:
		return r = clip_getansi(L, clip_codes[fmt]);
		break;
	}
	if (r) {
		lua_pushstring(L, clip_types[fmt]);
		return 2;
	}
	return 0;
}

// Accepts a string at stack index 'ix' and writes it to the clipboard in the format API code 'typ'.
void clip_putansi(lua_State* L, int ix, int typ) {
	HGLOBAL hgbl; const char* txt; size_t len; void* txtt;
	if ((ix == 0) || (!lua_isstring(L, ix))) {
		txt = ""; len = 0;
	} else {
		txt = lua_tolstring(L, ix, &len);
	}
	hgbl = GlobalAlloc(GMEM_MOVEABLE, len+1);
	txtt = GlobalLock(hgbl);
	memcpy(txtt, txt, len+1);
	GlobalUnlock(hgbl);
	SetClipboardData(typ, (HANDLE)txtt);
}

void clip_putunicode(lua_State* L, int ix) {
	HGLOBAL hgbl; const char* txt; size_t len; void* txtt;
	if ((ix == 0) || (!lua_isstring(L, ix))) {
		txt = ""; len = 0;
	} else {
		txt = lua_tolstring(L, ix, &len);
	}
	hgbl = GlobalAlloc(GMEM_MOVEABLE, len+1);
	txtt = GlobalLock(hgbl);
	memcpy(txtt, txt, len+1);
	GlobalUnlock(hgbl);
	SetClipboardData(CF_UNICODETEXT, (HANDLE)txtt);
}

// Accepts a table at stack index 'ix', which must be a list of filenames, and writes them in
// CF_HDROP (filelist) format to the clipboard.
void clip_putfilelist(lua_State* L, int ix) {
	HGLOBAL hgbl; size_t len; DROPFILES* txtt;
	CString ss; CPathString vv = CString("");
	if ((ix == 0) || (!lua_istable(L, ix))) return;
	for (int i = 1; (i < 10000); i++) {
		lua_rawgeti(L, ix, i);
		if (lua_isnil(L,-1)) break;
		ss = CString(lua_tostring(L, -1));
		vv += ss += CString("|");
	}
	vv.MakeMultistring();
	len = sizeof(DROPFILES)+sizeof(TCHAR)*(vv.GetLength()+1);
	hgbl = GlobalAlloc(GHND|GMEM_SHARE, len);
	txtt = (DROPFILES*)GlobalLock(hgbl);
	txtt->pFiles = sizeof(DROPFILES);
#ifdef _UNICODE
	txtt->fWide = TRUE;
#endif
	memcpy((void*)((char*)txtt + sizeof(DROPFILES)), vv, sizeof(TCHAR)*vv.GetLength());
	GlobalUnlock(hgbl);
	SetClipboardData(CF_HDROP, (HANDLE)txtt);
}

void clip_puthtml(lua_State* L, CString html, CString url) {
	int sh, eh, sf, ef; CString cb;
	sh = 97;
	eh = html.GetLength();
	sf = html.Find(CString("<!--StartFragment-->"));
	ef = html.Find(CString("<!--EndFragment-->"));
	if ((sf >= 0) && (ef > sf)) {
		sf += 20; ef -= 1;
	} else {
		sf = sh; ef = eh;
	}
	if (url.GetLength() > 0) {
		sh = sh + 12 + url.GetLength();
		eh += sh; sf += sh; ef += sh;
		cb.Format(CString("Version:0.9\r\nStartHTML:%08u\r\nEndHTML:%08u\r\nStartFragment:%08u\r\nEndFragment:%08u\r\nSourceURL:"), sh, eh, sf, ef);
		cb += url; cb += CString("\r\n");
	} else {
		eh += sh; sf += sh; ef += sh;
		cb.Format(CString("Version:0.9\r\nStartHTML:%08u\r\nEndHTML:%08u\r\nStartFragment:%08u\r\nEndFragment:%08u\r\n"), sh, eh, sf, ef);
	}
	cb += html;
	luaX_pushstring(L, cb);
	clip_putansi(L, -1, clip_codes[CC_HTML]);
}

// Pn: Pairs of format names and values or a single parameter is value in CF_TEXT format.
// All supplied formats are written to the clipboard. The 'value' must be a table for
// 'filelist' format, otherwise a string.
static int task_SetClipboard(lua_State* L) {
	UINT nc = lua_gettop(L); int ty = 0; CString ht; CString hu;
	if (OpenClipboard(NULL)) {
		EmptyClipboard();
		if (nc < 2) {
			clip_putansi(L, 1, CF_TEXT);
		} else {
			for (UINT i = 1; (i <= nc); i += 2) {
				ty = luaL_checkoption(L, i, "text", clip_types);
				switch (ty) {
				case CC_UNICODE:
					clip_putunicode(L, i+1);
					break;
				case CC_HDROP:
					clip_putfilelist(L, i+1);
					break;
				case CC_HTMLURL:
					hu = CString(luaL_checkstring(L, i+1));
					break;
				case CC_HTML:
				case CC_HTMLMIN:
				case CC_HTMLMAX:
					ht = CString(luaL_checkstring(L, i+1));
					break;
				default:
					clip_putansi(L, i+1, clip_codes[ty]);
					break;
				}
			}
		}
		if (ht.GetLength() > 0) clip_puthtml(L, ht, hu);
		CloseClipboard();
	}
	return 0;
}

#pragma endregion


// =================================================================================================

LUATKLIB_API int LUATKLIB_NGEN(luaopen_)(lua_State* L)
{
	static const luaL_Reg lf [] = {
		{"virtualscreen", task_VirtualScreen},
		{"monitors", task_Monitors},
		{"applications", task_Applications},
		{"application",task_Application},
		{"getclipboard", task_GetClipboard},
		{"setclipboard", task_SetClipboard},
		{NULL, NULL}
	};

	WINSH_LUA(2)
	H->Require(CString("class"));

// These functions are only available from Vista onwards, access dynamically for compatability with XP:
#ifdef UNICODE
	PQFPIN = (QFPIN*)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "QueryFullProcessImageNameW");
	PGMBN = (GMBN*)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "GetModuleBaseNameW");
#else
	PQFPIN = (QFPIN*)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "QueryFullProcessImageNameA");
	PGMBN = (GMBN*)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "GetModuleBaseNameA");
#endif
	PACFL = (ACFL*)GetProcAddress(GetModuleHandle(TEXT("USER32.DLL")), "AddClipboardFormatListener");

	//Register clipboard formats (if not already registered) and enable WM_CLIPBOARDUPDATE:
	clip_codes[CC_RICHTEXT] = RegisterClipboardFormatA("Rich Text Format");
	clip_codes[CC_HTMLURL] = RegisterClipboardFormatA("HTML Format");
	clip_codes[CC_HTML] = clip_codes[CC_HTMLURL];
	clip_codes[CC_HTMLMAX] = clip_codes[CC_HTMLURL];
	clip_codes[CC_HTMLMIN] = clip_codes[CC_HTMLURL];
	if (PACFL) (PACFL)(H->GetHWND());

	//Index of active Application objects keyed by process id.
	task_appix = H->GetRegistryTable(task_appix, 0, "v");
	lua_pop(L, 1);

	task_StopMonitorThread();		//In case we are resetting.
	int m = H->AllocLuaMessages(1);
	task_StartMonitorThread(m, H->GetHWND());
	lua_pushcfunction(L, task_OnMessage);
	H->SetLuaMessageHandler(m);
	lua_pop(L, 1);

	lua_createtable(L, 0, (int)(sizeof(lf)/sizeof(lf)[0] + 1)); // -1 for null terminator, +2 for classes
	luaL_setfuncs(L, lf, 0);

	application_Create(L);
	lua_setfield(L, -2, "Application");

	// Load and execute the Lua part of the library:
	H->LoadScriptResource(CString("LibTask"));
	lua_pushvalue(L, -2);
	H->ExecChunk(1, CString("LibTask-LuaPart"));

	return 1;
}
