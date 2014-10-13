// Compile-time inclusion of Lua libraries in the Winsh exe.
// =========================================================

#ifdef SF_CONSOLE
#undef SF_WINDOWS
#ifdef SF_USELUADLL
#define SF_DESC "CONSOLE Subsystem Flag; Using WinshLua.dll"
#else
#define SF_DESC "CONSOLE Subsystem Flag; Standalone Exe"
#endif
#else
#ifndef SF_WINDOWS
#define SF_WINDOWS
#endif
#ifdef SF_USELUADLL
#define SF_DESC "WINDOWS Subsystem Flag; Using WinshLua.dll"
#else
#define SF_DESC "WINDOWS Subsystem Flag; Standalone Exe"
#endif
#endif

#ifdef CF_MINIMUM
#include "Configs\Config-Minimum.h"
#elif defined CF_STANDARD
#include "Configs\Config-Standard.h"
#else
#include "Configs\Config-Maximum.h"
#endif
