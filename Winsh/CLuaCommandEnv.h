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

	// 
	CString GetCmdVar(LPCTSTR nm, LPCTSTR def);

private:
	int m_refCC;		//Index of table in registry, Environment for Immediate Commands
	lua_State* L;
	CString m_lasterror;
};
