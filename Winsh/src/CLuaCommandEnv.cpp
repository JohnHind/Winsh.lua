#include "stdafx.h"
#include "resource.h"
#include "CLuaCommandEnv.h"

CString getversioninfo(LPCTSTR exe, LPCTSTR kt)
{
	CString appfile(exe);
	DWORD res;
	VS_FIXEDFILEINFO FixedFileInfo;
	VS_FIXEDFILEINFO* pFixedInfo = &FixedFileInfo;
	UINT infosize;
	CString ret;
	BYTE* buf = NULL;
	bool ok; int LangID; int CodePage; CString k;
	DWORD size = ::GetFileVersionInfoSize(appfile, &res);
	if (size > 1)
	{
		buf = new BYTE[size];
		if (buf != NULL)
		{
			if (::GetFileVersionInfo(appfile, res, size, buf))
			{
				if (::VerQueryValue(buf, (LPTSTR)_T("\\"), (void**)&pFixedInfo, &infosize))
				{
					if (pFixedInfo != NULL)
					{
						ok = true;
						if (!::VerQueryValue(buf, _T("\\VarFileInfo\\Translation"), (void**)&pFixedInfo, &infosize)) ok = false;
						if (pFixedInfo == NULL) ok = false;
						if (ok && (infosize == 4))
						{
							DWORD lang; memcpy(&lang, pFixedInfo, 4);
							LangID = lang & 0xffff;
							CodePage = (lang & 0xffff0000) >> 16;
						}
						else
						{
							LangID = 0x0409;
							CodePage = 0x04b0;
						}
					}
				}
			}
		}
	}
	ret = CString("");
	if (ok)
	{
		k.Format(TEXT("\\StringFileInfo\\%04x%04x\\"), LangID, CodePage);
		k = k + kt;
		if (::VerQueryValue(buf, k, (void**)&pFixedInfo, &infosize))
		{
			if (pFixedInfo != NULL) ret = CString((LPTSTR)pFixedInfo, infosize);
		}
	}
	return ret;
}

#pragma pack( push )
#pragma pack( 2 )
typedef struct
{
	BYTE	bWidth;               // Width of the image
	BYTE	bHeight;              // Height of the image (times 2)
	BYTE	bColorCount;          // Number of colors in image (0 if >=8bpp)
	BYTE	bReserved;            // Reserved
	WORD	wPlanes;              // Color Planes
	WORD	wBitCount;            // Bits per pixel
	DWORD	dwBytesInRes;         // how many bytes in this resource?
	WORD	nID;                  // the ID
} MEMICONDIRENTRY, *LPMEMICONDIRENTRY;
typedef struct 
{
	WORD			idReserved;   // Reserved
	WORD			idType;       // resource type (1 for icons)
	WORD			idCount;      // how many images?
	MEMICONDIRENTRY	idEntries[1]; // the entries for each image
} MEMICONDIR, *LPMEMICONDIR;
#pragma pack( pop )

BOOL CALLBACK _ResIconsEnum(HMODULE hm, LPCTSTR ty, LPTSTR nm, LONG_PTR lp)
{
	if (IS_INTRESOURCE(nm)) return TRUE;
	lua_State* L = (lua_State*)lp;
	WINSH_LUA(2)
	HRSRC hRes = FindResource(hm, nm, RT_GROUP_ICON);
	HGLOBAL hResL;
	hResL = LoadResource(hm, hRes);
	LPMEMICONDIR dir = (LPMEMICONDIR)LockResource(hResL);
	CString d = CString("");
	CString x = CString("");
	for (int i = 0; (i < dir->idCount); i++)
	{
		x.Format(CString("%dx%dx%d;"), dir->idEntries[i].bWidth, dir->idEntries[i].bHeight, dir->idEntries[i].wBitCount);
		d = d + x;
	}
	luaX_pushstring(L, CString(nm));
	luaX_pushstring(L, d);
	lua_settable(L, -3);
	return TRUE;
}

// Return inventory information
static int luaX_GetInventory(lua_State* L)
{
    static const char* keys [] = {"libraries", "scripts", "files", "icons", NULL};
	WINSH_LUA(4)
	int k = luaL_checkoption(L, 1, "libraries", keys);
	switch (k) {
	case 3:
		lua_newtable(L);
		EnumResourceNames(_Module.GetResourceInstance(), RT_GROUP_ICON, _ResIconsEnum, (LONG_PTR)L);
		break;
	default:
		H->GetInventory(k);
		break;
	}
	return 1;
}

// Write a string, always to the GUI debug console.
static int luaX_DebugPrint(lua_State* L)
{
	WINSH_LUA(1);
	CString s(luaL_checkstring(L, 1));
	H->WriteMessage(s,TRUE,TRUE);
	return 0;
}

// Expand a help string substituting some configuration variables.
static int luaX_WriteHelp(lua_State* L)
{
	WINSH_LUA(1);
	CString appfile(H->GetExePath());
    appfile += H->GetExeName() + CString(".EXE");
	CString s(TEXT("-- "));
	s += CString(luaL_checkstring(L, 1));
	lua_settop(L, 0);
	s.Replace(CString("{exename}"), H->GetExeName());
	s.Replace(CString("{startname}"), H->GetInitName());
	s.Replace(CString("{luaext}"), H->GetLuaExt());
	s.Replace(CString("{subsystem}"), CString(SF_DESC));
	s.Replace(CString("{configuration}"), CString(CF_DESC));
	s.Replace(CString("{luaversion}"), CString(LUA_RELEASE));
	s.Replace(CString("{luaauthors}"), CString(LUA_AUTHORS));
	s.Replace(CString("{luacopyright}"), CString(LUA_COPYRIGHT));
	s.Replace(CString("{comments}"), getversioninfo(appfile, CString("Comments")));
	s.Replace(CString("{companyname}"), getversioninfo(appfile, CString("CompanyName")));
	s.Replace(CString("{filedescription}"), getversioninfo(appfile, CString("FileDescription")));
	s.Replace(CString("{fileversion}"), getversioninfo(appfile, CString("FileVersion")));
	s.Replace(CString("{internalname}"), getversioninfo(appfile, CString("InternalName")));
	s.Replace(CString("{legalcopyright}"), getversioninfo(appfile, CString("LegalCopyright")));
	s.Replace(CString("{legaltrademarks}"), getversioninfo(appfile, CString("LegalTrademarks")));
	s.Replace(CString("{originalfilename}"), getversioninfo(appfile, CString("OriginalFilename")));
	s.Replace(CString("{privatebuild}"), getversioninfo(appfile, CString("PrivateBuild")));
	s.Replace(CString("{productname}"), getversioninfo(appfile, CString("ProductName")));
	s.Replace(CString("{productversion}"), getversioninfo(appfile, CString("ProductVersion")));
	s.Replace(CString("{specialbuild}"), getversioninfo(appfile, CString("SpecialBuild")));
	s.Replace(TEXT("\n"), TEXT("\r\n-- "));
	luaX_pushstring(L, s);
	return luaX_DebugPrint(L);
}

CLuaCommandEnv::CLuaCommandEnv(lua_State* LL)
{
	L = LL;
	WINSH_LUA(3)
	// Create the command interpreter environment and load it from "init-cmd":
	lua_newtable(L);								//|T
	lua_pushcfunction(L, luaX_GetInventory);		//|F|T
	lua_setfield(L, -2, "getinventory");			//|T
	lua_pushcfunction(L, luaX_DebugPrint);			//|F|T
	lua_setfield(L, -2, "dprint");					//|T
	lua_pushcfunction(L, luaX_WriteHelp);			//|F|T
	lua_setfield(L, -2, "writehelp");				//|T
	lua_newtable(L);								//|M|T
	lua_pushglobaltable(L);							//|G|M|T
	lua_setfield(L, -2, "__index");					//|M|T
	lua_setmetatable(L, -2);						//|T
	lua_pushvalue(L, -1);							//|T|T
	m_refCC = luaL_ref(L, LUA_REGISTRYINDEX);		//|T
	if (luaX_loadresource(L, CString("init-cmd")) == 0)
	{												//|F|T
		lua_pushvalue(L, -2);						//|T|F|T
		lua_setupvalue(L, -2, 1);					//|F|T
		lua_remove(L, -2);							//|F
		H->ExecChunk();
	}
	else
	{
		m_lasterror = CString(luaL_optstring(L, -1, "Error compiling 'init-cmd'"));
		lua_pop(L, 1);
	}
}

void CLuaCommandEnv::SetCmdEnvironment(void)
{												//|F
	lua_checkstack(L, 3);
	lua_pushglobaltable(L);						//|G|F
	lua_rawgeti(L, LUA_REGISTRYINDEX, m_refCC);	//|C|G|F
	lua_remove(L, -2);							//|C|F
	lua_setupvalue(L, -2, 1);					//|F
}

BOOL CLuaCommandEnv::CheckCmdField(LPCTSTR nm, int type /*=-1*/)
{
	BOOL r = FALSE;
	lua_checkstack(L, 2);
	lua_pushglobaltable(L);							//|G
	lua_rawgeti(L, LUA_REGISTRYINDEX, m_refCC);		//|C|G
	lua_remove(L, -2);								//|C
	luaX_pushstring(L, nm);							//|N|C
	lua_gettable(L, -2);							//|E|C
	if (type < 0)
		r = (lua_type(L, -1) != LUA_TNIL);
	else
		r = (lua_type(L, -1) == type);
	lua_pop(L, 2);
	return r;
}

CString CLuaCommandEnv::GetCmdVar(LPCTSTR nm, LPCTSTR def)
{													//|
	lua_checkstack(L, 2);
	lua_rawgeti(L, LUA_REGISTRYINDEX, m_refCC);		//|C|
	luaX_pushstring(L, nm);							//|N|C|
	lua_gettable(L, -2);							//|E|C|
	CString eq(def);
	if (lua_isstring(L, -1)) eq = CString(lua_tostring(L, -1));
	lua_pop(L, 2);									//|
	return eq;
}

int CLuaCommandEnv::GetCmdVar(LPCTSTR nm)
{
	lua_checkstack(L, 2);
	lua_rawgeti(L, LUA_REGISTRYINDEX, m_refCC);		//|C|
	luaX_pushstring(L, nm);							//|N|C|
	lua_gettable(L, -2);							//|E|C|
	lua_remove(L, -2);								//|E|
	return 1;
}

void CLuaCommandEnv::SetCmdVar(LPCTSTR nm)
{													//|V|
	lua_checkstack(L, 3);
	lua_rawgeti(L, LUA_REGISTRYINDEX, m_refCC);		//|C|V|
	luaX_pushstring(L, nm);							//|N|C|V|
	lua_pushvalue(L, -3);							//|V|N|C|V|
	lua_settable(L, -3);							//|C|V|
	lua_pop(L, 2);									//|
}

CString CLuaCommandEnv::FindLuaLib(LPCTSTR name)
{
	CString n(name);
	lua_checkstack(L, 4);
	lua_getglobal(L, "package");
	if (lua_istable(L, -1)) {
		lua_getfield(L, -1, "searchpath");
		luaX_pushstring(L, name);
		lua_getfield(L, -3, "path");
		if ((lua_isfunction(L, -3) && (lua_isstring(L, -1)))) {
			lua_call(L, 2, 1);
			if (lua_isstring(L, -1)) {
				n = CString(lua_tostring(L, -1));
			} else {
				n = CString("");
			}
		}
		lua_pop(L, 3);
	}
	return n;
}

