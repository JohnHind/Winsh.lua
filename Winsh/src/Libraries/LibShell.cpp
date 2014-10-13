#include "stdafx.h"
#define LUASHLIB_BUILDING
#include "LibClass.h"
#include "LibShell.h"
#include "..\LuaLibIF.h"
#include "..\resource.h"
#include "LibTime.h"
#include "WtsApi32.h"
//NB: This file and the corresponding cfgmgr32.lib are from WDK 7.1.0
//http://www.microsoft.com/en-us/download/confirmation.aspx?id=11800
//Find them in the DDK install and copy to Dependencies directories.
#include "Cfgmgr32.h"
//
#include "..\CNameDlg.h"
#include "..\JHCPathString.h"
#include <dbt.h>


#ifdef LUASHLIB_DLL
HMODULE hM;
LUASHLIB_API BOOL APIENTRY DllMain( HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved )
	hM = hModule;
    return TRUE;
}
#endif

int shell_drvix = LUA_NOREF;

#pragma region Lua scripting object "Icon".

static void shell_CreateIcon(lua_State* L, HICON icon)
{
	HICON* p = (HICON*)lua_newuserdata(L, sizeof(HICON));
	*p = icon;
}

static int shell_IconGc(lua_State* L)
{
	WINSH_LUA(2)
	HICON h = *(HICON*)lua_touserdata(L, 1);
	if (h != NULL) DestroyIcon(h);
	return 0;
}

static int icon_Construct(lua_State* L)
{
	WINSH_LUA(2)
	HICON h = NULL;
	if (T < 1) return 0;
	int cx = luaL_optinteger(L, 2, 0); if (cx < 0) cx = 0;
	int cy = luaL_optinteger(L, 3, 0); if (cy <= 0) cy = cx;
	if (lua_isstring(L, 1))
	{
		h = H->LoadIcon(CString(lua_tostring(L, 1)), cx, cy);
	}
	else if (lua_isuserdata(L, 1))
	{
		h = CopyIcon(*(HICON*)lua_touserdata(L, 1));
	}
	if (h != NULL)
	{
		shell_CreateIcon(L, h);
		if (luaC_gettid(L,0) != 1) return luaL_error(L, "Bad Class");
		lua_setmetatable(L, -2);									//|T|
		return 1;
	}
	return 0;
}

static void icon_Create(lua_State* L)
{
	static const struct luaL_Reg ml [] = {
		{"__gc", shell_IconGc},
		{NULL, NULL}
	};
	luaC_newclass(L, icon_Construct, ml);
}

#pragma endregion

#pragma region Lua scripting object "ShellObject".

CString shell_ObjectGetText(LPCITEMIDLIST pidl, SHGDNF flags)
{
	LPCITEMIDLIST pidlRelative = NULL;
	IShellFolder *psfParent = NULL;
    STRRET strDispName;
	CString rv("");
    HRESULT hr;

	hr = SHBindToParent(pidl, IID_IShellFolder, (void**)&psfParent, &pidlRelative);
	if (hr == S_OK)
	{
		hr = psfParent->GetDisplayNameOf(pidlRelative, flags, &strDispName);
		if (hr == S_OK)
		{
			hr = StrRetToBuf(&strDispName, pidl, rv.GetBuffer(MAX_PATH), MAX_PATH);
			rv.ReleaseBuffer();
		}
		psfParent->Release();
	}
	return rv;
}

static int shell_ObjectIcon(lua_State* L)
{
	static const char* sk [] = {"file", "large", "small", "shell", "open", "link", "selected",
		"overlays", NULL};
	WINSH_LUA(2)
	luaC_checkmethod(L);
	LPCITEMIDLIST so = (LPCITEMIDLIST)lua_touserdata(L, 1);
	UINT f = SHGFI_PIDL;
	switch (luaL_checkoption(L, 2, "file", sk))
	{
	case 1:  //"large"
		f |= SHGFI_ICON | SHGFI_LARGEICON;
		break;
	case 2:  //"small"
		f |= SHGFI_ICON | SHGFI_SMALLICON;
		break;
	case 3:  //"shell"
		f |= SHGFI_ICON | SHGFI_SHELLICONSIZE;
		break;
	case 4:  //"open"
		f |= SHGFI_ICON | SHGFI_OPENICON;
		break;
	case 5:  //"link"
		f |= SHGFI_ICON | SHGFI_LINKOVERLAY;
		break;
	case 6:  //"selected"
		f |= SHGFI_ICON | SHGFI_SELECTED;
		break;
	case 7:  //"overlays"
		f |= SHGFI_ICON | SHGFI_ADDOVERLAYS;
		break;
	default: //"file"
		f |= SHGFI_ICON;
		break;
	}
	SHFILEINFO psfi;
	if (SHGetFileInfo((LPCTSTR)so, 0, &psfi, sizeof(psfi), f))
	{
		if (psfi.hIcon == NULL) return 0;
		shell_CreateIcon(L, psfi.hIcon);
		luaC_newobject(L, 1, "shell.Icon");
		return 1;
	}
	else
	{
		return 0;
	}
}

//P1 (String-shgdfn, opt): The type of name to be returned.
//R1 (String): The display name.
static int shell_ObjectDisplayname(lua_State* L)
{
	static const char* sk [] = {"fullparsing", "compacted", "relativeparsing", "editing",
		"display", "normal", "filename", "type", "drive", "url", "typename", NULL};
	WINSH_LUA(2)
	CPathString nm("");
	CPathString ex("");
	int pp = 0;
	SHGDNF uFlags; 

	luaC_checkmethod(L);
	LPCITEMIDLIST so = (LPCITEMIDLIST)lua_touserdata(L, 1);

	switch (luaL_checkoption(L, 2, "fullparsing", sk))
	{
	case 1: //compacted
		uFlags = SHGDN_FORPARSING;
		pp = -1;
		break;
	case 2: //relativeparsing
		uFlags = SHGDN_INFOLDER | SHGDN_FORPARSING;
		break;
	case 3: //editing
		uFlags = SHGDN_INFOLDER | SHGDN_FOREDITING;
		break;
	case 4: //display
		uFlags = SHGDN_INFOLDER;
		break;
	case 5: //normal
		uFlags = SHGDN_NORMAL;
		break;
	case 6: //filename
		pp = 1;
		uFlags = SHGDN_INFOLDER | SHGDN_FORPARSING;
		break;
	case 7: //type
		pp = 2;
		uFlags = SHGDN_INFOLDER | SHGDN_FORPARSING;
		break;
	case 8: //drive
		pp = 3;
		uFlags = SHGDN_FORPARSING;
		break;
	case 9: //url
		pp = -2;
		uFlags = SHGDN_FORPARSING;
		break;
	case 10: //typename
		pp = -3;
		uFlags = SHGDN_FORPARSING;
		break;
	default: //fullparsing
		uFlags = SHGDN_FORPARSING;
		break;
	}
	int w = luaL_optinteger(L, 3, 40);

	nm = shell_ObjectGetText(so, uFlags);
	if (pp == -1)
	{	//compacted
		nm.PathCompactPath(w);
	}
	else if (pp == -2)
	{	//url
		nm.UrlCreateFromPath();
	}
	else if (pp == -3)
	{
		SHFILEINFO psfi;
		if (SHGetFileInfo(nm.LockBuffer(), 0, &psfi, sizeof(SHFILEINFO), SHGFI_TYPENAME) != 0)
		{
			nm = CString(psfi.szTypeName);
		}
		else
		{
			nm.Empty();
		}
	}
	else if (pp == 3)
	{	//drive
		int n = nm.PathGetDriveNumber();
		nm.Empty();
		if (n >= 0) nm = CString('A' + n) + CString(":");
	}
	else if (pp > 0)
	{	//filename, type
		ex = nm.PathFindExtension();
		if (ex.GetLength() > 0) ex = ex.Mid(1);
		nm.PathStripPath(); nm.PathRemoveExtension();
		if (pp == 2) nm = ex;
	}
	luaX_pushstring(L, nm);
	return 1;
}

SFGAOF shell_ObjectGetAttributes(LPCITEMIDLIST so)
{
	LPCITEMIDLIST pidlRelative = NULL;
	IShellFolder *psfParent = NULL;
	SFGAOF at;
	HRESULT hr;

	at = SFGAO_CANCOPY | SFGAO_CANDELETE | SFGAO_CANMOVE | SFGAO_CANRENAME | SFGAO_COMPRESSED | SFGAO_ENCRYPTED | SFGAO_FILESYSTEM
		| SFGAO_FOLDER | SFGAO_HIDDEN | SFGAO_LINK | SFGAO_READONLY | SFGAO_REMOVABLE | SFGAO_SHARE | SFGAO_STORAGE | SFGAO_STREAM;
	hr = SHBindToParent(so, IID_IShellFolder, (void**)&psfParent, &pidlRelative);
	if (hr == S_OK) hr = psfParent->GetAttributesOf(1, &pidlRelative, &at);
	if (hr != S_OK) at = 0;
	return at;
}

//R1 (Set object): Returns the set of the attributes applying to this object.
static int shell_ObjectAttributes(lua_State* L)
{
	WINSH_LUA(4)
	SFGAOF at;
	CString nm("");
	WIN32_FILE_ATTRIBUTE_DATA fad;
	DOUBLE dbl;
	int rv = 0;

	luaC_checkmethod(L);
	LPCITEMIDLIST so = (LPCITEMIDLIST)lua_touserdata(L, 1);
	at = shell_ObjectGetAttributes(so);

	if (at != 0)
	{
		luaC_newobject(L, 0, "class.Set");
		lua_pushboolean(L, ((at & SFGAO_CANCOPY) != 0)); lua_setfield(L,-2, "cancopy");
		lua_pushboolean(L, ((at & SFGAO_CANDELETE) != 0)); lua_setfield(L,-2, "candelete");
		lua_pushboolean(L, ((at & SFGAO_CANMOVE) != 0)); lua_setfield(L,-2, "canmove");
		lua_pushboolean(L, ((at & SFGAO_CANRENAME) != 0)); lua_setfield(L,-2, "canrename");
		lua_pushboolean(L, ((at & SFGAO_COMPRESSED) != 0)); lua_setfield(L,-2, "compressed");
		lua_pushboolean(L, ((at & SFGAO_ENCRYPTED) != 0)); lua_setfield(L,-2, "encrypted");
		lua_pushboolean(L, ((at & SFGAO_FILESYSTEM) != 0)); lua_setfield(L,-2, "filesystem");
		lua_pushboolean(L, ((at & SFGAO_FOLDER) != 0)); lua_setfield(L,-2, "folder");
		lua_pushboolean(L, ((at & SFGAO_HIDDEN) != 0)); lua_setfield(L,-2, "hidden");
		lua_pushboolean(L, ((at & SFGAO_LINK) != 0)); lua_setfield(L,-2, "link");
		lua_pushboolean(L, ((at & SFGAO_READONLY) != 0)); lua_setfield(L,-2, "readonly");
		lua_pushboolean(L, ((at & SFGAO_REMOVABLE) != 0)); lua_setfield(L,-2, "removable");
		lua_pushboolean(L, ((at & SFGAO_SHARE) != 0)); lua_setfield(L,-2, "share");
		lua_pushboolean(L, ((at & SFGAO_STORAGE) != 0)); lua_setfield(L,-2, "storage");
		lua_pushboolean(L, ((at & SFGAO_STREAM) != 0)); lua_setfield(L,-2, "stream");
		rv = 1;

		if ((at & SFGAO_FILESYSTEM) != 0)
		{
			nm = shell_ObjectGetText(so, SHGDN_FORPARSING);
			if (nm.GetLength() > 0)
			{
				if (GetFileAttributesEx(nm, GetFileExInfoStandard, &fad))
				{
					rv = 4;
					lua_pushboolean(L, ((fad.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE) != 0)); lua_setfield(L,-2, "archive");
					lua_pushboolean(L, ((fad.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) != 0)); lua_setfield(L,-2, "system");
					lua_pushboolean(L, ((fad.dwFileAttributes & FILE_ATTRIBUTE_OFFLINE) != 0)); lua_setfield(L,-2, "offline");
					lua_pushboolean(L, ((fad.dwFileAttributes & FILE_ATTRIBUTE_TEMPORARY) != 0)); lua_setfield(L,-2, "temporary");
					luaC_newobject(L, 0, "time.Time"); luaTM_TimeFromFiletime(L, fad.ftLastWriteTime);
					luaC_newobject(L, 0, "time.Time"); luaTM_TimeFromFiletime(L, fad.ftLastAccessTime);
					luaC_newobject(L, 0, "time.Time"); luaTM_TimeFromFiletime(L, fad.ftCreationTime);
					if ((fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
					{	
						ULARGE_INTEGER tt;
						rv++;
						tt.HighPart = fad.nFileSizeHigh; tt.LowPart = fad.nFileSizeLow;
						dbl = (DOUBLE)tt.QuadPart; dbl /= 1024;
						lua_pushnumber(L, (lua_Number)dbl);
					}
				}
			}
		}
		return rv;
	}
	return 0;
}

static void CreateShellObject(lua_State* L, LPCITEMIDLIST pidlBase,  int levels = 0, LPCITEMIDLIST pidlRelative = NULL)
{
	WINSH_LUA(2)
	LPCITEMIDLIST pidl = pidlBase;

	int nSizeB = 0;
	int nSectB = 0;
	int nSizeR = 0;
	pidl = pidlBase;
	while (pidl->mkid.cb > 0)
	{
		nSizeB += pidl->mkid.cb;
		nSectB++;
		pidl = (LPITEMIDLIST)(((LPBYTE)pidl) + pidl->mkid.cb);
	}
	if (levels != 0)
	{
		if (levels < 0)
		{
			nSectB += levels;
			if (nSectB < 0)
			{
				lua_pushnil(L);
				return;
			}
		}
		else if (levels < nSectB)
		{
			nSectB = levels;
		}
		pidl = pidlBase;
		nSizeB = 0;
		while (nSectB > 0)
		{
			nSizeB += pidl->mkid.cb;
			nSectB--;
			pidl = (LPITEMIDLIST)(((LPBYTE)pidl) + pidl->mkid.cb);
		}
	}
	if (pidlRelative != NULL)
	{
		pidl = pidlRelative;
		while (pidl->mkid.cb > 0)
		{
			nSizeR += pidl->mkid.cb;
			pidl = (LPITEMIDLIST)(((LPBYTE)pidl) + pidl->mkid.cb);
		}
	}

	LPITEMIDLIST ud = (LPITEMIDLIST)lua_newuserdata(L, nSizeB + nSizeR + sizeof(USHORT));
	if (nSizeB > 0) CopyMemory(ud, pidlBase, nSizeB);
	if (nSizeR > 0) CopyMemory((LPBYTE)ud + nSizeB, pidlRelative, nSizeR);
	*((USHORT *)((LPBYTE)ud + nSizeB + nSizeR)) = 0;
}

//P1 (Number, opt): The number of directories to pop. The default is 1. Ascending too far returns nil.
//R1 (ShellObject): ShellObject representing the parent, grandparent etc. of this object.
static int shell_ObjectParent(lua_State* L)
{
	WINSH_LUA(1)
	luaC_checkmethod(L);
	LPCITEMIDLIST so = (LPCITEMIDLIST)lua_touserdata(L, 1);
	int up = luaL_optinteger(L, 2, 1);
	CreateShellObject(L, so, -1 * up, NULL);
	if (lua_isuserdata(L, -1)) {
	    if (luaC_gettid(L, lua_upvalueindex(1)) != 1) return luaL_error(L, "Bad Class");
	    lua_setmetatable(L, -2);					//|T|
	}
	return 1;
}

static int so_enum(lua_State* L)
{
	WINSH_LUA(1)
    LPITEMIDLIST pidlItems = NULL;
    ULONG celtFetched;
	LPENUMIDLIST ppenum = NULL;
    HRESULT hr;

	ppenum = (LPENUMIDLIST)lua_touserdata(L, lua_upvalueindex(1));

	LPCITEMIDLIST so = (LPCITEMIDLIST)lua_touserdata(L, 1);

	hr = ppenum->Next(1, &pidlItems, &celtFetched);
	if ((hr == S_OK) && (celtFetched == 1))
	{
		CreateShellObject(L, so, 0, pidlItems);
		if (lua_isuserdata(L, -1)) {
			lua_pushvalue(L, lua_upvalueindex(2));						//|MT|T|
			lua_setmetatable(L, -2);									//|T|
		}
	}
	else
	{
		ppenum->Release();
		lua_pushnil(L);
	}
	return 1;
}

//Px (String-shcallf, opt): Any number of keys for files to be excluded from child enumeration.
//Rx (generic For enumerator values).
static int shell_ObjectChildren(lua_State* L)
{
    static const char* sk [] = {"nofolder", "nofile", "nohidden", NULL};
	WINSH_LUA(2)
	LPCITEMIDLIST pidlRelative = NULL;
	IShellFolder *psfParent = NULL;
	IShellFolder *psfChild = NULL;
	ULONG uAttr;
	SHCONTF grfFlags = SHCONTF_FOLDERS | SHCONTF_NONFOLDERS | SHCONTF_INCLUDEHIDDEN;
	LPENUMIDLIST ppenum = NULL;
	HRESULT hr;

	luaC_checkmethod(L);
	LPCITEMIDLIST so = (LPCITEMIDLIST)lua_touserdata(L, 1);

	UINT opt = luaX_checkoptions(L, sk, 2, lua_gettop(L));
	if (ISOPTION(opt,0)) grfFlags &= (~SHCONTF_FOLDERS);
	if (ISOPTION(opt,1)) grfFlags &= (~SHCONTF_NONFOLDERS);
	if (ISOPTION(opt,2)) grfFlags &= (~SHCONTF_INCLUDEHIDDEN);
	
	if (so->mkid.cb == 0)
	{
		// Special handling for the root object:
		hr = SHGetDesktopFolder(&psfChild);
		if (hr == S_OK) hr = psfChild->EnumObjects(NULL, grfFlags, &ppenum);
		if (psfChild) psfChild->Release();
	}
	else
	{
		hr = SHBindToParent(so, IID_IShellFolder, (void**)&psfParent, &pidlRelative);
		if (hr == S_OK) hr = psfParent->BindToObject(pidlRelative, NULL, IID_IShellFolder, (LPVOID*)&psfChild);
		uAttr = SFGAO_FOLDER;
		if (hr == S_OK) hr = psfParent->GetAttributesOf(1, (LPCITEMIDLIST *) &pidlRelative, &uAttr);
		if ((hr == S_OK) && (uAttr & SFGAO_FOLDER)) hr = psfChild->EnumObjects(NULL, grfFlags, &ppenum);
		if (psfChild) psfChild->Release();
		if (psfParent) psfParent->Release();
	}
	if ((hr != S_OK) || (ppenum == NULL)) return 0;
	ppenum->Reset();
	lua_pushlightuserdata(L, (void*)ppenum);
	if (luaC_gettid(L, lua_upvalueindex(1)) != 1) return luaL_error(L, "Bad Class");
	lua_pushcclosure(L, so_enum, 2);
	lua_pushvalue(L, 1);
	return 2;
}

// P1 (String, opt): Prompt text to display in the selection UI. (Default is "Choose a folder").
// Px (String Keys): Options 'files' and/or 'nocreatefolder'.
// R1 (ShellObject or nil): ShellObject representing the user's selection, or nil.
static int shell_ObjectBrowse(lua_State* L)
{
	WINSH_LUA(3)
    static const char* sk [] = {"files", "nocreatefolder", NULL};
    LPITEMIDLIST pidlSelected = NULL;
    BROWSEINFO bi = {0};
	UINT opt = 0;
	luaC_checkmethod(L);
	LPCITEMIDLIST so = (LPCITEMIDLIST)lua_touserdata(L, 1);
	CString title(luaL_optstring(L, 2, "Choose a folder"));
	if (lua_isstring(L, 3)) opt = luaX_checkoptions(L, sk, 3, lua_gettop(L));

    bi.hwndOwner = NULL;
    bi.pidlRoot = so;
    bi.pszDisplayName = NULL;
    bi.lpszTitle = title;
    bi.ulFlags = BIF_USENEWUI;
	if (ISOPTION(opt,0)) bi.ulFlags |= BIF_BROWSEINCLUDEFILES;
	if (ISOPTION(opt,1)) bi.ulFlags |= BIF_NONEWFOLDERBUTTON;
    bi.lpfn = NULL;
    bi.lParam = 0;

    pidlSelected = SHBrowseForFolder(&bi);

	if (pidlSelected != NULL)
	{
		CreateShellObject(L, pidlSelected, 0, NULL);
		if (lua_isuserdata(L, -1)) {
			if (luaC_gettid(L, lua_upvalueindex(1)) != 1) return luaL_error(L, "Bad Class");
			lua_setmetatable(L, -2);
		}
		return 1;
	}

    return 0;	
}

// Operates on a ShellObject which must represent a folder.
// P1: ShellObject/String key "folder". The template for the new object, the object to shortcut to, key for folder creation.
// P2: Optional String. If supplied this will be a suggested file name presented to the user for approval/edit.
// Px: Remaining optional parameters may be string keys from the "shnewf" set.
// R1: ShellObject or nil. If sucessful the ShellObject representing the newly created object.
static int shell_ObjectNew(lua_State* L)
{
	static const char* sk [] = {"shortcut", "noui", "notypelock", "overwrite", NULL};
	WINSH_LUA(3)
	CPathString sourcefilename;
	CPathString targetfilename;
	CPathString defname;
	CPathString fileext;
	CPathString description;
	INT type = 0;

	// Must be called against a ShellObject representing a folder:
	luaC_checkmethod(L, 1);
	LPCITEMIDLIST so = (LPCITEMIDLIST)lua_touserdata(L, 1);
	SFGAOF at = shell_ObjectGetAttributes(so);
	if ((at & SFGAO_FOLDER) == 0) return luaL_error(L, "New method may only be invoked on a folder");
	targetfilename = shell_ObjectGetText(so, SHGDN_FORPARSING);

	// Any parameters after first two are switches:
	UINT options = luaX_checkoptions(L, sk, 4, lua_gettop(L));
	if (ISOPTION(options,0)) type = 3;

	// 1st Parameter is the template or string key "folder":
	if (lua_type(L, 2) == LUA_TSTRING)
	{
		CString x(lua_tostring(L, 2));
		x.TrimLeft(); x.TrimRight(); x.MakeUpper();
		if (x != CString("FOLDER")) return luaL_error(L, "Template parameter to new method must be ShellObject or 'folder'");
		description = CString("folder");
		type = 1;
	}
	else
	{
		if (!luaC_isclass(L, 2)) return luaL_argerror(L, 2, "ShellObject expected");
		so = (LPCITEMIDLIST)lua_touserdata(L, 2);
		if (type == 0)
		{
			at = shell_ObjectGetAttributes(so);
			if ((at & SFGAO_FILESYSTEM) == 0) return luaL_error(L, "Template for new method must be a file or folder");
			if ((at & SFGAO_CANCOPY) == 0) return luaL_error(L, "Template for new method is not available to be copied");
			sourcefilename = shell_ObjectGetText(so, SHGDN_FORPARSING);
			fileext = sourcefilename.PathFindExtension();
			description = sourcefilename.PathFindExtension();
			description.PathRemoveExtension();
			type = 2;
		}
	}

	// 2nd Parameter is optional default file name:
	defname = CString(luaL_optstring(L, 3, ""));

	if (type == 3)
	{
		fileext = CString(".lnk");
		description = CString("shortcut");
	}
	if (!ISOPTION(options,1))
	{
		CString title = CString("Enter name for new ") + description;
		CNameDlg* dlg = new CNameDlg(title, defname);
		INT r = dlg->DoModal();
		delete dlg;
		if (r == 0) return 0;
	}
	defname.PathRemoveBlanks();

	if (type == 2)
	{
		if (defname.GetLength() < 1)
		{
			defname = sourcefilename.PathFindFileName();
			defname.PathRemoveExtension();
		}
		CString ee = defname.PathFindExtension(); ee.MakeUpper();
		CString ne = fileext; ne.MakeUpper();
		if (ISOPTION(options,2))
		{	// If type is not locked, only add the type extension as a default if one is not already present:
			if ((ee.GetLength() < 2) && (fileext.GetLength() > 0)) defname += fileext;
		}
		else
		{	// If the type is locked, add the type extension unless the correct extension is already present:
			if ((ee != ne) && (fileext.GetLength() > 0)) defname += fileext;
		}
		// Create a file or folder by copying a template
		targetfilename.PathAppend(defname);
		if (!ISOPTION(options,3)) targetfilename.MakeNameUnique();
		targetfilename.PathRemoveBlanks(); targetfilename.MakeMultistring();
		sourcefilename.PathRemoveBlanks(); sourcefilename.MakeMultistring();
		BOOL aborted = FALSE;
		SHFILEOPSTRUCT fos;
		fos.wFunc = FO_COPY;
		fos.fFlags = FOF_SIMPLEPROGRESS | FOF_NOCONFIRMATION | FOF_NOCONFIRMMKDIR | FOF_MULTIDESTFILES;
		if (!ISOPTION(options,3)) fos.fFlags |= FOF_RENAMEONCOLLISION;
		fos.pFrom = sourcefilename;
		fos.pTo = targetfilename;
		fos.fAnyOperationsAborted = aborted;
		fos.hNameMappings = NULL;
		fos.hwnd = 0;
		fos.lpszProgressTitle = H->GetAppName();
		int r = SHFileOperation(&fos);
		if ((r == 0) && (!aborted))
		{
			lua_settop(L, 0);
			luaX_pushstring(L, targetfilename);
			luaC_newobject(L, 1);
			return 1;
		}
	}
	else if (type == 1)
	{
		// Create an empty folder
		if (defname.GetLength() < 1) defname = CString("New folder");
		targetfilename.PathAppend(defname);
		if (!ISOPTION(options,3)) targetfilename.MakeNameUnique();
		if (CreateDirectory(targetfilename, NULL))
		{
			lua_settop(L, 0);
			luaX_pushstring(L, targetfilename);
			luaC_newobject(L, 1);
			return 1;
		}
	}
	else
	{
		// Create a shortcut
		HRESULT hres; 
		IShellLink* psl; 
 		// Get a pointer to the IShellLink interface. 
		hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&psl); 
		if (SUCCEEDED(hres)) 
		{ 
			IPersistFile* ppf; 
 			// Set the target and directory. 
			psl->SetIDList(so);
			sourcefilename.PathRemoveFileSpec();
			if (sourcefilename.GetLength() > 0) psl->SetWorkingDirectory(sourcefilename);
 
			// Query IShellLink for the IPersistFile interface for saving the shortcut. 
			hres = psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf); 
 			if (SUCCEEDED(hres)) 
			{ 
				CPathString xx; BOOL pf;
				UINT fl = SHGNLI_PIDL | SHGNLI_PREFIXNAME;
				if (ISOPTION(options,3)) fl |= SHGNLI_NOUNIQUE;
				BOOL r = SHGetNewLinkInfo((LPCWSTR)so, targetfilename, xx.GetBufferSetLength(MAX_PATH), &pf, fl); xx.ReleaseBuffer();
				if (defname.GetLength() > 0)
				{
					CPathString ex = xx.PathFindExtension();
					xx.PathRemoveFileSpec();
					xx.PathAppend(defname);
					xx.PathAddExtension(ex);
				}
				targetfilename = xx;
				hres = ppf->Save(targetfilename, TRUE); 
				ppf->Release();
			} 
			psl->Release();

			if (SUCCEEDED(hres))
			{
				luaX_pushstring(L, targetfilename);
				luaC_newobject(L, 1);
				return 1;
			}
		} 
	}
	return 0;
}

static int shell_ObjectFileOperation(lua_State* L)
{
	static const char* shopcd [] = {"copy", "save", "move", "delete", "erase", "rename", NULL};
	static const char* shopcf [] = {"newfolder", "delete", "progress", "none", NULL};
	WINSH_LUA(6)
	SHFILEOPSTRUCT fos;
	BOOL fAbort = FALSE;
	INT op;
	BOOL multipath = TRUE;
	BOOL multitarget = FALSE;
	BOOL needsource = TRUE;
	CPathString A("");
	CPathString B("");

	//Must be called against a ShellObject:
	luaC_checkmethod(L);
	A = shell_ObjectGetText((LPCITEMIDLIST)lua_touserdata(L, 1), SHGDN_FORPARSING);

	//First passed parameter must be present and must be an operation flag:
	fos.fFlags = FOF_SIMPLEPROGRESS;
	op = luaL_checkoption(L, 2, NULL, shopcd);
	switch(op)
	{
	case 1:	//save
		fos.wFunc = FO_COPY;
		fos.fFlags |= FOF_RENAMEONCOLLISION;
		break;
	case 2: //move
		fos.wFunc = FO_MOVE;
		break;
	case 3: //delete
		fos.wFunc = FO_DELETE;
		fos.fFlags |= FOF_ALLOWUNDO;
		needsource = FALSE;
		break;
	case 4: //erase
		fos.wFunc = FO_DELETE;
		needsource = FALSE;
		break;
	case 5: //rename
		fos.wFunc = FO_MOVE;
		needsource = FALSE;
		break;
	default: //copy
		fos.wFunc = FO_COPY;
		break;
	}

	//Remaining parameters are all optional, those present must be in the right order.
	//Prepare to sort them into the correct slots.
	lua_settop(L, 5);

	//Find the confirm flag, which may be absent, or in any of the remaining slots:
	int cf = 6;
	if (lua_isstring(L, 5)) cf = 5;
	if (lua_isstring(L, 4)) cf = 4;
	if (lua_isstring(L, 3)) if (CString(lua_tostring(L, 3)).GetAt(0) != _T('\\')) cf = 3;
	for (int i = cf + 1; (i < 5); i++) if (!lua_isnil(L, i)) return luaL_argerror(L, i, "extra parameters found after confirm key");
	switch((cf == 6)? 1 : luaL_checkoption(L, cf, NULL, shopcf))
	{
	case 0: //newfolder
		break;
	case 2: //progress
		fos.fFlags |= (FOF_NOCONFIRMATION | FOF_NOCONFIRMMKDIR);
		break;
	case 3: //none
		fos.fFlags |= (FOF_NOCONFIRMATION | FOF_NOCONFIRMMKDIR | FOF_SILENT);
		break;
	default: //delete
		fos.fFlags |= FOF_NOCONFIRMMKDIR;
		break;
	}
	if (cf < 6) {lua_pushnil(L); lua_replace(L, cf);}

	//If the spec parameter is missing, move any source ShellObject to correct slot:
	if (luaC_isclass(L, 3))
	{
		lua_pushvalue(L, 3);
		lua_replace(L, 4);
		lua_pushnil(L);
		lua_replace(L, 3);
	}
	//If the spec parameter is a string, replace it with a table having this string as sole entry:
	if (lua_isstring(L, 3))
	{
		lua_newtable(L);
		if (op == 5) //rename
		{
			int p = A.ReverseFind('\\');
			if (p < 3) return luaL_error(L, "Cannot rename this ShellObject");
			luaX_pushstring(L, A.Mid(p));
			lua_pushvalue(L, 3);
			A = A.Left(p);
			multipath = FALSE;
		}
		else
		{
			lua_pushvalue(L, 3);
			lua_pushboolean(L, TRUE);
		}
		lua_settable(L, -3);
		lua_replace(L, 3);
	}
	//If the spec parameter is still missing, make it a table with an empty string as sole entry:
	if (lua_isnil(L, 3))
	{
		lua_newtable(L);
		lua_replace(L, 3);
		lua_pushstring(L, "");
		lua_pushboolean(L, TRUE);
		lua_settable(L, 3);
		multipath = FALSE;
	}
	if (!lua_istable(L, 3)) return luaL_argerror(L, 3, "Parameter must be a Table or a string");

	//Verify we have the parameters required to complete the operation:
	int sc = 0; int tc = 0;
	lua_pushnil(L);
	while (lua_next(L, 3) != 0)
	{
		if (lua_toboolean(L, -2)) sc++;
		if (lua_isstring(L, -1)) tc++;
		lua_pop(L, 1);
	}
	multitarget = (tc > 0);
	if ((op == 5) && (sc != tc)) luaL_argerror(L, 3, "When renaming, new names must be provided for all files");
	if (luaC_isclass(L, 4))
		B = shell_ObjectGetText((LPCITEMIDLIST)lua_touserdata(L, 4), SHGDN_FORPARSING);
	else
		if (!lua_isnil(L, 4)) return luaL_argerror(L, 4, "Parameter must be a ShellObject");
	if (op < 3)
		if ((shell_ObjectGetAttributes((LPCITEMIDLIST)lua_touserdata(L, 1)) & SFGAO_FOLDER) == 0)
			return luaL_error(L, "Target ShellObject must be a folder for copy, save or move");
	if (needsource)
	{
		if (lua_isnil(L, 4)) return luaL_error(L, "A source must be specified for this operation");
		if (multipath)
			if ((shell_ObjectGetAttributes((LPCITEMIDLIST)lua_touserdata(L, 4)) & SFGAO_FOLDER) == 0)
				return luaL_error(L, "Source must be a folder for multi-file operations");
	}
	else if (!lua_isnil(L, 4))
		return luaL_error(L, "Source is not valid for this operation");
	if (multitarget && ((op == 3) || (op == 4)))
		return luaL_error(L, "Target paths are not valid for this operation");

	//Determine the size required for the source and target file spec buffers:
	int as = 0; int bs = 0;
	lua_pushnil(L);
	while (lua_next(L, 3) != 0)
	{
		if (lua_isstring(L, -2) && lua_toboolean(L, -1))
		{
			if (B.GetLength() > 0) bs += B.GetLength(); else bs += A.GetLength();
			bs += lua_rawlen(L, -2);
			bs++;
			if (lua_isstring(L, -1))
			{
				as += A.GetLength();
				as += lua_rawlen(L, -1);
				as++;
			}
			else if (multitarget)
			{
				as += A.GetLength();
				as += lua_rawlen(L, -2);
				as++;
			}
		}
		lua_pop(L, 1);
	}

	//Create the buffers:
	LPTSTR bufB = (LPTSTR)new BYTE[(bs * 2) + 4];
	LPTSTR bufA = NULL;
	if (as > 0) bufA = (LPTSTR)new BYTE[(as * 2) + 4];

	//Fill the buffers:
	CString X("");
	as = bs = 0;
	lua_pushnil(L);
	while (lua_next(L, 3) != 0)
	{
		if (lua_isstring(L, -2) && lua_toboolean(L, -1))
		{
			X = CString(lua_tostring(L, -2)); X.TrimLeft(); X.TrimRight();
			if (multipath) if ((X.GetAt(0) != _T('\\')) || X.GetLength() < 2) return luaL_argerror(L, 3, "invalid relative path");
			if (multitarget)
					if (X.FindOneOf(TEXT("*?")) >= 0) return luaL_argerror(L, 3, "wildcards not allowed when destination paths are used");
			if (B.GetLength() > 0) X = CString(B) + X; else X = CString(A) + X;
			for (int i = 0; (i < X.GetLength()); i++) bufB[bs++] = X[i];
			bufB[bs++] = 0;
			if (bufA != NULL)
			{
				if (lua_isstring(L, -1))
				{
					X = CString(lua_tostring(L, -1));
					if ((X.GetAt(0) != _T('\\')) || X.GetLength() < 2) return luaL_argerror(L, 3, "invalid relative path");
					if (X.FindOneOf(TEXT("*?")) >= 0) return luaL_argerror(L, 3, "wildcards not allowed in destination path");
					X = CString(A) + X;
					for (int i = 0; (i < X.GetLength()); i++) bufA[as++] = X[i];
					bufA[as++] = 0;
				}
				else if (multitarget)
				{
					X = CString(lua_tostring(L, -2)); X.TrimLeft(); X.TrimRight();
					X = CString(A) + X;
					for (int i = 0; (i < X.GetLength()); i++) bufA[as++] = X[i];
					bufA[as++] = 0;
				}
			}
		}
		lua_pop(L, 1);
	}
	bufB[bs++] = 0;

	//If the spec did not supply target paths, make a single one from the ShellObject path:
	if (bufA == NULL)
	{
		as = 0;
		bufA = (LPTSTR)new BYTE[(A.GetLength() * 2) + 4];
		for (int i = 0; (i < A.GetLength()); i++) bufA[as++] = A[i];
		bufA[as++] = 0;
	}
	bufA[as++] = 0;

	//Build the structure required by the OS and pass to the API:
	fos.pFrom = bufB;
	fos.pTo = bufA;
	if (multitarget) fos.fFlags |= FOF_MULTIDESTFILES;
	fos.fAnyOperationsAborted = fAbort;
	fos.hNameMappings = NULL;
	fos.hwnd = 0;
	fos.lpszProgressTitle = H->GetAppName();
	int r = SHFileOperation(&fos);

	//Return the memory used for the path buffers:
	if (bufA != NULL) delete bufA;
	if (bufB != NULL) delete bufB;

	//Determine the return parameter:
	if (fAbort)
		lua_pushboolean(L, FALSE);
	else if (r != 0)
		lua_pushinteger(L, r);
	else
		lua_pushboolean(L, TRUE);
	return 1;
}

static int shell_ObjectExecute(lua_State* L)
{
	static const char* seset [] = {"noinvoke", "nounicode", "noerrorui", "ddewait", "logusage",
		"nozonecheck", "newconsole", NULL};
	WINSH_LUA(2)
	luaC_checkmethod(L);
	CString verb = CString(luaL_optstring(L, 2, ""));
	verb.TrimLeft(); verb.TrimRight(); verb.MakeLower();
	ULONG fmask = SEE_MASK_INVOKEIDLIST | SEE_MASK_UNICODE;

	UINT opt = luaX_checkoptions(L, seset, 3, lua_gettop(L));
	if (ISOPTION(opt,0)) {fmask &= ~SEE_MASK_INVOKEIDLIST; fmask |= SEE_MASK_IDLIST;}
	if (ISOPTION(opt,1)) {fmask &= ~SEE_MASK_UNICODE;}
	if (ISOPTION(opt,2)) {fmask |= SEE_MASK_FLAG_NO_UI;}
	if (ISOPTION(opt,3)) {fmask |= SEE_MASK_FLAG_DDEWAIT;}
	if (ISOPTION(opt,4)) {fmask |= SEE_MASK_FLAG_LOG_USAGE;}
//	if (ISOPTION(opt,5)) {fmask |= SEE_MASK_NOZONECHECKS;}
	if (ISOPTION(opt,6)) {fmask |= SEE_MASK_NO_CONSOLE;}

	lua_settop(L, 1);

	SHELLEXECUTEINFO shex;
    memset(&shex, 0, sizeof(shex));
    shex.cbSize = sizeof(SHELLEXECUTEINFO);
    shex.fMask = fmask;
	if (verb.GetLength() > 0) shex.lpVerb = verb;
    shex.nShow = SW_NORMAL;
	shex.lpIDList = lua_touserdata(L, 1);

    lua_pushboolean(L, (::ShellExecuteEx(&shex)));
	return 1;
}

//P1 (Time, String-touchset, opt): If a Time, that time is set for the object.
//Px (String-touchset, opt): These (plus P1 if a string) define the timestamp(s) to be set.
int shell_ObjectTouch(lua_State* L)
{
	static const char* touchset [] = {"created", "modified", "accessed", NULL};
	WINSH_LUA(2)
	luaC_checkmethod(L);
	SYSTEMTIME st;
	FILETIME ft; FILETIME* ct = NULL; FILETIME* at = NULL; FILETIME* wt = NULL;
	CPathString nm(""); nm = shell_ObjectGetText((LPCITEMIDLIST)lua_touserdata(L, 1), SHGDN_FORPARSING);

	GetSystemTime(&st);
    SystemTimeToFileTime(&st, &ft);

	int pp = 1;

	if (luaTM_FiletimeFromTime(L, 2, &ft)) pp = 2;

	for (int i = lua_gettop(L); (i > pp); i--)
	{
		switch(luaL_checkoption(L, i, NULL, touchset))
		{
		case 0:
			ct = &ft;
			break;
		case 1:
			wt = &ft;
			break;
		case 2:
			at = &ft;
			break;
		}
	}
	lua_settop(L, 1);
	if ((ct == NULL) && (at == NULL)) wt = &ft;

	HANDLE h = CreateFile(nm, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if (h == INVALID_HANDLE_VALUE) {lua_pushboolean(L, FALSE); return 1;}

	SetFileTime(h, ct, at, wt);
	lua_pushboolean(L, TRUE);
	return 1;
}

static int shell_Construct(lua_State* L)
{
	WINSH_LUA(2)
    LPITEMIDLIST pidlSystem = NULL;
	IShellFolder* psfDesktop = NULL;
    HRESULT hr;

	if (luaC_isclass(L, 1, "shell.Drive"))
	{
		lua_getfield(L, 1, "_dl");
		CPathString d(lua_tostring(L, -1)); d.MakeUpper();
		d += CString(":\\");
		LPITEMIDLIST pidlDT = NULL;
		CComBSTR p(d);
		hr = SHGetDesktopFolder(&psfDesktop);
		psfDesktop->ParseDisplayName(NULL, NULL, p, NULL, &pidlSystem, NULL);
		psfDesktop->Release();
	}
	else if (lua_type(L, 1) == LUA_TSTRING)
	{
		LPITEMIDLIST pidlDT = NULL;
		CComBSTR p(luaL_optstring(L, 1, ""));
		hr = SHGetDesktopFolder(&psfDesktop);
		psfDesktop->ParseDisplayName(NULL, NULL, p, NULL, &pidlSystem, NULL);
		psfDesktop->Release();
	}
	else if (lua_type(L, 1) == LUA_TNUMBER)
	{
		int csidl = lua_tointeger(L, 1);
		hr = SHGetFolderLocation(NULL, csidl, NULL, NULL, &pidlSystem);
	}
	else
	{
		hr = SHGetFolderLocation(NULL, CSIDL_DESKTOP, NULL, NULL, &pidlSystem);
	}
	if ((hr == S_OK) && (pidlSystem != NULL))
	{
		int nSizeB = 0;
		LPCITEMIDLIST pidl = pidlSystem;
		while (pidl->mkid.cb > 0)
		{
			nSizeB += pidl->mkid.cb;
			pidl = (LPITEMIDLIST)(((LPBYTE)pidl) + pidl->mkid.cb);
		}
		LPITEMIDLIST ud = (LPITEMIDLIST)lua_newuserdata(L, nSizeB + sizeof(USHORT));
		CopyMemory(ud, pidlSystem, nSizeB);
		*((USHORT *)((LPBYTE)ud + nSizeB)) = 0;
		CoTaskMemFree(pidlSystem);
		if (luaC_gettid(L,0) != 1) return luaL_error(L, "Bad Class");
		lua_setmetatable(L, -2);									//|T|
		return 1;
	}
	return 0;
}

static void shell_Create(lua_State* L)
{
	static const struct luaL_Reg ml [] = {
		{"browse", shell_ObjectBrowse},
		{"new", shell_ObjectNew},
		{"name", shell_ObjectDisplayname},
		{"icon", shell_ObjectIcon},
		{"parent", shell_ObjectParent},
		{"attributes", shell_ObjectAttributes},
		{"fileoperation", shell_ObjectFileOperation},
		{"execute", shell_ObjectExecute},
		{"touch", shell_ObjectTouch},
		{"children", shell_ObjectChildren},
		{"__tostring", shell_ObjectDisplayname},
		{"__iter", shell_ObjectChildren},
		{NULL, NULL}
	};
	luaC_newclass(L, shell_Construct, ml);
}

#pragma endregion

#pragma region Drive Object.

static int shell_drvmsgf(lua_State* L)
{
	WINSH_LUA(2)
	LPARAM lp = lua_tointeger(L, 3);
	lua_settop(L, 0);
	BOOL ar = TRUE;
	if (lp > 99) {ar = FALSE; lp -= 100;}
	H->GetRegistryTable(shell_drvix);				//|drv-ix|
	char c = (char)lp + 'A';
	lua_pushlstring(L, &c, 1);
	lua_gettable(L, -2);
	if (lua_istable(L, -1))
	{
		lua_getfield(L, -1, "OnChange");
		if (luaX_iscallable(L, -1)) H->ExecChunk(0, CString("Drive:OnChange"));
	}
	return 0;
}

LRESULT shell_drvmsgproc(UINT id, CMsgTrap* t, CWindow* w, UINT& msg, WPARAM& wp, LPARAM& lp, BOOL& h)
{
	if ((wp == DBT_DEVICEARRIVAL) || (wp == DBT_DEVICEREMOVECOMPLETE))
	{
		DEV_BROADCAST_HDR* dbh = (DEV_BROADCAST_HDR*)lp;
		if (dbh->dbch_devicetype == DBT_DEVTYP_VOLUME)
		{
			DEV_BROADCAST_VOLUME* dbv = (DEV_BROADCAST_VOLUME*)dbh;
			ULONG um = dbv->dbcv_unitmask;
			char i;
			for (i = 0; i < 26; ++i)
			{
				if (um & 0x1) w->PostMessage(WM_COMMAND, MAKEWPARAM(id, msg), i);
				um = um >> 1;
			}
		}
	}
	return 0;
}

#include <Winioctl.h>
#define bufsz (sizeof(VOLUME_DISK_EXTENTS) + (10 * sizeof(DISK_EXTENT)))

//P1 (String, 'shppfn', opt): The type of name to be returned.
//P2 (Number, opt): The ordinal of the physical name to be returned when there is more than 1.
//R1 (String, Number): The display name. If there is more than one physical name, returns a number, the count of names.
static int LuaDriveName(lua_State* L)
{
    static const char* shppfn [] = {"letter", "root", "volspec", "label", "display", "volguid", "logical", "physical", "partition", "ntpart", NULL};
	WINSH_LUA(3)
	luaC_checkmethod(L);
	int sw = luaL_checkoption(L, 2, "letter", shppfn);
	int pc = luaL_optinteger(L, 3, 0);
	lua_getfield(L, 1, "_dl");
	CString d(lua_tostring(L, -1)); d.MakeUpper();
	CString r("");
	CString l("");
	int nr = 0;
	if (sw >= 3)
	{
		CString p("");
		p.Format(TEXT("%s:\\"), d);
		UINT type = GetDriveType(p);
		if (sw > 5)
		{
			if ((sw > 6) && ((type == DRIVE_FIXED) || (type == DRIVE_REMOVABLE)))
			{
				p.Format(TEXT("\\\\.\\%s:"), d);
				HANDLE h = CreateFile(p,READ_CONTROL,0,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL);
				if(INVALID_HANDLE_VALUE != h)
				{
					DWORD dwRet;
					STORAGE_DEVICE_NUMBER sd;
					if(DeviceIoControl(h, IOCTL_STORAGE_GET_DEVICE_NUMBER, NULL, 0, &sd, sizeof(STORAGE_DEVICE_NUMBER), &dwRet, NULL))
					{
						if (sw == 7)
							l.Format(_T("\\\\.\\PhysicalDrive%d"), sd.DeviceNumber);
						else if (sw == 8)
							l.Format(_T("\\\\.\\Harddisk%dPartition%d"), sd.DeviceNumber, sd.PartitionNumber);
						else
							l.Format(_T("\\Device\\Harddisk%d\\Partition%d"), sd.DeviceNumber, sd.PartitionNumber);
					}
					else
					{
						BYTE buf[bufsz];
						if(DeviceIoControl(h, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, NULL, 0, &buf, bufsz, &dwRet, NULL))
						{
							VOLUME_DISK_EXTENTS* px = (VOLUME_DISK_EXTENTS*)buf;
							if ((pc < 1) && (px->NumberOfDiskExtents > 1))
							{
								nr = px->NumberOfDiskExtents;
							}
							else
							{
								int x = pc; if (x < 1) x = 1; if (x > (int)px->NumberOfDiskExtents) x = (int)px->NumberOfDiskExtents;
								if (sw == 9)
									l.Format(_T("\\Device\\Harddisk%d"), px->Extents[--x].DiskNumber);
								else
									l.Format(_T("\\\\.\\PhysicalDrive%d"), px->Extents[--x].DiskNumber);
							}
						}
					}
					CloseHandle(h);
				}
			}
			if (l.GetLength() < 1)
			{
				p.Format(TEXT("%s:"), d);
				QueryDosDevice(p, l.GetBufferSetLength(MAX_PATH + 1), MAX_PATH);
				l.ReleaseBuffer();
				if (sw < 9)
				{
					if (l.GetLength() > 8)
					{
						CString x = l.Left(8);
						x.MakeUpper();
						if (x == CString("\\DEVICE\\")) l = l.Mid(8);
					}
					if (l.GetLength() > 0) l = CString("\\\\.\\") + l;
				}
			}
		}
		else if (sw == 5)
		{
			GetVolumeNameForVolumeMountPoint(p, l.GetBufferSetLength(51), 50);
			l.ReleaseBuffer();
		}
		else
		{
			GetVolumeInformation(p, l.GetBufferSetLength(20), 20, NULL, NULL,NULL, NULL, 0);
			l.ReleaseBuffer();
			if ((sw == 4) && (l.GetLength() < 1))
			{
				switch (type)
				{
				case DRIVE_FIXED:
					l = CString("Local Disk");
					break;
				case DRIVE_CDROM:
					l = CString("DVD Drive");
					break;
				case DRIVE_REMOVABLE:
					l = CString("Removable Disk");
				default:
					l = CString("Drive");
					break;
				}
			}
		}
	}
	if (nr > 0)
	{
		lua_pushinteger(L, nr);
	}
	else
	{
		switch (sw)
		{
		case 0: //letter
			r = d;
			break;
		case 1: //root
			r.Format(TEXT("%s:\\"), d);
			break;
		case 2: //volspec
			r.Format(TEXT("\\\\.\\%s:"), d);
			break;
		case 4: //display
			r.Format(TEXT("%s (%s:)"), l, d);
			break;
		default:
			r = l;
			break;
		}
		luaX_pushstring(L, r);
	}
	return 1;
}

// R1: String or Nil. If the drive exists, returns "fixed", "removable", "network", "optical" or "ramdisk".
// R2: String or Nil. If the drive has a volume in it, returns the filesystem name ("FAT" or "NTFS").
// R3: Number or Nil. If the drive has a volume in it, returns the volume serial number.
int LuaDriveType(lua_State* L)
{
	WINSH_LUA(4)
	luaC_checkmethod(L);
	lua_getfield(L, 1, "_dl");
	CString p(lua_tostring(L, -1));
	if (p.GetLength() <= 1) {p.MakeUpper(); p += _T(":\\");}
	switch (GetDriveType(p))
	{
	case DRIVE_REMOVABLE:
		lua_pushstring(L, "removable");
		break;
	case DRIVE_FIXED:
		lua_pushstring(L, "fixed");
		break;
	case DRIVE_REMOTE:
		lua_pushstring(L, "network");
		break;
	case DRIVE_CDROM:
		lua_pushstring(L, "optical");
		break;
	case DRIVE_RAMDISK:
		lua_pushstring(L, "ramdisk");
		break;
	default:
		return 0;
		break;
	}
	CString fs; DWORD sn; DWORD cl; DWORD fl;
	if (GetVolumeInformation(p, NULL, 0, &sn, &cl, &fl, fs.GetBuffer(20), 20))
	{
		fs.ReleaseBuffer();
		luaX_pushstring(L, fs);
		lua_pushinteger(L, sn);
		return 3;
	}
	return 1;
}

// R1: Number. The number of kilobytes of space available in a volume for writing by the current user.
// R2: Number. The number of kilobytes of total space in the current user's quota on a volume.
// R3: Number. The number of kilobytes of space available in a volume for all users.
int LuaDriveSpace(lua_State* L)
{
	WINSH_LUA(3)
	ULARGE_INTEGER FreeBytesAvailable;
	ULARGE_INTEGER TotalNumberOfBytes;
	ULARGE_INTEGER TotalNumberOfFreeBytes;

	luaC_checkmethod(L);
	lua_getfield(L, 1, "_dl");
	CString p(lua_tostring(L, -1));
	if (p.GetLength() <= 1) {p.MakeUpper(); p += _T(":\\");}

	if (GetVolumeInformation(p, NULL, 0, NULL, NULL, NULL, NULL, 0))
	{
		if (GetDiskFreeSpaceEx(p, &FreeBytesAvailable, &TotalNumberOfBytes, &TotalNumberOfFreeBytes))
		{
			lua_pushnumber(L, (double)(FreeBytesAvailable.QuadPart) / 1024.0);
			lua_pushnumber(L, (double)(TotalNumberOfBytes.QuadPart) / 1024.0);
			lua_pushnumber(L, (double)(TotalNumberOfFreeBytes.QuadPart) / 1024.0);
			return 3;
		}
	}
	return 0;
}

int LuaDriveLabel(lua_State* L)
{
	WINSH_LUA(2)
	luaC_checkmethod(L);
	CString nn(lua_tostring(L, 2));
	lua_getfield(L, 1, "_dl");
	CString p(lua_tostring(L, -1));
	if (p.GetLength() <= 1) {p.MakeUpper(); p += _T(":\\");}
	BOOL r = SetVolumeLabel(p.LockBuffer(), nn.LockBuffer());
	p.ReleaseBuffer(); nn.ReleaseBuffer();
	lua_pushboolean(L, r);
	return 1;
}

// R1: Boolean true if the recycle bin for this drive was sucessfully emptied.
int LuaDriveEmptyBin(lua_State* L)
{
	WINSH_LUA(2)
	luaC_checkmethod(L);
	lua_getfield(L, 1, "_dl");
	CString p(lua_tostring(L, -1));
	if (p.GetLength() <= 1) {p.MakeUpper(); p += _T(":\\");}
	lua_pushboolean(L, (SHEmptyRecycleBin(NULL, p, SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND) == S_OK));
	return 1;
}

int LuaDriveFormat(lua_State* L)
{
	WINSH_LUA(2)
	luaC_checkmethod(L);
	lua_getfield(L, 1, "_dl");
	CString p(lua_tostring(L, -1));
	p.MakeUpper(); TCHAR x = p.GetAt(0); UINT n = x - 'A';
	DWORD d = SHFormatDrive(H->GetHWND(), n, SHFMT_ID_DEFAULT, SHFMT_OPT_FULL);
	lua_pushboolean(L, (d == 0));
	return 1;
}

// http://support.microsoft.com/default.aspx?scid=kb;en-us;165721
// P1: Optional Boolean. If set true, allows dismount of drives marked as fixed (may be necessary for caddied HDs).
// P2: Optional Number. Milliseconds to wait for volume lock (default is 10000 - 10 seconds).
// R1: Number. 0 - Drive was ejected. -1 .. -2 - Drive was dismounted but not ejected. 1 .. 4 - Drive could not be dismounted.
int LuaDriveEjectVolume(lua_State* L)
{
	WINSH_LUA(2)
	int res = 0; int nTryCount; int Timeout; BOOL AllowFixed = FALSE;
    DWORD dwAccessFlags; DWORD dwBytesReturned;
	HANDLE hVolume = INVALID_HANDLE_VALUE;
    PREVENT_MEDIA_REMOVAL PMRBuffer;

	lua_settop(L, 3);
	luaC_checkmethod(L);
	lua_getfield(L, 1, "_dl");

	// Build the Root format and Volume format names:
	CString s(lua_tostring(L, -1));
	s.TrimLeft(); s.Left(1); s.MakeUpper();
	CString RootName(""); RootName.Format(TEXT("%s:\\"), s);
	CString VolumeName(""); VolumeName.Format(TEXT("\\\\.\\%s:"), s);

	// Interpret the optional parameters:
	if (lua_isnumber(L, 2))
	{
		Timeout = lua_tointeger(L, 2);
		AllowFixed = FALSE;
	}
	else
	{
		Timeout = luaL_optinteger(L, 3, 10000);
		AllowFixed = lua_toboolean(L, 2);
	}
	if (Timeout < 500) Timeout = 500;

	// Configure according to the drive type:
    switch(GetDriveType(RootName))
	{
    case DRIVE_REMOVABLE:
		dwAccessFlags = GENERIC_READ | GENERIC_WRITE;
        break;
    case DRIVE_CDROM:
		dwAccessFlags = GENERIC_READ;
        break;
	case DRIVE_FIXED:
		dwAccessFlags = GENERIC_READ | GENERIC_WRITE;
		res = 1;
		break;
    default:
		AllowFixed = FALSE;
        res = 1;
		break;
    }
	if (AllowFixed) res = 0;

	// Try to open the volume (this will fail if the drive does not exist or is empty):
	if (res == 0)
	{
		hVolume = CreateFile(VolumeName, dwAccessFlags, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
		if (hVolume == INVALID_HANDLE_VALUE) res = 2;
	}

	// Try to obtain an exclusive lock on the volume (this will fail if the system or an app is using it):
	if (res == 0)
	{
		res = 3;
		for (nTryCount = Timeout / 500; nTryCount > 0; nTryCount--)
		{
			if (DeviceIoControl(hVolume, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &dwBytesReturned, NULL)) {res = 0; break;}
			Sleep(500);
		}
	}

	// Try to dismount the locked volume (this may fail if the drive does not allow dismounting):
	if (res == 0)
	{
		if (!DeviceIoControl(hVolume, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &dwBytesReturned, NULL)) res = 4;
	}

	// Try to remove any lock on media removal (this may fail if the drive does not allow removal):
	if (res == 0)
	{
		PMRBuffer.PreventMediaRemoval = FALSE;
		if (!DeviceIoControl(hVolume, IOCTL_STORAGE_MEDIA_REMOVAL, &PMRBuffer, sizeof(PREVENT_MEDIA_REMOVAL), NULL, 0,
			&dwBytesReturned, NULL)) res = 5;
	}

	// Try to eject the media (this may fail if the drive does not support automatic ejection):
	if (res == 0)
	{
		if (!DeviceIoControl(hVolume, IOCTL_STORAGE_EJECT_MEDIA, NULL, 0, NULL, 0, &dwBytesReturned, NULL)) res = 6;
	}

	// If the volume was opened, close it to allow the drive to be used with a new volume:
	if (hVolume != INVALID_HANDLE_VALUE) CloseHandle(hVolume);

	// Return the status code:
	lua_pushinteger(L, (res >= 5)? (4 - res) : res);
    return 1;
}

static int LuaDriveTostring(lua_State* L)
{
	WINSH_LUA(1)
	luaC_checkmethod(L);
	lua_getfield(L, 1, "_dl");
	CString r1(lua_tostring(L, -1));
	lua_settop(L, 0);
	luaX_pushstring(L, r1);
	return 1;
}

static int drive_Construct(lua_State* L)
{
	WINSH_LUA(4)
	CPathString drv("C:");
	BOOL nw = TRUE;
	char p;
	char d[4];
	char v[11];

	int np = lua_gettop(L);
	if (np > 0)
	{
		if (luaC_isclass(L, 1, "shell.ShellObject"))
		{
			LPCITEMIDLIST so = (LPCITEMIDLIST)lua_touserdata(L, 1);
			CPathString nm = shell_ObjectGetText(so, SHGDN_FORPARSING);
			TCHAR d = nm.PathGetDriveNumber();
			if (d < 0) return 0;
			drv = CString('A' + d) + CString(":");
			if (np > 1) nw = lua_toboolean(L, 2); else nw = FALSE;
		}
		else if (!lua_isstring(L, 1))
		{
			nw = lua_toboolean(L, 1);
			if (!nw) luaL_argerror(L, 1, "Drive letter or volume label must be specified");
		}
		else
		{
			drv = CString(luaL_checkstring(L, 1));
			if (np > 1) nw = lua_toboolean(L, 2); else nw = FALSE;
		}
	}
	drv.TrimLeft(); drv.TrimRight(); drv.MakeUpper();
	if ((drv.GetLength() == 2) && ((char)drv.GetAt(1) == ':'))
	{
		p = (char)drv.GetAt(0);
		if ((p < 'A') || (p > 'Z')) luaL_error(L, "Invalid Drive Letter");
		drv = drv.Left(1);
	}
	else
	{
		d[1] = ':'; d[2] = '\\'; d[3] = 0;
		CString vn;
		for (p = 'A'; p <= 'Z'; p++)
		{
			d[0] = p;
			if (GetVolumeInformationA(d, v, 12, NULL, NULL, NULL, NULL, 0))
			{
				vn = CString(v); vn.MakeUpper();
				if (vn == drv) break;
			}
			if (p == 'Z') return 0;
		}
		drv = CString(p);
	}

	if (nw)
	{
		p -= 'A';
		DWORD pm = 0x1; pm = pm << p;
		DWORD dm = GetLogicalDrives();
		if (dm == 0) return 0;
		if ((dm & pm) != 0)
		{
			DWORD x = pm;
			for (int i = 0; i < (25 - p); i++)
			{
				if ((dm & x) == 0) break;
				x = x << 1;
			}
			if ((dm & x) != 0)
			{
				x = pm;
				for (int i = p; i >= 0; i--)
				{
					if ((dm & x) == 0) break;
					x = x >> 1;
				}
			}
			if ((dm & x) != 0) return 0;
			pm = x;
		}
		p = 'A'; for (int i = 0; i <= 25; i++) {if ((pm & 0x1) > 0) break; pm = pm >> 1; p++;}
		drv = CString(p);
	}
	H->GetRegistryTable(shell_drvix);				//|drv-ix|
	luaX_pushstring(L, drv);						//|DL|drv-ix|
	lua_gettable(L, -2);							//|T|drv-ix|
	if (lua_type(L, -1) != LUA_TTABLE)
	{
		lua_pop(L, 1);								//|drv-ix|
		lua_newtable(L);							//|T|drv-ix|
		luaX_pushstring(L, drv);					//|DL|T|drv-ix|
		lua_setfield(L, -2, "_dl");					//|T|drv-ix|
		luaX_pushstring(L, drv);					//|DL|T|drv-ix|
		lua_pushvalue(L, -2);						//|T|DL|T|drv-ix|
		lua_settable(L, -4);						//|T|drv-ix|
	}
	lua_remove(L, -2);								//|T|
	if (luaC_gettid(L,0) != 1) return luaL_error(L, "Bad Class");
	lua_setmetatable(L, -2);						//|T|
	return 1;
}

static void drive_Create(lua_State* L)
{
	static const struct luaL_Reg ml [] = {
		{"name", LuaDriveName},
		{"type", LuaDriveType},
		{"space", LuaDriveSpace},
		{"emptybin", LuaDriveEmptyBin},
		{"label", LuaDriveLabel},
		{"ejectvolume", LuaDriveEjectVolume},
		{"format", LuaDriveFormat},
		{"__tostring", LuaDriveTostring},
		{NULL, NULL}
	};
	luaC_newclass(L, drive_Construct, ml);
}

#pragma endregion

#pragma region Shell Library Functions

// R1:	Table.
int shell_Platform(lua_State* L)
{
	WINSH_LUA(2)
	OSVERSIONINFOEX vi;  ZeroMemory(&vi, sizeof(OSVERSIONINFOEX)); vi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	if (!GetVersionEx((OSVERSIONINFO*)&vi))
		{vi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO); GetVersionEx((OSVERSIONINFO*)&vi);};
	SYSTEM_INFO si; ZeroMemory(&si, sizeof(SYSTEM_INFO));
	if (GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")), "GetNativeSystemInfo") == NULL)
		GetSystemInfo(&si);
	else
		GetNativeSystemInfo(&si);
	BOOL wow64 = false;
	if (GetProcAddress(GetModuleHandle(TEXT("kernel32")),"IsWow64Process") != NULL)
		IsWow64Process(GetCurrentProcess(), &wow64);

	lua_settop(L, 0);
	luaC_newobject(L, 0, "class.Table");
	lua_Number x;
	x = (lua_Number)vi.dwMinorVersion; x = (x >= 10)? x / 100.0 : x / 10.0;
	lua_pushnumber(L, (lua_Number)vi.dwMajorVersion + x);
	lua_setfield(L, 1, "OsVersion");
	lua_pushinteger(L, vi.dwBuildNumber);
	lua_setfield(L, 1, "OsBuildNumber");
	if ((vi.wServicePackMajor > 0) || (vi.wServicePackMinor > 0))
	{
		x = (lua_Number)vi.wServicePackMinor; x = (x >= 10)? x / 100.0 : x / 10.0;
	}
	else
	{
		x = 0;
	}
	lua_pushnumber(L, (lua_Number)vi.wServicePackMajor + x);
	lua_setfield(L, 1, "ServicePack");
	lua_pushinteger(L, (wow64)? 64 : 32);
	lua_setfield(L, 1, "WordWidth");
	lua_pushinteger(L, si.dwNumberOfProcessors);
	lua_setfield(L, 1, "Processors");
	CString s(vi.szCSDVersion);
	luaX_pushstring(L, s);
	lua_setfield(L, 1, "ServicePackId");

	luaC_newobject(L, 0, "class.Set");
	lua_pushboolean(L, si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64); lua_setfield(L, 2, "X64");
	lua_pushboolean(L, si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_IA64); lua_setfield(L, 2, "IA64");
	lua_pushboolean(L, si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL); lua_setfield(L, 2, "X86");
	lua_pushboolean(L, vi.wProductType == VER_NT_DOMAIN_CONTROLLER); lua_setfield(L, 2, "DomainController");
	lua_pushboolean(L, vi.wProductType == VER_NT_SERVER); lua_setfield(L, 2, "Server");
	lua_pushboolean(L, vi.wProductType == VER_NT_WORKSTATION); lua_setfield(L, 2, "Workstation");
	lua_pushboolean(L, vi.wSuiteMask & VER_SUITE_BACKOFFICE); lua_setfield(L, 2, "BackOffice");
	lua_pushboolean(L, vi.wSuiteMask & VER_SUITE_BLADE); lua_setfield(L, 2, "BladeServer");
	lua_pushboolean(L, vi.wSuiteMask & VER_SUITE_COMPUTE_SERVER); lua_setfield(L, 2, "ComputeServer");
	lua_pushboolean(L, vi.wSuiteMask & VER_SUITE_DATACENTER); lua_setfield(L, 2, "DataCenter");
	lua_pushboolean(L, vi.wSuiteMask & VER_SUITE_ENTERPRISE); lua_setfield(L, 2, "Enterprise");
	lua_pushboolean(L, vi.wSuiteMask & VER_SUITE_EMBEDDEDNT); lua_setfield(L, 2, "Embedded");
	lua_pushboolean(L, vi.wSuiteMask & VER_SUITE_PERSONAL); lua_setfield(L, 2, "Personal");
	lua_pushboolean(L, vi.wSuiteMask & VER_SUITE_STORAGE_SERVER); lua_setfield(L, 2, "StorageServer");
	lua_pushboolean(L, vi.wSuiteMask & 0x00008000); lua_setfield(L, 2, "HomeServer");
	lua_pushboolean(L, GetSystemMetrics(SM_TABLETPC)); lua_setfield(L, 2, "Tablet");
	lua_pushboolean(L, GetSystemMetrics(SM_MEDIACENTER)); lua_setfield(L, 2, "MediaCenter");
	lua_pushboolean(L, IsProcessorFeaturePresent(PF_MMX_INSTRUCTIONS_AVAILABLE)); lua_setfield(L, 2, "MMX");
	lua_pushboolean(L, IsProcessorFeaturePresent(PF_XMMI_INSTRUCTIONS_AVAILABLE)); lua_setfield(L, 2, "SSE");
	lua_pushboolean(L, IsProcessorFeaturePresent(PF_XMMI64_INSTRUCTIONS_AVAILABLE)); lua_setfield(L, 2, "SSE2");
	lua_pushboolean(L, IsProcessorFeaturePresent(13)); lua_setfield(L, 2, "SSE3");

	lua_setfield(L, 1, "FeatureSet");
	return 1;
}

// Returns the power status of the computer.
// R1: Boolean. True if the computer is on AC power. (Exceptionally may return nil if power status cannot be determined)
// R2: Number or Nil. Battery charge in percent 0..100. Nil if computer has no battery.
// R3: Number or Nil. Estimated endurance of computer in seconds on a full battery charge.
int shell_PowerStatus(lua_State* L)
{
	WINSH_LUA(3)
	SYSTEM_POWER_STATUS st;
	if (GetSystemPowerStatus(&st))
	{
		if (st.ACLineStatus > 1) return 0;
		lua_pushboolean(L, (st.ACLineStatus == 1));
		if (((st.BatteryFlag & 0xF0) > 0) || (st.BatteryLifePercent > 100)) return 1;
		lua_pushinteger(L, st.BatteryLifePercent);
		if (st.BatteryFullLifeTime < 0) return 2;
		lua_pushinteger(L, st.BatteryFullLifeTime);
		return 3;
	}
	return 0;
}

// Allows the shudown options to be invoked programatically.
// P1: String, 'shutdownset'. The operation to be performed - defaults to "shutdown".
// P2: Boolean (optional). If specified and boolean true, the shutdown is forced even if an application refuses.
// R1: Boolean. True if the operation suceeds.
int shell_Shutdown(lua_State* L)
{
    static const char* shutdownset [] = {"shutdown", "switchuser", "logoff", "lock", "restart", "sleep", "hibernate", NULL};
	WINSH_LUA(1)
	HANDLE hToken; 
	TOKEN_PRIVILEGES tkp;

	int ty = luaL_checkoption(L, 1, "shutdown", shutdownset);
	BOOL fr = lua_toboolean(L, 2);

	if (ty == 1)
	{
		// http://blog.dotsmart.net/2008/01/17/shortcut-to-switch-user-in-windows-vista/
		if (!WTSDisconnectSession(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, TRUE))
			{lua_pushboolean(L, FALSE); return 1;}
		lua_pushboolean(L, TRUE);
		return 1;
	}
	if (ty == 3)
	{
		if (LockWorkStation())
			lua_pushboolean(L, TRUE);
		else
			lua_pushboolean(L, FALSE);
		return 1;
	}

	// Get a token for this process. 
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
		{lua_pushboolean(L, FALSE); return 1;} 
 
	// Get the LUID for the shutdown privilege. 
	LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid); 
	tkp.PrivilegeCount = 1;  // one privilege to set    
	tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED; 
 
	// Get the shutdown privilege for this process. 
	AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, (PTOKEN_PRIVILEGES)NULL, 0); 
	if (GetLastError() != ERROR_SUCCESS) {lua_pushboolean(L, FALSE); return 1;} 
 
	UINT fl = EWX_FORCEIFHUNG;
	if (fr) fl = EWX_FORCE;

	if (ty == 0)
	{
		// Close applications (forcing any that are hung), shut down the system and turn the power off. 
		if (!ExitWindowsEx(EWX_POWEROFF | fl, SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_MINOR_OTHER | SHTDN_REASON_FLAG_PLANNED))
			{lua_pushboolean(L, FALSE); return 1;} 
	}
	else if (ty == 2)
	{
		// Close applications (forcing any that are hung) and log the user off. 
		if (!ExitWindowsEx(EWX_LOGOFF | fl, SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_MINOR_OTHER | SHTDN_REASON_FLAG_PLANNED))
			{lua_pushboolean(L, FALSE); return 1;} 
	}
	else if (ty == 4)
	{
		// Close applications (forcing any that are hung), shut down the system and restart it.
		if (!ExitWindowsEx(EWX_REBOOT | fl, SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_MINOR_OTHER | SHTDN_REASON_FLAG_PLANNED))
			{lua_pushboolean(L, FALSE); return 1;} 
	}
	else if (ty == 5)
	{
		if (!SetSystemPowerState(TRUE, fr)) {lua_pushboolean(L, FALSE); return 1;} 
	}

	else if (ty == 6)
	{
		if (!SetSystemPowerState(FALSE, fr)) {lua_pushboolean(L, FALSE); return 1;} 
	}

	lua_pushboolean(L, TRUE);
	return 1;
}

int shell_ScanDevices(lua_State* L)
{
	WINSH_LUA(1)
    DEVINST devRoot;
	int r = CM_Locate_DevNode_Ex(&devRoot,NULL,CM_LOCATE_DEVNODE_NORMAL,NULL);
	if (r == CR_SUCCESS) r = CM_Reenumerate_DevNode_Ex(devRoot, 0, NULL); 
	lua_pushinteger(L, r);
 	return 1;
}

#pragma endregion

// =================================================================================================

LUASHLIB_API int LUASHLIB_NGEN(luaopen_)(lua_State* L)
{
	static const luaL_Reg fl [] = {
		{"powerstatus", shell_PowerStatus},
		{"platform", shell_Platform},
		{"shutdown", shell_Shutdown},
		{"scandevices", shell_ScanDevices},
		{NULL, NULL}
	};

	WINSH_LUA(4)
	H->Require(CString("class"));
	H->Require(CString("time"));

	//Index of Drive objects keyed by drive letter.
	shell_drvix = H->GetRegistryTable(shell_drvix, 0, "v");
	lua_pop(L, 1);

	int m = H->CaptureMessage(WM_DEVICECHANGE, shell_drvmsgproc);
	lua_pushcfunction(L, shell_drvmsgf);
	H->SetLuaMessageHandler(m);
	lua_pop(L, 1);

	lua_createtable(L, 0, sizeof(fl)/sizeof((fl)[0]) - 1);
	luaL_setfuncs(L, fl, 0);
	icon_Create(L);
	lua_setfield(L, -2, "Icon");
	shell_Create(L);
	lua_setfield(L, -2, "ShellObject");
	drive_Create(L);
	lua_setfield(L, -2, "Drive");

	// Load and execute the Lua part of the library:
	H->LoadScriptResource(CString("LibShell"));
	lua_pushvalue(L, -2);
	H->ExecChunk(1, CString("LibShell-LuaPart"));

	return 1;
}
