#pragma once

// GUIThread.h: Template for running a window on a seperate thread.
//
// Recipy:
// 1. Create a CWindow derived class (usually CFrameWindow derived) to implement the window to be
//    run on the GUI thread. This must have a constructor taking a single parameter of type pointer
//    to the Proxy type (next step). The Proxy pointer should be saved to a private field and
//    used to access the Proxy in message handlers.
// 2. Create a Proxy class derived from template CGUIThreadProxy. For example:
//    class CMyProxy : public CGUIThreadProxy<CMyWindow, CGUIThreadProxy>
//    This class should have a constructor taking a CWindow pointer to the host and a boolean
//    which will be true if threading should be enabled. This constructor calls Init inherited from
//    the template and passes the parameters straight through. The class may also add any number of
//    additional public fields for data transfer between host and thread windows.
// 3. In the parent window create a field of the Proxy type (or a pointer to it) and construct
//    a proxy object passing the 'this' pointer and boolean 'true' to initialise on a thread. 'false'
//    may be passed to run in the parent thread (for example for testing).
// 4. Initialise any data transfer fields in the Proxy object as required and then call method
//    'Create()'. This actually creates the window, on its own thread if specified.
// 5. After 'Create' has been called, you must wrap any access to the public fields of the Proxy
//    with 'EnterCS()' before and 'LeaveCS()' after. This must be done both in the parent window
//    and in the threaded window for thread safety. Time spent between these brackets should be
//    minimised and ON NO ACCOUNT CALL ANY OTHER METHODS OF THE PROXY within the brackets.
// 6. Send a message from the parent window to the thread window using SendMessageDown. The message
//    is picked up in the threaded window using a normal message handler function, but this MUST
//    call 'SignalDone()' on completion of processing. In the parent thread, SendMessageDown blocks
//    until this signal is received. Use 'PostMessageDown' to post a message without blocking.
// 7. Post a message from the thread window to the parent using 'PostMessageUp'. This is non-blocking.
// 8. To destroy the thread and the window, call the 'Destroy' method of the proxy.
//
// Low-level alternative:
// 1. Create a CWindow derived class (usually CFrameWindow derived) to implement the window to be
//    run on the GUI thread. This must have a constructor taking a single parameter of type pointer
//    to any type of object used for data transfer.
// 2. Construct the CWindow as follows:
//    CData data;
//    CMyWindow* win = new CGUIThread<CMyWindow, CData*>(data);
// 3. This creates the window on its own thread and shares the data structure. However you are
//    responsible for synchronising access to the data and messaging in a thread-safe way.

#include <atlwin.h>

template<class W, typename P> class CGUIThreadProxy
{
public:
	// Add shared fields in the derived object.

public:
	void Init(CWindow* h, bool threaded);
	void Create();
	void Destroy();
	~CGUIThreadProxy() { Destroy(); }

	bool Threaded() { return (done != NULL); }

	// In both Host and Window, bracket all access to public fields of this class with these:
	void EnterCS() { if (Threaded()) EnterCriticalSection(&cs); }
	void LeaveCS() { if (Threaded()) LeaveCriticalSection(&cs); }

	// Use these in Host Only!
	void SendMessageDown(UINT u, WPARAM w, LPARAM l);
	void PostMessageDown(UINT u, WPARAM w, LPARAM l);

	// Use these in Window Only! Call SignalDone after handling message from Host.
	void PostMessageUp(UINT u, WPARAM w, LPARAM l);
	void SignalDone() { if (Threaded()) SetEvent(done); }

	// Provide direct access to client window only if NOT threaded:
	W* GetClient(void) { return (Threaded())? NULL : (W*)window; }

private:
	CRITICAL_SECTION cs;
	HANDLE done;
	CWindow* host;
	void* window;
};

template<class W, typename P> class CGUIThread
{
public:
	CGUIThread(P p);
	~CGUIThread();
	void PostMessage(UINT Msg, WPARAM wParam, LPARAM lParam);

private:
	static DWORD WINAPI ThreadProc(LPVOID lpPara);
	HANDLE handle;
	struct ST {HANDLE h; CWindow* w; DWORD id; P param;} st;

public:
	HWND GetHwnd() {return (HWND)st.w;}
};

template<class W, typename P> CGUIThread<W,P>::CGUIThread(P p)
{
	st.param = p;
	st.h = CreateEvent(NULL, TRUE, FALSE, NULL);
	handle = CreateThread(NULL, 0, ThreadProc, (void *)&st, 0, &st.id);
	WaitForSingleObject(st.h, 3000);
	CloseHandle(st.h);
};

template<class W, typename P> DWORD WINAPI CGUIThread<W,P>::ThreadProc(LPVOID lpPara)
{
	MSG AMessage;
	W* obj;
	ST* st;

	st = (ST*)lpPara;
	obj = new W(st->param);
	st->w = (CWindow*)obj;
	PeekMessage(&AMessage, 0, 0, 0, PM_NOREMOVE);
	SetEvent(st->h);
	while ( GetMessage(&AMessage, 0, 0, 0) )
	{
		TranslateMessage(&AMessage);
		DispatchMessage(&AMessage);
	}
	st->id = 0;
	obj->DestroyWindow();
	delete obj;
	return 0;
};

template<class W, typename P> CGUIThread<W,P>::~CGUIThread()
{
	if (st.id != 0) PostThreadMessage(st.id, WM_QUIT, 0, 0);
	WaitForSingleObject(handle, 4000);
	CloseHandle(handle);
};

template<class W, typename P> void CGUIThread<W,P>::PostMessage(UINT Msg, WPARAM wParam, LPARAM lParam)
{
	if (st.id != 0) st.w->PostMessage(Msg, wParam, lParam);
};

template<class W, typename P> void CGUIThreadProxy<W,P>::Init(CWindow* h, bool threaded)
{
	host = h;
	done = NULL;
	window = NULL;
	if (threaded)
	{
		InitializeCriticalSection(&cs);
		done = CreateEvent(NULL, FALSE, FALSE,NULL);
	}
}

template<class W, typename P> void CGUIThreadProxy<W,P>::Create()
{
	if (done == NULL)
	{
		window = new W((P*)this);
	}
	else
	{
		window = new CGUIThread<W, P*>((P*)this);
	}
}

template<class W, typename P> void CGUIThreadProxy<W,P>::Destroy()
{
	if (window != NULL) delete window;
	if (done != NULL)
	{
		DeleteCriticalSection(&cs);
		CloseHandle(done);
	}
}

template<class W, typename P> void CGUIThreadProxy<W,P>::SendMessageDown(UINT u, WPARAM w, LPARAM l)
{
	if (done == NULL)
	{
		((CWindow*)window)->SendMessage(u, w, l);
	}
	else
	{
		ResetEvent(done);
		((CGUIThread<W, P*>*)window)->PostMessage(u, w, l);
		WaitForSingleObject(done, 4000);
	}
}

template<class W, typename P> void CGUIThreadProxy<W,P>::PostMessageDown(UINT u, WPARAM w, LPARAM l)
{
	((CGUIThread<W, P*>*)window)->PostMessage(u, w, l);
}

template<class W, typename P> void CGUIThreadProxy<W,P>::PostMessageUp(UINT u, WPARAM w, LPARAM l)
{
	host->PostMessage(u, w, l);
}
