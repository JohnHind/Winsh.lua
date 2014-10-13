#ifndef userconfig_h
#define userconfig_h

// Configuration file for Lua source build with JH-LUA-VARIANT patch applied,
// plus any of the patches below which are included applied.

/* Variant prefix for _VERSION string */
#define LUA_VARIANT "[GN]"

/* PowerPatches to be included: */
//#define JH_LUA_TYPEMETA
//#define JH_LUA_TABLECLASS
//#define JH_LUA_BINCONST
#define JH_LUA_SETINIT
#define JH_LUA_ITER

#endif
