// WriteFrm.h : interface of the CWriteFrame class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "JHCEdit.h"
#include "JHCCMenu.h"
#include "LuaLibIF.h"
#include "JHCGUIThread.h"
#include "JHCDropFiles.h"

#define WF_WRITE 1
#define WF_TITLE 2
#define WF_CLEAR 3
#define WF_CLOSE 4
#define WF_MOVE  5
#define WF_ICON  6
#define WF_DELETE 7
#define WF_RESET 8
#define WF_PROGRESS 9
#define WF_PAUSED 10
#define WF_DONE 11
#define WF_ERROR 12
#define WF_OVERLAY 13
#define WF_MINIMIZE 14
#define WF_CONTEXT 15
#define WF_ACCEPTDROP 16

class CWriteFrame;

class CWriteFrameProxy : public CGUIThreadProxy<CWriteFrame, CWriteFrameProxy>
{
public:
	CRect rect;
	HICON iconl;
	HICON icons;
	bool visible;
	CString string_data;
	UINT number_data;

	CWriteFrameProxy(CWindow* h, bool threaded) {Init(h, threaded);}
};

class CWriteFrame : public CFrameWindowImpl<CWriteFrame>, public CUpdateUI<CWriteFrame>,
	public CCMenu, public CDropFiles<CWriteFrame>
{
protected:
	JHCEdit m_wndEdit;
	CWriteFrameProxy* m_proxy;
	DWORD m_W7tb;
	bool m_w7button;
	UINT m_progress;
	UINT m_charwidth;
	UINT m_progresschars;
	CString m_title;
	CRect m_rect;
	bool m_parked;
	bool m_lock;
	bool m_chevrons;
	bool m_bContextMenu;
	UINT m_state;

public:
	DECLARE_FRAME_WND_CLASS(NULL, IDR_WRITEFRAME)

	CWriteFrame(CWriteFrameProxy* wp)
	{
		m_proxy = wp;
		if(CreateEx(0, m_proxy->rect, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX, 0) == NULL)
		{
			ATLTRACE(_T("Write window creation failed!\n"));
			return;
		}
	}

	BEGIN_UPDATE_UI_MAP(CWriteFrame)
	END_UPDATE_UI_MAP()

	BEGIN_MSG_MAP(CWriteFrame)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(WM_ACTIVATE, OnActivate)
		MESSAGE_HANDLER(WM_CLOSE, OnClose)
		MESSAGE_HANDLER(WM_CTLCOLORSTATIC, OnColor)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		MESSAGE_HANDLER(WM_MOVE, OnMove)
		MESSAGE_HANDLER(WM_USER, OnUser)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		MESSAGE_HANDLER(WM_CONTEXTMENU, OnContextMenu)
		MESSAGE_RANGE_HANDLER(0xC000, 0xFFFF, OnRegisteredMessage)
		COMMAND_ID_HANDLER(ID_EDIT_COPY, OnEditCopy)
		CHAIN_MSG_MAP(CUpdateUI<CWriteFrame>)
		CHAIN_MSG_MAP(CFrameWindowImpl<CWriteFrame>)
		CHAIN_MSG_MAP(CCMenu)
		CHAIN_MSG_MAP(CDropFiles<CWriteFrame>)
	END_MSG_MAP()

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)


	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		m_w7button = false;
		m_progresschars = 0;
		m_W7tb = 0;
		m_parked = false;
		m_lock = false;
		m_chevrons = false;
		m_progress = 0;
		m_state = WF_RESET;

		// If Windows 7, allow the taskbar messages through even if process "run as administrator":
		typedef BOOL WINAPI cwmfe (HWND,UINT,DWORD action,PCHANGEFILTERSTRUCT);
		FARPROC proc = GetProcAddress(GetModuleHandle(TEXT("User32.dll")), "ChangeWindowMessageFilterEx");
		if (proc != NULL)
		{
			m_W7tb = RegisterWindowMessage(L"TaskbarButtonCreated");
			((cwmfe*)proc)(this->m_hWnd, m_W7tb, MSGFLT_ALLOW, NULL);
		}

		CString title("Report Window");
		SetWindowText(title);

		// create the edit control
		DWORD dwEditStyle = WS_CHILD | WS_VISIBLE | WS_VSCROLL |WS_HSCROLL
			| ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_READONLY;
		m_wndEdit.Create(*this, rcDefault, NULL, dwEditStyle);
		m_wndEdit.SetFont((HFONT)::GetStockObject(SYSTEM_FIXED_FONT));
		m_wndEdit.SetMargins(10, 10);
		m_wndEdit.SetTabStops(16);

		m_hWndClient = m_wndEdit;
		UpdateLayout();

		m_charwidth = m_wndEdit.GetCharsInLine();

		return 0;
	}

	LRESULT OnClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
	{
		m_proxy->EnterCS();
		m_proxy->visible = false;
		ShowWindow(SW_HIDE);
		m_proxy->LeaveCS();
		m_proxy->PostMessageUp(WM_COMMAND, ID_CLOSE_REPORT, 0);
		return 1;
	}

	LRESULT OnMove(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
	{
		if (m_lock) return 0;
		if (IsIconic()) return 0;
		if (!m_parked)
		{
			m_proxy->EnterCS();
			GetWindowRect(m_proxy->rect);
			m_proxy->LeaveCS();
			GetWindowRect(m_rect);
			m_charwidth = m_wndEdit.GetCharsInLine();
		}
		return 0;
	}

	LRESULT OnSize(UINT, WPARAM wp, LPARAM, BOOL& bHandled)
	{
		if (m_lock) return 0;
		switch (wp)
		{
		case SIZE_MINIMIZED:
			Minimize();
			bHandled = TRUE;
			return 1;
		case SIZE_RESTORED:
			if (m_parked)
			{
				m_lock = true;
				this->SetWindowLongPtrW(GWL_EXSTYLE, 0);
				MoveWindow(m_rect);
				m_parked = false;
				m_lock = false;
			}
			else
			{
				m_proxy->EnterCS();
				GetWindowRect(m_proxy->rect);
				m_proxy->LeaveCS();
				GetWindowRect(m_rect);
				m_charwidth = m_wndEdit.GetCharsInLine();
				InitW7();
			}
			break;
		}
		bHandled = false;
		return 0;
	}

	LRESULT OnActivate(UINT, WPARAM wp, LPARAM lp, BOOL& bHandled)
	{
		if (m_lock) return 0;
		if ((wp == WA_ACTIVE) && (m_parked))
		{
			m_lock = true;
			this->SetWindowLongPtrW(GWL_EXSTYLE, 0);
			MoveWindow(m_rect);
			m_parked = false;
			m_lock = false;
		}
		bHandled = FALSE;
		return 0;
	}

	void Minimize()
	{
		if (m_parked) return;
		if (!IsWindowVisible())
		{
			m_proxy->EnterCS();
			m_proxy->visible = true;
			m_proxy->LeaveCS();
			ShowWindow(SW_NORMAL);
		}
		if (m_w7button)
		{
			m_lock = true;
			ShowWindow(SW_RESTORE);
			MoveWindow(INT_MIN, INT_MIN, m_rect.Width(), m_rect.Height());
			this->SetWindowLongPtrW(GWL_EXSTYLE, WS_EX_NOACTIVATE | WS_EX_APPWINDOW);
			m_parked = true;
			m_lock = false;
		}
		else
		{
			ShowWindow(SW_MINIMIZE);
		}
	}

	LRESULT OnColor(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled)
	{
		if ((HWND)lParam == m_wndEdit.m_hWnd)
		{
			bHandled = TRUE;
			return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
		}
		bHandled = FALSE;
		return 0;
	}

	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
	{
		bHandled = FALSE;
		return 1;
	}

	LRESULT OnUser(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
	{
		m_proxy->EnterCS();
		switch (wParam)
		{
		case WF_WRITE:
			m_proxy->visible = true;
			ShowWindow(SW_NORMAL);
			SetProgress(WF_WRITE, m_progress);
			m_wndEdit.AppendText(m_proxy->string_data);
			SetFocus();
			break;
		case WF_TITLE:
			m_title = m_proxy->string_data;
			SetWindowText(m_title);
			break;
		case WF_CLEAR:
			m_wndEdit.SetSelAll(1);
			m_wndEdit.ReplaceSel(TEXT(""), 0);
			break;
		case WF_CLOSE:
			if (IsWindowVisible())
			{
				m_proxy->visible = false;
				ShowWindow(SW_HIDE);
				m_proxy->PostMessageUp(WM_COMMAND, ID_CLOSE_REPORT, 0);
			}
			break;
		case WF_MOVE:
			MoveWindow(m_proxy->rect);
			break;
		case WF_ICON:
			SetIcon(m_proxy->iconl, TRUE);
			SetIcon(m_proxy->icons, FALSE);
			break;
		case WF_DELETE:
			m_wndEdit.DeleteLine(-2);
			break;
		case WF_PROGRESS:
			if (m_proxy->Threaded())
			{
				m_progress = m_proxy->number_data;
				SetProgress(wParam, m_progress);
			}
			break;
		case WF_RESET:
			if (m_proxy->Threaded()) SetProgress(wParam, 0);
			break;
		case WF_OVERLAY:
			SetOverlay(m_proxy->icons, m_proxy->string_data);
			break;
		case WF_MINIMIZE:
			Minimize();
			break;
		case WF_CONTEXT:
			if (!m_proxy->Threaded()) m_bContextMenu = (m_proxy->number_data != 0);
			break;
		case WF_ACCEPTDROP:
			AcceptDropFiles(m_proxy->number_data != 0);
			break;
		default:
			if (m_proxy->Threaded()) SetProgress(wParam, m_progress);
			break;
		}
		m_proxy->LeaveCS();
		m_proxy->SignalDone();
		return 0;
	}

	LRESULT OnRegisteredMessage(UINT uMsg, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
	{
		if (uMsg != m_W7tb) {bHandled = FALSE; return 0;}
		m_w7button = true;
		InitW7();
		SetW7Progress();
		bHandled = TRUE;
		return 1;
	}

	LRESULT OnTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		m_wndEdit.AppendText(CString(">"));
		if (++m_progresschars > m_charwidth)
		{
			m_wndEdit.DeleteLine(-1);
			m_progresschars = 0;
		}
		return 0;
	}

	void SetChevrons(UINT val)
	{
		double x = (double)m_charwidth / 100.0;
		UINT p = (UINT)(x * val);
		if (p < m_progresschars)
		{
			m_wndEdit.DeleteLine(-1);
			m_progresschars = 0;
		}
		for (; (m_progresschars < p); m_progresschars++) m_wndEdit.AppendText(CString(">"));
	}

	void SetProgress(UINT state, UINT val)
	{
		CString tx;

		KillTimer(1);
		if ((m_progresschars > 0) && (state != WF_PAUSED) && (state != WF_PROGRESS))
		{
			m_wndEdit.DeleteLine(-1);
			m_progresschars = 0;
		}
		if (state == WF_PROGRESS) m_chevrons = true;
		if (state == WF_WRITE)
		{
			m_chevrons = false;
			m_progresschars = 0;
			state = m_state;
		}
		switch (state)
		{
		case WF_ERROR:
			SetWindowText(m_title + CString(" [ERROR]"));
			m_state = WF_ERROR;
			break;
		case WF_PAUSED:
			SetWindowText(m_title + CString(" [PAUSED]"));
			m_state = WF_PAUSED;
			break;
		case WF_DONE:
			SetWindowText(m_title + CString(" [DONE]"));
			m_state = WF_DONE;
			break;
		case WF_PROGRESS:
			if (val > 100)
			{
				if (m_chevrons) SetTimer(1, 1000, NULL);
				SetWindowText(m_title + CString(" [RUNNING]"));
				m_state = WF_PROGRESS;
			}
			else
			{
				tx.Format(CString(" [%u%%]"), val);
				SetWindowText(m_title + tx);
				if (m_chevrons) SetChevrons(val);
				m_state = WF_PROGRESS;
			}
			break;
		default:
			SetWindowText(m_title);
			m_state = WF_RESET;
			break;
		}
		SetW7Progress();
	}

	void SetW7Progress()
	{
		if (!m_w7button) return;
		ITaskbarList3* ptl;
		CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_ALL, IID_ITaskbarList3, (void**)&ptl);
		if (ptl == NULL) return;

		switch (m_state)
		{
		case WF_ERROR:
			ptl->SetProgressState(m_hWnd, TBPF_ERROR);
			ptl->SetProgressValue(m_hWnd, 100, 100);
			break;
		case WF_PAUSED:
			ptl->SetProgressState(m_hWnd, TBPF_PAUSED);
			ptl->SetProgressValue(m_hWnd, m_progress, 100);
			break;
		case WF_DONE:
			ptl->SetProgressState(m_hWnd, TBPF_NORMAL);
			ptl->SetProgressValue(m_hWnd, 100, 100);
			break;
		case WF_PROGRESS:
			if (m_progress > 100)
			{
				ptl->SetProgressState(m_hWnd, TBPF_INDETERMINATE);
			}
			else
			{
				ptl->SetProgressState(m_hWnd, TBPF_NORMAL);
				ptl->SetProgressValue(m_hWnd, m_progress, 100);
			}
			break;
		default:
			ptl->SetProgressState(m_hWnd, TBPF_NOPROGRESS);
			break;
		}
	}

	void SetOverlay(HICON icon, CString text)
	{
		if (!m_w7button) return;
		ITaskbarList3* ptl;
		CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_ALL, IID_ITaskbarList3, (void**)&ptl);
		if (ptl == NULL) return;
		ptl->SetOverlayIcon(m_hWnd, icon, text);
	}

	void InitW7()
	{
		if (!m_w7button) return;
		ITaskbarList3* ptl;
		CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_ALL, IID_ITaskbarList3, (void**)&ptl);
		if (ptl == NULL) return;
		CRect rect;
		m_wndEdit.GetClientRect(&rect);
		ptl->SetThumbnailClip(m_hWnd, rect);
	}

	LRESULT OnEditCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		m_wndEdit.Copy();
		return 0;
	}

	LRESULT OnContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
	{
		CMenuHandle menu((HMENU)wParam);
		return 0;
	}

	virtual bool OnFileDrop(UINT n)
	{
		return false;
	}
};
