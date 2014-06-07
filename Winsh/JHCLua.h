#pragma once

// Support for hosting Lua in a C++ Windows GUI program. Any framework which has a 'reasonably'
// standard CString implementation should be OK (i.e. ATL, WTL, MFC etc.)
// The template class CLua is instantiated with a particular type implementing an interface
// derived from ILuaLib. The interface defines C++ functions which may be called from within
// Lua functions implemented in C (or in C++ with the static attribute). The Lua state and the
// interface pointer are held in the CLua object and there is a template function for recovering
// the interface pointer from the Lua state.
// The functions prefixed "luaX_" are API extensions for use within Lua C functions.
// The memory allocator, the panic handler and the global print function are replaced with versions
// that output to functions WriteError and WriteMessage which must be supplied in the Interface, and
// maintain memory usage statistics which may be interrogated in the CLua object.
// An optional resource loader is provided which enables Lua scripts to be loaded from the exe
// resources (either explicitly, or optionally also implicitly by 'require').
// The Lua module loader system is extended to include a documentation string for each module and
// an inventory system to display this documentation within Lua programs. Support is provided for
// compiling modules into the exe and optionally preloading them when Lua starts. Global symbols
// may be deleted or decorated with documentation after loading.

#define INV_KEY_EXE (0)
#define INV_KEY_RES (1)

// Like lua_pushstring, but takes a CString and pushes it onto the stack as a Lua string.
// Note: Conversion the other way is trivial e.g. CString s = CString(lua_tostring(L, 1));
LUALIB_API void luaX_pushstring(lua_State* L, LPCTSTR s);

// Like luaL_loadstring except takes a CString and also allows the chunk name to be specified.
LUALIB_API int luaX_loadstring(lua_State* L, LPCTSTR code, LPCTSTR name);

// Like luaL_loadfile except takes a CString filename and uses unicode file handling. The 'fp'
// parameter can be a file path used to expand 'fn' to a full path.
LUALIB_API int luaX_loadfile (lua_State *L, LPCTSTR fn, LPCTSTR fp);

// Like luaL_loadfile except it loads and compiles a chunk from the resources of
// this executible. If OK returns 0 with the function on the stack. If not, returns
// an error code as per luaL_loadfile and an error message string on the stack.
LUALIB_API int luaX_loadresource(lua_State* L, LPCTSTR resname);

// Checks the parameters from 'first' to 'last' against the 'lst' of option strings.
// Returns a bitmask with the bit number set for each of the options present in the list.
LUALIB_API UINT luaX_checkoptions(lua_State* L, const char* const lst[], int first, int last);
#define ISOPTION(m,b) (((m) & (1 << (b))) > 0)

// Return TRUE if the object at 'inx' is a function or a table or userdata with a call metafunction.
LUALIB_API int luaX_iscallable(lua_State* L, int inx);

#ifdef _DEBUG
// Debugging functions

// Dumps a stack listing to the debugger output channel, with an optional text 'label'.
LUALIB_API void luaX_showstack(lua_State* L, const char* label = 0);

// Dumps a display of the content of the table at index 'ix' on the stack to the output window,
// with an optional text 'label'.
LUALIB_API void luaX_showtable(lua_State* L, int ix, const char* label = 0);

#endif

// Base class for library interfaces. Inherit from this to define the interface between any
// application specific Lua libraries and the application. The object implementing this
// interface must also implement WriteError and WriteMessage.
class ILuaLib
{
public:
	// Write a message to the Error location (if 'fg', always to the GUI console).
	virtual void WriteError(CString &s, BOOL fg = FALSE) = 0;

	// Write a message to the Message location (if 'fg', always to the GUI console).
	virtual void WriteMessage(LPCTSTR s, BOOL nl = TRUE, BOOL fg = FALSE) = 0;

//internal:
	size_t m_rm(int cmd = 0, size_t c = 0)
	{
		switch (cmd) {
		case 1:  { size_t x = memmax; if (c == 0) memmax = memnow; return x; }
		case 2:  { memnow = memmax = c; return memmax; }
		case 3:  { memnow += c; if (memnow > memmax) memmax = memnow; return memnow; }
		default: return memnow;
		}
	}
	CString m_rt;
private:
	size_t memnow;
	size_t memmax;
};

// Lua functions used internally.
void* luaX_alloc(void* ud, void* ptr, size_t osize, size_t nsize);
int luaX_panic(lua_State* L);
int luaX_print(lua_State* L);
int luaX_resldr(lua_State *L);
BOOL CALLBACK luaX_resenm(HMODULE hm, LPCTSTR ty, LPTSTR nm, LONG_PTR lp);

// Extended luaL_Reg structure for configuration options. Compatable with luaL_register, contains
// additional description field for documentation.
typedef struct luaX_Reg {
  const char *name;
  lua_CFunction func;
  const char *desc;
} luaX_Reg;

// Please do not edit these default tables, copy them to a new header, rename
// them and pass the new ones to the CLua constructor. These defaults initialise
// lua per the standard command line interpreter.
static const luaX_Reg luaX_defloads[] = {
//  {"name", luaopen_name, "description"},
	{"_G", luaopen_base, "Standard Lua basic library"},
	{"package", luaopen_package, "Standard Lua 'package' library"},
	{"coroutine", luaopen_coroutine, "Standard Lua 'coroutine' library"},
	{"table", luaopen_table, "Standard Lua 'table' library"},
	{"io", luaopen_io, "Standard Lua 'io' library"},
	{"os", luaopen_os, "Standard Lua 'os' library"},
	{"string", luaopen_string, "Standard Lua 'string' library"},
	{"bit32", luaopen_bit32, "Standard Lua 'bit32' library"},
	{"math", luaopen_math, "Standard Lua 'math' library"},
	{"debug", luaopen_debug, "Standard Lua 'debug' library"},
	{NULL, NULL, NULL}
};
static const luaX_Reg luaX_defpreloads[] = {
//  {"name", luaopen_name, "description"},
	{NULL, NULL, NULL}
};
static const luaX_Reg luaX_defpostload[] = {
//  {"name", NULL, "description"},	//Describe a global field.
//  {"name", NULL, NULL},			//Delete a global field.
	{NULL, NULL, NULL}
};

// Type I must be ILuaLib or an interface inheriting from it. Pass a pointer to an object implementing
// this interface to the constructor. If library installation other than standard is required, pass
// any of 'load', 'preload' and/or 'postload' arrays to the constructor to override the defaults above.
template<typename I> class CLua
{
public:
	CLua(I* lif, const luaX_Reg* loads = NULL, const luaX_Reg* preloads = NULL, const luaX_Reg* postload = NULL)
	{
		m_lif = lif;
		m_loads = loads; m_preloads = preloads; m_postload = postload;
		if (m_loads == NULL) m_loads = luaX_defloads;
		if (m_preloads == NULL) m_preloads = luaX_defpreloads;
		if (m_postload == NULL) m_postload = luaX_defpostload;
		L = NULL;
		m_lif->m_rm(2);
	}

	lua_State* OpenLua(void)
	{
		CloseLua();
		L = lua_newstate(luaX_alloc, (void*)m_lif);
		if (L == NULL)
		{
			ATLTRACE(atlTraceException, 0, _T("Lua panic: Insufficient memory to create state.\n"));
			AtlThrow(E_FAIL);
		}
		else
		{
			lua_atpanic(L, luaX_panic);
			const luaX_Reg *lib;
			if (m_loads != NULL)
			//'require' libraries in 'loads' array into global environment:
			for (lib = m_loads; lib->func; lib++)
			{
				luaL_requiref(L, lib->name, lib->func, 1);
				lua_pop(L, 1);
			}
			// --
			// Replace the standard 'print' function with our own:
			lua_pushcfunction(L, luaX_print);
			lua_setglobal(L, "print");
			// --
			//insert libraries in 'preloads' array into '_PRELOAD' table:
			luaL_getsubtable(L, LUA_REGISTRYINDEX, "_PRELOAD");
			for (lib = m_preloads; lib->func; lib++) {
				lua_pushcfunction(L, lib->func);
				lua_setfield(L, -2, lib->name);
			};
			lua_pop(L, 1);
			// --
			//remove names in 'postload' array from global environment:
			for (lib = m_postload; lib->name; lib++) {
				if (lib->desc == NULL)
				{
					lua_pushnil(L);
					lua_setglobal(L, lib->name);
				}
			}
			// --
		}
		return L;
	}

	void CloseLua(void)
	{
		if (L != NULL)
		{
			lua_close(L);
			L = NULL;
		}
		m_lif->m_rm(2);
	}

	// Enables Lua scripts stored in the resources of the exe. 'type' is the name of the resource
	// type (suggest: 'LUA'). If 'libraries' is false, this enables scripts to be explicitly loaded
	// using luaX_loadresource. If true, resources will additionally be searched for 'require'.
	void EnableResourceScripts(LPCTSTR type, bool libraries = true)
	{
		m_lif->m_rt = CString(type);
		if (!libraries) return;
		if (m_lif->m_rt.GetLength() < 1) return;
		// Insert new loader in package.searchers to search for packages in the resources of this exe.
		lua_checkstack(L, 2);
		lua_getglobal(L, "package");
		lua_getfield(L, -1, "searchers");
		if (lua_istable(L, -1))
		{
			size_t s = lua_rawlen(L, -1);
			for (size_t i = s; (i > 1); i--)
			{
				lua_rawgeti(L, -1, i);
				lua_rawseti(L, -2, i + 1);
			}
			lua_pushcfunction(L, luaX_resldr);
			lua_rawseti(L, -2, 2);
		}
		lua_pop(L, 2);
	}

	size_t GetMemCount(void)
	{
		return m_lif->m_rm(4);
	}

	size_t GetMemMax(void)
	{
		return m_lif->m_rm(1);
	}

	// Pushes an inventory table onto the Lua stack and returns 1. 'key' INV_KEY_EXE lists
	// the Lua C libraries compiled into the exe. INV_KEY_RES lists the Lua scripts in the
	// resources of the exe file.
	int GetInventory(int key)
	{
		lua_newtable(L);
		if (key == INV_KEY_EXE)
		{
			const luaX_Reg *lib;
			for (lib = m_loads; lib->func; lib++)
			{
				lua_pushstring(L, lib->desc);
				lua_setfield(L, -2, lib->name);
			}
			for (lib = m_preloads; lib->func; lib++)
			{
				lua_pushstring(L, lib->desc);
				lua_setfield(L, -2, lib->name);
			}
			for (lib = m_postload; lib->name; lib++)
			{
				if (lib->desc != NULL)
				{
					lua_pushstring(L, lib->desc);
					lua_setfield(L, -2, lib->name);
				}
				else
				{
					lua_pushnil(L);
					lua_setfield(L, -2, lib->name);
				}
			}
		}
		else
		{
			if (m_lif->m_rt.GetLength() > 0)
			{
				EnumResourceNames(_Module.GetResourceInstance(), m_lif->m_rt, luaX_resenm, (LONG_PTR)L);
			}
		}
		return 1;
	}

	operator lua_State*() { return L; }

	operator I*() { return m_lif; }

private:
	I* m_lif;					// The interface object
	lua_State* L;				// The Lua state
	const luaX_Reg* m_loads;	// Libraries to be compiled in and loaded
	const luaX_Reg* m_preloads;	// Libraries to be compiled in for loading via 'require'
	const luaX_Reg* m_postload;	// Libraries to be removed or documented after loading
};

// Call this in explicit form i.e. IMyHost H = luaX_host<IMyHost>(L, 2); as the first statement in C Lua
// functions. In addition to returning the interface pointer, it does a checkstack if s > 0, so 's' should
// be the maximum number of additional stack levels used in the function.
template<typename I> I* luaX_host(lua_State* L, int s)
{
	void* ptr; lua_getallocf(L, &ptr);
	if (s > 0) luaL_checkstack(L,s,"Out of memory");
	return (I*)ptr;
};
// It is advisable to create a macro to call this function, for example:
// #define MYAPP_LUA(s) \
//		ILuaLibMyApp* H=luaX_host<ILuaLibMyApp>(L,s); \
//		int T=lua_gettop(L);
// This assumes the Lua stack pointer is passed as 'L' and sets two additional local variables:
// 'H' - The interface pointer.
// 'T' - The index of the top of the stack at function entry.
