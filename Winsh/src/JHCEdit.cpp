#include "stdafx.h"
#include "resource.h"
#include "MainFrm.h"

int JHCEdit::GetLine(CString& str, int line)
{
	int n, lc, i;
	lc = GetLineCount();
	if (line < 0) i = lc + line + 1; else i = line;
	if ((i < 0) || (i > lc)) return 0;
	i--; if (i < 0) i = LineFromChar();
	n = LineLength(LineIndex(i));
	CEdit::GetLine(i, str.GetBuffer(n+1), n); str.ReleaseBuffer(n);
	return n;
}

void JHCEdit::SetSelStartLine(int line)
{
	int lc, i;
	lc = GetLineCount();
	if (line < 0) i = lc + line + 1; else i = line;
	if ((i < 0) || (i > lc)) return;
	i--; if (i < 0) i = LineFromChar();
	i = LineIndex(i);
	SetSel(i, i);
}

int JHCEdit::SetSelFullLines(void)
{
	int s, e, sl, el;
	GetSel(s, e);
	if (e < 0) return 0;
	sl = LineFromChar(s); el = LineFromChar(e);
	s = LineIndex(sl);
	if (e > LineIndex(el))
		e = LineIndex(el) + LineLength(e);
	else
		e = LineIndex(el) + LineLength(LineIndex(el));
	SetSel(s, e);
	return (el - sl + 1);
}

void JHCEdit::ReplaceLine(CString& nl, int line)
{
	int n, lc, i;
	lc = GetLineCount();
	if (line < 0) i = lc + line + 1; else i = line;
	if ((i < 0) || (i > lc)) return;
	i--; if (i < 0) i = LineFromChar();
	i = LineIndex(i);
	n = LineLength(i);
	SetSel(i, i + n);
	ReplaceSel(nl, TRUE);
	n = LineLength(i);
	SetSel(i + n, i + n);
}

void JHCEdit::DeleteLine(int line)
{
	int n, lc, i;
	lc = GetLineCount();
	if (line < 0) i = lc + line + 1; else i = line;
	if ((i < 0) || (i > lc)) return;
	i--; if (i < 0) i = LineFromChar();
	i = LineIndex(i);
	n = LineLength(i);
	SetSel(i, i + n + 1);
	ReplaceSel(CString(""), TRUE);
	n = LineLength(i);
	SetSel(i, i);
}

int JHCEdit::DeleteChar(int count)
{
	if (count == 0) return 0;
	int s, e, ls, le;
	GetSel(s, e);
	ls = LineIndex(LineFromChar(s));
	le = ls + LineLength(ls);
	if (count > 0)
	{
		e = s + count;
		if (e > le) e = le;
	}
	else
	{
		e = s;
		s = s + count;
		if (s < ls) s = ls;
	}
	SetSel(s, e);
	Clear();
	return e - s + 1;
}

int JHCEdit::GetString(CString& str, int start, int end)
{
	int s, e, ls, le, n;
	CString ss;
	if (start < 0) GetSel(s, e); else {s = start; e = end;}
	if (e < s) e = s;
	ls = LineFromChar(s);
	le = LineFromChar(e);
	str = CString("");
	for (int i = ls; (i <= le); i++)
	{
		n = LineLength(LineIndex(i));
		CEdit::GetLine(i, ss.GetBuffer(n+1), n); ss.ReleaseBuffer(n);
		if (i == ls) ss = ss.Mid(s - LineIndex(i)); else str += CString("\r\n");
		if (i == le)
		{
			n = e - (s + str.GetLength());
			ss = ss.Left(n);
		}
		str += ss;
	}
	return str.GetLength();
}

int JHCEdit::GetCharsInLine(void)
{
	CRect r;
	LOGFONT lf;
	CFont f(GetFont());
	f.GetLogFont(&lf);
	GetRect(&r);
	return (r.Width() / lf.lfWidth) - 1;
}

LRESULT JHCEdit::OnContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	UINT cmd;
	CPoint mousePt(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
	DWORD sel = GetSel();
	this->SetFocus();
	SetCCMenuGutter(FALSE);
	conMenu.CreatePopupMenu();
	if (this->GetStyle() & ES_READONLY)
	{
		conMenu.AppendMenu(((LOWORD(sel) == HIWORD(sel))? MF_GRAYED : MF_ENABLED), WM_COPY, _T("Copy"));
	}
	else
	{
		conMenu.AppendMenu(((LOWORD(sel) == HIWORD(sel))? MF_GRAYED : MF_ENABLED), WM_CUT, _T("Cut"));
		conMenu.AppendMenu(((LOWORD(sel) == HIWORD(sel))? MF_GRAYED : MF_ENABLED), WM_COPY, _T("Copy"));
		conMenu.AppendMenu(((::IsClipboardFormatAvailable(CF_TEXT) || ::IsClipboardFormatAvailable(CF_UNICODETEXT))?
								MF_ENABLED : MF_GRAYED), WM_PASTE, _T("Paste"));
		conMenu.AppendMenu(((LOWORD(sel) == HIWORD(sel))? MF_GRAYED : MF_ENABLED), WM_CLEAR, _T("Delete"));
	}
	this->SendMessage((HWND)this->GetTopLevelParent(), WM_CONTEXTMENU, (WPARAM)conMenu.m_hMenu, (LPARAM)sel);
	cmd = conMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_RETURNCMD, mousePt.x, mousePt.y, m_hWnd);
	conMenu.DestroyMenu();
	switch (cmd)
	{
	case WM_CUT:
	case WM_COPY:
	case WM_PASTE:
	case WM_CLEAR:
		this->SendMessage(cmd, 0, -1);
		break;
	default:
		this->SendMessage((HWND)this->GetTopLevelParent(), WM_COMMAND, (WPARAM)cmd, 0);
		break;
	}
	return 0;
}

HBRUSH JHCEdit::OnCtlColorEdit(CDCHandle dc, CEdit edit)
{
	dc.SetTextColor(colorTx);
	dc.SetBkColor(colorBg);
	dc.SetDCBrushColor(colorBg);
	return HBRUSH(GetStockObject(DC_BRUSH));
}

LRESULT JHCEdit::OnDoubleClick(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	SetSelFullLines();
	return 0;
}
