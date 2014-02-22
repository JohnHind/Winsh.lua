// JHCMultiPaneStatusBar.h : added features for the WTL CMultiPaneStatusBarCtrl class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

class JHCMultiPaneStatusBarCtrl : public CMultiPaneStatusBarCtrlImpl<JHCMultiPaneStatusBarCtrl>
{
public:
	DECLARE_WND_SUPERCLASS(_T("WTL_MultiPaneStatusBar"), GetWndClassName())

	// see http://www.codeproject.com/KB/wtl/multipanestatusbar.aspx
	void SetPaneWidths(int* arrWidths, int nPanes)
	{ 
		// find the size of the borders
		int arrBorders[3];
		GetBorders(arrBorders);

		// calculate right edge of default pane (0)
		arrWidths[0] += arrBorders[2];
		for (int i = 1; i < nPanes; i++)
			arrWidths[0] += arrWidths[i];

		// calculate right edge of remaining panes (1 thru nPanes-1)
		for (int j = 1; j < nPanes; j++)
			arrWidths[j] += arrBorders[2] + arrWidths[j - 1];

		// set the pane widths
		SetParts(m_nPanes, arrWidths); 
	}

};

