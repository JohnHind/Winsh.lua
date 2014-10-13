#include "stdafx.h"
#define LUACMLIB_BUILDING
#include "LibClass.h"
#include "LibComLink.h"
#include "..\LuaLibIF.h"
#include "..\resource.h"

typedef struct com_ud {
	HANDLE h;		    // Handle of Com Port
	OVERLAPPED wo;      // Overlapped structure for Asynch file handling
	DWORD tw;           // Timeout in milliseconds per byte for write operations
	UINT rdsernum;		// Counter incremented each Read 'init' call
	UINT chars;			// Maximum number of bytes in the Read Packet
	UINT bufsz;			// Size of buffer
	UINT bufpt;			// Pointer to next location in buffer
	BYTE matchpt;       // Current position of terminator match in term
	BYTE matchsz;       // Number of bytes in terminator
	const char* buf;	// Packet buffer (at least chars + 4)
	char term[4];       // Terminator bytes
} com_ud;

int checkfield(lua_State* L, int ix, const char* key) {
	lua_getfield(L, ix, key);
	if (lua_isnil(L, -1)) { lua_pop(L, 1); return FALSE; }
	return TRUE;
}

UINT pop_unsigned(lua_State* L) {
	UINT r = lua_tounsigned(L, -1);
	lua_pop(L, 1);
	return r;
}

double pop_real(lua_State* L) {
	double r = (double)lua_tonumber(L, -1);
	lua_pop(L, 1);
	return r;
}

int pop_option(lua_State* L, const char *const lst[]) {
	int r = luaL_checkoption(L, -1, NULL, lst);
	lua_pop(L, 1);
	return r;
}

/* Basic IO, synchronous with timeouts. Must use asynchronous operations with completion waits
** as this is the only way to get timeouts on modem signal waits.
*/

/* Write 'len' bytes from buffer 'ss' and wait for completion. Return TRUE on success.
*/
int writebytes(com_ud* ud, const char* ss, DWORD len) {
	DWORD lw;
	ud->wo.Offset = ud->wo.OffsetHigh = 0; ResetEvent(ud->wo.hEvent);
	if (!WriteFile(ud->h, (LPCVOID)ss, len, NULL, &ud->wo)) if (GetLastError() != ERROR_IO_PENDING) return FALSE;
	if (WaitForSingleObject(ud->wo.hEvent, ud->tw * len) != WAIT_OBJECT_0) return FALSE;
	if (!GetOverlappedResult(ud->h, &ud->wo, &lw, FALSE)) return FALSE;
	if (len != lw) return FALSE;
	return TRUE;
}

/* Read bytes to buffer in 'ud' until 'to' milliseconds elapsed, 'len' bytes read, or buffer full. Return number of bytes read.
*/
DWORD readbytes(com_ud* ud, DWORD len, DWORD to) {
	DWORD lr;
	ud->wo.Offset = ud->wo.OffsetHigh = 0; ResetEvent(ud->wo.hEvent);
	if (!ReadFile(ud->h, (LPVOID)(ud->buf + ud->bufpt), len, NULL, &ud->wo)) if (GetLastError() != ERROR_IO_PENDING) return 0;
	WaitForSingleObject(ud->wo.hEvent, to);
	if (!GetOverlappedResult(ud->h, &ud->wo, &lr, FALSE)) return 0;
	return lr;
}

/* Wait for up to 'to' milliseconds for a modem event. Return TRUE if event detected, FALSE on timeout.
** 'ev' must be EV_CTS; EV_DSR; EV_RING; EV_RLSD; EV_BREAK; EV_ERR; EV_RXCHAR; EV_RXFLAG; EV_TXEMPTY or an OR combination of these.
** For the first four, may also distinguish assert from unassert by including the last two parameters
** 'ms' must be MS_CTS_ON; MS_DSR_ON; MS_RING_ON; MS_RLSD_ON and must match the 'ev' flag.
** 'state' should be TRUE to trigger when signal is asserted, FALSE for unasserted.
** RLSD is also known as DCD (Data Carrier Detect).
*/
int waitfor(com_ud* ud,  DWORD ev, DWORD to, DWORD ms = 0, int state = FALSE) {
	DWORD cs, em;
	if (ms > 0) {
		if (!GetCommModemStatus(ud->h, &cs)) return FALSE;
		if (((cs & ms) > 1) == to) return TRUE; // Already in the desired state
	}
	if (!SetCommMask(ud->h, ev)) return FALSE;
	ud->wo.Offset = ud->wo.OffsetHigh = 0; ResetEvent(ud->wo.hEvent);
	if (!WaitCommEvent(ud->h, &em, &ud->wo)) if (GetLastError() != ERROR_IO_PENDING) return FALSE;
	if (WaitForSingleObject(ud->wo.hEvent, to) == WAIT_OBJECT_0) return TRUE;
	return FALSE;
}

/* P1: Nil (Invariant State)
** P2: Number index of previous device (Control Variable)
** R1: Number index of current device
** R2: Table with field "open", true if port is open, false if it is available.
*/
static int iterf(lua_State* L) {
	char fn[12]; HANDLE hPort;
	DWORD p = (DWORD)lua_tointeger(L, 2);
	while (p <= 255) {
		sprintf_s(fn, 12, "\\\\.\\COM%u", ++p);
		hPort = CreateFileA(fn, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
		if (hPort != INVALID_HANDLE_VALUE) {
			CloseHandle(hPort);
			lua_pushinteger(L, p);
			lua_createtable(L, 0, 0);
			return 2;
		} else if (GetLastError() == 5) {
			lua_pushinteger(L, p);
			lua_createtable(L, 0, 0);
			lua_pushboolean(L, TRUE);
			lua_setfield(L, -2, "open");
			return 2;
		}
	}
	return 0;
}

/* R1: Function iterf.
** R2: Nil (Invariant State)
** R3: Number 0 (Initial value of Control Variable)
*/
static int ports(lua_State* L) {
	lua_checkstack(L, 3);
	lua_pushcfunction(L, iterf);
	lua_pushnil(L);
	lua_pushinteger(L, 0);
	return 3;
}

/* P1: String opcode key.
** P2: Optional Boolean or Number parameter.
** P3: Optional Boolean or Number parameter.
** R1: Number (an opaque op code).
*/
static int op(lua_State* L) {
	lua_checkstack(L, 1);
	static const char* const ops[] =
	{ "nop", "dtr", "rts", "brk", "dly", "cts", "dsr", "ri", "dcd", "rxd", "bi", "eab", "prx", "ptx", NULL };
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
	timo = (timo << 7) + ((stat) ? 64 : 0) + op;
	lua_pushunsigned(L, timo);
	return 1;
}

static int port_construct(lua_State* L) {
	lua_checkstack(L, 4);
	static const char* const paritytx[] = { "none", "odd", "even", "mark", "space", NULL };
	static const char* const flowtx[] = { "none", "rtscts", "dtrdsr", "xonxoff", NULL };
	HANDLE h = INVALID_HANDLE_VALUE; DWORD dw = 0; char fn[12]; double rv;
	com_ud* ud = NULL;
	DCB dcb; COMMTIMEOUTS cto;
	luaL_checktype(L, 1, LUA_TTABLE);
	if (checkfield(L, 1, "index")) {
		dw = pop_unsigned(L);
		if (dw > 0) {
			sprintf_s(fn, 12, "\\\\.\\COM%u", dw);
			h = CreateFileA(fn, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
		}
	}
	/* Check device opened: */
	if (h == INVALID_HANDLE_VALUE) {
		lua_pushboolean(L, FALSE);
		return 1;
	}
	/* Establish default settings: */
	dcb.DCBlength = sizeof(dcb);
	dcb.fOutxCtsFlow = false; dcb.fOutxDsrFlow = false;
	dcb.fOutX = false; dcb.fInX = false;
	dcb.fRtsControl = RTS_CONTROL_ENABLE; dcb.fDtrControl = DTR_CONTROL_ENABLE;
	dcb.fTXContinueOnXoff = true; dcb.fDsrSensitivity = false;
	dcb.BaudRate = 2400; dcb.ByteSize = 8;
	dcb.EofChar = 0x04; dcb.ErrorChar = 0; dcb.EvtChar = 0;
	dcb.fAbortOnError = true; dcb.fBinary = true; dcb.fDummy2 = false;
	dcb.fErrorChar = false; dcb.fNull = false; dcb.fParity = false;
	dcb.Parity = NOPARITY; dcb.StopBits = ONESTOPBIT;
	dcb.wReserved = 0; dcb.wReserved1 = 0;
	dcb.XoffChar = 0x13; dcb.XoffLim = 0; dcb.XonChar = 0x11; dcb.XonLim = 0;
	cto.ReadIntervalTimeout = MAXDWORD; cto.ReadTotalTimeoutConstant = 0; cto.ReadTotalTimeoutMultiplier = 0;
	cto.WriteTotalTimeoutConstant = 0; cto.WriteTotalTimeoutMultiplier = 0;

	/* Baud Rate: */
	if (checkfield(L, 1, "baudrate")) {
		dw = pop_unsigned(L);
		if (dw > 0) dcb.BaudRate = dw;
	}
	/* Data Characteristics: */
	if (checkfield(L, 1, "wordlength")) {
		dw = pop_unsigned(L);
		if ((dw >= 4) && (dw <= 8)) dcb.ByteSize = (BYTE)dw;
	}
	if (checkfield(L, 1, "stopbits")) {
		rv = pop_real(L);
		if ((rv >= 1) && (rv <= 2)) {
			if (rv == 1.0)
				dcb.StopBits = 0;
			else if (rv == 2.0)
				dcb.StopBits = 2;
			else
				dcb.StopBits = 1;
		}
	}
	if (checkfield(L, 1, "parity")) {
		dcb.Parity = pop_option(L, paritytx);
		if (dcb.Parity != 0) dcb.fParity = TRUE;
	}
	/* Flow Control: */
	if (checkfield(L, 1, "flowcontrol")) {
		switch (pop_option(L, flowtx)) {
		case 1: //rtscts
			dcb.fOutxCtsFlow = true; dcb.fOutxDsrFlow = false;
			dcb.fOutX = false; dcb.fInX = false;
			dcb.fRtsControl = RTS_CONTROL_HANDSHAKE; dcb.fDtrControl = DTR_CONTROL_ENABLE;
			dcb.fTXContinueOnXoff = true; dcb.fDsrSensitivity = false;
			break;
		case 2: //dtrdsr
			dcb.fOutxCtsFlow = false; dcb.fOutxDsrFlow = true;
			dcb.fOutX = false; dcb.fInX = false;
			dcb.fRtsControl = RTS_CONTROL_ENABLE; dcb.fDtrControl = DTR_CONTROL_HANDSHAKE;
			dcb.fTXContinueOnXoff = true; dcb.fDsrSensitivity = false;
			break;
		case 3: //xonxoff
			dcb.fOutxCtsFlow = false; dcb.fOutxDsrFlow = false;
			dcb.fOutX = true; dcb.fInX = true;
			dcb.fRtsControl = RTS_CONTROL_ENABLE; dcb.fDtrControl = DTR_CONTROL_ENABLE;
			dcb.fTXContinueOnXoff = true; dcb.fDsrSensitivity = false;
			if (checkfield(L, 1, "xon")) {
				dw = pop_unsigned(L);
				if ((dw > 0) && (dw <= 0xFF)) dcb.XonChar = (char)dw;
			}
			if (checkfield(L, 1, "xoff")) {
				dw = pop_unsigned(L);
				if ((dw > 0) && (dw <= 0xFF)) dcb.XonChar = (char)dw;
			}
			break;
		default: //none
			break;
		}
	}
	/* Apply the settings: */
	if (!SetCommTimeouts(h, &cto)) {CloseHandle(h); lua_pushboolean(L, FALSE); return 1;};
	if (!SetCommState(h, &dcb)) {CloseHandle(h); lua_pushboolean(L, FALSE); return 1;};

	/* Wrap handle as userdata object and return it: */
	ud = (com_ud*)lua_newuserdata(L, sizeof(com_ud));
	ud->tw = (1000*12) / dcb.BaudRate; // milliseconds per character, 12 bits per character for worst case headroom.
	if (ud->tw < 1) ud->tw = 1;
	ud->rdsernum = 0; ud->h = h; ud->buf = NULL; ud->bufpt = ud->bufsz = ud->chars = 0; ud->matchpt = ud->matchsz = 0;
	ud->wo.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	lua_pushvalue(L, lua_upvalueindex(2)); //Metatable is second upvalue in this closure
	lua_setmetatable(L, -2);									//|T|
	return 1;
}

/* P1: A Device object (or use method syntax).
** Px: A list of parameters which may be strings or numbers.
** R1: Boolean true for success, false for failure.
** The parameters are processed in series. Strings are written directly as a series of bytes.
** Numbers are reduced to 32-bit unsigned comprising 6-bit op code and 26-bit parameter which
** can convieniently be created using the 'op' function from Lua.
*/
static int port_write(lua_State* L) {
	lua_checkstack(L, 4);
	luaL_checktype(L, 1, LUA_TUSERDATA);
	com_ud* ud = (com_ud*)lua_touserdata(L, 1);
	size_t len = 0; DWORD lw = 0; int r = FALSE; int i; UINT c; UCHAR op; BOOL s; int pc = 0;
	const char* ss = NULL;
	for (i = 2; (i <= lua_gettop(L)); i++) {
		pc++;
		if (lua_type(L, i) == LUA_TSTRING) {
			r = FALSE;
			ss = lua_tolstring(L, i, &len);
			if ((ss != NULL) && (len > 0)) r = writebytes(ud, ss, len);
			if (!r) break;
		}
		else {
			c = lua_tounsigned(L, i);
			op = (UCHAR)(c & 0x3F);
			s = (BOOL)((c & 0x40) != 0);
			c = (c >> 7) & 0xFFF;
			switch (op) {
			case 0: r = TRUE; break;
			case 1: r = EscapeCommFunction(ud->h, s ? SETDTR : CLRDTR); break;
			case 2: r = EscapeCommFunction(ud->h, s ? SETRTS : CLRRTS); break;
			case 3: r = EscapeCommFunction(ud->h, s ? SETBREAK : CLRBREAK); break;
			case 4: Sleep(c); r = TRUE; break;
			case 5: r = waitfor(ud, EV_CTS, c, MS_CTS_ON, s); break;
			case 6: r = waitfor(ud, EV_DSR, c, MS_DSR_ON, s); break;
			case 7: r = waitfor(ud, EV_RING, c, MS_RING_ON, s); break;
			case 8: r = waitfor(ud, EV_RLSD, c, MS_RLSD_ON, s); break;
			case 9: r = waitfor(ud, EV_RXCHAR, c); break;
			case 10: r = waitfor(ud, EV_BREAK, c); break;
			case 11: r = FALSE;  if (ClearCommError(ud->h, &lw, NULL)) r = (lw == 0); break;
			case 12: r = PurgeComm(ud->h, PURGE_RXCLEAR | PURGE_RXABORT); break;
			case 13: r = PurgeComm(ud->h, PURGE_TXABORT | PURGE_TXCLEAR); break;
			default: r = FALSE; break;
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

/* Read data and check for packet termination conditions. If t is supplied and is greater than 0, the routine
** will block for up to that number of milliseconds per byte to be received. Else only data already received
** is processed and the routine does not block. Returns: 1 if the complete packet is available; 2 for timeout
** with incomplete packet or no data; -1 for communications error.
*/
static int checkrxdata(com_ud* ud, DWORD t = 0) {
	DWORD tim = t; DWORD rlen; int r; DWORD er; COMSTAT cs;
	if ((ud->bufpt >= ud->chars) || (ud->matchpt == 255)) {
		return 1; // Already complete
	}
	ClearCommError(ud->h, &er, &cs);
	if (ud->matchsz < 1) {  /* Fixed length */
		if (t < 1) { // Immediate: get what is already available up to length:
			if (cs.cbInQue < 1) return 2; // No data available
			rlen = ud->chars - ud->bufpt >= cs.cbInQue ? cs.cbInQue : ud->chars - ud->bufpt;
			r = (readbytes(ud, rlen, 0) == rlen);
		}
		else { // With timeout: get data up to timeout or length, whichever occurs first:
			rlen = readbytes(ud, ud->chars - ud->bufpt, tim*(ud->chars - ud->bufpt));
			r = TRUE;
		}
		if (r) {
			ud->bufpt += rlen;
			if (ud->chars == ud->bufpt) { /* Done */
				return 1;
			}
			else { /* Timeout */
				return 2;
			}
		}
		else { /* Error */
			return -1;
		}
	}
	else { /* Variable length terminated */
		if (ud->matchpt >= ud->matchsz) ud->matchpt = 0;
		UINT max = (ud->chars + ud->matchsz) - ud->bufpt;
		if (t < 1) {
			if (cs.cbInQue < 1) return 2; // No data available
			cs.cbInQue = (cs.cbInQue > max) ? max : cs.cbInQue;
		}
		else {
			cs.cbInQue = max;
		}
		while (1) { // Read data one byte at a time checking for matches:
			if (readbytes(ud, 1, tim) == 1) {
				ud->bufpt++;
				if (ud->term[ud->matchpt] == ud->buf[ud->bufpt - 1]) {
					ud->matchpt++;
				} else {
					ud->matchpt = 0;
				}
				if (ud->matchpt >= ud->matchsz) {
					ud->bufpt -= ud->matchsz;
					ud->matchpt = 255;
					return 1;
				}
				if (ud->bufpt == ud->chars + ud->matchsz) {
					ud->matchpt = 255;
					return 1;
				}
				if (--cs.cbInQue == 0) return 2; // Read all available characters in immediate mode
			}
			else {
				return 2; // Timeout
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
	com_ud* ud = (com_ud*)lua_touserdata(L, lua_upvalueindex(1));
	if (ud->rdsernum != lua_tointeger(L, lua_upvalueindex(3))) { lua_pushboolean(L, FALSE); return 1; }
	if (checkrxdata(ud) == 1) {
		lua_pushvalue(L, lua_upvalueindex(2));
		lua_pushvalue(L, lua_upvalueindex(1));
		if (lua_pcall(L, 1, 0, 0) != 0) {
			return luaL_error(L, "Com Asynchronous Read: %s", lua_tostring(L, -1));
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
static int port_read(lua_State* L) {
	lua_checkstack(L, 6);
	luaL_checktype(L, 1, LUA_TUSERDATA);
	com_ud* ud = (com_ud*)lua_touserdata(L, 1);
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
			ud->bufsz = len + 4;
		}
		ud->chars = len;
		ud->rdsernum = (ud->rdsernum >= 65535) ? 0 : ud->rdsernum + 1;
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
** Pn: Any number of string keys specifying which status values to return in what order.
** Rn: The same number of status values of the appropriate types.
** 'cts','dsr','ri','dcd' - Boolean state of modem control inputs.
** 'oe','pe','fe' - Boolean presense of error flags for overrun, parity, framing errors.
** 'bi' - Boolean flag for break condition interrupt.
** 'rxq','txq' - Number of characters in the receive or transmit queue.
** 'ctsh', 'dsrh', 'dcdh', 'xoffh', 'xoffsh' - Transmission held waiting condition.
** 'eof' - End of file was received.
** 'im' - An "immediate" character is waiting transmission ahead of the queue.
*/
static int port_status(lua_State* L) {
	lua_checkstack(L, 4);
	static const char* const skey[] = { "cts", "dsr", "ri", "dcd", "oe", "pe", "fe", "bi", "rxq", "txq", 
		"ctsh", "dsrh", "dcdh", "xoffh", "xoffsh", "eof", "im", NULL };
	luaL_checktype(L, 1, LUA_TUSERDATA);
	HANDLE h = ((com_ud*)lua_touserdata(L, 1))->h;
	int n = lua_gettop(L); int i; int t; DWORD ms = 0xFFFF; COMSTAT cs; DWORD er = 0xFFFF;
	for (i = n; (i > 1); i--) {
		t = luaL_checkoption(L, i, NULL, skey);
		lua_remove(L, i);
		if ((ms == 0xFFFF) && (t < 4)) GetCommModemStatus(h, &ms);
		if ((er == 0xFFFF) && (t >= 4)) ClearCommError(h, &er, &cs);
		switch (t) {
		case 0: //cts
			lua_pushboolean(L, (ms & MS_CTS_ON) > 0);
			break;
		case 1: //dsr
			lua_pushboolean(L, (ms & MS_DSR_ON) > 0);
			break;
		case 2: //ri
			lua_pushboolean(L, (ms & MS_RING_ON) > 0);
			break;
		case 3: //dcd
			lua_pushboolean(L, (ms & MS_RLSD_ON) > 0);
			break;
		case 4: //oe
			lua_pushboolean(L, (er & CE_OVERRUN) > 0);
			break;
		case 5: //pe
			lua_pushboolean(L, (er & CE_RXPARITY) > 0);
			break;
		case 6: //fe
			lua_pushboolean(L, (er & CE_FRAME) > 0);
			break;
		case 7: //bi
			lua_pushboolean(L, (er & CE_BREAK) > 0);
			break;
		case 8: //rxq
			lua_pushinteger(L, cs.cbInQue);
			break;
		case 9: //txq
			lua_pushinteger(L, cs.cbOutQue);
			break;
		case 10: //ctsh
			lua_pushboolean(L, cs.fCtsHold);
			break;
		case 12: //dsrh
			lua_pushboolean(L, cs.fDsrHold);
			break;
		case 13: //dcdh
			lua_pushboolean(L, cs.fRlsdHold);
			break;
		case 14: //xoffh
			lua_pushboolean(L, cs.fXoffHold);
			break;
		case 15: //xoffsh
			lua_pushboolean(L, cs.fXoffSent);
			break;
		case 16: //eof
			lua_pushboolean(L, cs.fEof);
			break;
		case 17: //im
			lua_pushboolean(L, cs.fTxim);
			break;
		default:
			lua_pushnil(L);
		}
		lua_insert(L, i);
	}
	n--;
	return n;
}

static int port_info(lua_State* L) {
	lua_checkstack(L, 3);
	lua_createtable(L, 0, 1);
	lua_pushboolean(L, TRUE);
	lua_setfield(L, -2, "open");
	return 1;
}

/* P1: A Device object (or use method syntax).
** R1: The instance data table. Table is created if it does not already exist.
*/
static int port_instancetable(lua_State* L) {
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

static int port_close(lua_State* L) {
	lua_checkstack(L, 2);
	void* u = NULL; int r = 1; lua_Alloc alloc;
	luaL_checktype(L, 1, LUA_TUSERDATA);
	com_ud* ud = (com_ud*)lua_touserdata(L, 1);
	if (ud->h != INVALID_HANDLE_VALUE) {
		CloseHandle(ud->h);
		ud->h = INVALID_HANDLE_VALUE;
	}
	if (ud->wo.hEvent != INVALID_HANDLE_VALUE) {
		CloseHandle(ud->wo.hEvent);
		ud->wo.hEvent = INVALID_HANDLE_VALUE;
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

static void com_Create(lua_State* L)
{
	static const struct luaL_Reg ml [] = {
		{"write", port_write},
		{"read", port_read},
		{"status", port_status},
		{"info", port_info},
		{"instancetable", port_instancetable},
		{"close", port_close},
		{"__gc", port_close},
		{NULL, NULL}
	};
	luaC_newclass(L, port_construct, ml);
}

/* library */
static const luaL_Reg comlink_funcs[] = {
	{ "ports", ports },
	{ "op", op },
	{ NULL, NULL }
};

LUACMLIB_API int LUACMLIB_NGEN(luaopen_)(lua_State* L)
{
	WINSH_LUA(2)
	luaL_newlib(L, comlink_funcs);
	com_Create(L);
	lua_setfield(L, -2, "Port");
	return 1;
}
