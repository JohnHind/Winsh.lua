#include "stdafx.h"
#define LUACLLIB_BUILDING
#include "LibClass.h"
#include "..\LuaLibIF.h"


#pragma region "C Interface and Library functions"

#define LUAJH_TIDNAME "_TID"
#define LUAJH_TIDNAME_L (4)

CString pop_name(const char* name, int pos) {
	CString n = CString(name);
	int p1 = -1; int p2;
	for (int i = 0; (i < pos); i++) {
		p1 = n.Find('.', p1+1);
		if (p1 < 0) return CString("");
	}
	p2 = n.Find('.', ++p1);
	if (p2 < 0) p2 = n.GetLength();
	n = n.Mid(p1,p2-p1);
	return n;
};

int get_class(lua_State* L, const char* name) {
	lua_getglobal(L, "package");
	if (lua_istable(L, -1)) {
		lua_getfield(L, -1, "loaded");
		lua_remove(L, -2);
		int c = 0;
		while (lua_istable(L, -1)) {
			luaX_pushstring(L, pop_name(name, c++));
			lua_gettable(L, -2);
			lua_remove(L, -2);
		}
	}
	if (!lua_isfunction(L, -1)) return luaL_error(L,"Class not found: %s",name);
	return 1;
};

int matchtidmarker(const char* tn) {
	int i; const char* tnn = LUAJH_TIDNAME;
	for (i = 0; (i < LUAJH_TIDNAME_L); i++) if (tn[i] != tnn[i]) return 0;
	return (tn[LUAJH_TIDNAME_L] == tnn[LUAJH_TIDNAME_L])? 1 : 2;
};

// Recover the TID from a function closure at ixx and push on stack. If ixx is 0, get the TID of the
// current function context. Returns 0 and pushes nothing if not a function with a TID, 1 if a
// class, 2 if an object.
int getfunctid(lua_State* L, int ixx) {
	const char* s1; int r, i;
	int ix = ixx;
	lua_checkstack(L, 1);
	if (ix == 0) {
		lua_pushvalue(L,lua_upvalueindex(1)); // Must be a C closure if this is in its context.
		if (!lua_isstring(L, -1)) {lua_pop(L, 1); return 0;}
	} else {
		ix = lua_absindex(L, ix);
		if (!lua_isfunction(L, ix)) return 0; // Not a function closure.
		s1 = lua_getupvalue(L, ix, 1);
		if (s1 == NULL) return 0; // No upvalues.
		if (s1[0] != 0) { // Non-empty name, must be Lua closure.
			i = 1;
			while (s1 != NULL) {
				r = matchtidmarker(s1); if (r != 0) return r; // TID found in Lua closure
				lua_pop(L, 1);
				s1 = lua_getupvalue(L, ix, ++i);
			}
			return 0; // TID not found in Lua closure.
		}
	} // C closure, first upvalue at stack top.
	s1 = lua_tostring(L, -1); // Marker is first upvalue
	lua_pop(L, 1);
	r = matchtidmarker(s1); if (r == 0) return 0;
	if (ix == 0) { // Second upvalue is the actual TID
		lua_pushvalue(L,lua_upvalueindex(2));
	} else {
		s1 = lua_getupvalue(L, ix, 2); if (s1 == NULL) return 0;
	}
	return r;
};

LUACLLIB_API int luaC_newclass(lua_State* L, lua_CFunction init, const luaL_Reg* meth, const char* name /* = 0*/)
{
	lua_checkstack(L, 3);
	lua_createtable(L, 0, sizeof(meth)/sizeof(meth[0])-1); //|MT|
	if (name == 0) {
		lua_pushnil(L);			//|NIL|MT|
	} else {
		get_class(L, name);     //|CL|MT|
		if (!getfunctid(L, -1)) return luaL_error(L,"Invalid class: %s",name);
		lua_remove(L, -2);
	}
	lua_setmetatable(L, -2);	//|MT|
	lua_pushvalue(L, -1);		//|MT|MT|
	lua_setfield(L, -2, "__index");	//|MT|
	lua_pushstring(L, LUAJH_TIDNAME); //|MT|TM|
	lua_pushvalue(L, -2);		//|MT|TM|MT|
	lua_pushcclosure(L, init, 2);	//|CL|MT|
	lua_pushvalue(L, -1);		//|CL|CL|MT|
	lua_insert(L, -3);			//|CL|MT|CL|
	luaL_setfuncs(L, meth, 1);	//|MT|CL|
	lua_pop(L, 1);				//|CL|
	return 1;
};

LUACLLIB_API int luaC_newobject(lua_State* L, int nargs/* = 0*/, const char* name/* = NULL*/)
{
	lua_checkstack(L, 1);
	if (name == NULL) { // Try to get the class from a method of that class:
		lua_pushvalue(L, lua_upvalueindex(1));
		if (!lua_isfunction(L, -1)) {lua_pop(L, 1); return luaL_error(L,"Cannot determine Class from context.");}
	} else { // Fetch the class by name from the loaded modules table:
		get_class(L, name);
	}
	if (nargs > 0) lua_insert(L, -(nargs+1));
	lua_call(L, nargs, 1);
	return 1;
};

LUACLLIB_API void luaC_checkmethod(lua_State* L, int stk) {
	if (stk > 0) lua_checkstack(L, stk);
	if (luaC_isclass(L, 1)) return;
	luaL_argerror(L, 1, "Invalid method call - did you use ':'?");
};

LUACLLIB_API int luaC_isclass(lua_State* L, int inx, const char* name /*= NULL*/)
{
	int r = -1;
	int ix = lua_absindex(L, inx);
	lua_checkstack(L, 1);
	if (name == NULL) {
		lua_pushvalue(L, lua_upvalueindex(1));
		if (!lua_isfunction(L, -1)) r = 0; // Not a class, assume in class closure not method.
	} else {
		get_class(L, name);	//|CL|
	}
	r = luaC_istype(L, ix, r);
	lua_pop(L, 1);
	return r;
};

LUACLLIB_API int luaC_gettid(lua_State* L, int inx)
{
	int r; int ix = inx; int ty = LUA_TFUNCTION;
	lua_checkstack(L, 1);
	if (ix != 0) {
		ix = lua_absindex(L, ix);
		ty = lua_type(L, ix);
		// Rule 1: look for metafield "__tid":
		if (luaL_getmetafield(L, ix, "__tid")) return 4;
	}
	switch(ty){
		// Rule 2: metatable of userdata or table:
		case LUA_TUSERDATA:
		case LUA_TTABLE:
			if (lua_getmetatable(L, ix)) return 3;
			break;
		// Rule 3 & 4: Closure with upvalue pattern:
		case LUA_TFUNCTION:
			r = getfunctid(L, ix);
			if (r != 0) return r;
	}
	if (ix == 0) return 0;
	// Rule 5: The Lua type name:
	lua_pushstring(L, luaL_typename(L, ix));
	return 5;
}

LUACLLIB_API int luaC_istype(lua_State* L, int ixo, int ixc)
{
	int r;
	lua_checkstack(L, 3);
	int xo = lua_absindex(L, ixo);
	int xc = (ixc == 0)? 0 : lua_absindex(L, ixc);
	int tc = luaC_gettid(L, xc); if (tc == 0) return FALSE;
	int to = luaC_gettid(L, xo); if (to == 0) {lua_pop(L, 1); return FALSE;}
	if ((lua_type(L, -1) == LUA_TFUNCTION) && (lua_compare(L, -1, -2, LUA_OPEQ))) {
		lua_pushvalue(L, xo);
		lua_pushvalue(L, xc);
		lua_call(L, 2, 1);
		r = lua_toboolean(L, -1);
		lua_pop(L, 1);
		return r;
	}
	r = lua_compare(L, -1, -2, LUA_OPEQ);
	while ((to == 3) && (tc == 1) && (!r)) {
		if (!lua_getmetatable(L, -1)) break;
		lua_remove(L, -2);
		r = lua_compare(L, -1, -2, LUA_OPEQ);
	}
	lua_pop(L, 2);
	return r;
}

static int class_GetTID(lua_State* L)
{
	lua_settop(L, 1);
	switch (luaC_gettid(L, 1)) {
	case 1:
		lua_pushstring(L, "class");
		return 2;
	case 2:
		lua_pushstring(L, "object_fn");
		return 2;
	case 3:
		lua_pushstring(L, "object_mt");
		return 2;
	case 4:
		lua_pushstring(L, "object_mf");
		return 2;
	default:
		lua_pushstring(L, "type");
		return 2;
	}
}

#pragma endregion

#pragma region "List Class"

// This is a direct lift from ltablib.c in the Lua 5.2 distribution (except for two added parameters to
// auxsort routine). This re-implements the Table library as methods of the List class, avoiding the
// need to include the table library. Sort and Unpack are implemented here in 'C'. The remaining functions
// are added by the Lua part of the library.

/*
** {======================================================
** Quicksort
** (based on `Algorithms in MODULA-3', Robert Sedgewick;
**  Addison-Wesley, 1993.)
*/

#define aux_getn(L,n)  \
	(luaL_checktype(L, n, LUA_TTABLE), luaL_len(L, n))

static void set2 (lua_State *L, int tabix, int i, int j) {
  lua_rawseti(L, tabix, i);
  lua_rawseti(L, tabix, j);
}

static int sort_comp (lua_State *L, int a, int b, int compix) {
  if (!lua_isnil(L, compix)) {  /* function? */
    int res;
    lua_pushvalue(L, compix);
    lua_pushvalue(L, a-1);  /* -1 to compensate function */
    lua_pushvalue(L, b-2);  /* -2 to compensate function and `a' */
    lua_call(L, 2, 1);
    res = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return res;
  }
  else  /* a < b? */
    return lua_compare(L, a, b, LUA_OPLT);
}

static void auxsort (lua_State *L, int l, int u, int tabix, int funcix) {
  while (l < u) {  /* for tail recursion */
    int i, j;
    /* sort elements a[l], a[(l+u)/2] and a[u] */
    lua_rawgeti(L, tabix, l);
    lua_rawgeti(L, tabix, u);
    if (sort_comp(L, -1, -2, funcix))  /* a[u] < a[l]? */
      set2(L, tabix, l, u);  /* swap a[l] - a[u] */
    else
      lua_pop(L, 2);
    if (u-l == 1) break;  /* only 2 elements */
    i = (l+u)/2;
    lua_rawgeti(L, tabix, i);
    lua_rawgeti(L, tabix, l);
    if (sort_comp(L, -2, -1, funcix))  /* a[i]<a[l]? */
      set2(L, tabix, i, l);
    else {
      lua_pop(L, 1);  /* remove a[l] */
      lua_rawgeti(L, tabix, u);
      if (sort_comp(L, -1, -2, funcix))  /* a[u]<a[i]? */
        set2(L, tabix, i, u);
      else
        lua_pop(L, 2);
    }
    if (u-l == 2) break;  /* only 3 elements */
    lua_rawgeti(L, tabix, i);  /* Pivot */
    lua_pushvalue(L, -1);
    lua_rawgeti(L, tabix, u-1);
    set2(L, tabix, i, u-1);
    /* a[l] <= P == a[u-1] <= a[u], only need to sort from l+1 to u-2 */
    i = l; j = u-1;
    for (;;) {  /* invariant: a[l..i] <= P <= a[j..u] */
      /* repeat ++i until a[i] >= P */
      while (lua_rawgeti(L, tabix, ++i), sort_comp(L, -1, -2, funcix)) {
        if (i>=u) luaL_error(L, "invalid order function for sorting");
        lua_pop(L, 1);  /* remove a[i] */
      }
      /* repeat --j until a[j] <= P */
      while (lua_rawgeti(L, tabix, --j), sort_comp(L, -3, -1, funcix)) {
        if (j<=l) luaL_error(L, "invalid order function for sorting");
        lua_pop(L, 1);  /* remove a[j] */
      }
      if (j<i) {
        lua_pop(L, 3);  /* pop pivot, a[i], a[j] */
        break;
      }
      set2(L, tabix, i, j);
    }
    lua_rawgeti(L, tabix, u-1);
    lua_rawgeti(L, tabix, i);
    set2(L, tabix, u-1, i);  /* swap pivot (a[u-1]) with a[i] */
    /* a[l..i-1] <= a[i] == P <= a[i+1..u] */
    /* adjust so that smaller half is in [j..i] and larger one in [l..u] */
    if (i-l < u-i) {
      j=l; i=i-1; l=i+2;
    }
    else {
      j=i+1; i=u; u=j-2;
    }
    auxsort(L, j, i, tabix, funcix);  /* call recursively the smaller one */
  }  /* repeat the routine for the larger one */
}

/*
** {======================================================
*/

//P1 (Function, opt): Comparison function which takes two parameters and returns boolean true
//                    if the first should sort before the second, boolean false otherwise. If
//                    not supplied, the Lua '<' operator is used.
static int list_LuaSort(lua_State *L) {
	luaC_checkmethod(L, 1);
	int n = aux_getn(L, 1);
	luaL_checkstack(L, 40, "");  /* assume array is smaller than 2^40 */
	if (!lua_isnoneornil(L, 2))  /* is there a 2nd argument? */
		luaL_checktype(L, 2, LUA_TFUNCTION);
	lua_settop(L, 2);  /* make sure there is two arguments */
	auxsort(L, 1, n, 1, 2);
	return 0;
}

LUACLLIB_API void luaC_sortlist(lua_State* L, int tabix, int funcix /*=0*/)
{
	luaL_checkstack(L, 41, "");
	luaL_checktype(L, tabix, LUA_TTABLE);
	int t = lua_gettop(L);
	int ix = tabix; if (ix < 0) ix = t + (ix + 1);
	if (funcix == 0)
		lua_pushnil(L);
	else
		lua_pushvalue(L, funcix);
	int n = aux_getn(L, ix);
	auxsort(L, 1, n, ix, t + 1);
	lua_settop(L, t);
}

static int list_Unpack(lua_State* L) {
	luaC_checkmethod(L, 1);
	lua_len(L, 1);
	int n = lua_tointeger(L, -1);
	lua_pop(L, 1);
	int s = luaL_optinteger(L, 2, 1);
	if (s < 0) s = n + s + 1;
	if ((s < 1) || (s > n)) return luaL_argerror(L, 2, "'from' index out of range");
	int e = luaL_optinteger(L, 3, s);
	if (e < 0) e = n + e + 1;
	if ((e < s) || (e > n)) return luaL_argerror(L, 3, "'to' index out of range");
	n = e - s + 1;
	lua_settop(L, 1);
	lua_checkstack(L, n);
	for (int i = s; (i <= e); i++) lua_rawgeti(L, 1, i);
	return n;
}

static int list_Construct(lua_State* L) {
	if (lua_istable(L, 1)) {
		lua_settop(L, 1);
	} else {
		int T = lua_gettop(L);
		lua_newtable(L);
		for (int i = 1; (i <= T); i++) {
			lua_pushvalue(L, i);
			lua_rawseti(L, T+1, i);
		}
	}
	if (luaC_gettid(L, 0) != 1) return luaL_error(L, "invalid class");
	lua_setmetatable(L, -2);
	return 1;
};

void list_Create(lua_State* L)
{
	static const struct luaL_Reg mt [] = {
		{"sort", list_LuaSort},
		{"unpack", list_Unpack},
		{NULL, NULL}
	};
	luaC_newclass(L, list_Construct, mt);
};

#pragma endregion

LUACLLIB_API int LUACLLIB_NGEN(luaopen_)(lua_State* L)
{
	static const struct luaL_Reg fn [] = {
		{"gettid", class_GetTID},
		{NULL, NULL}
	};

	WINSH_LUA(2);

	lua_createtable(L, 0, sizeof(fn)/sizeof(fn[0]) - 1);
	luaL_setfuncs(L, fn, 0);
	list_Create(L);
	lua_setfield(L, -2, "List");

	// Load and execute the Lua part of the library:
	H->LoadScriptResource(CString("LibClass"));
	lua_pushvalue(L, -2);
	H->ExecChunk(1, CString("LibClass-LuaPart"));

	return 1;
}

/******************************************************************************
* Copyright (C) 1994-2008 Lua.org, PUC-Rio.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************/
