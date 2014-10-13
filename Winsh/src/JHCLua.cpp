#include "stdafx.h"
#include "JHCLua.h"

#pragma region "Lua Callback Functions"

void* luaX_alloc(void* ud, void* ptr, size_t osize, size_t nsize)
{
	((ILuaLib*)ud)->m_rm(3, nsize - osize);
	if (nsize == 0)
	{
		free(ptr);
		return NULL;
	}
	else
	{
		return realloc(ptr, nsize);
	}
}

int luaX_panic(lua_State* L)
{
	CString e(lua_tostring(L, -1));
	luaX_host<ILuaLib>(L,0)->WriteError(e);
	ATLTRACE(atlTraceException, 0, _T("Lua panic: '%s'\n"), e );
	AtlThrow(E_FAIL);
	return 0;
}

int luaX_print(lua_State* L)
{
	lua_checkstack(L, 1);
	CString ms(""); CString tx("");
	int T = lua_gettop(L);
	for (int i = 1; (i <= T); i++)
	{
		if (ms.GetLength() > 0) ms += CString(" ");
		tx = CString(luaL_tolstring(L, i, NULL));
		lua_pop(L, 1);
		tx.Replace(TEXT("\n"), TEXT("\r\n"));
		ms += tx;
	}
	luaX_host<ILuaLib>(L,0)->WriteMessage(ms);
	return 0;
}

#pragma endregion

#pragma region "Resource Loader Functions"

int luaX_resldr(lua_State *L)
{
	CString type(luaX_host<ILuaLib>(L,0)->m_rt);
	CString name(luaL_checkstring(L, 1));
	if (name.Left(1) != CString(TEXT("."))) name = CString(TEXT(".")) + name;
	int r = luaX_loadresource(L, name);
	if ((r == 0) || (r == LUA_ERRFILE)) return 1;
	CString err(luaL_optstring(L, -1, ""));
	luaX_host<ILuaLib>(L,0)->WriteError(err);
	return 1;
}

BOOL CALLBACK luaX_resenm(HMODULE hm, LPCTSTR ty, LPTSTR nm, LONG_PTR lp)
{
	lua_State* L = (lua_State*)lp;
	lua_checkstack(L, 2);
	HRSRC hRes = FindResource(hm, nm, luaX_host<ILuaLib>(L,0)->m_rt);
	HGLOBAL hResL;
	char* buf;
	size_t size = 0;
	hResL = LoadResource(hm, hRes);
	buf = (char*)LockResource(hResL);
	size = (size_t)SizeofResource(hm, hRes);
	if (size > 80) size = 80;
	if ((size > 4) && (buf[0] == '-') && (buf[1] == '-'))
	{
		for (size_t i = 2; (i < size); i++) if (buf[i] == '\n') size = i;
	}
	else
	{
		size = 0;
	}
	luaX_pushstring(L, CString(nm));
	if (size > 0)
		lua_pushlstring(L, buf + 2, size - 2);
	else
		lua_pushstring(L, "[Unlabeled]");
	lua_settable(L, -3);
	return TRUE;
}

#pragma endregion

#pragma region "Lua C API additions"

LUALIB_API int luaX_iscallable(lua_State* L, int inx)
{
	if (lua_isfunction(L, inx)) return TRUE;
	lua_checkstack(L, 1);
	if (luaL_getmetafield(L, inx, "__call")) {lua_pop(L, 1); return TRUE;}
	return FALSE;
}

LUALIB_API void luaX_pushstring(lua_State* L, LPCTSTR s)
{
	CString ss(s);
	int sz = ss.GetLength();
	int bs = WideCharToMultiByte(CP_THREAD_ACP, 0, ss, sz + 1, NULL, 0, NULL, NULL);
	char* b = new char[bs];
	int rs = WideCharToMultiByte(CP_THREAD_ACP, 0, ss, sz + 1, b, bs, NULL, NULL);
	lua_pushlstring(L, b, rs - 1);
	delete b;
}

LUALIB_API UINT luaX_checkoptions(lua_State* L, const char* const lst[], int first, int last)
{
	if (first > last) return 0;
	UINT m = 0;
	for (int i = first; (i <= last); i++) m += 1 << luaL_checkoption(L, i, "", lst);
	return m;
}

LUALIB_API int luaX_loadstring(lua_State* L, LPCTSTR code, LPCTSTR name)
{
	CString x1(code);
	int sz = x1.GetLength();
	int bs = WideCharToMultiByte(CP_THREAD_ACP, 0, x1, sz + 1, NULL, 0, NULL, NULL);
	char* b1 = new char[bs];
	int rs = WideCharToMultiByte(CP_THREAD_ACP, 0, x1, sz + 1, b1, bs, NULL, NULL);

	CString x2(name);
	x2 = CString("=String-") + x2;
	sz = x2.GetLength();
	bs = WideCharToMultiByte(CP_THREAD_ACP, 0, x2, sz + 1, NULL, 0, NULL, NULL);
	char* b2 = new char[bs];
	WideCharToMultiByte(CP_THREAD_ACP, 0, x2, sz + 1, b2, bs, NULL, NULL);

	int r = luaL_loadbuffer(L, b1, rs - 1, b2);

	delete b1;
	delete b2;
	return r;
}

typedef struct LuaX_LoadF
{
	FILE *f;
	char buff[LUAL_BUFFERSIZE];
} LuaX_LoadF;

static const char* LuaX_FileReader(lua_State *L, void *ud, size_t *size)
{
	LuaX_LoadF *lf = (LuaX_LoadF *)ud;
	if (feof(lf->f)) return NULL;
	*size = fread(lf->buff, 1, sizeof(lf->buff), lf->f);
	return (*size > 0) ? lf->buff : NULL;
}

LUALIB_API int luaX_loadfile (lua_State *L, LPCTSTR fn, LPCTSTR fp)
{
	lua_checkstack(L, 2);
	CString cn(fn);
	CString ep(fp);
	int p = cn.Find(ep);
	if (p >= 0) cn = CString("\\") + cn.Mid(p + ep.GetLength());
	cn = CString('@') + cn;
	int sz = cn.GetLength();
	int bs = WideCharToMultiByte(CP_THREAD_ACP, 0, cn, sz + 1, NULL, 0, NULL, NULL);
	char* chunkname = new char[bs];
	int rs = WideCharToMultiByte(CP_THREAD_ACP, 0, cn, sz + 1, chunkname, bs, NULL, NULL);
	LuaX_LoadF lf;
	int r = 0;
	if (_wfopen_s(&lf.f, fn, TEXT("r")) != 0) r = LUA_ERRFILE;
	if (r != 0)
		luaX_pushstring(L, CString("Cannot open script file: ") + fn);
	else
		r = lua_load(L, LuaX_FileReader, &lf, chunkname, NULL);
	delete chunkname;
	if (lf.f != NULL) fclose(lf.f);
	return r;
}

static const char* LuaX_ResReader(lua_State* L, void* data, size_t* size)
{
	HMODULE hM = _Module.GetResourceInstance();
	HRSRC* phRes = (HRSRC*)data;
	if (*phRes == NULL) return NULL;
	HGLOBAL hResL;
	char* buf;
	*size = 0;
	hResL = LoadResource(hM, *phRes);
	if (hResL == NULL) return NULL;
	buf = (char*)LockResource(hResL);
	*size = (size_t)SizeofResource(hM, *phRes);
	*phRes = NULL;
	return buf;
}

LUALIB_API int luaX_loadresource(lua_State* L, LPCTSTR resname)
{
	if (luaX_host<ILuaLib>(L,0)->m_rt.GetLength() < 1) return 0;
	lua_checkstack(L, 2);
	HRSRC hRes;
	HMODULE hM = _Module.GetResourceInstance();
	CString name(resname);
	hRes = FindResource(hM, name, luaX_host<ILuaLib>(L,0)->m_rt);
	if (hRes == NULL)
	{
		luaX_pushstring(L, CString("\tno resource '") + name + CString("'"));
		return LUA_ERRFILE;
	}
	name = CString("=Resource-") + name;
	size_t sz = name.GetLength() + 1;
	char* nm = new char[sz];
	wcstombs_s(NULL, nm, sz, name, sz);
	int r = lua_load(L, LuaX_ResReader, (void*)&hRes, nm, NULL);
	delete nm;
	return r;
}

#pragma endregion

#pragma region "Debug Functions"

#ifdef _DEBUG
// Debugging functions

const char* luaC_pushdstring(lua_State* L, int idx, int lem)
{
	size_t len;
	lua_checkstack(L, 4);
	int t = lua_type(L, idx);
	lua_pushvalue(L, idx);						//|XX|
	lua_pushstring(L, "[");
	lua_pushstring(L, lua_typename(L, t));
	lua_pushstring(L, "]");
	lua_concat(L, 3);							//|TS|XX|
	lua_insert(L, -2);							//|XX|TS|
	const char* s = lua_tolstring(L, -1, &len);
	if (s == NULL) {
		if (t == LUA_TBOOLEAN) {
			if (lua_toboolean(L, -1)) lua_pushstring(L, "<true>"); else lua_pushstring(L, "<false>"); //|VS|XX|TS|
			lua_remove(L, -2);					//|VS|TS|
		} else {
			lua_pushstring(L, "");				//|""|XX|TS|
			lua_remove(L, -2);					//|""|TS|
		}
	} else if (t == LUA_TSTRING) {				//|VS|TS|
		lua_pushstring(L, "\"");				//|"|VS|TS|
		lua_pushstring(L, "\"");				//|"|"|VS|TS|
		lua_insert(L, -3);						//|"|VS|"|TS|
		lua_concat(L, 3);						//|VS|TS|
	}
	lua_concat(L, 2);							//|RS|
	s = lua_tolstring(L, -1, &len);
	if ((int)len >= lem) {
		lua_pushlstring(L, s, lem-1);			//|RS|RS|
		lua_remove(L, -2);						//|RS|
		lua_pushstring(L, ">");					//|>|RS|
		lua_concat(L, 2);						//|RS|
	}
	return lua_tostring(L, -1);
}

LUALIB_API void luaX_showstack(lua_State* L, const char* label/* = 0*/)
{
	lua_checkstack(L, 4);
	int T = lua_gettop(L);
	ATLTRACE2("%s:: Stack top: %i.\n", label, T);
	// 0 - nil; 1 - boolean; 2 - light UD; 3 - number; 4 - string; 5 - table;
	// 6 - function; 7 - UD; 8 - thread;
	for (int i = T; i > 0; i--)
	{
		ATLTRACE2("++ Stack index: %i|%i, %s\n", i, -(T - i + 1), luaC_pushdstring(L, i, 60));
		lua_pop(L, 1);
	}
}

LUALIB_API void luaX_showtable(lua_State* L, int ix, const char* label/* = 0*/)
{
	lua_checkstack(L, 6);
	if (lua_istable(L, ix))
	{
		ATLTRACE2("%s:: Contents of table at index %i.\n", label, ix);
		int aix = ix; if (aix < 0) aix = lua_gettop(L) + aix + 1;
		lua_pushnil(L);
		while (lua_next(L, aix) != 0)
		{
			const char* ke = luaC_pushdstring(L, -2, 20);
			const char* vl = luaC_pushdstring(L, -2, 40);
			ATLTRACE2("++ Key: %s; Value: %s.\n", ke, vl);
			lua_pop(L, 3);
		}
	}
	else
	{
		ATLTRACE2("%s:: Value at index %i is not a table.\n", label, ix);
	}
}
#endif

#pragma endregion
