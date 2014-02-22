#include "stdafx.h"
#include "resource.h"
#include "MainFrm.h"

int _procret;		//The return code for this process to return.
UINT _uwmAreYouMe;	//Registered Windows System Message number unique to Mutex group.
CString _StdinBuf = CString("");	  //Buffer for reading standard input channel.

#pragma region Support Functions

CRect PutWindow(int w, int h)
{
	CRect r;
	CRect d;
	SystemParametersInfo(SPI_GETWORKAREA, 0, (PVOID)d, 0);
	r.bottom = d.bottom; r.right = d.right;
	r.top = r.bottom - h; if (r.top < d.top) r.top = d.top;
	r.left = r.right - w; if (r.left < d.left) r.left = d.left;
	return r;
}

void CMainFrame::SetLibraryPaths()
{
	// Set the search paths for external Lua and C libraries (these can be supplied in seperate files and
	// loaded into the Lua environment on demand using a 'require' statement).
	lua_State* L = *m_lua;
	CString x1; CString x2;
	if (LibPath.GetLength() > 0)
	{
		x1 = LibPath + CString("?") + LuaExt;
		if (LuaExt != CString(".LUA"))
			x1 = x1 + CString(";") + LibPath + CString("?.LUA");
		x2 = LibPath + CString("?.dll");
	}
	else if (ExePath.GetLength() > 0)
	{
		x1 = ExePath + CString("?") + LuaExt;
		if (LuaExt != CString(".LUA"))
			x1 = x1 + CString(";") + ExePath + CString("?.LUA");
		x2 = ExePath + CString("?.dll");
	}
	lua_getglobal(L, "package");
	luaX_pushstring(L, x1);
	lua_setfield(L, -2, "path");
	luaX_pushstring(L, x2);
	lua_setfield(L, -2, "cpath");
	lua_pop(L, 1);
}

void UnQuote(CString& x)
{
	BOOL r = TRUE; TCHAR q;
	while (r)
	{
		x.TrimLeft(); x.TrimRight();
		q = x.GetAt(x.GetLength() - 1);
		if (((q == '\"') || (q == '\'')) && (q == x.GetAt(0)))
			x = x.Mid(1, x.GetLength() - 2);
		else
			r = FALSE;
	}
}

void CMainFrame::ExecCommandLine(CString& cmd, BOOL nodef /*= FALSE*/, BOOL nocmd /*= FALSE*/, BOOL nofile /*= FALSE*/)
{
	lua_State* L = *m_lua;
	int n; CString x; CString y; BOOL er = FALSE; int r;
	CString slb = CString(""); CString sla = CString("");
	int sp = 0; BOOL im = FALSE; BOOL op = TRUE; BOOL vr = FALSE;
	CString em; BOOL con = FALSE;
#if defined SF_CONSOLE
	con = TRUE;
#endif
	LPWSTR* c = CommandLineToArgvW(cmd, &n);
	if (n > 1)
	{	// Parse the command line collecting script texts, marking the position of
		// the script name, and registering presence of -i and -v parameters.
		for (int i = 1; (i < n); i++)
		{
			x = CString(c[i]); UnQuote(x);
			if ((x[0] == '-') && op && (!er))
			{
				if (x.GetLength() > 1)
					switch(x[1])
					{
					case 'e':
						x = x.Mid(2); UnQuote(x);
						if (x.GetLength() > 0)
						{
							if (sla.GetLength() > 0) sla += CString("; ");
							sla += x;
						}
						else
						{
							er = TRUE;
						}
						break;
					case 'l':
						x = x.Mid(2); UnQuote(x);
						if (x.GetLength() > 0)
						{
							if (slb.GetLength() > 0) slb += CString("; ");
							slb += x;
							slb += CString(" = require(\'");
							slb += x;
							slb += CString("\')");
						}
						else
						{
							er = TRUE;
						}
						break;
					case 'i':
						if (x.GetLength() > 2) er = TRUE;
						im = TRUE;
						break;
					case 'v':
						if (x.GetLength() > 2) er = TRUE;
						vr = TRUE;
						break;
					case '-':
						if (x.GetLength() > 2) er = TRUE;
						op = FALSE;
						break;
					default:
						er = TRUE;
						break;
					}
				else
				{
					if (x.GetLength() != 1) er = TRUE;
					op = FALSE;
				}
			}
			else
			{
				if (sp == 0) {sp = i; op = FALSE;}
			}
		}
	}
	LibPath = CString("");
	r = LUA_ERRFILE;
	if (er)
	{
		em = CString("Invalid Command Line parameter");
	}
	else
	{
		if ((im) || (vr)) m_repmode = (con)? ERM_STDIO : ERM_GUI;
		if (vr) ExecLuaString(CString("onabout()"), TRUE);
		if ((sp > 0) && (!nocmd))
		{	// There is a script name in the command line. We try to load the script
			// onto the stack, r will be 0 if we succeed.
			lua_checkstack(L, 2);
			x = CString(c[sp]); UnQuote(x);
			if (x.FindOneOf(TEXT(":.\\")) < 0) r = LoadScriptResource(x);
			if (r == LUA_ERRFILE)
			{	// Not a resource, try file:
				lua_pop(L, 1); //Remove error message from failed LoadResource.
				if (nofile)
				{
					er = TRUE;
					em = CString("Command Line Script not found in resources, file loading disabled.");
				}
				else
				{
					r = 0;
					y = CString(PathFindExtension(x)); y.MakeUpper();
					if (y.GetLength() < 1)
					{	// Add default extension if extension is absent
						PathAddExtension(x.GetBuffer(MAX_PATH), LuaExt);
						x.ReleaseBuffer();
					}
					else if (y != LuaExt)
					{	// Fail if wrong extension
						r = LUA_ERRFILE;
						er = TRUE;
						em = CString("Command line script '") + CString(c[sp]) + CString("' has invalid extension");
					}
					if (r == 0)
					{
						if (PathIsFileSpec(x))
						{	// Prefix exe path if there is no path
							y = x; x = ExePath;
							PathAppend(x.GetBuffer(MAX_PATH), y);
							x.ReleaseBuffer();
						}
						if (PathIsDirectory(x))
						{	// If it is a folder, use init file in that folder
							LibPath = x + CString("\\");
							PathAppend(x.GetBuffer(MAX_PATH), InitName); x.ReleaseBuffer();
							PathAddExtension(x.GetBuffer(MAX_PATH), LuaExt); x.ReleaseBuffer();
						}
						r = LoadScriptFile(x);
					}
				}
			}
		}
		else if (!nodef)
		{	// There was no script name on the command line
			r = LoadScriptResource(InitName);
			if (r == LUA_ERRFILE)
			{
				lua_pop(L, 1);	// Remove error message from failed LoadResource
				if (!nofile)
				{
					x = ExePath;
					PathAppend(x.GetBuffer(MAX_PATH), ExeName); x.ReleaseBuffer();
					PathAddExtension(x.GetBuffer(MAX_PATH), LuaExt); x.ReleaseBuffer();
					if (PathIsDirectory(x))
					{	// If it is a folder, use init file in that folder
						LibPath = x + CString("\\");
						PathAppend(x.GetBuffer(MAX_PATH), InitName); x.ReleaseBuffer();
						PathAddExtension(x.GetBuffer(MAX_PATH), LuaExt); x.ReleaseBuffer();
					}
					r = LoadScriptFile(x);
				}
			}
		}
		if ((r != 0) && (r != LUA_ERRFILE))
		{
			// Report any error when loading script file or resource.
			er = TRUE;
			em = CString(lua_tostring(L, -1));
		}
		if ((r != 0) && (sla.GetLength() == 0)) im = TRUE;
		SetLibraryPaths();
		if ((slb.GetLength() > 0) && (!nocmd))
		{	// If we have -l text, execute it leaving the main chunk (if any) under it.
			lua_checkstack(L, 2);
			SetRunning();
			int rr = luaX_loadstring(L, slb, CString("CommandLine-Require"));
			if (rr == 0) rr = lua_pcall(L, 0, 0, 0);
			if (rr != 0)
			{
				WriteError(CString(lua_tostring(L, -1)));
				lua_pop(L, 1);
			}
			else
			{
				WriteOK();
			}
		}
		if (r == 0)
		{	// If we have a chunk from a script file or resource, execute it giving it any
			// post-script parameters from the command line. If it returns a string, print
			// it, if a number that becomes the return code for Winsh when it terminates.
			int np = 0; if (n > 0) np = n - sp - 1;
			if (np <= 20)
			{
				lua_checkstack(L, np);
				for (int i = 1; (i <= np); i++)
				{
					x = CString(c[sp + i]);
					if (x == CString("true"))
					{
						lua_pushboolean(L, TRUE);
					}
					else if (x == CString("false"))
					{
						lua_pushboolean(L, FALSE);
					}
					else if (x == CString("nil"))
					{
						lua_pushnil(L);
					}
					else
					{
						luaX_pushstring(L, x);
						if (lua_isnumber(L, -1) == 1)
						{
							lua_Number nv = lua_tonumber(L, -1);
							lua_pop(L, 1);
							lua_pushnumber(L, nv);
						}
						else
						{
							lua_pop(L, 1);
							UnQuote(x);
							luaX_pushstring(L, x);
						}
					}
				}
				SetRunning();
				r = lua_pcall(L, np, 1, 0);
				if (r == 0)
				{
					if (lua_type(L, -1) == LUA_TSTRING)
					{
						WriteMessage(CString(lua_tostring(L, -1)));
					}
					else if (lua_type(L, -1) == LUA_TNUMBER)
					{
						_procret = lua_tointeger(L, -1);
					}
					WriteOK();
				}
				else
				{
					WriteError(CString(lua_tostring(L, -1)));
				}
				lua_pop(L, 1);
			}
		}
		if ((sla.GetLength() > 0) && (!nocmd))
		{	// If we have -e text, execute it.
			lua_checkstack(L, 2);
			SetRunning();
			int rr = luaX_loadstring(L, sla, CString("CommandLine-Execute"));
			if (rr == 0) rr = lua_pcall(L, 0, 0, 0);
			if (rr != 0)
			{
				WriteError(CString(lua_tostring(L, -1)));
				lua_pop(L, 1);
			}
			else
			{
				WriteOK();
			}
		}
	}
	if (er)
	{
		m_repmode = (con)? ERM_STDIO : ERM_GUI;
		WriteError(em);
	}
	else if (im && (!vr))
	{
		m_repmode = (con)? ERM_STDIO : ERM_GUI;
		if (con)
			ExecLuaString(CString("oncommandline()"), TRUE);
		else
			WriteMessage(CString(""));
	}
	ReportMem();
}

#pragma endregion

#pragma region Open and Close Lua

void CMainFrame::CloseLua(void)
{
	ResetMsgTrap();
	m_lua->CloseLua();
	delete(m_cmd);
}

void CMainFrame::OpenLua(void)
{
	// Reset internal state:
	ResetMsgTrap();
#if defined SF_CONSOLE
	m_repmode = ERM_STDIO;
	m_errmode = ERM_STDIO;
#else
	m_repmode = ERM_REPORT;
	m_errmode = ERM_GUI;
#endif
	LastError = CString("");
	_procret = EXIT_SUCCESS;
	AppName.LoadString(IDS_APNAME_DEF);
	SetAppName(AppName);
	CString mf; mf.LoadString(IDR_MAINFRAME);
	SetAppIcon(mf);

	// Reset the report window:
	m_paras->SendMessageDown(WM_USER, WF_CLEAR, 0);
	m_paras->EnterCS();
	m_paras->rect = PutWindow(400, 300);
	m_paras->LeaveCS();
	m_paras->SendMessageDown(WM_USER, WF_MOVE, 0);

	// Create the Lua state:
	lua_State* L = m_lua->OpenLua();
	m_lua->EnableResourceScripts(ResType, true);

	// Create the message table:
	m_refMM = GetRegistryTable(LUA_NOREF);

	// Create the command interpreter environment and load it from "init-cmd":
	m_cmd = new CLuaCommandEnv(L);
	LastError = CString(*m_cmd);

	// Clear interactive console (Must be this late to see command environment):
	if (m_wndTopEdit.GetWindowTextLength() == 0) ClearTop();
	ClearBot();
	if (LastError.GetLength() > 0) WriteError(LastError);

	// Run any initialisation script "init":
	if (luaX_loadresource(L, CString("init")) != LUA_ERRFILE) ExecChunk();

	// Process command line and initial script:
	CString x; BOOL nocmd = FALSE; BOOL nofile = FALSE; BOOL nodef = FALSE;
	x.LoadString(IDS_NOCMDLINE); x.MakeUpper(); nocmd = (x.Left(1) == CString("T"));
	x.LoadString(IDS_NOFILESCRIPT); x.MakeUpper(); nofile = (x.Left(1) == CString("T"));
	x.LoadString(IDS_NODEFSCRIPT); x.MakeUpper(); nodef = (x.Left(1) == CString("T"));
	ExecCommandLine(CString(GetCommandLine()), nodef, nocmd, nofile);

	// Fire the startup message:
	PostMessage(GT_STARTUP);
}

#pragma endregion

#pragma region UI Control

void CMainFrame::IncUI()
{
	uicount++;
}

void CMainFrame::DecUI()
{
	if (--uicount < 1) 	PostMessage(WM_COMMAND, ID_APP_EXIT, 0);
}

void CMainFrame::OnNotifyRightclick()
{
	if (m_Rmenu)
	{
		ShowNotifyMenu(1);
	}
	else
	{
		PostMessage(GT_NOTIFY_RCLICK);
	}
}

void CMainFrame::OnNotifyLeftclick()
{
	if (m_Lmenu)
	{
		ShowNotifyMenu(2);
	}
	else
	{
		PostMessage(GT_NOTIFY_LCLICK);
	}
}

void CMainFrame::PrepareNotifyMenu(CMenuHandle m, int k)
{
	if (k == 1)
		SendMessage(GT_NOTIFY_RCLICK, 0, (LPARAM)m.m_hMenu);
	else
		SendMessage(GT_NOTIFY_LCLICK, 0, (LPARAM)m.m_hMenu);
}

HICON CMainFrame::GetMenuIcon(UINT uCmd)
{
	HICON hs = (HICON)LoadImage(_Module.GetResourceInstance(), CString("LOCK"), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
	return hs;
}

BOOL CMainFrame::ExecuteImmediate(BOOL enter)
{
	int st, ed, ll, pl;
	CString s; CString x; CString a; BOOL echo = FALSE;
	pl = m_prompt.GetLength();
	if (m_wndTopEdit.GetSelLength() < 1)
	{	//There is no selection.
		ll = m_wndTopEdit.GetLine(s);
		if ((ll >= pl) && (s.Left(pl) == m_prompt))
		{	//Current line is prompted - strip prompt and execute line.
			s = s.Mid(pl); s.TrimLeft(); s.TrimRight();
			if (m_wndTopEdit.GetSelStartLine() < m_wndTopEdit.GetLineCount()) a = s;
			if (s.Left(1) == CString("="))
			{
				s = s.Mid(1); s.TrimLeft();
				x = m_cmd->GetCmdVar(CString("_EQUAL"), CString("return (!!)"));
				x.Replace(CString("!!"), s);
				s = x;
			}
			st = 0;
		}
		else
		{	//No prompt and no selection - execute entire content.
			if (enter) return FALSE;
			st = 1; ed = m_wndTopEdit.GetLineCount();
		}
	}
	else
	{	//There is a selection - execute selection only.
		if (enter) return FALSE;
		m_wndTopEdit.SetSelFullLines();
		st = m_wndTopEdit.GetSelStartLine();
		ed = m_wndTopEdit.GetSelEndLine();
	}
	if (st > 0)
	{
		s = CString("");
		for (int i = st; (i <= ed); i++)
		{
			ll = m_wndTopEdit.GetLine(x, i);
			if ((ll < pl) || (x.Left(pl) != m_prompt)) s += x + CString("\r\n");
		}
	}
	if (s.GetLength() > 0) ExecLuaString(s, TRUE);
	AddCommand(a);
	return TRUE;
}

void CMainFrame::AddCommand(CString &s)
{
	CString x; int t = 0;
	CString e("\r\n");
	m_wndTopEdit.GetLine(x, -1);
	x.TrimRight();
	s.TrimRight(); s.TrimLeft();
	if (s.GetLength() > 0)
	{
		if (x == m_prompt)
			t = 1;
		else if (s.GetLength() > 0)
			t = 3;
		else
			t = 2;
	}
	else
	{
		if (x.GetLength() < 1)
			t = 4;
		else if (x != m_prompt)
			t = 5;
	}
	x = m_cmd->GetCmdVar(CString("_PROMPT"), CString(">"));
	x += CString(" ");
	switch (t)
	{
	case 1:
		m_wndTopEdit.AppendText(s + e + x);
		break;
	case 2:
		m_wndTopEdit.AppendText(x + s + e + x);
		break;
	case 3:
		m_wndTopEdit.AppendText(e + x + s + e + x);
		break;
	case 4:
		m_wndTopEdit.AppendText(x);
		break;
	case 5:
		m_wndTopEdit.AppendText(e + x);
		break;
	default:
		break;
	}
}

void CMainFrame::ExecLuaString(CString &s, BOOL cmd /*=FALSE*/)
{
	lua_State* L = *m_lua;
	IncUI();
	int top = lua_gettop(L);
	SetRunning();
	int r = luaX_loadstring(L, s, TEXT("Immediate"));

	if (r == 0)
	{													//|F
		if (cmd) m_cmd->SetCmdEnvironment();
		r = lua_pcall(L, 0, 1, 0);						//|R
		if ((r == 0) && (lua_isstring(L, -1))) WriteMessage(CString(lua_tostring(L, -1)));
	}
	if (r != 0)
	{
		CString err(luaL_optstring(L, -1, "Error running immediate script."));
		WriteError(err);
	}
	else
	{
		WriteOK();
	}
	lua_settop(L, top);
	DecUI();
	ReportMem();
}

void CMainFrame::ClearTop(void)
{
	m_wndTopEdit.SetWindowTextW(TEXT(""));
	AddPrompt();
	m_wndTopEdit.SetFocus();
}

void CMainFrame::AddPrompt(void)
{
	CString s;
	int ll = m_wndTopEdit.GetLine(s);
	int pl = m_prompt.GetLength();
	if (pl == 0) {m_prompt.LoadString(IDS_PROMPT); pl = m_prompt.GetLength();}
	if ((ll >= pl) && (s.Left(pl) == m_prompt))
	{
		s = s.Mid(pl);
		s.TrimLeft(); s.TrimRight();
	}
	else
	{
		m_prompt = m_cmd->GetCmdVar(CString("_PROMPT"), m_prompt);
		s.TrimLeft(); s.TrimRight();
		s = m_prompt + CString(" ") + s;
	}
	if (m_prompt.GetLength() == 0) m_prompt.LoadString(IDS_PROMPT);
	m_wndTopEdit.ReplaceLine(s);
}

void CMainFrame::ClearBot(void)
{
	m_wndBotEdit.SetWindowText(TEXT(""));
	CString nm("onreportclear");
	if (m_cmd->CheckCmdField(nm, LUA_TFUNCTION)) ExecLuaString(nm + CString("()"), TRUE);
}

void CMainFrame::ReportMem()
{
	if (this->m_hWnd == NULL) return;
	CString x;
	x.Format(TEXT("%d:%d"), m_lua->GetMemCount(), m_lua->GetMemMax());
	m_status.SetPaneText(IDR_STATUS_PANE2, x);
}

#pragma endregion

#pragma region Process Mutex & Command Exchange

// We are trying to make this a duel-purpose GUI/Console app and have to be quite tricky!
// There are three cases to accomodate:
// 1. Started using GUI methods: We create a new console to display output.
// 2. Started from some shell with redirected stdio: We use the supplied handles.
// 3. Started from a command console: We send output to that console.
// To achieve all this we need two versions, differing only that one is marked as a Console
// app and the other as a Windows (GUI) app. They are broadly interchangable except:
// - The GUI app will not allow re-direction of I/O or piping and needs to be started with
//   start /wait when launched from a batch file or command console.
// - The Console app will briefly flash a redundant Console window on screen when started
//   from Windows or by shelling from a GUI app,

// chan must be STD_OUTPUT_HANDLE or STD_ERROR_HANDLE
void CMainFrame::WriteConsole(DWORD chan, LPCTSTR string)
{
	BOOL cmd = AttachConsole(ATTACH_PARENT_PROCESS);
	if ((!cmd) && (GetLastError() != ERROR_ACCESS_DENIED))
	{
		AllocConsole();
		CString t(""); this->GetWindowTextW(t.GetBuffer(200), 200); t.ReleaseBuffer();
		SetConsoleTitle(t);
	}
	HANDLE h = (HANDLE)GetStdHandle(chan);
	CString s(string);
	if (GetFileType(h) == FILE_TYPE_CHAR)
	{
		if (cmd) ::WriteConsole(h, _T("\r\n"), 2, NULL, NULL);
		::WriteConsole(h, s, s.GetLength(), NULL, NULL);
	}
	else
	{
		int sz = s.GetLength();
		int bs = WideCharToMultiByte(CP_OEMCP, 0, s, sz + 1, NULL, 0, NULL, NULL);
		char* b = new char[bs];
		int rs = WideCharToMultiByte(CP_OEMCP, 0, s, sz + 1, b, bs, NULL, NULL);
		DWORD ln;
		::WriteFile(h, b, rs - 1, &ln, NULL);
		::FlushFileBuffers(h);
		delete b;
	}
}

BOOL CALLBACK CMainFrame::searcher(HWND hWnd, LPARAM lParam)
{
    HWND* target = (HWND*)lParam;
	if (*target == hWnd) return TRUE;
    DWORD result;
    LRESULT ok = ::SendMessageTimeout(hWnd, _uwmAreYouMe, 0, 0, SMTO_BLOCK | SMTO_ABORTIFHUNG, 200, &result);
    if(ok == 0) return TRUE;
    if(result == _uwmAreYouMe)
    {
        *target = hWnd;
        return FALSE;
    }
    return TRUE;
}

/****************************************************************************
*                             ExclusionName
* Ref:     http://www.codeproject.com/KB/cpp/avoidmultinstance.aspx
* Inputs:
*       LPCTSTR GUID: The GUID for the exclusion
*       UINT kind: Kind of exclusion
*               SYSTEM		0
*               DESKTOP		1
*               SESSION		2
*               USER		3
* Result: CString
*       A name to use for the exclusion mutex
* Notes:
*       The GUID is created by a declaration such as
*       #define UNIQUE _T("MyAppName-{44E678F7-DA79-11d3-9FE9-006067718D04}")
****************************************************************************/
CString CMainFrame::ExclusionName(LPCTSTR GUID, UINT kind)
{
    switch(kind)
    {
        case 0:		//System
            return CString(GUID);

        case 1:		//Desktop
        {
            CString s = GUID;
            DWORD len;
            HDESK desktop = GetThreadDesktop(GetCurrentThreadId());
            BOOL result = GetUserObjectInformation(desktop, UOI_NAME, NULL, 0, &len);
            DWORD err = ::GetLastError();
            if(!result && err == ERROR_INSUFFICIENT_BUFFER)
            { /* NT/2000 */
                LPBYTE data = new BYTE[len];
                result = GetUserObjectInformation(desktop, UOI_NAME, data, len, &len);
                s += _T("-");
                s += (LPCTSTR)data;
                delete [ ] data;
            } /* NT/2000 */
            else
            { /* Win9x */
                s += _T("-Win9x");
            } /* Win9x */
            return s;
        }

        case 2:		//Session
        {
            CString s = GUID;
            HANDLE token;
            DWORD len;
            BOOL result = OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token);
            if(result)
            { /* NT */
                GetTokenInformation(token, TokenStatistics, NULL, 0, &len);
                LPBYTE data = new BYTE[len];
                GetTokenInformation(token, TokenStatistics, data, len, &len);
                LUID uid = ((PTOKEN_STATISTICS)data)->AuthenticationId;
                delete [ ] data;
                CString t;
                t.Format(_T("-%08x%08x"), uid.HighPart, uid.LowPart);
                return s + t;
            } /* NT */
            else
            { /* 16-bit OS */
                return s;
            } /* 16-bit OS */
        }

        default:	//User
        {
            CString s = GUID;
#define NAMELENGTH 64
            TCHAR userName[NAMELENGTH];
            DWORD userNameLength = NAMELENGTH;
            TCHAR domainName[NAMELENGTH];
            DWORD domainNameLength = NAMELENGTH;

            if(GetUserName(userName, &userNameLength))
            {
                // The NetApi calls are very time consuming
                // This technique gets the domain name via an
                // environment variable
                domainNameLength = ExpandEnvironmentStrings(_T("%USERDOMAIN%"), domainName, NAMELENGTH);
                CString t;
                t.Format(_T("-%s-%s"), domainName, userName);
                s += t;
            }
            return s;
        }
    }
    return CString(GUID);
}

LRESULT CMainFrame::OnRegisteredMessage(UINT uMsg, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (uMsg != _uwmAreYouMe) {bHandled = FALSE; return 0;}
	return _uwmAreYouMe;
}

LRESULT CMainFrame::OnRemoteCommand(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled)
{
	COPYDATASTRUCT* lpcd = (COPYDATASTRUCT*) lParam;
	if (lpcd->dwData != 123) return 0;
	CString p((LPCTSTR)lpcd->lpData, lpcd->cbData / sizeof(TCHAR));
	ExecCommandLine(p, TRUE);
	return 1;
}

#pragma endregion

#pragma region ILuaLib Library Interface

void CMainFrame::SetAppName(LPCTSTR s)
{
	AppName = CString(s);
	SetWindowText(AppName);
	m_paras->EnterCS();
	m_paras->string_data = AppName;
	m_paras->LeaveCS();
	m_paras->SendMessageDown(WM_USER, WF_TITLE, 0);
}

BOOL CMainFrame::SetAppIcon(LPCTSTR icon)
{
	HICON hl = (HICON)LoadImage(_Module.GetResourceInstance(), icon, IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
	HICON hs = (HICON)LoadImage(_Module.GetResourceInstance(), icon, IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
	if ((hl == NULL) || (hs == NULL)) return FALSE;
	SetIcon(hl, TRUE);
	SetIcon(hs, FALSE);
	m_paras->EnterCS();
	m_paras->iconl = hl;
	m_paras->icons = hs;
	m_paras->LeaveCS();
	m_paras->SendMessageDown(WM_USER, WF_ICON, 0);
	return TRUE;
}

BOOL CMainFrame::Require(LPCTSTR name)
{
	lua_State* L = *m_lua;
	lua_getglobal(L, "require");
	luaL_checktype(L, -1, LUA_TFUNCTION);
	luaX_pushstring(L, name);
	if (lua_pcall(L, 1, 0, 0) == 0)
		return TRUE;
	else
		return FALSE;
}

int CMainFrame::GetRegistryTable(int key/* = LUA_NOREF*/, int ix/* = 0*/, const char* w/* = NULL*/)
{
	lua_State* L = *m_lua;
	lua_checkstack(L, 3);
	lua_rawgeti(L, LUA_REGISTRYINDEX, key);
	if (lua_istable(L, -1)) return key;
	lua_pop(L, 1);
	if (ix == 0)
		lua_newtable(L);
	else
		lua_pushvalue(L, ix);
	if ((w != NULL) && (*w != NULL))
	{
		lua_newtable(L);
		lua_pushstring(L, w);
		lua_setfield(L, -2, "__mode");
		lua_setmetatable(L, -2);
	}
	lua_pushvalue(L, -1);
	return luaL_ref(L, LUA_REGISTRYINDEX);
}

UINT CMainFrame::AllocLuaMessages(UINT num)
{
	lua_State* L = *m_lua;
	lua_checkstack(L, 3);
	int top = lua_gettop(L);
	int s = AllocTrapIDs(num);
	lua_rawgeti(L, LUA_REGISTRYINDEX ,m_refMM);
	for (UINT i = s; (i < GetTrapTopID()); i++)
	{
		lua_pushinteger(L, i);
		lua_pushinteger(L, i);
		lua_settable(L, -3);
	}
	lua_settop(L, top);
	return s;
}

void CMainFrame::SetTimerMessage(UINT code, UINT time/* = 0*/)
{
	if (!IsWindow()) return;
	if (time > 0)
	{
		SetTimer((UINT_PTR)code, time, NULL);
	}
	else
	{
		KillTimer((UINT_PTR)code);
	}
}

void CMainFrame::SetLuaMessageHandler(UINT code)
{
	if ((code < 1) || (code >= GetTrapTopID())) return;
	lua_State* L = *m_lua;
	int top = lua_gettop(L);
	lua_checkstack(L, 4);
	if ((top < 1) || (!luaX_iscallable(L, -1))) lua_pushinteger(L, code);
	lua_rawgeti(L, LUA_REGISTRYINDEX ,m_refMM);
	lua_pushinteger(L, code);
	lua_pushvalue(L, -3);
	lua_settable(L, -3);
	lua_settop(L, top);
}

UINT CMainFrame::FindFreeLuaMessage(UINT code, UINT num/* = 1*/)
{
	lua_State* L = *m_lua;
	lua_checkstack(L, 2);
	if (num < 1) return 0;
	if (code < 1) return 0;
	if ((code + num) > GetTrapTopID()) return 0;
	lua_rawgeti(L, LUA_REGISTRYINDEX ,m_refMM);
	for (UINT i = 0; (i < num); i++)
	{
		lua_pushinteger(L, code + i);
		lua_gettable(L, -2);
		if (lua_type(L, -1) != LUA_TFUNCTION)
		{
			lua_pop(L, 2);
			return code + i;
		}
		lua_pop(L, 1);
	}
	lua_pop(L, 1);
	return 0;
}

void CMainFrame::SetReportMode(UINT r, UINT e)
{
	m_repmode = r;
	m_errmode = e;
}

BOOL CMainFrame::SetFile(LPCTSTR fnn, BOOL clr/* = FALSE*/)
{
	CString fn(fnn);
	int p = fn.ReverseFind('\\'); CString file; CString path;
	if (p > 0)
	{
		path = fn.Left(p + 1);
		file = fn.Mid(p + 1);
	}
	else
	{
		path = ExePath;
		file = fn;
	}
	if (!PathIsDirectory(path)) {return FALSE;}
	m_file = path + file;
	HANDLE h = CreateFile(m_file, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE) {m_file = CString(""); return FALSE;}
	CloseHandle(h);
	m_clr = clr;
	return TRUE;
}

void CMainFrame::WriteFile(CString& msg)
{
	if (m_file.GetLength() == 0) return;
	HANDLE h = CreateFile(m_file, GENERIC_WRITE, FILE_SHARE_WRITE, NULL,
		(m_clr)? TRUNCATE_EXISTING : OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE) return;
	m_clr = FALSE;
	LARGE_INTEGER x; x.QuadPart = 0;
	SetFilePointerEx(h, x, NULL, FILE_END);
	int sz = msg.GetLength();
	int bs = WideCharToMultiByte(CP_OEMCP, 0, msg, sz + 1, NULL, 0, NULL, NULL);
	char* b = new char[bs];
	int rs = WideCharToMultiByte(CP_OEMCP, 0, msg, sz + 1, b, bs, NULL, NULL);
	DWORD ln;
	::WriteFile(h, b, rs - 1, &ln, NULL);
	::FlushFileBuffers(h);
	delete b;
	CloseHandle(h);
}

void CMainFrame::WriteMessage(CString& s, BOOL nl/* = TRUE*/)
{
	CString ss;
	if (nl)
		ss.Format(_T("%s\r\n"), s);
	else
		ss = s;
	if ((m_repmode & ERM_GUI) != 0)
	{
		if (!IsWindowVisible())
		{
			ShowWindow(SW_NORMAL);
			IncUI();
		}
		m_wndBotEdit.AppendText(ss);
	}
	if ((m_repmode & ERM_REPORT) != 0)
	{
		m_paras->EnterCS();
		if (!m_paras->visible) IncUI();
		m_paras->string_data = ss;
		m_paras->LeaveCS();
		m_paras->SendMessageDown(WM_USER, WF_WRITE, 0);
	}
	if ((m_repmode & ERM_STDIO) != 0)
	{
		WriteConsole(STD_OUTPUT_HANDLE, ss);
	}
	if ((m_repmode & ERM_FILE) != 0)
	{
		WriteFile(ss);
	}
	if ((m_repmode & ERM_DEBUGGER) != 0)
	{
		OutputDebugString(ss);
	}
}

void CMainFrame::WriteError(CString& s)
{
	LastError = s;
	if (_procret == 0) _procret = EXIT_FAILURE;
	PostMessage(GT_ERROR);

	CString ss;
	ss.Format(_T("ERROR: %s\r\n"), s);

	if (((m_errmode & ERM_GUI) != 0) || ((m_errmode & ERM_REPORT) != 0))
	{
		if (!IsWindowVisible())
		{
			ShowWindow(SW_NORMAL);
			IncUI();
		}
		m_wndBotEdit.AppendText(ss);
	}
	if ((m_errmode & ERM_STDIO) != 0)
	{
		WriteConsole(STD_ERROR_HANDLE, ss);
	}
	if ((m_errmode & ERM_FILE) != 0)
	{
		int i = 1;
	}
	if ((m_errmode & ERM_DEBUGGER) != 0)
	{
		OutputDebugString(ss);
	}
	if (IsWindowVisible()) m_status.SetPaneText(IDR_STATUS_PANE1, TEXT("ERROR"));
}

void CMainFrame::SetReportWindow(int act, int px, int py)
{
	int x = px; int y = py;
	CRect wr;
	CRect bd;
	m_paras->EnterCS();
	wr = m_paras->rect;
	m_paras->LeaveCS();
	switch (act)
	{
	case 1: // "hide"
		m_paras->SendMessageDown(WM_USER, WF_CLOSE, 0);
		break;
	case 2: // "resize"
		bd.right = GetSystemMetrics(SM_CXFULLSCREEN); bd.bottom = GetSystemMetrics(SM_CYFULLSCREEN);
		if (x < 10) x = 200; if (x > bd.right) x = bd.right;
		if (y < 10) y = 100; if (y > bd.bottom) y = bd.bottom;
		wr.right = wr.left + x;
		wr.bottom = wr.top + y;
		m_paras->EnterCS();
		m_paras->rect = wr;
		m_paras->LeaveCS();
		m_paras->SendMessageDown(WM_USER, WF_MOVE, 0);
		break;
	case 3: // "position"
		bd.left = GetSystemMetrics(SM_XVIRTUALSCREEN); bd.right = bd.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);	
		bd.top = GetSystemMetrics(SM_YVIRTUALSCREEN); bd.bottom = bd.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
		wr.MoveToX(x);
		wr.MoveToY(y);
		if (wr.left < bd.left) wr.MoveToX(bd.left);
		if (wr.top < bd.top) wr.MoveToY(bd.top);
		if (wr.right > bd.right) wr.MoveToX(bd.right - wr.Width());
		if (wr.bottom > bd.bottom) wr.MoveToY(bd.bottom = wr.Height());
		m_paras->EnterCS();
		m_paras->rect = wr;
		m_paras->LeaveCS();
		m_paras->SendMessageDown(WM_USER, WF_MOVE, 0);
		break;
	case 4: // "bottomright"
		wr = PutWindow(wr.Width(), wr.Height());
		m_paras->EnterCS();
		m_paras->rect = wr;
		m_paras->LeaveCS();
		m_paras->SendMessageDown(WM_USER, WF_MOVE, 0);
		break;
	case 5: // "delete"
		m_paras->SendMessageDown(WM_USER, WF_DELETE, 0);
		break;
	case 6: // "reset"
		m_paras->SendMessageDown(WM_USER, WF_RESET, 0);
		break;
	case 7: // "running"
		m_paras->EnterCS();
		m_paras->number_data = px;
		m_paras->LeaveCS();
		m_paras->SendMessageDown(WM_USER, WF_PROGRESS, 0);
		break;
	case 8: // "paused"
		m_paras->SendMessageDown(WM_USER, WF_PAUSED, 0);
		break;
	case 9: // "error"
		m_paras->SendMessageDown(WM_USER, WF_ERROR, 0);
		break;
	case 10: // "done"
		m_paras->SendMessageDown(WM_USER, WF_DONE, 0);
		break;
	case 11: // "overlay"
		break;
	case 12: // "minimise"
		m_paras->SendMessageDown(WM_USER, WF_MINIMIZE, 0);
		break;
	default:
		m_paras->SendMessageDown(WM_USER, WF_CLEAR, 0);
		break;
	}
}

void CMainFrame::SetOverlay(LPCTSTR icon)
{
	HICON hs = (HICON)LoadImage(_Module.GetResourceInstance(), icon, IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
	m_paras->EnterCS();
	m_paras->icons = hs;
	m_paras->LeaveCS();
	m_paras->SendMessageDown(WM_USER, WF_OVERLAY, 0);
}

TCHAR CMainFrame::ReadConsole()
{
	BOOL cmd = AttachConsole(ATTACH_PARENT_PROCESS);
	if ((!cmd) && (GetLastError() != ERROR_ACCESS_DENIED))
	{
		AllocConsole();
		CString t(""); this->GetWindowTextW(t.GetBuffer(200), 200); t.ReleaseBuffer();
		SetConsoleTitle(t);
	}
	HANDLE h = (HANDLE)GetStdHandle(STD_INPUT_HANDLE);
	if (GetFileType(h) != FILE_TYPE_CHAR) return 0;
	SetForegroundWindow(GetConsoleWindow());
	TCHAR buf;
	DWORD num = 0;
	::SetConsoleMode(h, 0);
	::ReadConsole(h, &buf, 1, &num, NULL);
	return buf;
}

LPCTSTR CMainFrame::GetLine(BOOL start)
{
	HANDLE h; BOOL std; BOOL end = FALSE;
	if (start)
	{
		m_filepos = 0;
		return NULL;
	}
	if (m_file.GetLength() == 0)
	{
		BOOL cmd = AttachConsole(ATTACH_PARENT_PROCESS);
		if ((!cmd) && (GetLastError() != ERROR_ACCESS_DENIED))
		{
			AllocConsole();
			CString t(""); this->GetWindowTextW(t.GetBuffer(200), 200); t.ReleaseBuffer();
			SetConsoleTitle(t);
		}
		h = (HANDLE)GetStdHandle(STD_INPUT_HANDLE);
		if (GetFileType(h) == FILE_TYPE_CHAR) SetForegroundWindow(GetConsoleWindow());
		std = TRUE;
	}
	else
	{
		h = CreateFile(m_file, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (h == INVALID_HANDLE_VALUE) return NULL;
		LARGE_INTEGER x;
		GetFileSizeEx(h, &x);
		end = TRUE;
		if (m_filepos < x.QuadPart)
		{
			x.QuadPart = m_filepos;
			SetFilePointerEx(h, x, NULL, FILE_BEGIN);
			end = FALSE;
		}
		std = FALSE;
	}
	BYTE buf;
	DWORD n = (end)? 0 : 1;
	_StdinBuf.Empty();
	while (n == 1)
	{
		if (::ReadFile(h, &buf, 1, &n, NULL) && (n == 1))
		{
			m_filepos++;
			if (buf == '\r')
				break;
			else if ((buf == 3) || (buf == 4) || ((buf >= 24) && (buf <= 27)))
				{end = TRUE; break;}
			else if (buf != '\n')
				_StdinBuf += CString(buf);
		}
	}
	if (!(h == INVALID_HANDLE_VALUE)) CloseHandle(h);
	if (end) return NULL;
	return _StdinBuf;
}

BOOL CMainFrame::Mutex(LPCTSTR key, UINT scope, LPCTSTR cm)
{
	CString g(""); g.LoadString(IDS_GUID);
	CString cmd(cm);
	CString n = ExclusionName(ExeName + CString('-') + g, scope);
	CString exname = n + CString('-') + key;
	exname.Replace(' ', '_');
	_uwmAreYouMe = RegisterWindowMessage(exname);
	if (_uwmAreYouMe == 0) {WriteError(CString("Mutex: Invalid Exclusion Name")); return TRUE;}
    HANDLE hMutex = ::CreateMutex( NULL, FALSE, exname);
	DWORD err = ::GetLastError();
    // The call fails with ERROR_ACCESS_DENIED if the Mutex was 
    // created in a different users session because of passing
    // NULL for the SECURITY_ATTRIBUTES on Mutex creation).
    if ((err == ERROR_ALREADY_EXISTS) || (err == ERROR_ACCESS_DENIED))
	{
		HWND hMine = this->m_hWnd;
	    HWND hOther = hMine;
		EnumWindows(searcher, (LPARAM)&hOther);
        if ((hOther != hMine) && (cmd.GetLength() > 0))
        {
			//NB: This path is never taken in debug because the frame window
			//is owned by VS and is not seen by EnumWindows.
			CString c = CString("\"-\" ") + cmd;
			COPYDATASTRUCT cp;
			cp.dwData = 123;
			cp.cbData = c.GetLength() * sizeof(TCHAR);
			cp.lpData = c.LockBuffer();
			SendMessage(hOther, WM_COPYDATA, (WPARAM)this->m_hWnd, (LPARAM)(LPVOID)&cp);
			c.UnlockBuffer();
        }
		return FALSE;
    }
	return TRUE;
}

BOOL CMainFrame::SetTaskIcon(HICON icon, bool lmenu, bool rmenu)
{
	BOOL r = IsIconVisible();
	if (icon == INVALID_HANDLE_VALUE)
	{
		if (r)
		{
			RemoveNotifyIcon();
			m_Lmenu = false;
			m_Rmenu = false;
			DecUI();
		}
	}
	else 
	{
		if (!r)
		{
			InstallNotifyIcon(AppName, icon, IDR_NOTIFY);
			IncUI();
		}
		ChangeNotifyIcon(icon);
		m_Lmenu = lmenu;
		m_Rmenu = rmenu;
	}
	return TRUE;
}

HICON CMainFrame::LoadIcon(LPCTSTR nm, int cx, int cy)
{
	HICON h = (HICON)LoadImage(_Module.GetResourceInstance(), nm, IMAGE_ICON, cx, cy, LR_DEFAULTCOLOR);
	if (h == NULL)
		return NULL;
	else
		return h;
}

CCMenu* CMainFrame::Menu(int from)
{
	switch(from)
	{
	case 1:
		return (CCMenu*) m_paras->GetClient();
	default:
		return (CCMenu*) this;
	}
}
	
void CMainFrame::Balloon(LPCTSTR mtt, LPCTSTR ms, UINT to, DWORD ic)
{
	CString mt(mtt);
	if (mt.GetLength() == 0)
	{
		HideNotifyBalloonTip();
	}
	else
	{
		ShowNotifyBalloonTip(mt, ms, to, ic);
	}
}

int CMainFrame::LoadScriptFile(LPCTSTR fn)
{
	return luaX_loadfile(*m_lua, fn, ExePath);
}

int CMainFrame::LoadScriptResource(LPCTSTR fn)
{
	return luaX_loadresource(*m_lua, fn);
}

int CMainFrame::ExecChunk(int parms, LPCTSTR context)
{
	lua_State* L = *m_lua;
	int r = 1;
	CString em = CString("");
	CString ct = CString(context);
	if (ct.GetLength() > 0) em = ct + CString("::");
	IncUI();
	int top = lua_gettop(L) - (1 + parms);
	SetRunning();
	if (lua_isstring(L, top + 1))
	{
		WriteError(em + CString(lua_tostring(L, top + 1)));
		r = 0;
	}
	else
	{
		if (lua_pcall(L, parms, LUA_MULTRET, 0) != 0)
		{
			WriteError(em + CString(lua_tostring(L, -1)));
			r = 0;
		}
		else
		{
			int nr = lua_gettop(L) - top;
			if (nr == 0)
			{
				r = 0;
				WriteOK();
			}
			else if (lua_type(L, top+1) == LUA_TSTRING)
			{
				r = 1;
				WriteError(em + CString(lua_tostring(L, top+1)));
			}
			else if (lua_type(L, top+1) == LUA_TNUMBER)
			{
				r = lua_tointeger(L, top+1);
				WriteOK();
			}
			else
			{
				r = (lua_toboolean(L, top+1))? 0 : 1;
				if (r != 0) WriteError(em + CString("Unspecified Error"));
			}
		}
	}
	lua_settop(L, top);
	DecUI();
	ReportMem();
	return r;
}
	
int CMainFrame::GetInventory(int type)
{
	return m_lua->GetInventory(type);
}

#pragma endregion
