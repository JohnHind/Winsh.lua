// JHCEdit.h : added features for the WTL CEdit class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "resource.h"
#include "JHCCMenu.h"
#include "atlcrack.h"

class JHCEdit : public CWindowImpl<JHCEdit, CEdit>, public CCMenu/*<JHCEdit>*/
{
public:

	// Sets the background and text color for the control.
	void SetColors(COLORREF background, COLORREF text)
	{
		colorBg = background; colorTx = text;
	}

	// Return the number of characters per line in the current window width.
	// (Approx for proportional fonts).
	int GetCharsInLine(void);

	// Return the number of characters in the current selection, 0 if no selection.
	int GetSelLength(void)
	{
		int s, e;
		GetSel(s, e);
		return (e - s);
	}

	// Return the line number containing the start of the selection (or the cursor).
	int GetSelStartLine(void)
	{
		int s, e;
		GetSel(s, e);
		return(LineFromChar(s) + 1);
	}


	// Return the line number containing the end of the selection (or the cursor).
	int GetSelEndLine(void)
	{
		int s, e;
		GetSel(s, e);
		return(LineFromChar(e) + 1);
	}

	// Sets a CString to the text of a specified line. No parameter (0) gives the
	// current line, (1) the first line etc. (-1) counts from last line back.
	// Returns the number of characters in the line.
	int GetLine(CString& str, int line = 0);

	// Deletes the indexed line. No parameter (0) gives the
	// current line, (1) the first line etc. (-1) counts from last line back.
	void DeleteLine(int line = 0);

	// Sets the selection point to the start of the given line, or the current line if default 0.
	void SetSelStartLine(int line = 0);

	// Expands the current selection if necessary to comprise full lines and returns
	// the number of lines in the selection.
	int SetSelFullLines(void);

	// Replaces the indexed line with the new text supplied. No parameter (0) gives the
	// current line, (1) the first line etc. (-1) counts from last line back.
	void ReplaceLine(CString& nl, int line = 0);

	// Deletes n characters within the current line (if possible). If n is positive,
	// deletes right from the current position, if n is negitive left. Returns actual
	// number of characters deleted (always positive).
	int DeleteChar(int n = 1);

	// Sets a CString to the text between two character positions. No start or end,
	// gets the current selection. No end gets one character from the start.
	int GetString(CString& str, int start = -1, int end = -1);

	BEGIN_MSG_MAP(JHCEdit)
		MSG_OCM_CTLCOLOREDIT(OnCtlColorEdit)
		MESSAGE_HANDLER(WM_CONTEXTMENU, OnContextMenu)
		MESSAGE_HANDLER(WM_LBUTTONDBLCLK, OnDoubleClick)
		CHAIN_MSG_MAP(CCMenu/*<JHCEdit>*/)
	END_MSG_MAP()

	LRESULT OnContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
	HBRUSH OnCtlColorEdit(CDCHandle dc, CEdit edit);
	LRESULT OnDoubleClick(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);

private:
	CMenu conMenu;
	COLORREF colorBg;
	COLORREF colorTx;
};

