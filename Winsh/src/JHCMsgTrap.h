#pragma once

// CMsgTrap - Dynamic message handlers for CWindow derived WTL classes.

// Inherit the CWindow class with a message map publicly from CMsgTrap. Add the
// MESSAGE_TRAP macro at the top of the message map. Handlers must be of the
// prototype 'msgtraphandler' and are installed using 'TrapMsg'. Handlers have
// an 'ID' which is specified in the call or allocated uniquely if passed 0.
// Handlers are selected by message number and optionally also by WPARAM value.

// Four strategies are supported within message handlers:
// 1. A handler can use the CWindow pointer to post a new message onto its queue
// (or execute any other CWindow method).
// 2. The host class can overload the virtual method 'OnMsgTrap' and the handler
// can call this using the CMsgTrap pointer. Being virtual, OnMsgTrap has full
// access to the host object.
// 3. A handler can filter messages by setting the final BOOL reference parameter
// TRUE before returning.
// 4. A handler can simply translate the message by changing its message number
// or parameters. The translated message will then be processed through the
// rest of the chain.

// For 3 and 4 the order in which the dynamic handlers are inserted becomes
// important. Handlers are encountered in reverse order of insertion: the last
// handler inserted is encountered first. Handlers can be bumped up in priority
// by removing them using 'UnTrapMsg' and then calling 'TrapMsg' again with the
// same ID. Also notice that the final BOOL 'bHandled' reference parameter has
// the reverse default to that for static handlers: it is assumed that multiple
// dynamic handlers should see every message except for message filtering, in
// which case 'bHandled' should be changed from FALSE to TRUE.

// Using the default automatic ID allocation, IDs will be unique, however it is
// permitted to use the same ID explicitly in multiple 'TrapMsg' calls. For
// this reason, 'UnTrapMsg' removes ALL messages with the given ID and returns
// a count of handlers deleted. Automatic allocation does not reuse ID numbers,
// so if handlers are continually removed and added they will eventually run out
// (at which point 'AllocTrapIDs' and 'TrapMsg' return 0). You can either cache
// and re-use IDs yourself, or when they run out, call 'ResetMsgTrap' and then
// 'TrapMsg' to reinsert each active handler with a new ID allocation.
// To facilitate caching, 'AllocTrapIDs' can allocate a specified number of
// sequential IDs in a single call.

class CMsgTrap;

typedef LRESULT msgtraphandler(UINT, CMsgTrap*, CWindow*, UINT&, WPARAM&, LPARAM&, BOOL&);

class CMsgTrap
{
public:
	CMsgTrap()
	{
		firstTrans = NULL;
		nextid = 1;
		maxid = 0xFFFF;
	};

	UINT AllocTrapIDs(UINT count = 1)
	{
		if (nextid == 0) return 0;
		if ((nextid + count) > maxid) { nextid = 0; return 0; }
		UINT m = nextid;
		nextid += count;
		return m;
	};

	UINT TrapMsg(UINT msg, msgtraphandler fMsg, WPARAM wfltr = 0, UINT id = 0)
	{
		UINT idx = (id == 0)? AllocTrapIDs(1) : id;
		if (idx == 0) return 0;
		CMsgTrans* p = new CMsgTrans;
		p->next = firstTrans;
		firstTrans = p;
		p->mFltr = msg;
		p->wFltr = wfltr;
		p->fMsgH = fMsg;
		p->id = idx;
		return idx;
	};

	UINT UnTrapMsg(UINT id)
	{
		CMsgTrans* p1;
		CMsgTrans* p2;
		UINT r = 0;
		while ((firstTrans != NULL) && (firstTrans->id == id))
		{
			p2 = firstTrans;
			firstTrans = firstTrans->next;
			delete(p2);
			r++;
		}
		p1 = firstTrans;
		while (p1 != NULL)
		{
			p2 = p1->next;
			if (p2->id == id)
			{
				p1->next = p2->next;
				p1 = p2->next;
				delete(p2);
				r++;
			}
			else
			{
				p1 = p2;
			}
		}
		return r;
	};

	void ResetMsgTrap(void)
	{
		nextid = 1;
		CMsgTrans* p;
		CMsgTrans* nx = firstTrans;
		firstTrans = NULL;
		while (nx != NULL)
		{
			p = nx->next;
			delete(nx);
			nx = p;
		}
	};

	void SetTrapMaxID(UINT m)
	{
		maxid = (m > nextid)? m : nextid;
	};

	UINT GetTrapTopID(void)
	{
		return nextid;
	};

	virtual LRESULT OnMsgTrap(UINT id, UINT msg, LPARAM lp, void* vp) { return 0; }

protected:
	class CMsgTrans
	{
	public:
		CMsgTrans* next;	//Pointer to next message translater.
		UINT mFltr;			//Filter selects if msg parameter matches this value.
		WPARAM wFltr;		//If passes msg filter, filter wParam against this if it is non-zero.
		UINT id;			//This becomes the WM_COMMAND id (wParam).
		msgtraphandler* fMsgH;	//Function used to process messages which pass the filters.
	};

	CMsgTrans* GetFirstTrans(void) { return firstTrans; };

private:
	CMsgTrans* firstTrans;	//Head pointer for message translator chain.
	UINT nextid;			//Next free message index for AllocLuaMessages
	UINT maxid;				//Maximum allowed ID number.
};

#define MESSAGE_TRAP { \
	CMsgTrans* nx = GetFirstTrans(); \
	while (nx != NULL){ \
		if (uMsg == nx->mFltr){ \
			if ((nx->wFltr == 0) || (nx->wFltr == wParam)){ \
				bHandled = FALSE; \
				lResult = (*nx->fMsgH)(nx->id, (CMsgTrap*)this, (CWindow*)this, uMsg, wParam, lParam, bHandled); \
				if (bHandled) return TRUE; \
			} \
		} \
		nx = nx->next; \
	} \
}
