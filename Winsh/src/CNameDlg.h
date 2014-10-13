// CNameDlg.h : Dialog box to enter object name.
//
/////////////////////////////////////////////////////////////////////////////

#pragma once
#include "..\resource.h"

class CNameDlg : public CDialogImpl<CNameDlg>
{
private:
	CString& m_tx;
	CString& m_wx;
	CContainedWindowT<CEdit> m_edit;

public:
    enum { IDD = IDD_DIALOG_NAME };

	CNameDlg(CString& wx, CString& tx) : m_wx(wx), m_tx(tx), m_edit(this, 1) {};

	BEGIN_MSG_MAP(CNameDlg)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        COMMAND_ID_HANDLER(IDC_BUTTON_T_OK, OnOK)
        COMMAND_ID_HANDLER(IDC_BUTTON_T_CAN, OnCancel)
	ALT_MSG_MAP(1)
		MESSAGE_HANDLER(WM_CHAR, OnChar)
	END_MSG_MAP()

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		SetWindowText(m_wx);
		m_edit.SubclassWindow(GetDlgItem(IDC_EDIT_T));
		m_edit.SetWindowText(m_tx);
		return 0;
	}

	LRESULT OnOK(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		int sz = m_edit.GetWindowTextLength() + 1;
		m_edit.GetWindowText(m_tx.GetBufferSetLength(sz), sz);
		m_tx.ReleaseBuffer(sz - 1);
		EndDialog(1);
		return 0;
	}

	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		EndDialog(0);
		return 0;
	}

	LRESULT OnChar(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		if (!iswcntrl(wParam))
			if ((PathGetCharType((TCHAR)wParam) & GCT_LFNCHAR) == 0) return 0;
		return m_edit.DefWindowProcW(uMsg, wParam, lParam);
	}
};