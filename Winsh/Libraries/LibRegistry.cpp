#include "stdafx.h"
#define LUAREGLIB_BUILDING
#include "LibClass.h"
#include "LibRegistry.h"
#include "..\LuaLibIF.h"
#include "..\resource.h"

#ifdef LUAREGLIB_DLL
HMODULE hM;
LUASHLIB_API BOOL APIENTRY DllMain( HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved )
	hM = hModule;
    return TRUE;
}
#endif

#pragma region RegistryKey Object.

static void reg_CreateKey(lua_State* L, HKEY k)
{
	HKEY* p = (HKEY*)lua_newuserdata(L, sizeof(HKEY));
	*p = k;
}

static int reg_KeyOpen(lua_State* L)
{
	WINSH_LUA(2)
	luaC_checkmethod(L);
	HKEY h = *(HKEY*)lua_touserdata(L, 1);
	CString k = luaL_checkstring(L, 2);
	HKEY hh = NULL;
	if (RegOpenKeyEx(h, k, 0, KEY_ALL_ACCESS, &hh) == ERROR_SUCCESS)
	{
		reg_CreateKey(L, hh);
		if (luaC_gettid(L, lua_upvalueindex(1)) != 1) return luaL_error(L, "Bad Class");
		lua_setmetatable(L, -2);									//|T|
		return 1;
	}
	else
	{
		return 0;
	}
}

int reg_KeyCreate(lua_State* L)
{
	WINSH_LUA(2)
	luaC_checkmethod(L);
	HKEY h = *(HKEY*)lua_touserdata(L, 1);
	CString k = luaL_checkstring(L, 2);
	HKEY hh = NULL;
	DWORD disp = 0;
	if (RegCreateKeyEx(h, k, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hh, &disp) == ERROR_SUCCESS)
	{
		if (disp == REG_CREATED_NEW_KEY)
		{
			reg_CreateKey(L, hh);
			if (luaC_gettid(L, lua_upvalueindex(1)) != 1) return luaL_error(L, "Bad Class");
			lua_setmetatable(L, -2);									//|T|
			return 1;
		}
		else
		{
			RegCloseKey(hh);
			return 0;
		}
	}
	else
	{
		return 0;
	}
}

static int reg_KeyGet(lua_State* L)
{
	WINSH_LUA(2)
	luaC_checkmethod(L);
	HKEY h = *(HKEY*)lua_touserdata(L, 1);
	CString n = luaL_optstring(L, 2, "");
	DWORD type;
	LPBYTE data = NULL;
	DWORD size = 0;
	int r = 0;
	if (RegQueryValueEx(h, n, NULL, &type, data, &size) == ERROR_SUCCESS)
	{
		data = (LPBYTE)new byte[size];
		if (RegQueryValueEx(h, n, NULL, &type, data, &size) == ERROR_SUCCESS)
		{
			r = 2;
			switch (type)
			{
			case REG_BINARY: {
				lua_pushlstring(L, (LPCSTR)data, size);
				lua_pushstring(L, "BINARY");
				break;}
			case REG_DWORD: {
				lua_pushinteger(L, *(DWORD32*)data);
				lua_pushstring(L, "DWORD");
				break;}
			case REG_DWORD_BIG_ENDIAN: {
				BYTE x;
				x = *data; *data = *(data+3); *(data+3) = x;
				x = *(data+1); *(data+1) = *(data+2); *(data+2) = x;
				lua_pushinteger(L, *(DWORD32*)data);
				lua_pushstring(L, "DWORDBE");
				break;}
			case REG_QWORD: {
				double d = (double)*(DWORD64*)data;
				lua_pushnumber(L, d);
				lua_pushstring(L, "QWORD");
				break;}
			case REG_SZ: {
				CString s((LPCTSTR)data, size/sizeof(TCHAR));
				luaX_pushstring(L, s);
				lua_pushstring(L, "SZ");
				break;}
			case REG_EXPAND_SZ: {
				CString s((LPCTSTR)data, size/sizeof(TCHAR));
				luaX_pushstring(L, s);
				lua_pushstring(L, "ESZ");
				break;}
			case REG_LINK: {
				CString s((LPCTSTR)data, size/sizeof(TCHAR));
				luaX_pushstring(L, s);
				lua_pushstring(L, "LINK");
				break;}
			case REG_MULTI_SZ: {
				//TODO: Parse multi-string to a List of strings.
				lua_pushinteger(L, 0);
				lua_pushstring(L, "MSZ");
				break;}
			case REG_NONE: {
				lua_pushinteger(L, size);
				lua_pushstring(L, "NONE");
				break;}
			}
		}
		delete data;
		return r;
	}
	return 0;
}

static const char* rgtypes [] = {"BINARY", "DWORD", "DWORDBE", "QWORD", "SZ", "ESZ", "LINK", "MSZ", "NONE", NULL};
static int reg_KeySet(lua_State* L)
{
	WINSH_LUA(2)
	luaC_checkmethod(L);
	HKEY h = *(HKEY*)lua_touserdata(L, 1);
	CString n = luaL_checkstring(L, 2);
	DWORD type = REG_NONE;
	int t = luaL_checkoption(L, 4, "NONE", rgtypes);
	if (t > 7)
	{
		switch (lua_type(L, 3))
		{
		case LUA_TNUMBER:
			t = 1;
			break;
		case LUA_TSTRING:
			t = 4;
			break;
		}
	}
	LPBYTE data = NULL;
	DWORD size = 0;
	switch (t)
	{
	case 0: {
		size = lua_rawlen(L, 3);
		const byte* s = (byte*)lua_tostring(L, 3);
		data = new byte[size];
		for (DWORD i = 0; (i < size); i++) *(data+i) = *(s+i);
		type = REG_BINARY;
		break; }
	case 1: {
		data = new byte[4];
		size = 4;
		*(DWORD32*)data = (DWORD32)lua_tointeger(L, 3);
		type = REG_DWORD;
		break; }
	case 2: {
		data = new byte[4];
		size = 4;
		*(DWORD32*)data = (DWORD32)lua_tointeger(L, 3);
		BYTE x;
		x = *data; *data = *(data+3); *(data+3) = x;
		x = *(data+1); *(data+1) = *(data+2); *(data+2) = x;
		type = REG_DWORD_BIG_ENDIAN;
		break; }
	case 3: {
		data = new byte[8];
		size = 8;
		double d = lua_tonumber(L, 3);
		*(DWORD64*)data = (DWORD64)d;
		type = REG_QWORD;
		break; }
	case 4: {
		CString s(lua_tostring(L, 3));
		size = (s.GetLength() * sizeof(TCHAR)) + 1;
		data = new byte[size]; data[size-1] = 0;
		for (int i = 0; (i < s.GetLength()); i++) *((TCHAR*)data + i) = s[i];
		type = REG_SZ;
		break; }
	case 5: {
		CString s(lua_tostring(L, 3));
		size = (s.GetLength() * sizeof(TCHAR)) + 1;
		data = new byte[size]; data[size-1] = 0;
		for (int i = 0; (i < s.GetLength()); i++) *((TCHAR*)data + i) = s[i];
		type = REG_EXPAND_SZ;
		break; }
	case 6: {
		CString s(lua_tostring(L, 3));
		size = (s.GetLength() * sizeof(TCHAR)) + 1;
		data = new byte[size]; data[size-1] = 0;
		for (int i = 0; (i < s.GetLength()); i++) *((TCHAR*)data + i) = s[i];
		type = REG_LINK;
		break; }
	case 7:
		//TODO: Parse multi-string from a List of strings.
		size = 0;
		type = REG_MULTI_SZ;
		break;
	default:
		size = 0;
		type = REG_NONE;
		break;
	}
	LONG r = RegSetValueEx(h, n, 0, type, data, size);
	if (data != NULL) delete data;
	if (r != ERROR_SUCCESS) return luaL_error(L, "RegistryKey:Set - Unable to set registry value.");
	return 0;
}

static int reg_keyenum(lua_State* L)
{
	HKEY key = *(HKEY*)lua_touserdata(L, lua_upvalueindex(1));
	DWORD index = lua_tointeger(L, lua_upvalueindex(2));
	DWORD size = lua_tointeger(L, lua_upvalueindex(3)) + 2;
	lua_pushinteger(L, --index);
	lua_replace(L, lua_upvalueindex(2));
	if (index < 0) return 0;
	CString name;
	LONG r = RegEnumKeyEx(key, index, name.GetBuffer(size), &size, NULL, NULL, NULL, NULL);
	name.ReleaseBuffer(size);
	if (r == ERROR_SUCCESS)
	{
		luaX_pushstring(L, name);
		return 1;
	}
	else
	{
		return 0;
	}
}

static int reg_KeySubKeys(lua_State* L)
{
	WINSH_LUA(4)
	luaC_checkmethod(L);
	lua_settop(L, 1);
	HKEY h = *(HKEY*)lua_touserdata(L, 1);
	DWORD num = 0;
	DWORD siz = 0;
	RegQueryInfoKey(h, NULL, NULL, NULL, &num, &siz, NULL, NULL, NULL, NULL, NULL, NULL);
	lua_pushinteger(L, num);
	lua_pushinteger(L, siz);
	lua_pushcclosure(L, reg_keyenum, 3);
	return 1;
}

static int reg_valenum(lua_State* L)
{
	HKEY key = *(HKEY*)lua_touserdata(L, lua_upvalueindex(1));
	DWORD index = lua_tointeger(L, lua_upvalueindex(2));
	DWORD size = lua_tointeger(L, lua_upvalueindex(3)) + 2;
	lua_pushinteger(L, --index);
	lua_replace(L, lua_upvalueindex(2));
	if (index < 0) return 0;
	CString name;
	LONG r = RegEnumValue(key, index, name.GetBuffer(size), &size, NULL, NULL, NULL, NULL);
	name.ReleaseBuffer(size);
	if (r == ERROR_SUCCESS)
	{
		luaX_pushstring(L, name);
		return 1;
	}
	else
	{
		return 0;
	}
}

static int reg_KeyValues(lua_State* L)
{
	WINSH_LUA(4)
	luaC_checkmethod(L);
	lua_settop(L, 1);
	HKEY h = *(HKEY*)lua_touserdata(L, 1);
	DWORD num = 0;
	DWORD siz = 0;
	RegQueryInfoKey(h, NULL, NULL, NULL, NULL,NULL, NULL, &num, &siz, NULL, NULL, NULL);
	lua_pushinteger(L, num);
	lua_pushinteger(L, siz);
	lua_pushcclosure(L, reg_valenum, 3);
	return 1;
}

int reg_KeyDeleteTree(lua_State* L)
{
	WINSH_LUA(1)
	luaC_checkmethod(L);
	HKEY h = *(HKEY*)lua_touserdata(L, 1);
//  This is VISTA or above only:
//	RegDeleteTree(h, NULL);
//  So try this instead:
	SHDeleteKey(h, _T(""));
	return 0;
}

int reg_KeyDeleteValue(lua_State* L)
{
	WINSH_LUA(1)
	luaC_checkmethod(L);
	HKEY h = *(HKEY*)lua_touserdata(L, 1);
	CString n(luaL_checkstring(L, 2));
	RegDeleteValue(h, n);
	return 0;
}

static int reg_KeyGc(lua_State* L)
{
	WINSH_LUA(2)
	luaC_checkmethod(L);
	HKEY h = *(HKEY*)lua_touserdata(L, 1);
	if (h != NULL) RegCloseKey(h);
	return 0;
}

static int reg_Construct(lua_State* L)
{
	WINSH_LUA(2)
	lua_Integer key = lua_tointeger(L, 1);
	lua_settop(L, 0);
	HKEY h = (( HKEY ) (ULONG_PTR)((LONG)0x80000000 + (LONG)key));
	if (RegOpenKeyEx(h, NULL, 0, KEY_ALL_ACCESS, &h) == ERROR_SUCCESS) {
		reg_CreateKey(L, h);
		if (luaC_gettid(L,0) != 1) return luaL_error(L, "Bad Class");
		lua_setmetatable(L, -2);									//|T|
	} else {
		lua_pushnil(L);
	}
	return 1;
}

void reg_Create(lua_State* L)
{
	static const struct luaL_Reg ml [] = {
		{"open", reg_KeyOpen},
		{"create", reg_KeyCreate},
		{"get", reg_KeyGet},
		{"set", reg_KeySet},
		{"subkeys", reg_KeySubKeys},
		{"values", reg_KeyValues},
		{"deletetree", reg_KeyDeleteTree},
		{"deletevalue", reg_KeyDeleteValue},
		{"__gc", reg_KeyGc},
		{NULL, NULL}
	};
	luaC_newclass(L, reg_Construct, ml);
}

#pragma endregion

// =================================================================================================

LUAREGLIB_API int LUAREGLIB_NGEN(luaopen_)(lua_State* L)
{
	WINSH_LUA(1)
	H->Require(CString("class"));
	reg_Create(L);
	// Load and execute the Lua part of the library:
	H->LoadScriptResource(CString("LibRegistry"));
	lua_pushvalue(L, -2);
	H->ExecChunk(1, CString("LibRegistry-LuaPart"));
	return 1;
}
