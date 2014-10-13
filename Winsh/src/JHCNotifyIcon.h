// Implementation of the CNotifyIconData class and the CNotifyIconImpl template.
// 12 December 2008: Changed Install and Remove methods so they can be safely called with nothing to do.
// 17 January 2011: Added support for TaskbarCreated message to re-install taskbar icon if Explorer.exe is restarted in Windows.
// 26 November 2012: Removed double-click handling and default both left and right to show context menu with prep menu key.

#pragma once

//#include <atlmisc.h>

// Wrapper class for the Win32 NOTIFYICONDATA structure
class CNotifyIconData : public NOTIFYICONDATA
{
public:	
	CNotifyIconData()
	{
		memset(this, 0, sizeof(NOTIFYICONDATA));

		// There are three major variants of this structure: Prior to V5; V5 and V6. This code
		// supports compatable binaries by behaving appropriate to the lesser of the version
		// compiled for (using symbol _WIN32_IE) and actually available at run time. To determine
		// which version is in use, compare the cbSize field with the symbols NID_V4_SIZE or
		// NID_V5_SIZE (i.e. if (nid.cbSize > NID_V4_SIZE) // V5 features are available).

		// Determine the version actually available at runtime:
		DLLVERSIONINFO dvi;
		ZeroMemory(&dvi, sizeof(dvi));
		dvi.cbSize = sizeof(dvi);
		HINSTANCE hinstDll;
		hinstDll = LoadLibrary(TEXT("comctl32.dll"));
		if(hinstDll)
		{
			DLLGETVERSIONPROC pDllGetVersion;
			pDllGetVersion = (DLLGETVERSIONPROC)GetProcAddress(hinstDll, "DllGetVersion");
			if(pDllGetVersion) (*pDllGetVersion)(&dvi);
			FreeLibrary(hinstDll);
		}

		// Set the appropriate size and determination symbols:
		cbSize = sizeof(NOTIFYICONDATA);
#if (_WIN32_IE >= 0x0600)
		if (dvi.dwMajorVersion >= 6) cbSize = NOTIFYICONDATA_V2_SIZE;
	#define NID_V4_SIZE ((DWORD)(NOTIFYICONDATA_V1_SIZE))
	#define NID_V5_SIZE ((DWORD)(NOTIFYICONDATA_V2_SIZE))
#elif (_WIN32_IE >= 0x0500)
		if (dvi.dwMajorVersion < 5) cbSize = NOTIFYICONDATA_V1_SIZE;
	#define NID_V4_SIZE ((DWORD)(NOTIFYICONDATA_V1_SIZE))
	#define NID_V5_SIZE ((DWORD)(NOTIFYICONDATA_V1_SIZE))
#else
	#define NID_V4_SIZE ((DWORD)0)
	#define NID_V5_SIZE ((DWOED)0)
#endif
	}
};

// Template used to support adding an icon to the taskbar.
// This class will maintain a taskbar icon and associated context menu.
template <class T> class CNotifyIconImpl
{
private:
	UINT WM_TRAYICON;
	UINT WM_TASKBARCREATED;
	CNotifyIconData m_nid;
	bool m_bInstalled;

protected:
	bool m_notifymenu; // Use in WM_ONMENUPOPUP in host window to discriminate between
	                   // main menu and taskbar menu.
public:	
	CNotifyIconImpl() : m_bInstalled(false)
	{
		m_notifymenu = false;
		WM_TRAYICON = ::RegisterWindowMessage(_T("WM_TRAYICON"));
		WM_TASKBARCREATED = RegisterWindowMessage(TEXT("TaskbarCreated"));
	}
	
	~CNotifyIconImpl()
	{
		RemoveNotifyIcon();
	}

	// Install a taskbar icon
	// 	lpszToolTip 	- The tooltip to display
	//	hIcon 		- The icon to display
	// 	nID		- The resource ID of the context menu
	// returns true on success (or if the icon is already installed)
	bool InstallNotifyIcon(LPCTSTR lpszToolTip, HICON hIcon, UINT nID)
	{
		if (m_bInstalled) return true;
		T* pT = static_cast<T*>(this);
		m_nid.hWnd = pT->m_hWnd;
		m_nid.uID = nID;
		m_nid.hIcon = hIcon;
		m_nid.uCallbackMessage = WM_TRAYICON;
		_tcscpy_s(m_nid.szTip, (m_nid.cbSize > NID_V4_SIZE)? 128: 64, lpszToolTip);
		m_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
		m_bInstalled = Shell_NotifyIcon(NIM_ADD, &m_nid)? true : false;

#if (_WIN32_IE >= 0x0500)
		if (m_nid.cbSize > NID_V4_SIZE)
		{
			m_nid.uVersion = NOTIFYICON_VERSION;
			m_nid.uFlags = 0;
			Shell_NotifyIcon(NIM_SETVERSION, &m_nid);
		}
		// If Windows 7, remove the security filter on WM_TASKBARCREATED:
		FARPROC proc = GetProcAddress(GetModuleHandle(TEXT("User32.dll")), "ChangeWindowMessageFilterEx");
		typedef BOOL WINAPI cwmfx(HWND,UINT,DWORD,PCHANGEFILTERSTRUCT);
		if ((proc != NULL) && (WM_TASKBARCREATED > 0))
		{
			((cwmfx*)proc)((static_cast<T*>(this))->m_hWnd, WM_TASKBARCREATED, MSGFLT_ALLOW, NULL);
		}
#endif
		return m_bInstalled;
	}

	// Remove taskbar icon
	// returns true on success (or if the icon is not installed)
	bool RemoveNotifyIcon()
	{
		if (!m_bInstalled) return true;
		m_nid.uFlags = 0;
		m_bInstalled = Shell_NotifyIcon(NIM_DELETE, &m_nid)? false : true;
		return !m_bInstalled;
	}

	bool IsIconVisible()
	{
		return m_bInstalled;
	}

	// Changes the icon tooltip text
	// returns true on success
	bool ChangeNotifyTooltipText(LPCTSTR pszTooltipText)
	{
		if (!m_bInstalled) return false;
		if (pszTooltipText == NULL) return false;
		m_nid.uFlags = NIF_TIP;
		_tcscpy_s(m_nid.szTip, (m_nid.cbSize > NID_V4_SIZE)? 128: 64, pszTooltipText);
		return Shell_NotifyIcon(NIM_MODIFY, &m_nid) ? true : false;
	}

	// Shows a balloon tip above the Icon.
	// pszInfoTitle: Short string to show in bold at the top of the balloon.
	// pszInfo: Longer text (up to 255 characters) to show in the body of the balloon.
	// uTimeout: Maximum time in milliseconds to show the balloon for (10000 - 30000).
	// dwInfoFlags: NIIF_ERROR, NIIF_INFO, NIIF_NONE, NIIF_USER, NIIF_WARNING (icons) or NIIF_NOSOUND.
	bool ShowNotifyBalloonTip(LPCTSTR pszInfoTitle, LPCTSTR pszInfo, UINT uTimeout, DWORD dwInfoFlags)
	{
		if (!m_bInstalled) return false;
#if (_WIN32_IE >= 0x0500)
		if (m_nid.cbSize > NID_V4_SIZE)
		{
			_tcscpy_s(m_nid.szInfoTitle, 63, pszInfoTitle);
			_tcscpy_s(m_nid.szInfo, 255, pszInfo);
			m_nid.uTimeout = uTimeout;
			m_nid.dwInfoFlags = dwInfoFlags;
			m_nid.uFlags = NIF_INFO;
			return Shell_NotifyIcon(NIM_MODIFY, &m_nid) ? true : false;
		}
#endif
		return false;
	}

	// Remove the balloon tip before it is timed out or removed by the user.
	bool HideNotifyBalloonTip()
	{
		if (!m_bInstalled) return false;
#if (_WIN32_IE >= 0x0500)
		if (m_nid.cbSize > NID_V4_SIZE)
		{
			_tcscpy_s(m_nid.szInfo, 255, _T(""));
			m_nid.uTimeout = 0;
			m_nid.dwInfoFlags = 0;
			m_nid.uFlags = NIF_INFO;
			return Shell_NotifyIcon(NIM_MODIFY, &m_nid) ? true : false;
		}
#endif
		return false;
	}

	// Changes the icon visual representation
	// returns true on success
	bool ChangeNotifyIcon(HICON hIcon)
	{
		if (!m_bInstalled) return false;
		m_nid.hIcon = hIcon;
		m_nid.uFlags = NIF_ICON;
		return Shell_NotifyIcon(NIM_MODIFY, &m_nid) ? true : false;
	}

	BEGIN_MSG_MAP(CNotifyIcon)
		MESSAGE_HANDLER(WM_TRAYICON, OnNotifyIcon)
		MESSAGE_HANDLER(WM_TASKBARCREATED, OnTaskbarCreated)
	END_MSG_MAP()

	LRESULT OnTaskbarCreated(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
	{
		if (m_bInstalled)
		{
			m_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
			m_bInstalled = Shell_NotifyIcon(NIM_ADD, &m_nid)? true : false;

#if (_WIN32_IE >= 0x0500)
			if (m_nid.cbSize > NID_V4_SIZE)
			{
				m_nid.uVersion = NOTIFYICON_VERSION;
				m_nid.uFlags = 0;
				Shell_NotifyIcon(NIM_SETVERSION, &m_nid);
			}
#endif
		}
		return 0;
	}

	LRESULT OnNotifyIcon(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
	{
		// Is this the ID we want?
		if (wParam != m_nid.uID) return 0;

		T* pT = static_cast<T*>(this);

		switch (LOWORD(lParam))
		{
		case WM_RBUTTONUP:
			pT->OnNotifyRightclick();
			break;

		case WM_LBUTTONUP:
			pT->OnNotifyLeftclick();
			break;

		default:
			break;
		}
		return 0;
	}

	// Shows and transacts the menu - this is the default action for click.
	void ShowNotifyMenu(int k)
	{
		T* pT = static_cast<T*>(this);

		// Load the menu
		CMenu oMenu;
		if (oMenu.LoadMenu(m_nid.uID))
		{
			// Set main menu lockout flag:
			m_notifymenu = true;
			// Get the sub-menu
			CMenuHandle oPopup(oMenu.GetSubMenu(0));
			// Prepare
			pT->PrepareNotifyMenu(oPopup,k);
			// Get the menu position
			CPoint pos; GetCursorPos(&pos);
			// Make app the foreground
			SetForegroundWindow(pT->m_hWnd);
			// Track
			oPopup.TrackPopupMenu(TPM_LEFTALIGN, pos.x, pos.y, pT->m_hWnd);
			// BUGFIX: See "PRB: Menus for Notification Icons Don't Work Correctly"
			pT->PostMessage(WM_NULL);
			// Done
			oMenu.DestroyMenu();
			// Reset main menu lockout flag:
			m_notifymenu = false;
		}
	}

	// Does the command corresponding to the default item (if any) on the menu.
	void DoNotifyMenuDefault()
	{
		T* pT = static_cast<T*>(this);

		SetForegroundWindow(pT->m_hWnd);
		CMenu oMenu;
		if (oMenu.LoadMenu(m_nid.uID))
		{
			CMenuHandle oPopup(oMenu.GetSubMenu(0));
			pT->PrepareNotifyMenu(oPopup);
			UINT di = oMenu.GetMenuDefaultItem(FALSE, GMDI_GOINTOPOPUPS);
			if (di > 0)	pT->SendMessage(WM_COMMAND, di, 0);
			oMenu.DestroyMenu();
		}
	}

	// Override to allow the menu items to be enabled/checked/etc.
	virtual void PrepareNotifyMenu(CMenuHandle m, int k) {}

	// Override to act on right-clicking the icon, default shows the menu.
	virtual void OnNotifyRightclick()
	{
		ShowNotifyMenu(1);
	}

	// Override to act on clicking the icon, default shows the menu.
	virtual void OnNotifyLeftclick()
	{
		ShowNotifyMenu(2);
	}

};
