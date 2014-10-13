#pragma once
//#include "windows.h"

//extern "C" {
//#include ".\..\lua\lua.h"
//#include ".\..\lua\lauxlib.h"
//#include ".\..\lua\lualib.h"
//}

// The Library Name (for Lua 'require') should be defined below:============================
#define LUACLLIB_NAME "class"
#define LUACLLIB_NGEN(p) p##class
// =========================================================================================

// If building or consuming the library as a DLL, uncomment this block:=====================
//#if defined(LUACLLIB_BUILDING)
//#define LUACLLIB_API extern "C" __declspec(dllexport)
//#else
//#define LUACLLIB_API extern "C" __declspec(dllimport)
//#endif
// =========================================================================================

// If building or consuming the library as a static-link code library, uncomment this block:
//#define LUACLLIB_API extern
// =========================================================================================

// If compiling the library into an EXE file, uncomment this block:=========================
#define LUACLLIB_API
// =========================================================================================

LUACLLIB_API int LUACLLIB_NGEN(luaopen_)(lua_State* L);

// **********************
// 'C' Interface.
// **********************

// Where a Class 'name' is called for below, it should be a key in the package.loaded global table.
// Usually it will be the library name and the class name separated by a period character, but it may
// just be the class name if a library supplies just one class, or a heirarchical name if the library
// supplies the class within a sub-library. The name may be NULL in which case the Class is determined
// as the Class in whose context the function is called.
// A Lua error is raised if the Class cannot be found.

// Pushes a Class function closure, and returns 1, or pushes nothing and returns 0.
// 'init' is the initialisation function which will be enclosed, it may take any parameters and must return 
// either a userdata or a table being an object of the class, or nothing.
// 'meth' is a function list which will be used to build the metatable. This metatable is set as the
// TID value of the closure and the closure is set as the first upvalue of each entry in the metatable.
// 'name' is the name of the parent class (see above) or NULL to create a new base class.
LUACLLIB_API int luaC_newclass(lua_State* L, lua_CFunction init, const luaL_Reg* meth, const char* name = NULL);

// Executes a Class closure, pushes one return value (assumed to be the object) and returns 1.
// 'name' is either a full Class name (see above), or NULL to create a new object of the same type within
// a method. If 'nargs' is greater than 0, that number of values is popped off the stack and passed as
// arguments to the Class closure.
LUACLLIB_API int luaC_newobject(lua_State* L, int nargs = 0, const char* name = NULL);

// Should be the first statement in a method. Checks that the method is correctly called and (optionally
// when 'stk' > 0) that there is at least 'stk' stack entries available.
LUACLLIB_API void luaC_checkmethod(lua_State* L, int stk = 0);

// Returns TRUE if the object at inx is an object of the Class specified by 'name' (see above). With a
// NULL name, the class may be that of either a method context or a class closure context.
LUACLLIB_API int luaC_isclass(lua_State* L, int inx, const char* name = NULL);

// Pushes the TID of the value at 'inx' onto the stack. 'inx' may be 0 if calling from the context of a
// 'C' closure, in which case the function may push nothing and return 0 if the TID cannot be determined.
// Returns 1 if the value is a Class, 2 if it is an object or any other type of value except, 3 if the TID
// is a metatable.
LUACLLIB_API int luaC_gettid(lua_State* L, int inx);

// Compares the values at ixo and ixc using TID rules and returns TRUE only if the first is of the same
// type or a compatable type as the second. 'inx' may be zero if called in the context of a class closure
// in which case the TID of the contextual class is returned. This is the lua comparison between the TID
// values unless: 1. The TID values are lua equal functions, in this case it is the (Boolean) result of
// calling this function passing it the two original values; or 2. The first TID is a metatable and the
// second value is a Class, in which case the Class TID is compared recursively with the metatable of the
// metatable etc.
LUACLLIB_API int luaC_istype(lua_State* L, int ixo, int ixc);

// Sorts a list-structure table at 'tabix' in place (by reassigning indexes). If 'funcix' is non-zero a
// function at that index is used as the comparison function (it should receive two parameters and return
// 'true' if the first parameter should be placed before the second), otherwise the lua '<' operator is used.
LUACLLIB_API void luaC_sortlist(lua_State* L, int tabix, int funcix = 0);
