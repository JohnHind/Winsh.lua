#pragma once
#include "JHCLua.h"
#include "JHCMsgTrap.h"
#include "JHCCMenu.h"
#include "Configuration.h"

// Lua functions implemented in C which use the Winsh Lua environment should commence
// with the Macro "WINSH_LUA(s)" defined in this file. This macro requires the symbol
// 'L' to be the Lua state pointer. It sets the symbol 'H' pointing to the Winsh
// interface implementation and 'T' being an integer, the Lua stack top at entry. The
// parameter 's' is an integer 1 or greater which documents the maximum number of stack
// slots used in the function.
// Example template:
// #include "LuaLibIF.h"
// static int MyLuaFunction(lua_State* L)
// {
//     WINSH_LUA(1)
//     CString s = H->GetAppName();
//     luaX_pushstring(L, s);
//     return 1;
// }

// Error/Message report mode masks.
#define ERM_GUI			(0x01)
#define ERM_REPORT		(0x02)
#define ERM_STDIO		(0x04)
#define ERM_FILE		(0x08)
#define ERM_DEBUGGER	(0x10)

// Winsh Messages which may be used (like Windows messages) in CaptureMessage.
#define GT_STARTUP				(WM_USER)		//1024
#define GT_ERROR				(WM_USER+1)		//1025
#define GT_NOTIFY_LCLICK		(WM_USER+3)		//1027; Use Send, not Post; P3 is menu handle.
#define GT_NOTIFY_RCLICK		(WM_USER+4)		//1028; Use Send, not Post; P3 is menu handle.
#define GT_REPORT_RCLICK		(WM_USER+5)		//1029; Use Send, not Post; P3 is menu handle.
#define GT_REPORT_DROP			(WM_USER+6)		//1030

// The Macro WINSH_LUA uses the Lua State (L) to recover a pointer to an implementation
// of this interface with the symbol H, so use for example H->GetInitName().
class ILuaLibWinsh : public ILuaLib
{
public:
	virtual	CString& GetInitName() = 0;
	virtual CString& GetResType() = 0;
	virtual CString& GetLuaExt() = 0;
	virtual CString& GetExePath() = 0;
	virtual CString& GetExeName() = 0;
	virtual CString& GetLastError() = 0;
	virtual CString& GetAppName() = 0;
	virtual HWND GetHWND() = 0;
	virtual void SetAppName(LPCTSTR s) = 0;
	virtual BOOL SetAppIcon(LPCTSTR icon) = 0;

	virtual BOOL Require(LPCTSTR name) = 0;
	// Allows one library to load another if it is not already loaded. "name"
	// is the name of the library which must be in the preloads table. Returns
	// TRUE if the library was already loaded or is now loaded, FALSE if it could
	// not be loaded.

	virtual int GetRegistryTable(int key = LUA_NOREF, int ix = 0, const char* w = NULL) = 0;
	// Returns after pushing a Lua table onto the stack. If present, this table
	// is recovered from the Lua internal Registry table stored under the given
	// key and that key is returned. If the key does not already exist in the
	// registry, a new key is created using the auxilliary library reference
    // mechanism to guarantee global uniqueness (this may not be the key passed
    // in and to create a new entry it is recommended to pass LUA_NOREF). If
    // ix is 0, a new table is created, otherwise the existing table at the given
    // stack index is referenced by the new entry. The final parameter may be
    // "k", "v" or "kv" and if present it makes the table week in respect to
    // key, value or both key and value.
		
	virtual UINT AllocLuaMessages(UINT num) = 0;
	// Allocate "num" LuaMessages and return the "code" of the first message
	// in the block.

	virtual UINT CaptureMessage(UINT msg, msgtraphandler fMsg, WPARAM wfltr = 0) = 0;
	// Bind a Windows or Winsh message to a LuaMessage and return its "code".
	// "msg" is the Windows message ID or one of the "GT_" codes defined above.
	// fMsg is a (C++) function which processes the message. wfltr, if non-zero,
	// specifies that the WPARAM must match as well.

	virtual void SetTimerMessage(UINT code, UINT time = 0) = 0;
	// Sets a timer for a message. 'code' must be a value in the range returned
	// by a previous AllocLuaMessages call. 'time' should be the interval in
	// milliseconds or 0 to cancel the timer. The function may be called again
	// on the same message code to change the interval.

	virtual void SetLuaMessageHandler(UINT code) = 0;
	// If there is a lua funtion at the top of the stack, that function is set
	// as the function which will be called when the message fires. Otherwise
	// the message handler is marked as unused, deleting any handler. The
	// stack is unaffected by this function. "code" must be a value in the range
	// returned by "AllocLuaMessages" or the value returned by "CaptureMessage".
	// The Lua Function will be called with three integer parameters. First is
	// "code" allowing the same handler to service several Lua messages; second
	// is the "id" a 16-bit value; third is a 32-bit value. The content of the
	// last two is determined by the CaptureMessage handler function, but the
	// 16-bit value is conventionally the original Windows message code and the
	// 32-bit value is either the original wParam or lParam.

	virtual UINT FindFreeLuaMessage(UINT code, UINT num = 1) = 0;
	// Checks messages from "code" to "code" + "num" until an unused one is found.
	// The code of this message is returned, or 0 if none are free. The whole
	// range of messages must have been previously allocated using "AllocLuaMessages"
	// or "CaptureMessage".
	
	virtual void SetReportMode(UINT r, UINT e) = 0;
	// Sets the Reporting Mode. Parameters r and e are bitmaps using the ERM
	// defines above.

	virtual BOOL SetFile(LPCTSTR fn, BOOL clr = FALSE) = 0;
	// Sets the Report File. fn is the filename, clr is true to clear the file
	// if it exists. Returns true if the file already existed or could not be
	// created.

	virtual TCHAR ReadConsole() = 0;
	// Does nothing unless the ReportMode is 3 in which case waits for and returns
	// a character received on STDIN.

	virtual BOOL SetTaskIcon(HICON icon, bool lmenu, bool rmenu) = 0;
	// Sets or removes a Task Tray icon. "icon" should be a valid Windows icon or
	// INVALID_HANDLE_VALUE to remove the icon. This can also be used to change the
	// visual icon when the Task Tray icon is already displayed. Returns TRUE if
	// icon was already visible, FALSE if not.
	
	virtual void Balloon(LPCTSTR mt, LPCTSTR ms, UINT to, DWORD ic) = 0;
	// If the Task Tray icon is visible, displays an information balloon on it. "mt"
	// is the title string, "ms" the body text. "to" is the timeout in milliseconds.
	// "ic" is the icon code (these icons are a fixed set not user-definable, per the
	// Windows API).

	virtual int LoadScriptFile(LPCTSTR fn) = 0;
	// Loads the specified file as a Lua chunk. Returns 0 on success or the error code
	// per luaL_loadfile. Either the compiled function or a string error code is
	// pushed onto the stack.

	virtual int LoadScriptResource(LPCTSTR fn) = 0;
	// Loads the specified resource as a Lua chunk. Returns 0 on success or the error code
	// per luaL_loadfile. Either the compiled function or a string error code is
	// pushed onto the stack.

	virtual HICON LoadIcon(LPCTSTR nm, int cx = 0, int cy = 0) = 0;
	// Loads a named icon from the resources optionally in a version closest to the size
	// specified. Returns the icon handle or NULL.
	
	virtual CCMenu* Menu(int from = 0) = 0;
	// Get CCMenu object for Lua configured menus: from = 0 - Task Icon; 1 - Report Window.

	virtual int ExecChunk(int parms = 0, LPCTSTR context = _T("")) = 0;
	// Normally call with 'parms' parameters on the stack over a function (or other
	// callable object). However if a string is in this slot, that string is simply printed
	// as an error message and no call attampt is made (allowing this to be used immediately
	// after LoadFile or LoadResource without handling any error they might return). On
	// return from this the function or string, any calling and any return parameters will
	// have been popped off the stack. Any errors are printed out and if the function returns
	// a string, that is also printed as an error message. Boolean false is also printed as
	// a (generic) error. If the function returns a number, that number is the return value
	// for ExecChunk, otherwise 0 for success, 1 for any error. 'context' is prefixed to
	// any error message printed.

	virtual BOOL Mutex(LPCTSTR key, UINT scope, LPCTSTR cmd) = 0;
	// Tests to see if another instance of "this" application is running. If it is, the
	// command line parameters of this instance are sent to the other instance and this
	// function returns FALSE. If not, this function returns TRUE. Identity depends on the
	// exe name, a GUID stored in the resources, any string supplied as "key", and is
	// within the scope identified by the "scope" code ( 0 - "system", 1 - "desktop",
	// 2 - "session", 3 - "user").

	virtual LPCTSTR GetLine(BOOL start = FALSE) = 0;
	// Reads a line of data from the file or stdin channel and returns it.

	virtual void SetReportWindow(int act, int px, int py) = 0;
	// Configures the Report Window. 'act' 0 - clear the window; 1 - hide the window;
	// 2 - Resize the window to px, py; 3 - Position the window at px, py; 4 - Position
	// the window at the bottom right of the primary monitor.

	virtual void SetOverlay(LPCTSTR icon) = 0;
	// Sets a Windows 7 taskbar icon overlay on the report window.

	virtual int GetInventory(int type) = 0;
	// Pushes a table containing library descriptions indexed by library name onto the stack.

};

#define WINSH_LUA(s) \
	ILuaLibWinsh* H=luaX_host<ILuaLibWinsh>(L,s); \
	int T=lua_gettop(L);
