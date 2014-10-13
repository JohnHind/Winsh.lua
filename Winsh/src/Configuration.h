// Compile-time inclusion of Lua libraries in the Winsh exe.
// =========================================================

#ifdef SF_CONSOLE
#undef SF_WINDOWS
#define SF_DESC "CONSOLE Subsystem Flag on "
#else
#ifndef SF_WINDOWS
#define SF_WINDOWS
#endif
#define SF_DESC "WINDOWS Subsystem Flag"
#endif

#ifdef CF_MINIMUM
#include "Configs\Config-Minimum.h"
#elif defined CF_STANDARD
#include "Configs\Config-Standard.h"
#else
#include "Configs\Config-Maximum.h"
#endif
