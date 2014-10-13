#pragma once

#include "LuaLibIF.h"

class CLuaCommandEnv
{
public:
	CLuaCommandEnv(lua_State* L);
	operator LPCTSTR() { return m_lasterror; }

	// Set the command environment for a function at stacktop:
	void SetCmdEnvironment(void);

	// Check if a symbol in the command environment exists and is of a specific type:
	BOOL CheckCmdField(LPCTSTR nm, int type /*=-1*/);

	// Get a string from a string field in the command environment, nm is the name, def a default value.
	CString GetCmdVar(LPCTSTR nm, LPCTSTR def);

	// Pushes the named field from the command environment onto the stack and returns 1.
	int GetCmdVar(LPCTSTR nm);

	// Pop a value off the stack and set it as a field in the command environment under nm:
	void SetCmdVar(LPCTSTR nm);

	// Returns the full path to a Lua library specified by its name (as for 'require'):
	CString FindLuaLib(LPCTSTR name);

private:
	int m_refCC;		//Index of table in registry, Environment for Immediate Commands
	lua_State* L;
	CString m_lasterror;
};
