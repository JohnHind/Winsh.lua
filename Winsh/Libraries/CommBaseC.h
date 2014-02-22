#pragma once

enum Handshake {HandShake_None, HandShake_XonXoff, HandShake_CtsRts, HandShake_DsrDtr};

struct COMMSETTINGS
{
	UINT PortNumber;
	UINT rxQueueSize;
	UINT txQueueSize;
};

class JHCommBase
{
public:
	JHCommBase();
	~JHCommBase();
	void destruct();
	static void InitSettings(COMMSETTINGS* cs, DCB* dcb, COMMTIMEOUTS* cto);
	int Open();
	void Close();
	bool IsOpen();
	int Send(LPBYTE ch, int n);
	int CheckSend(bool wait = false);
	void Receive();
	static void SetHandshake(DCB* dcb, Handshake hs);
	BOOL GetCTS();
	BOOL GetDSR();
	BOOL GetRLSD();
	BOOL GetRI();
	void SetRTS(bool s);
	void SetDTR(bool s);
	void SetBreak(bool s);
	void Lock();
	void Unlock();
	int BytesInTxQueue();
	HANDLE hRxEvent;
	HANDLE hTxEvent;
	HANDLE hPort;
	int rxmode;
	virtual bool OnRxChar(BYTE ch){return false;};
	virtual void OnTxDone(){};
	virtual void OnError(int error){};

protected:
	virtual void OnOpen(COMMSETTINGS* cs, DCB* dcb, COMMTIMEOUTS* cto){};

private:
	HANDLE hRxThread;
	OVERLAPPED wo;
	CRITICAL_SECTION rxcs;
	int sendcount;
};

DWORD WINAPI RxThread(LPVOID lpParam);
