// Compile-time inclusion of Lua libraries in the Winsh exe.
// =========================================================

/*
To create a new configuration:
1. Make a copy of this file with a new name and make any changes required.
2. Add a conditional to include it in Configuration.h under a new CF_xxx symbol.
3. In Configuration Manager, Active Solution Configuration dropdown, New, create a new copy
   of the Release (Std Con) and then the Release (Std Win) configurations.
4. In both the new configurations, bring up the project (Winsh) properties and make the
   following changes:
5. In Preprocessor Definitions, edit the CF_STANDARD symbol to the new one at 2 above.
6. In General Configuration, change the Target Name.
7. In Linker Configuration, change the Output File name.
*/

#define CF_DESC "MAXIMUM Library Configuration"

// Unless specifically commented, libraries are taken from ".\..\lua\lua5.1.lib".
extern "C"
{
//	#include "name\name.h" // Pure "C" library
}

//	#include "name\name.h" // "C++" library
#include "Libraries\LibClass.h"
#include "Libraries\LibGC.h"
#include "Libraries\LibTime.h"
#include "Libraries\LibTask.h"
#include "Libraries\LibShell.h"
#include "Libraries\LibRegistry.h"
#include "Libraries\LibComLink.h"
#include "Libraries\bitfield.h"
#include "Libraries\LibTimer.h"

// These libraries are compiled in the exe and loaded into the environment on reset.
static const luaX_Reg luaX_loads[] = {
//  {"name", luaopen_name, "description"},
	{"_G", luaopen_base, "Standard Lua basic library"},
	{"package", luaopen_package, "Standard Lua 'package' library"},
	{NULL, NULL, NULL}
};

// These libraries are compiled in the exe and put in the preloads table so they are
// loaded on demand from "require" in scripts:
static const luaX_Reg luaX_preloads[] = {
//  {"name", luaopen_name, "description"},
	{"table", luaopen_table, "Standard Lua 'table' library"},
	{"coroutine", luaopen_coroutine, "Standard Lua 'coroutine' library"},
	{"io", luaopen_io, "Standard Lua 'io' library"},
	{"os", luaopen_os, "Standard Lua 'os' library"},
	{"string", luaopen_string, "Standard Lua 'string' library"},
	{"math", luaopen_math, "Standard Lua 'math' library"},
	{"debug", luaopen_debug, "Standard Lua 'debug' library"},
	{"bit32", luaopen_bit32, "Standard Lua 'bit32' library"},
	{"winsh", luaopen_winsh, "Winsh.lua base library v1.0"},
	{"class", luaopen_class, "Winsh 'class' object orientation library v1.0"},
	{"time", luaopen_time, "Winsh 'time' library v1.0"},
	{"timer", luaopen_timer, "Performance Timer library v1.0"},
	{"task", luaopen_task, "Winsh 'task' library v2.0"},
	{"shell", luaopen_shell, "Winsh 'shell' library v1.0"},
	{"registry", luaopen_registry, "Winsh 'registry' library v1.0"},
	{"ComLink", luaopen_ComLink, "Winsh 'ComLink' serial communications library v1.0"},
	{"bitfield", luaopen_bitfield, "Bitfield library v1.0"},
	{NULL, NULL, NULL}
};

// This table provides information on global tables not specifically provided by
// one of the above methods OR causes a specific global field (not necessarily
// a table) to be removed after luaX_loads is processed.
static const luaX_Reg luaX_postload[] = {
//  {"name", NULL, "description"},	//Describe a global table.
//  {"name", NULL, NULL},			//Delete a global field.
	{NULL, NULL, NULL}
};

