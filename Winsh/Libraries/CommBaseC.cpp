#include "stdafx.h"
#include "CommBaseC.h"

#undef wReserved //This is a hack for an odd compiletime error due to a define for wReserved in
				 //richedit.h which is unrelated to the DCD.

// JHCommBase. By John.Hind@zen.co.uk.
// Generic base class for non-blocking serial communications drivers in Win32.

// Error Codes (returned by Open() or passed to OnError(int))
// ----------------------------------------------------------
// -1   - Unknown error.
// -3   - Send Timeout.
// -4   - Send Failure.
// -5   - Attempt to send when port not open.
// -6   - Asynchronous send failure.
// -10  - Receive error.
// -11  - Asynchronous receive error.
// -12  - Driver receive error (i.e. framing, parity etc.)
// -13  - Error reading from reception buffer.
// -100 - Unable to open specified comm port.
// -101 - Unable to set timeouts.
// -102 - Unable to set queue sizes.
// -103 - Unable to set comms settings.
// -104 - Unable to create reception event.
// -105 - Unable to create transmission event.
// -106 - Unable to create reception thread.
// -107 - Specified comm port is already open to another process.
// -200 - Nothing in download buffer to send.
// -201 - Download timeout.
// -202 - Upload timeout.

// Constructor.
JHCommBase::JHCommBase()
{
	hPort = INVALID_HANDLE_VALUE;
	hRxThread = NULL;
	hRxEvent = NULL;
	InitializeCriticalSection(&rxcs);
}

void JHCommBase::destruct()
{
	Close();
	WaitForSingleObject(hRxThread, 4000);
	DeleteCriticalSection(&rxcs);
}

// Destructor ensures port is closed and cleans up.
JHCommBase::~JHCommBase()
{
//  Removed because we have to create a temporary object to create as userdata and we
//  do not want the object finalised when the temporary goes out of scope. Instead we
//  call destruct above in a GC metamethod.
//	destruct();
}

void JHCommBase::InitSettings(COMMSETTINGS* cs, DCB* dcb, COMMTIMEOUTS* cto)
{
	cs->PortNumber = 1;
	cs->rxQueueSize = 0;
	cs->txQueueSize = 0;
	dcb->DCBlength = sizeof(dcb);
	SetHandshake(dcb, HandShake_None);
	dcb->BaudRate = 2400;
	dcb->ByteSize = 8;
	dcb->EofChar = 0x04;
	dcb->ErrorChar = 0;
	dcb->EvtChar = 0;
	dcb->fAbortOnError = true;
	dcb->fBinary = true;
	dcb->fDummy2 = false;
	dcb->fErrorChar = false;
	dcb->fNull = false;
	dcb->fParity = false;
	dcb->Parity = NOPARITY;
	dcb->StopBits = ONESTOPBIT;
	dcb->wReserved = 0;
	dcb->wReserved1 = 0;
	dcb->XoffChar = 0x13;
	dcb->XoffLim = 0;
	dcb->XonChar = 0x11;
	dcb->XonLim = 0;
	cto->ReadIntervalTimeout = MAXDWORD;
	cto->ReadTotalTimeoutConstant = 0;
	cto->ReadTotalTimeoutMultiplier = 0;
	cto->WriteTotalTimeoutConstant = 0;
	cto->WriteTotalTimeoutMultiplier = 0;
}

// Open the port. Returns 0 on success, or a negative error number.
// Establishes default comms settings, but calls OnOpen virtual function to
// change the default settings (including the port number).
// DEFAULTS: COM1; 2400baud; 8 Data bits; 1 Stop Bit; No Parity; No Handshaking.
int JHCommBase::Open()
{
	char fn[10];
	DCB dcb;
	COMMTIMEOUTS cto;
	COMMPROP cp;
	COMMSETTINGS cs;

	InitSettings(&cs, &dcb, &cto);
	OnOpen(&cs, &dcb, &cto);
	dcb.DCBlength = sizeof(dcb);
	cto.ReadIntervalTimeout = MAXDWORD;
	cto.ReadTotalTimeoutConstant = 0;
	cto.ReadTotalTimeoutMultiplier = 0;
	sprintf_s(fn, 10, "\\\\.\\COM%u", cs.PortNumber);
	hPort = CreateFileA(fn, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
	if (hPort == INVALID_HANDLE_VALUE)
	{
		switch (GetLastError())
		{
		case 5:
			return -107;
		default:
			return -100;
		}
	}
	if (!SetCommTimeouts(hPort, &cto)) {Close(); return -101;};
	if ((cs.rxQueueSize != 0) || (cs.txQueueSize != 0))
		if (!SetupComm(hPort, cs.rxQueueSize, cs.txQueueSize)) {Close(); return -102;};
	if ((dcb.XoffLim == 0) || (dcb.XonLim == 0))
	{
		if (!GetCommProperties(hPort, &cp))	cp.dwCurrentRxQueue = 0;
		if (cp.dwCurrentRxQueue > 0)
		{
			//If we can determine the queue size, default to 1/10th, 8/10ths, 1/10th.
			//Note that HighWater is measured from top of queue.
			dcb.XoffLim = dcb.XonLim = (short)((int)cp.dwCurrentRxQueue / 10);
		}
		else
		{
			//If we do not know the queue size, set very low defaults for safety.
			dcb.XoffLim = dcb.XonLim = 8;
		}
	}
	if (!SetCommState(hPort, &dcb)) {Close(); return -103;};
	hRxEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (NULL == hRxEvent) {Close(); return -104;};
	hTxEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (NULL == hTxEvent) {Close(); return -105;};
	rxmode = 0;
	hRxThread = CreateThread(NULL, 0, RxThread, (LPVOID)this, 0, NULL);
	if (NULL == hRxThread) {Close(); return -106;};
	sendcount = 0;
	//OutputDebugString("Port Opened");
	return 0;
}

// Returns TRUE if the port is open.
bool JHCommBase::IsOpen()
{
	return (hPort != INVALID_HANDLE_VALUE);
}

// Fills in the DCB with proper settings for the common handshake scenarios.
// Call this from within an OnOpen virtual function override.
// Parameter dcd: Pointer to the DCD as passed into OnOpen.
// Parameter hs:  Enumeration constant HandShake_None; HandShake_XonXoff; HandShake_CtsRts; HandShake_DsrDtr.
void JHCommBase::SetHandshake(DCB* dcb, Handshake hs)
{
	switch (hs)
	{
	case HandShake_None:
		dcb->fOutxCtsFlow = false; dcb->fOutxDsrFlow = false;
		dcb->fOutX = false; dcb->fInX = false;
		dcb->fRtsControl = RTS_CONTROL_ENABLE; dcb->fDtrControl = DTR_CONTROL_ENABLE;
		dcb->fTXContinueOnXoff = true; dcb->fDsrSensitivity = false;
		break;
	case HandShake_XonXoff:
		dcb->fOutxCtsFlow = false; dcb->fOutxDsrFlow = false;
		dcb->fOutX = true; dcb->fInX = true;
		dcb->fRtsControl = RTS_CONTROL_ENABLE; dcb->fDtrControl = DTR_CONTROL_ENABLE;
		dcb->fTXContinueOnXoff = true; dcb->fDsrSensitivity = false;
		break;
	case HandShake_CtsRts:
		dcb->fOutxCtsFlow = true; dcb->fOutxDsrFlow = false;
		dcb->fOutX = false; dcb->fInX = false;
		dcb->fRtsControl = RTS_CONTROL_HANDSHAKE; dcb->fDtrControl = DTR_CONTROL_ENABLE;
		dcb->fTXContinueOnXoff = true; dcb->fDsrSensitivity = false;
		break;
	case HandShake_DsrDtr:
		dcb->fOutxCtsFlow = false; dcb->fOutxDsrFlow = true;
		dcb->fOutX = false; dcb->fInX = false;
		dcb->fRtsControl = RTS_CONTROL_ENABLE; dcb->fDtrControl = DTR_CONTROL_HANDSHAKE;
		dcb->fTXContinueOnXoff = true; dcb->fDsrSensitivity = false;
		break;
	}
}

// Use in derived classes to synchronise access to data between the receive thread
// virtual functions and the main thread. For any data accessed from buth threads,
// Lock(); Read/Write data; Unlock(); Minimize time between Lock and Unlock.
void JHCommBase::Lock()
{
	EnterCriticalSection(&rxcs);
}
void JHCommBase::Unlock()
{
	LeaveCriticalSection(&rxcs);
}

// Initiate sending of data.
// Parameter ch: Pointer to array of BYTE to be sent.
// Parameter n:  Number of bytes in the array.
int JHCommBase::Send(LPBYTE ch, int n)
{
	if (hPort == INVALID_HANDLE_VALUE) return -5;
	DWORD wr;
	wo.Offset = 0;
	wo.OffsetHigh = 0;
	wo.hEvent = hTxEvent;
	sendcount = 0;
	if (!WriteFile(hPort, (LPCVOID)ch, n, &wr, &wo))
	{
		if (GetLastError() != ERROR_IO_PENDING) return -6;
	}
	sendcount = n - wr;
	//CString m = "Send: ";
	//for (int i = 0; (i < n); i++) m.AppendFormat(" %d", ch[i]);
	//OutputDebugString(m);
	return 0;
}

// Close the port.
void JHCommBase::Close()
{
	if (hPort != INVALID_HANDLE_VALUE)
	{
		//OutputDebugString("Port Close - starting");
		Lock(); rxmode = -1; Unlock();
		SetEvent(hRxEvent);
		WaitForSingleObject(hRxThread, 2000);
		CancelIo(hPort);
		CloseHandle(hPort);
		CloseHandle(hTxEvent);
		CloseHandle(hRxEvent);
		sendcount = 0;
		hPort = INVALID_HANDLE_VALUE;
		//OutputDebugString("Port Close - done");
	}
}

void JHCommBase::Receive()
{
	if (hPort != INVALID_HANDLE_VALUE)
	{
		Lock(); rxmode = 1; Unlock();
		SetEvent(hRxEvent);
	}
}

// Returns the number of bytes in the transmission queue.
int JHCommBase::BytesInTxQueue()
{
	COMSTAT cs;
	DWORD er;
	if (hPort != INVALID_HANDLE_VALUE)
	{
		if (ClearCommError(hPort, &er, &cs))
		{
			return cs.cbOutQue;
		}
	}
	return -1;
}

// Optionally waits for completion of send and checks for errors.
// Parameter wait: If true, blocks caller until send is done.
// Return: 0 - Complete (or pending if wait is false); >0 error.
int JHCommBase::CheckSend(bool wait)
{
	DWORD c;
	if (sendcount < 1) return 0;
	if (GetOverlappedResult(hPort, &wo, &c, wait))
	{
		if ((sendcount - c) > 0) 
			{return -3;};
		sendcount = 0;
		return 0;
	}
	else
	{
		if (GetLastError() != ERROR_IO_INCOMPLETE) return -4;

		if (wait)
			return -3;
		else
			return 0;
		//NB: Should be able to test for completion using this and returning a code
		//for "incomplete" above, and this works in XP. However in W98 ERROR_IO_INCOMPLETE
		//is returned AFTER the completion event in the mutex is signalled.
	}
}

BOOL JHCommBase::GetCTS()
{
	DWORD ms;
	if (!GetCommModemStatus(hPort, &ms)) return false;
	return ((ms & MS_CTS_ON) != 0);
}

BOOL JHCommBase::GetDSR()
{
	DWORD ms;
	if (!GetCommModemStatus(hPort, &ms)) return false;
	return ((ms & MS_DSR_ON) != 0);
}

BOOL JHCommBase::GetRLSD()
{
	DWORD ms;
	if (!GetCommModemStatus(hPort, &ms)) return false;
	return ((ms & MS_RLSD_ON) != 0);
}

BOOL JHCommBase::GetRI()
{
	DWORD ms;
	if (!GetCommModemStatus(hPort, &ms)) return false;
	return ((ms & MS_RING_ON) != 0);
}

void JHCommBase::SetRTS(bool s)
{
	if (hPort == INVALID_HANDLE_VALUE) return;
	if (s)
		EscapeCommFunction(hPort, 3);
	else
		EscapeCommFunction(hPort, 4);
}

void JHCommBase::SetDTR(bool s)
{
	if (hPort == INVALID_HANDLE_VALUE) return;
	if (s)
		EscapeCommFunction(hPort, 5);
	else
		EscapeCommFunction(hPort, 6);
}

void JHCommBase::SetBreak(bool s)
{
	if (hPort == INVALID_HANDLE_VALUE) return;
	if (s)
		EscapeCommFunction(hPort, 8);
	else
		EscapeCommFunction(hPort, 9);
}

// Runs on a worker thread to receive data asynchronously and monitor transmission
// progress. Calls the following virtual functions (override these to implement functionality).
//  OnRxChar(BYTE)  - Called each time a byte is received, with the received byte.
//  OnTxDone()      - Called whenever all bytes in transmission queue have been sent.
//  OnError(int)    - Called if an error occurs - err is a negative error number.
DWORD WINAPI RxThread(LPVOID lpParam)
{
	int err = 0; int r;
	//OutputDebugString("Starting Receive Thread");
	OVERLAPPED ov;
	HANDLE ev;
	HANDLE waits[3];
	JHCommBase* cb = (JHCommBase*)lpParam;
	DWORD cm;
	DWORD gb;
	int rxmode;
	BYTE buf[1];
	ev = CreateEvent(NULL, FALSE, FALSE, NULL);
	ov.Offset = 0; ov.OffsetHigh = 0;
	ov.hEvent = ev;
	waits[0] = ev;
	waits[1] = cb->hRxEvent;
	waits[2] = cb->hTxEvent;
	while (true)
	{
		cb->Lock(); rxmode = cb->rxmode; cb->Unlock();
		if (rxmode == 1)
			SetCommMask(cb->hPort, EV_RXCHAR | EV_ERR);
		else
			SetCommMask(cb->hPort, EV_ERR);
		// EV_RXCHAR | EV_TXEMPTY | EV_CTS | EV_DSR | EV_BREAK | EV_RLSD | EV_RING | EV_ERR
		if (!WaitCommEvent(cb->hPort, &cm, &ov))
		{
			if (GetLastError() != ERROR_IO_PENDING)
			{
				err = -10;
				break;
			};
			r = WaitForMultipleObjects(3, waits, false, INFINITE);
			if (r == (WAIT_OBJECT_0 + 1))
			{
				if (rxmode < 0) break;
				cm = 0;
			}
			else if (r == (WAIT_OBJECT_0 + 2))
			{
				cb->OnTxDone();
				cm = 0;
			}
			else if (r != WAIT_OBJECT_0)
			{
				err = -11;
				break;
			};
		}
		if ((cm & EV_ERR) != 0)
		{
			err = -12; break;
		}
		if ((cm & EV_RXCHAR) != 0)
		{
			do 
			{
				gb = 0;
				if (!ReadFile(cb->hPort, buf, 1, &gb, &ov)) {err = -13; break;};
				if (gb == 1)
				{
					if (!cb->OnRxChar(buf[0]))
					{
						rxmode = 0;
						cb->Lock(); cb->rxmode = 0; cb->Unlock();
					}
				}
			}
			while (gb > 0);
		}
	}
	CancelIo(cb->hPort);
	CloseHandle(ev);
	//OutputDebugString("Exiting Receive Thread");
	if (err != 0) cb->OnError(err);
	return 0;
}

