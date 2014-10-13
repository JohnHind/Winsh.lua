/* LibFTDI.cpp      (c) 2014 John Hind 
**      Lua interface to FTDI ftd2xx
*/

#define LUAFTLIB_BUILDING
#include <stdlib.h>

#include "LibFTDI.h"
extern "C" {
#include "FTD2XX.H"
}

typedef struct ft_ud {
	FT_HANDLE h;		// FTDI Handle of device
	UINT rdsernum;		// Counter incremented each Read 'init' call
	UINT chars;			// Maximum number of bytes in the Read Packet
	UINT bufsz;			// Size of buffer
	UINT bufpt;			// Pointer to next location in buffer
	BYTE matchpt;       // Current position of terminator match in term
	BYTE matchsz;       // Number of bytes in terminator
	const char* buf;	// Packet buffer (at least chars + 4)
	char term[4];       // Terminator bytes
} ft_ud;

int setflag (lua_State* L, int flag, const char* key, const char** nm) {
	if (flag != 0) {
		lua_pushboolean(L, TRUE);
		lua_setfield(L, -2, key);
		if (nm != NULL) *nm = key;
	}
	return flag;
}

int checkfield (lua_State* L, int ix, const char* key) {
	lua_getfield(L, ix, key);
	if (lua_isnil(L, -1)) { lua_pop(L, 1); return FALSE; }
	return TRUE;
}

UINT pop_unsigned (lua_State* L) {
	UINT r = lua_tounsigned(L, -1);
	lua_pop(L, 1);
	return r;
}

const char* pop_string (lua_State* L) {
	const char* r = lua_tostring(L, -1);
	lua_pop(L, 1);
	return r;
}

int pop_option (lua_State* L, const char *const lst[]) {
	int r = luaL_checkoption(L, -1, NULL, lst);
	lua_pop(L, 1);
	return r;
}

int waitfor(FT_HANDLE h, UINT mask, UINT to) {
	HANDLE hEvent;
	if (to == 0) return TRUE;
	hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	FT_SetEventNotification(h, mask, (PVOID)hEvent);
	return (WaitForSingleObject(hEvent, to) == 0);
}

int waitstatus(FT_HANDLE h, UINT mask1, UINT mask2, int state, UINT to) {
	DWORD st = 0;
	while (TRUE) {
		if (!FT_SUCCESS(FT_GetModemStatus(h, &st))) return FALSE;
		if (state)
			{ if ((st & mask2) != 0) return TRUE; }
		else
			{ if ((st & mask2) == 0) return TRUE; }
		if (to == 0) return FALSE;
		if (!waitfor(h, mask1, to)) return FALSE;
	}
	return FALSE;
}

/* P1: The index number of the device, 1 .. number of devices from the above call. OR
** P1: A Device object (or use method syntax).
** R1: A table containing all the information that can be gleened about the device. This depends on the device
** type, the operating system and whether the device is open or not. If the device is open, you can access via
** either index number or device object. If using the index, the information is current at the last execution
** of 'devices'.
*/
static int info (lua_State* L) {
	lua_checkstack(L, 3);
	DWORD flg = 0, typ = 0, id = 0, loc = 0;
	char sn[16]; char des[64]; int err = FALSE;
	DWORD drv = 0; LONG com = 0; UCHAR lat = 0; DWORD ix = 0; FT_HANDLE h = NULL; DWORD ees = 0;
	const char* pn = NULL;
	sn[0] = 0; des[0] = 0;
	if (lua_type(L, -1) == LUA_TUSERDATA) {
		h = ((ft_ud*)lua_touserdata(L, -1))->h;
	} else {
		ix = lua_tounsigned(L, -1);
	}
	if (h == NULL) err = !FT_SUCCESS(FT_GetDeviceInfoDetail(ix-1, &flg, &typ, &id, &loc, sn, des, &h));
	if ((!err) && (h != NULL)) {
		err = !FT_SUCCESS(FT_GetDeviceInfo(h, &typ, &id, sn, des, NULL));
		if (!FT_SUCCESS(FT_GetDriverVersion(h, &drv))) drv = 0;
		if (!FT_SUCCESS(FT_GetComPortNumber(h, &com))) com = 0;
		if (!FT_SUCCESS(FT_GetLatencyTimer(h, &lat))) lat = 0;
		if (!FT_SUCCESS(FT_EE_UASize(h, &ees))) ees = 0;
		if (flg == 0) flg = FT_FLAGS_OPENED;
	}
	if (!err) {
		lua_createtable(L, 0, 2);
		setflag(L, ((flg & FT_FLAGS_HISPEED) != 0), "hispeed", NULL);
		setflag(L, ((flg & FT_FLAGS_OPENED) != 0), "open", NULL);
		setflag(L, (typ == FT_DEVICE_BM), "FT-BM", &pn);
		setflag(L, (typ == FT_DEVICE_AM), "FT-AM", &pn);
		setflag(L, (typ == FT_DEVICE_100AX), "FT100AX", &pn);
		setflag(L, (typ == FT_DEVICE_2232C), "FT2232C", &pn);
		setflag(L, (typ == FT_DEVICE_232R), "FT232R", &pn);
		setflag(L, (typ == FT_DEVICE_2232H), "FT2232H", &pn);
		setflag(L, (typ == FT_DEVICE_4232H), "FT4232H", &pn);
		setflag(L, (typ == FT_DEVICE_232H), "FT232H", &pn);
		setflag(L, (typ == FT_DEVICE_X_SERIES), "FT-X", &pn);
		if (pn != NULL) {
			lua_pushstring(L, pn);
			lua_setfield(L, -2, "type");
		}
		lua_pushstring(L, des);
		lua_setfield(L, -2, "description");
		lua_pushstring(L, sn);
		lua_setfield(L, -2, "serial");
		lua_pushunsigned(L, (id & 0xFFFF));
		lua_setfield(L, -2, "pid");
		lua_pushunsigned(L, ((id >> 16) & 0xFFFF));
		lua_setfield(L, -2, "vid");
		if (loc != 0) {
			lua_pushunsigned(L, loc);
			lua_setfield(L, -2, "locid");
		}
		if (drv != 0) {
			lua_pushunsigned(L, drv);
			lua_setfield(L, -2, "driverversion");
		}
		if (com != 0) {
			lua_pushstring(L, "COM");
			lua_pushunsigned(L, com);
			lua_concat(L, 2);
			lua_setfield(L, -2, "comport");
		}
		if (lat != 0) {
			lua_pushunsigned(L, lat);
			lua_setfield(L, -2, "latency");
		}
		if (ees != 0) {
			lua_pushunsigned(L, ees);
			lua_setfield(L, -2, "eeusersize");
		}
	} else {
		lua_pushboolean(L, FALSE);
	}
	return 1;
}

/* P1: Number of available devices (Invariant State)
** P2: Number index of previous device (Control Variable)
** R1: Number index of current device
** R2: Table of information about current device (or nil)
*/
static int iterf(lua_State* L) {
	DWORD n = (DWORD)lua_tointeger(L, 1);
	DWORD p = (DWORD)(lua_tointeger(L, 2) + 1);
	if (p <= n) {
	    lua_pushinteger(L, p);
		lua_pushinteger(L, p);
		info(L);
		return 2;
	}
	return 0;
}

/* R1: Function iterf.
** R2: Number of available devices (Invariant State)
** R3: Number 0 (Initial value of Control Variable)
*/
static int ports(lua_State* L) {
	lua_checkstack(L, 3);
	DWORD n;
	lua_pushcfunction(L, iterf);
	if (!FT_SUCCESS(FT_CreateDeviceInfoList(&n))) n = 0;
	lua_pushinteger(L, n);
	lua_pushinteger(L, 0);
	return 3;
}

/* P1: A table containing the device identifier and any configuration parameters.
** R1: Device object (userdata) or boolean false on error.
** R2: In the event of an error, a string describing it.
** Table must include one of the following device identifier fields: 'index', 'locid', 'serial' or 'description'.
** NB: This function is enclosed to make a class closure for the device driver class.
*/
static int open(lua_State* L) {
	lua_checkstack(L, 4);
	static const char* const modetx[] = { "uart", "asyncbb", "mpsse", "syncbb", "mcu", "opto", "cbusbb", "fifo", NULL };
	static const int modenum[] = {0,1,2,4,8,0x10,0x20,0x40};
	static const char* const paritytx[] = {"none","odd","even","mark","space",NULL};
	static const char* const flowtx[] = {"none","rtscts","dtrdsr","xonxoff",NULL};
	static const int flownum[] = {FT_FLOW_NONE,FT_FLOW_RTS_CTS,FT_FLOW_DTR_DSR,FT_FLOW_XON_XOFF};
	FT_HANDLE h = NULL;
	ft_ud* ud = NULL;
	const char* sp;
	DWORD dw; UINT u1; UINT u2; UINT u3; UINT u4; int ex; UINT mode = 0xFFFF;
	luaL_checktype(L, 1, LUA_TTABLE);

	/* Open the device: */
	if (checkfield(L, 1, "index")) {
		dw = pop_unsigned(L);
		if (dw > 0) if (!FT_SUCCESS(FT_Open(dw-1, &h))) h = NULL;
	}
	if ((h == NULL) && (checkfield(L, 1, "locid"))) {
		dw = pop_unsigned(L);
		if (dw > 0) if (!FT_SUCCESS(FT_OpenEx((PVOID)dw, FT_OPEN_BY_LOCATION, &h))) h = NULL;
	}
	if ((h == NULL) && (checkfield(L, 1, "serial"))) {
		sp = pop_string(L);
		if (sp != NULL) if (!FT_SUCCESS(FT_OpenEx((PVOID)sp, FT_OPEN_BY_SERIAL_NUMBER, &h))) h = NULL;
	}
	if ((h == NULL) && (checkfield(L, 1, "description"))) {
		sp = pop_string(L);
		if (sp != NULL) if (!FT_SUCCESS(FT_OpenEx((PVOID)sp, FT_OPEN_BY_DESCRIPTION, &h))) h = NULL;
	}
	/* Check device opened: */
	if (h == NULL) {
		lua_pushboolean(L, FALSE);
		return 1;
	}
	sp = NULL;
	/* Reset and drain if mode is to change: */
	if (checkfield(L, 1, "mode")) { 
		mode = modenum[pop_option(L, modetx)];
		FT_ResetDevice(h);
		FT_Purge(h, FT_PURGE_RX);
	}
	/* Baud Rate: */
	if (checkfield(L, 1, "baudrate")) {
		dw = pop_unsigned(L);
		if (dw > 0) {if (!FT_SUCCESS(FT_SetBaudRate(h, dw))) sp = "baudrate";} else sp = "baudrate";
	}
	/* Data Characteristics: */
	ex = FALSE; u1 = FT_BITS_8; u2 = FT_STOP_BITS_1; u3 = FT_PARITY_NONE;
	if (checkfield(L, 1, "wordlength")) { 
		dw = pop_unsigned(L);
		u1 = (dw==7)? FT_BITS_7 : FT_BITS_8;
		ex = TRUE;
	}
	if (checkfield(L, 1, "stopbits")) {
		dw = pop_unsigned(L);
		u2 = (dw==2)? FT_STOP_BITS_2 : FT_STOP_BITS_1;
		ex = TRUE;
	}
	if (checkfield(L, 1, "parity")) {
		u3 = pop_option(L, paritytx);
		ex = TRUE;
	}
	if (ex) if (!FT_SUCCESS(FT_SetDataCharacteristics(h, u1, u2, u3))) sp = "datacharacteristics";
	/* Flow Control: */
	if (checkfield(L, 1, "flowcontrol")) {
		u1 = pop_option(L, flowtx);
		u2 = 0x11;
		if (checkfield(L, 1, "xon")) {
			dw = pop_unsigned(L);
			u2 = ((dw > 0) && (dw <= 0xFF))? dw : 0x11;
		}
		u3 = 0x13;
		if (checkfield(L, 1, "xoff")) {
			dw = pop_unsigned(L);
			u3 = ((dw > 0) && (dw <= 0xFF))? dw : 0x13;
		}
		if (!FT_SUCCESS(FT_SetFlowControl(h, u1, u2, u3))) sp = "flowcontrol";
	}
	/* Marker Characters: */
	ex = u2 = u4 = FALSE;
	u1 = u3 = 0;
	if (checkfield(L, 1, "eventmarker")) {
		u1 = pop_unsigned(L);
		if (u1 > 255) u1 = 0;
		ex = u2 = TRUE;
	}
	if (checkfield(L, 1, "errormarker")) {
		u3 = pop_unsigned(L);
		if (u3 > 255) u3 = 0;
		ex = u4 = TRUE;
	}
	if (ex) if (!FT_SUCCESS(FT_SetChars(h,u1,u2,u3,u4))) sp = "setchars";
	/* USB Parameters: */
	if (checkfield(L, 1, "usbinsize")) {
		u1 = pop_unsigned(L);
		if (u1 < 64) u1 = 64;
		if (u1 > 65536) u1 = 65536;
		u1 = (u1/64)*64;
		if (!FT_SUCCESS(FT_SetUSBParameters(h, u1, 0))) sp = "usbinsize";
	}
	/* Latency Timer: */
	if (checkfield(L, 1, "latency")) {
		u1 = pop_unsigned(L);
		if (u1 < 2) u1 = 2;
		if (u1 > 255) u1 = 255;
		if (!FT_SUCCESS(FT_SetLatencyTimer(h, u1))) sp = "latency";
	}
	/* Retry Count: */
	if (checkfield(L, 1, "piperetry")) {
		u1 = pop_unsigned(L);
		if (!FT_SUCCESS(FT_SetResetPipeRetryCount(h, u1))) sp = "piperetry";
	}
	/* Deadman Timeout: */
	if (checkfield(L, 1, "deadman")) {
		u1 = pop_unsigned(L);
		if (!FT_SUCCESS(FT_SetDeadmanTimeout(h, u1))) sp = "deadman";
	}
	/* Bit Mode: */
	dw = 0;
	if (checkfield(L, 1, "iomask")) dw = pop_unsigned(L);
	if (mode != 0xFFFF) {
		if (!FT_SUCCESS(FT_SetBitMode(h, 0, 0))) sp = "resetbitmode";
		if (!FT_SUCCESS(FT_SetBitMode(h, (UCHAR)dw, mode))) sp = "setbitmode";
		Sleep(50);
	}
	/* Check for config failure: */
	if (sp != NULL) {
		FT_Close(h);
		lua_pushboolean(L, FALSE);
		lua_pushstring(L, sp);
		return 2;
	}
	/* Wrap handle as userdata object and return it: */
	ud = (ft_ud*)lua_newuserdata(L, sizeof(ft_ud));
	ud->rdsernum = 0; ud->h = h; ud->buf = NULL; ud->bufpt = ud->bufsz = ud->chars = 0; ud->matchpt = ud->matchsz = 0;
	lua_pushvalue(L, lua_upvalueindex(2)); //Metatable is second upvalue in this closure
	lua_setmetatable(L, -2);									//|T|
	return 1;
}

/* P1: A Device object (or use method syntax).
** Pn: Any number of string keys specifying which status values to return in what order.
** Rn: The same number of status values of the appropriate types.
** 'cts','dsr','ri','dcd' - Boolean state of modem control inputs.
** 'oe','pe','fe' - Boolean presense of error flags for overrun, parity, framing errors.
** 'bi' - Boolean flag for break condition interrupt.
** 'rxq','txq' - Number of characters in the receive or transmit queue.
** 'rxe','mse','lse' - Boolean event flags for received character, modem status change, line status change.
** 'bm' - Number 0-255 representing the current state of the bitbang databus.
*/
static int status(lua_State* L) {
	lua_checkstack(L, 4);
	static const char* const skey[] = { "cts", "dsr", "ri", "dcd", "oe", "pe", "fe", "bi", "rxq", "txq",
		"rxe","mse","lse","bm",NULL};
	static const int mask[] = {0x10,0x20,0x40,0x80,0x0200,0x0400,0x0800,0x1000,0,0,
		FT_EVENT_RXCHAR,FT_EVENT_MODEM_STATUS,FT_EVENT_LINE_STATUS};
	luaL_checktype(L, 1, LUA_TUSERDATA);
	FT_HANDLE h = ((ft_ud*)lua_touserdata(L, 1))->h;
	int n = lua_gettop(L); int t; int ms = FALSE; DWORD stat = 0; int i; DWORD rq;
	int gs = FALSE; DWORD tq; DWORD es; UCHAR bm;
	for (i = n; (i > 1); i--) {
		t = luaL_checkoption(L, i, NULL, skey);
		lua_remove(L,i);
		if (t < 8) {
			if (!ms) {
				FT_GetModemStatus(h, &stat);
				ms = TRUE;
			}
			lua_pushboolean(L, ((stat & mask[t]) != 0));
		} else if (t == 13) {
			FT_GetBitMode(h, &bm);
			lua_pushunsigned(L, bm);
		} else if (t == 8) {
			if (!gs) FT_GetQueueStatus(h, &rq);
			lua_pushunsigned(L, rq);
		} else {
			if (!gs) {
				FT_GetStatus(h, &rq, &tq, &es);
				gs = TRUE;
			}
			if (t == 9) {
				lua_pushunsigned(L, tq);
			} else {
				lua_pushboolean(L, ((es & mask[t]) != 0));
			}
		}
		lua_insert(L,i);
	}
	n--;
	return n;
}

/* Read data and check for packet termination conditions. If t is supplied and is greater than 0, the routine
** will block for up to that number of milliseconds per byte to be received. Else only data already received
** is processed and the routine does not block. Returns: 1 if the complete packet is available; 2 for timeout
** with incomplete packet or no data; -1 for communications error.
*/
static int checkrxdata(ft_ud* ud, DWORD t = 0) {
	DWORD tim = t; DWORD rlen; DWORD rxb, txb, ev; int r;
	if ((ud->bufpt >= ud->chars) || (ud->matchpt == 255)) {
		return 1; // Already complete
	}
	if (ud->matchsz < 1) {  /* Fixed length */
		if (t < 1) { // Immediate: get what is already available up to length:
			FT_GetStatus(ud->h, &rxb, &txb, &ev);
			if (rxb < 1) return 2; // No data available
			r = FT_Read(ud->h, (LPVOID)(ud->buf+ud->bufpt), ud->chars-ud->bufpt >= rxb? rxb : ud->chars-ud->bufpt, &rlen);
		} else { // With timeout: get data up to timeout or length, whichever occurs first:
			FT_SetTimeouts(ud->h, tim*(ud->chars-ud->bufpt), 0);
			r = FT_Read(ud->h, (LPVOID)(ud->buf+ud->bufpt), ud->chars-ud->bufpt, &rlen);
		}
		if (FT_SUCCESS(r)) {
		    ud->bufpt += rlen;
			if (ud->chars == ud->bufpt) { /* Done */
				return 1;
			} else { /* Timeout */
				return 2;
			}
		} else { /* Error */
			return -1;
		}
	} else { /* Variable length terminated */
		if (ud->matchpt>=ud->matchsz) ud->matchpt = 0;
		UINT max = (ud->chars + ud->matchsz) - ud->bufpt;
		if (t < 1) {
			FT_GetStatus(ud->h, &rxb, &txb, &ev);
			if (rxb < 1) return 2; // No data available
			rxb = (rxb > max)? max : rxb;
		} else {
			FT_SetTimeouts(ud->h, tim, 0);
			rxb = max;
		}
		while (1) { // Read data one byte at a time checking for matches:
			if (FT_SUCCESS(FT_Read(ud->h, (LPVOID)(ud->buf+ud->bufpt), 1, &rlen))) {
				ud->bufpt += rlen;
				if (rlen == 1) {
					if (ud->term[ud->matchpt] == ud->buf[ud->bufpt-1])
						{ud->matchpt++;}
					else 
						{ud->matchpt=0;}
					if (ud->matchpt >= ud->matchsz) {
						ud->bufpt -= ud->matchsz;
						ud->matchpt = 255;
						return 1;
					}
					if (ud->bufpt == ud->chars+ud->matchsz) {
						ud->matchpt = 255;
						return 1;
					}
					if (--rxb == 0) return 2; // Read all available characters in immediate mode
				} else { /* Timeout */
					return 2;
				}
			} else { /* Error */
				return -1;
			}
		}
	}
}

/* Function for async polling closure.
** U1: The Device Object (uservalue).
** U2: The async callback function (must be passed U1 when called).
** U3: The Read Serial Number in U1 when the initial read corresponding to this was made.
** R1: Boolean false to cancel timer callback if packet processed, else true.
*/
static int async(lua_State* L) {
	lua_checkstack(L, 2);
	ft_ud* ud = (ft_ud*)lua_touserdata(L, lua_upvalueindex(1));
	if (ud->rdsernum != lua_tointeger(L, lua_upvalueindex(3))) {lua_pushboolean(L, FALSE); return 1;}
	if (checkrxdata(ud) == 1) {
		lua_pushvalue(L, lua_upvalueindex(2));
		lua_pushvalue(L, lua_upvalueindex(1));
		if (lua_pcall(L, 1, 0, 0) != 0) {
			return luaL_error(L, "FTDI Asynchronous Read: %s", lua_tostring(L, -1));
		}
		lua_pushboolean(L, FALSE);
		return 1;
	}
	lua_pushboolean(L, TRUE);
	return 1;
}

/* P1: Device Object (or use method syntax)
** P2: Optional Number - timeout (milliseconds) OR Function - asynchronous callback function.
** P3: Optional Number - maximum number of bytes to be received.
** P4: Optional String - 1 to 4 bytes which are the terminator values for a variable length input packet.
** R1: Boolean false for timeout or error OR String containing received packet OR asynchronous polling function.
** R2: Number of bytes received (so far if R1 = false) OR Error Message.
** Three or Four parameters is an 'initial' call. Two parameters is a 'continue' call (gets more characters
** if an 'initial' call previously returned false. One parameter is a 'peek' call (returns data received
** so far even if packet criteria are not yet met). Initial call with an asynchronous callback function
** returns an asynchronous polling function (even if the packet is already available in the input queue).
** This function is non-blocking and should be called periodically for example on a timer. It takes no
** parameters. Once a complete packet is available, the polling function will call the callback function
** passing it the device object. When the callback function returns, the polling function returns true. If
** the callback is not called, the polling function returns false.
*/
static int read(lua_State* L) {
	lua_checkstack(L, 6);
	luaL_checktype(L, 1, LUA_TUSERDATA);
	ft_ud* ud = (ft_ud*)lua_touserdata(L, 1);
	DWORD len; size_t termn = 0; int i;
	const char* term = NULL; DWORD tim = 0; void* u = NULL;
	lua_Alloc alloc;
	switch (lua_gettop(L)) {
	default: /* Initial */
		len = luaL_checkunsigned(L, 3);
		if (!lua_isnone(L, 4)) term = lua_tolstring(L, 4, &termn);
		if (termn > 4) return luaL_argerror(L, 4, "Terminator string must be 4 characters or fewer");
		ud->bufpt = ud->chars = 0; ud->matchpt = ud->matchsz = 0;
		for (i = 0; (i < (int)termn); i++) ud->term[i] = term[i];
		ud->matchsz = termn;
		if ((len + 4) > ud->bufsz) {
			alloc = lua_getallocf(L, &u);
			ud->buf = (const char*)alloc(u, (void*)ud->buf, ud->bufsz, (len + 4));
			ud->bufsz = len+4;
		}
		ud->chars = len;
		ud->rdsernum = (ud->rdsernum >= 65535)? 0 : ud->rdsernum + 1;
		if (lua_type(L, 2) == LUA_TFUNCTION) {
			lua_pushvalue(L, 1);
			lua_pushvalue(L, 2);
			lua_pushinteger(L, ud->rdsernum);
			lua_pushcclosure(L, async, 3);
			return 1;
		}
	case 2:  /* Continue, deliberate drop-through to complete Initial */
		if (tim == 0) tim = luaL_checkunsigned(L, 2);
		if (tim < 1) tim = 1;
		if (ud->chars < 1) return luaL_error(L, "Must initiate read before continuation");
		switch (checkrxdata(ud, tim)) {
		case 1: // Data ready
			lua_pushlstring(L, (const char*)ud->buf, ud->bufpt);
			lua_pushunsigned(L, ud->bufpt);
			return 2;
		case 2: // Timeout
			lua_pushboolean(L, FALSE);
			lua_pushunsigned(L, ud->bufpt);
			return 2;
		default:
			return luaL_error(L, "Error reading data from device");
		}
		break;
	case 1:  /* peek */
		if (ud->chars < 1) return luaL_error(L, "Must initiate read before any peek call");
		lua_pushlstring(L, (const char*)ud->buf, ud->bufpt);
		return 1;
		break;
	}
}

/* P1: A Device object (or use method syntax).
** Px: A list of parameters which may be strings or numbers.
** R1: Boolean true for success, false for failure.
** The parameters are processed in series. Strings are written directly as a series of bytes.
** Numbers are reduced to 32-bit unsigned comprising 6-bit op code and 26-bit parameter which
** can convieniently be created using the 'op' function from Lua.
*/
static int write(lua_State* L) {
	lua_checkstack(L, 4);
	static FT_STATUS(_stdcall *sets[])(FT_HANDLE) = { NULL, FT_SetDtr, FT_SetRts, FT_SetBreakOn };
	static FT_STATUS (_stdcall *resets[])(FT_HANDLE) = {NULL, FT_ClrDtr, FT_ClrRts, FT_SetBreakOff};
	luaL_checktype(L, 1, LUA_TUSERDATA);
	FT_HANDLE h = ((ft_ud*)lua_touserdata(L, 1))->h;
	size_t len = 0; DWORD lw = 0; int r = FALSE; int i; UINT c; UCHAR op; BOOL s; int pc = 0;
	const char* ss = NULL;
	for (i=2; (i <= lua_gettop(L)); i++) {
		pc++;
		if (lua_type(L, i) == LUA_TSTRING) {
			ss = lua_tolstring(L, i, &len);
			if ((ss != NULL) && (len > 0)) {
				r = FT_SUCCESS(FT_Write(h, (LPVOID)ss, len, &lw));
				if (len != lw) r = FALSE;
			}
			if (!r) break;
		} else {
			c = lua_tounsigned(L, i);
			op = (UCHAR)(c & 0x3F);
			s = (BOOL)((c & 0x40) != 0);
			c = (c >> 7) & 0xFFF;
			switch (op) {
			case 0: r = TRUE; break;
			case 1: case 2: case 3:
				r = FT_SUCCESS((*((s)? sets[op] : resets[op]))(h)); break;
			case 4: Sleep(c); r = TRUE; break;
			case 5: r = waitstatus(h, FT_EVENT_MODEM_STATUS, 0x10, s, c); break;
			case 6: r = waitstatus(h, FT_EVENT_MODEM_STATUS, 0x20, s, c); break;
			case 7: r = waitstatus(h, FT_EVENT_MODEM_STATUS, 0x40, TRUE, c); break;
			case 8: r = waitstatus(h, FT_EVENT_MODEM_STATUS, 0x80, TRUE, c); break;
			case 9: r = waitfor(h, FT_EVENT_RXCHAR, c); break;
			case 10: r = waitstatus(h, FT_EVENT_LINE_STATUS, 0x1000, FALSE, c); break;
			case 11: FT_GetModemStatus(h, &lw); r = ((lw & 0x0E00) == 0); break;
			case 12: r = FT_SUCCESS(FT_Purge(h, FT_PURGE_RX)); break;
			case 13: r = FT_SUCCESS(FT_Purge(h, FT_PURGE_TX)); break;
			}
			if (!r) break;
		}
	}
	lua_pushboolean(L, r);
	if (!r) {
	    lua_pushinteger(L, pc);
		return 2;
	}
	return 1;
}

/* P1: String opcode key.
** P2: Optional Boolean or Number parameter.
** P3: Optional Boolean or Number parameter.
** R1: Number (an opaque op code).
*/
static int op(lua_State* L) {
	lua_checkstack(L, 1);
	static const char* const ops[] =
	  {"nop","dtr","rts","brk","dly","cts","dsr","ri","dcd","rxd","bi","eab","prx","ptx",NULL};
	UCHAR op = luaL_checkoption(L, 1, "nop", ops);
	UINT timo = 0; BOOL stat = TRUE;
	for (int i = 2; (i <= lua_gettop(L)); i++) {
		if (i > 3) luaL_argerror(L, 3, "To many parameters");
	    switch (lua_type(L, i)) {
	    case LUA_TBOOLEAN:
		    stat = lua_toboolean(L, i);
			break;
		case LUA_TNUMBER:
			timo = lua_tointeger(L, i);
			break;
		default:
			luaL_argerror(L, i, "Incorrect argument type");
			break;
		}
	}
	if ((timo < 0) || (timo > 4095)) luaL_error(L, "Timeout must be 0..4095");
	timo = (timo << 7) + ((stat)? 64 : 0) + op;
	lua_pushunsigned(L, timo);
	return 1;
}

/* P1: A Device object (or use method syntax).
** Close the device.
*/
static int close(lua_State* L) {
	lua_checkstack(L, 2);
	void* u = NULL; int r = 1; lua_Alloc alloc;
	luaL_checktype(L, 1, LUA_TUSERDATA);
	ft_ud* ud = (ft_ud*)lua_touserdata(L, 1);
	if (ud->h != NULL) {
		r = FT_SUCCESS(FT_Close(ud->h));
		ud->h = NULL;
	}
	if (ud->buf != NULL) {
		alloc = lua_getallocf(L, &u);
		alloc(u, (void*)ud->buf, ud->bufsz, 0);
		ud->buf = NULL; ud->bufpt = ud->bufsz = ud->matchpt;
	}
	lua_pushnil(L);
	lua_setuservalue(L, 1);
	lua_pushboolean(L, r);
	return 1;
}

/* P1: A Device object (or use method syntax).
** R1: The instance data table. Table is created if it does not already exist.
*/
static int instancetable(lua_State* L) {
	lua_checkstack(L, 2);
	luaL_checktype(L, 1, LUA_TUSERDATA);
	lua_getuservalue(L, 1);
	if (!lua_istable(L, -1)) {
		lua_newtable(L);
		lua_pushvalue(L, -1);
		lua_setuservalue(L, 1);
	}
	return 1;
}

/* device object */
void dev_Create(lua_State* L)
{
	static const luaL_Reg ml [] = {
		{"info", info},
		{"status", status},
		{"read", read},
		{"write", write},
		{"instancetable", instancetable},
		{"close", close},
		{"__gc", close},
		{NULL, NULL}
	};
	lua_createtable(L, 0, sizeof(ml) / sizeof(ml[0]) - 1);
	lua_pushnil(L);
	lua_setmetatable(L, -2);	//Make sure metatable has no metatable
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");	//Metatable indexes to self
	lua_pushstring(L, "_TID");
	lua_pushvalue(L, -2);		//First upvalue of class closure is TID marker
	lua_pushcclosure(L, open, 2);	//Create the class closure with two upvalues, marker and metatable
	lua_pushvalue(L, -1);
	lua_insert(L, -3);
	luaL_setfuncs(L, ml, 1);	//Insert methods in the metatable, each with upvalue referencing the class closure
	lua_pop(L, 1);				//Leave just the class closure on the stack
};

/* library */
static const luaL_Reg ftd2xx_funcs[] = {
	{"ports", ports },
	{"op", op},
	{NULL, NULL}
};

LUAFTLIB_API int LUAFTLIB_NGEN(luaopen_)(lua_State *L)
{
	lua_checkstack(L, 6);
	luaL_newlib(L, ftd2xx_funcs);
	dev_Create(L);
	lua_setfield(L, -2, "Port");
	return 1;
}
