#pragma once

// JHCDropFiles.h: Mix-in template to handle files dropped on a window.
//
// Based on work by Pablo Aliskevicius:
// http://www.codeproject.com/Articles/6166/Dropping-Files-Into-a-WTL-Window-The-Easy-Way
//
// Inherit any CWindow derived class (usually CFrameWindow) from CDropFiles<MyCWindowClass>
// Add CHAIN_MSG_MAP(CDropFiles<MyCWindowClass>) to the Message Map of the CWindowClass.
// Call AcceptDropFiles(true) to turn feature on (or false to turn off again).
// Override OnFileDrop to handle the drop. Within this, call GetDropFilePath to recover the path to
// the dropped file(s) and GetDropPoint to recover the drop position.

template <class T> class CDropFiles
{
public:
	// Call this to enable or disable file drop
	void AcceptDropFiles(bool bTurnOn = true)
	{
        T* pT = static_cast<T*>(this);
		ATLASSERT(::IsWindow(pT->m_hWnd));
		::DragAcceptFiles(pT->m_hWnd, bTurnOn);
	}

	// Override this to handle file drops, n is number of files dropped
	// return true if files were handled, false to allow other handlers a chance.
	virtual bool OnFileDrop(UINT n) { return false; }
	
	// Call this *WITHIN OnFileDrop* to recover each path. c = 0 upto n - 1.
	LPCTSTR GetDropFilePath(UINT c = 0)
	{
		if (c >= m_nFiles) return NULL;
		::DragQueryFile(m_hd, c, m_buff, MAX_PATH);
		return (LPCTSTR)m_buff;
	}

	// Call this *WITHIN OnFileDrop* to get the drop point within the window 
	CPoint GetDropPoint()
	{
		POINT pt;
		::DragQueryPoint(m_hd, &pt);
		return CPoint(pt);
	}

protected:
	UINT m_nFiles;	// Number of dropped files.
	HDROP m_hd;		// Drop files buffer.
	TCHAR m_buff[MAX_PATH + 5];

	BEGIN_MSG_MAP(CDropFilesHandler<T>)
		MESSAGE_HANDLER(WM_DROPFILES, OnDropFiles) 
	END_MSG_MAP()

	// WM_DROPFILES handler:
	LRESULT OnDropFiles(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
	{
		T* pT = static_cast<T*>(this);

		m_buff[0] = '\0';
		m_hd = (HDROP)wParam;
		m_nFiles = ::DragQueryFile(m_hd, 0xFFFFFFFF, NULL, 0);
		bHandled = pT->OnFileDrop(m_nFiles);
		if (bHandled) ::DragFinish(m_hd);
		return 0;
	}

};
