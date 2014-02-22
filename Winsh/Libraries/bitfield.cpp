/* bitfield.c      (c) 2013 John Hind (john.hind@zen.co.uk)
**      Lua add-in data type bitfield.dll
** Same license as Lua 5.2.2 (c) 1994-2012 Lua.org, PUC-Rio
** http://www.lua.org/license.html
*/

/* on Linux compile in directory also containing lua header files using:
** > cc -shared bitfield.c -o bitfield.so
*/

/* on Windows, define the symbols 'LUA_BUILD_AS_DLL' and 'LUA_LIB' and
** compile and link with stub library lua52.lib (for lua52.dll)
** generating bitfield.dll. If necessary, generate lua52.lib and lua52.dll by 
** compiling Lua sources with 'LUA_BUILD_AS_DLL' defined.
*/

#include "stdafx.h"
#define LUACLLIB_BUILDING
#include "LibClass.h"
#include "..\LuaLibIF.h"

//#include <stdlib.h>
//#include <ctype.h>

//#include "lua.h"
//#include "lauxlib.h"
//#include "lualib.h"

/* Enable with Lua 5.1 - from https://github.com/hishamhm/lua-compat-5.2 */
//#include "compat-5.2.h"

#define LIBVERSION ("Bitfield 1.0")

//#define METANAME ("jh-bitfield")

#define BYTE unsigned char

#define USBITS (sizeof(lua_Unsigned)*8)				 /* Max number of bits in Unsigned type */

/* USERDATA: In C, byte indexes and bitfield width are stored in BYTE. Indexes run 0..255
** and width is one less than the actual width (0 means 1 bit, 255 means 256 bits). On the Lua
** side, width is expressed correctly and indexes are 1 based (1..256).
*/
typedef struct bf_ud { BYTE c; BYTE d; } bf_ud;
#define UDBYTES(w) ((((((w)+1)&7)==0)?0:1)+(((w)+1)>>3)) /* Width in bytes from ud->c (bits - 1) */
#define UDBYTEIX(ud,ix) (*((&(ud)->d)+((ix)>>3)))        /* Reference to byte containing bit index */
#define UDBYTEOF(ud,of) (*((&(ud)->d)+(of)))			 /* Reference to byte at given offset */
#define UDMASK(ix) (1<<((ix)&7))                         /* Mask for bit in byte, from bit index */
#define UDBITS(ud) ((ud)->c)							 /* Reference to bits-1 in ud */

/* RANGE: Ranges are keyed using a 4-byte Lua string. The first byte is constant 255 (to make
** sure the string is non-printable). The second byte is the type constant (BF_ keys below).
** The third byte is the start index (0-base) and the fourth is the end index (0-base). For
** constants, followed by the value in packed format. For unbound constants, the start index
** is ignored and the end index is one less than the length in bits.
*/
typedef struct ra_ke { BYTE m; BYTE t; BYTE s; BYTE e; } ra_ke;
#define RAMARK(s) ((BYTE)(s)[0])
#define RATYPE(s) ((BYTE)(s)[1])
/* Type keys */
#define BF_TBITFIELD (1)	/* Userdata type in Lua with bitfield metatable */
#define BF_TBOOLEAN (2)		/* Boolean type in Lua - represents single bit */
#define BF_TUNSIGNED (3)	/* Number type in Lua - represents up to 32 bits as unsigned */
#define BF_TBINARY (4)		/* String type in Lua - comprising '1' and '0' characters */
#define BF_TPACKED (5)		/* String type in Lua - non-printable, character codes 8-bit binary */
#define BF_TCONST (6)		/* Constant bound to a field */
#define BF_TUCONST (7)		/* Constant not bound to a field */
#define BF_TMAX (7)
/* */
#define RASTART(s) ((BYTE)(s)[2])
#define RAEND(s) ((BYTE)(s)[3])
#define RACONST(s) ((const char*)((s)+4))

/* Create new ud of width, init to all 0, push it onto stack and return pointer to ud */
bf_ud* makeud(lua_State* L, int width) {
	bf_ud* ud; int i;
	if (width > 256) width = 256; if (width < 1) width = 1;
	ud = (bf_ud*)lua_newuserdata(L, 1 + UDBYTES(width-1));
#if !defined(LUA_VERSION_NUM) || LUA_VERSION_NUM == 501
	/* compat-5.2 does not address this: in 5.1, userdata will have an environment table */
	/* note that the uservalue cannot be nil in 5.1, so we use an empty table for no NRT */
	lua_createtable(L,0,0);
	lua_setuservalue(L,-2);
#endif
	UDBITS(ud) = width-1;
	for (i=0; (i < UDBYTES(UDBITS(ud))); i++) UDBYTEOF(ud,i) = 0;
//	luaL_getmetatable(L, METANAME);
//	
//	lua_setmetatable(L, -2);
	return ud;
}

/* Set an individual bit in ud */
void setbit(bf_ud* ud, BYTE ix, BYTE v) {
	if (v==0)
		UDBYTEIX(ud,ix) &= ~UDMASK(ix);
	else
		UDBYTEIX(ud,ix) |= UDMASK(ix);
}

/* Return an individual bit from ud */
int getbit(bf_ud* ud, BYTE ix) {
	if (ix > UDBITS(ud)) return 0;
	return (UDBYTEIX(ud,ix) & UDMASK(ix)) != 0;
}

/* Sets range starting at sp in ud from all bits in su, truncates any excess bits
** Returns next index after the range
*/
int setbitfield(bf_ud* ud, bf_ud* su, BYTE sp) {
	int i;
	for (i = 0; ((i <= UDBITS(su)) && ((i + sp) <= UDBITS(ud))); i++)
		setbit(ud, i + sp, getbit(su, i));
	return sp + UDBITS(su) + 1;
}

/* Sets range sp to ep in ud from an unsigned value at stack ix
** Truncates excess most significant bits or fill with 0 if too few bits
** Returns the next index after the range
*/
int setunsigned(lua_State* L, bf_ud* ud, int ix, BYTE sp, BYTE ep) {
	int i;
	lua_Unsigned u = lua_tounsigned(L, ix);
	for (i = sp; (i <= ep); i++) {
		setbit(ud, i, u&1); // Masks to 0 after full width of lua_Unsigned.
		u = u>>1;
	}
	return ep + 1;
}

/* Sets range sp to ep in ud from a binary string at stack ix
** ep may be < 0 in which case it is taken from the size of the string
** Repeats the string from the right end as necessary to fill the range, fills with '0' if string is empty
** Returns the next index after the range
*/
int setbinary(lua_State* L, bf_ud* ud, int ix, BYTE sp, int ep) {
	int i; const char* p; size_t sz; const char* x; char v; BYTE e;
	p = lua_tolstring(L, ix, &sz);
	if (sz > 256) sz = 256;
	if (ep < 0) e = sp + sz; else e = ep;
	x = p + sz - 1; v = '0';
	for (i = (int)sp; (i <= (int)e); i++) {
		if (sz > 0) { v = *x; if (x-- == p) x = p + sz - 1; }
		setbit(ud, i, (v != '0'));
	}
	return e + 1;
}

/* Sets range sp to ep in ud from packed format string at stack ix
** Returns the next index after the range
*/
int setpacked(lua_State* L, bf_ud* ud, int ix, BYTE sp, BYTE ep) {
	const char* ss; size_t sl; int i; int p; BYTE m; BYTE v;
	ss = lua_tolstring(L, ix, &sl);
	p = sl - 1; m = 1;
	for (i = (int)sp; (i <= (int)ep); i++) {
		if (p < 0) {v = 0;} else {v = ss[p];}
		setbit(ud, i, (v & m) != 0);
		if (m == 128) {p--; m = 1;} else {m = m<<1;}
	}
	return ep + 1;
}

/* Allocate a string buffer of size sz in a userdata.
** Returns pointer to UD which is also pushed on stack
*/
char* buffinitsize(lua_State* L, size_t sz) {
	return (char*)lua_newuserdata(L, sz * sizeof(char));
}

/* Converts UD at stack top into a string of size sz */
void bufftostring(lua_State* L, size_t sz) {
	lua_pushlstring(L, (char*)lua_touserdata(L, -1), sz);
	lua_remove(L, -2);
}

/* Creates a packed string from the contents of ud between sp and ep and pushes it onto the stack
** Returns the number of values pushed (1)
*/
int savepacked(lua_State* L, bf_ud* ud, BYTE sp, BYTE ep) {
	int i; int bc; int by; int bp; BYTE x; char* bb;
	by = UDBYTES(ep - sp);
	bb = buffinitsize(L, by);
	bc = 0; x = 0; bp = by;
	for (i = (int)sp; (i <= (int)ep); i++) {
		x |= ((BYTE)(getbit(ud, i)? 1 : 0)) << bc;
		if (++bc == 8) { bb[--bp] = x; bc = 0; x = 0; }
	}
	if (bc > 0) bb[0] = x;
	bufftostring(L, by);
	return 1;
}

/* Pushes a packed string representing the constant value at ix which may be a boolean,
** a binary string or a number (unsigned). Returns the number of values pushed (1)
*/
int getpacked(lua_State* L, int ix, int nb) {
	const char* s; size_t ln; int i; int bc; int bp; BYTE x; int by; lua_Unsigned v; char* bb;
	by = UDBYTES(nb - 1);
	bb = buffinitsize(L, by);
	for (i=0; (i < by); i++) bb[i] = 0;
	switch (lua_type(L, ix)) {
	case LUA_TBOOLEAN:
		bb[0] = (BYTE)(lua_toboolean(L, ix)? 1 : 0);
		break;
	case LUA_TSTRING:
		s = lua_tolstring(L, ix, &ln);
		if ((int)ln > nb) ln = nb;
		bc = 0; x = 0; bp = by;
		for (i = (int)ln - 1; (i >= 0); i--) {
			x |= ((BYTE)(s[i] == '1')? 1 : 0) << bc;
			if (++bc == 8) {bb[--bp] = x; bc = 0; x = 0;}
		}
		if (bc > 0) bb[--bp] = x;
		break;
	case LUA_TNUMBER:
		v = lua_tounsigned(L, ix);
		bp = by;
		for (i = 0; (i <= USBITS); i++) {
			x = (BYTE)((v >> (8*i)) & 0xFF);
			bb[--bp] = x;
			if (bp < 1) break;
		}
		break;
	default:
		return luaL_argerror(L, ix, "Invalid constant value");
	}
	bufftostring(L, by);
	return 1;
}

/* ty - type code; sp, ep - start and end indexes
** pushes extracted value onto stack and returns number of values pushed (1 or 0)
*/
int extractvalue(lua_State* L, BYTE ty, bf_ud* ud, BYTE sp, BYTE ep) {
	bf_ud* ux; char* bp; int i, j; lua_Unsigned u = 0;
	switch (ty) {
	case BF_TBOOLEAN:
		if (ep > sp) return luaL_error(L, "Bitfield range too large for Boolean value");
		lua_pushboolean(L, getbit(ud, sp));
		return 1;
	case BF_TUNSIGNED:
		if (((ep - sp) + 1) > USBITS)
			return luaL_error(L, "Bitfield range too large for Number value");
		for (i = ep; (i >= (int)sp); i--) {
			u = u<<1;
			if (getbit(ud, i)) u |= 1;
		}
		lua_pushunsigned(L, u);
		return 1;
	case BF_TBITFIELD:
		ux = makeud(L, (ep - sp) + 1);
		if (luaC_gettid(L, lua_upvalueindex(1)) != 1) return luaL_error(L, "Bad Class");
		lua_setmetatable(L, -2);
		j = 0; for (i = sp; (i <= ep); i++) setbit(ux, j++, getbit(ud, i));
		return 1;
	case BF_TBINARY:
		bp = buffinitsize(L, (ep-sp)+1);
		for (i = sp; (i <= ep); i++) bp[i-sp] = (getbit(ud, ep - i))? '1' : '0';
		bufftostring(L, (ep-sp)+1);
		return 1;
	case BF_TPACKED:
		savepacked(L, ud, sp, ep);
		return 1;
	default:
		return 0;
	}
	return 0;
}

/* ixx - ud on stack; ixn - range string or range name on stack
** Returns 0 if not a range or range name; the type code if valid range ; or negative error code.
** If valid range, sets sp and ep to start and end indexes (unless the pointer is null).
** if range is a constant, the packed string constant value is pushed onto stack.
*/
int getrange(lua_State* L, int ixx, int ixn, BYTE* sp, BYTE* ep) {
	size_t sz; const char* nm;
	nm = lua_tolstring(L, ixn, &sz);
	if ((sz < 4) || (RAMARK(nm) != 255)) { /* If not a range, check for range name */
		lua_getuservalue(L, ixx);
		if (lua_isnil(L, -1)) {lua_pop(L, 1); return 0;} /* No name index */
		lua_getfield(L, -1, nm);
		if (lua_type(L, -1) != LUA_TSTRING) {lua_pop(L, 2); return 0;} /* Name not in index */
		nm = lua_tolstring(L, -1, &sz);
		lua_pop(L, 2);
	}
	if ((sz < 4) || (RAMARK(nm) != 255)) return 0; /* No range found */
	if ((RATYPE(nm) < 1) || (RATYPE(nm) > BF_TMAX)) return -1; /* Invalid type */
	if (sp != NULL) *sp = RASTART(nm);
	if (ep != NULL) *ep = RAEND(nm);
	if (RATYPE(nm) >= BF_TCONST) { /* If a constant, push the packed string value */
		if (sz < 5) return -2; /* Invalid constant (no value) */
		lua_pushlstring(L, RACONST(nm), sz-4);
	}
	return RATYPE(nm); /* Return the type of the range */
}

/* Check Named Range Table at ix on stack and return maximimum index in contained ranges */
int checkNRT(lua_State* L, int ix) {
	const char* x; size_t sz; BYTE m = 0;
	lua_pushnil(L); 
	while (lua_next(L, ix) != 0) {
		if ((lua_type(L, -1) != LUA_TSTRING) || (lua_type(L, -2) != LUA_TSTRING))
			return luaL_error(L, "Invalid entry in Named Range Table");
		x = lua_tolstring(L, -1, &sz);
		if ((x != NULL) && (sz >= 4) && (RAMARK(x) == 255)) {
			if (RAEND(x) > m) m = RAEND(x);
		}
		lua_pop(L, 1);
	}
	return m;
}

/* validate a Lua bit index (1..256) on the stack and returns it as C index (0..255) */
BYTE checkindex(lua_State* L, int ix) {
	lua_Unsigned v = luaL_checkunsigned(L, ix);
	if ((v < 1) || (v > 256))
		return luaL_argerror(L, ix, "Bitfield index must be 1..256");
	return (BYTE)(v-1);
}

/* check if string at ix on stack is a binary string and return number of binary digits
** return 0 if not binary string
*/
int checkbinary(lua_State* L, int ix) {
	const char* x; size_t sx;
	x = lua_tolstring(L, ix, &sx);
	if (sx < 1) return 0;
	if ((x[0] != '0')&&(x[0] != '1')) return 0;
	return (int)sx;
}

/* Library function (return range index) */
static int bitrange(lua_State* L) {
	static const char* const types[] = {"bitfield","boolean","unsigned","binary","packed","const","uconst",NULL};
	int p = 1; size_t sz;
	ra_ke b; b.m = 255; b.t = b.s = b.e = 0;
	if (lua_type(L, 1) == LUA_TSTRING) { b.t = luaL_checkoption(L, 1, NULL, types) + 1; p++; }
	if (b.t == BF_TUCONST) { /* For an umbound constant, may need to determine width from value */
		b.s = 0;
		switch (lua_type(L, p)) {
		case LUA_TBOOLEAN:
			b.e = 0;
			break;
		case LUA_TSTRING:
			lua_tolstring(L, p, &sz);
			b.e = sz - 1;
			break;
		case LUA_TNUMBER:
			b.e = (BYTE)lua_tounsigned(L, p);
			break;
		default:
			return luaL_argerror(L, p, "Bad parameter type");
		}
	} else { /* For other types, process range */
		if (lua_isnone(L, p)) { /* Default to full bitfield range */
			b.s = 0; b.e = 255;
		} else { /* Compulsory start of range and optional end */
			if (lua_type(L, p) != LUA_TNUMBER) return luaL_argerror(L, p, "Bad parameter type");
			b.s = checkindex(L, p++);
			b.e = b.s;
			if (!lua_isnone(L, p)) b.e = checkindex(L, p++);
		}
	}
	if (b.e < b.s) return luaL_argerror(L, p, "End of range cannot be before start");
	if (b.t == 0) { /* Default the type if not explicitly given */
		b.t = (b.s == b.e)? BF_TBOOLEAN : BF_TUNSIGNED;
		if ((b.e - b.s) >= USBITS) b.t = BF_TBINARY;
	}
	if ((b.t == BF_TBOOLEAN) && (b.s != b.e))
		return luaL_argerror(L, 1, "Boolean type invalid for range of more than one bit");
	lua_pushlstring(L, (const char*)&b, 4); /* Assemble range key */
	if ((b.t == BF_TCONST) || (b.t == BF_TUCONST)) { /* For constants, append the packed value */
		getpacked(L, p, (b.e - b.s) + 1);
		lua_concat(L, 2);
	}
	return 1;
}

/* Metamethod for length operator */
static int bf_len(lua_State* L) {
	luaC_checkmethod(L, 1);
	lua_pushunsigned(L, (UDBITS((bf_ud*)lua_touserdata(L, 1))) + 1);
	return 1;
}

/* Metamethod for __tostring */
static int bf_tostring(lua_State* L) {
	luaC_checkmethod(L, 1);
	bf_ud* ud = (bf_ud*)lua_touserdata(L, 1);
	return extractvalue(L, BF_TBINARY, ud, 0, UDBITS(ud));
}

/* Metamethod for reference indexing operation */
static int bf_index(lua_State* L) {
	lua_Unsigned ix = 0; BYTE st, ed; int ty;
	luaC_checkmethod(L, 1);
	bf_ud* ud = (bf_ud*)lua_touserdata(L, 1);
	if (lua_type(L, 2) == LUA_TSTRING) {
		if (luaL_getmetafield(L, 1, lua_tostring(L, 2))) return 1; /* Allows methods to be added by user */
		ty = getrange(L, 1, 2, &st, &ed);
		ix = st;
		if (ty <= 0) return luaL_argerror(L, 2, "Invalid range name");
	} else { /* If not a string, assume it is supposed to be a numeric bit index */
		ix = checkindex(L, 2);
		ty = BF_TBOOLEAN; ed = 0;
	}
	if (ed > UDBITS(ud)) ed = UDBITS(ud); /* Saturate range end to width of bitfield */
	if (ed < ix) ed = ix; /* Default to single bit if range is empty */
	return extractvalue(L, ty, ud, ix, ed);
}

/* Metamethod for assignment indexing operation */
static int bf_newindex(lua_State* L) {
	lua_Unsigned ix = 0; int ty = -1; BYTE  sp, ep, spc, epc; int tx;
	bf_ud* sd = NULL;
	luaC_checkmethod(L, 2);
    bf_ud* ud = (bf_ud*)lua_touserdata(L, 1);
	if (lua_type(L, 2) == LUA_TSTRING) { /* String key could be range or range name */
		ty = getrange(L, 1, 2, &sp, &ep);
		if (ty <= 0) return luaL_argerror(L, 2, "Invalid range");
		if (ep > UDBITS(ud)) ep = UDBITS(ud);
		ix = sp;
	} else { /* If not a string key, must be numeric bit index */
		ix = checkindex(L, 2);
		ep = ix;
	}
	if (ix > UDBITS(ud)) return 0;
	switch (lua_type(L, 3)) { /* Lua type of new value */
	case LUA_TUSERDATA: /* bitfield */
		if (!luaC_isclass(L, 3) || ((ty >= 0) && (ty != BF_TBITFIELD)))
			return luaL_argerror(L, 3, "Bitfield invalid for this range");
		sd = (bf_ud*)lua_touserdata(L, 3);
		if (UDBITS(sd) != (ep - ix)) return luaL_argerror(L, 3, "Bitfield not the same width as range");
		if ((ix + UDBITS(sd)) > UDBITS(ud)) return luaL_argerror(L, 3, "Bitfield too wide to insert here");
		setbitfield(ud, sd, ix);
		break;
	case LUA_TNUMBER: /* unsigned value */
		if ((ty >= 0) && (ty != BF_TUNSIGNED))
			return luaL_argerror(L, 3, "Number invalid for this range");
		setunsigned(L, ud, 3, ix, ep);
		break;
	case LUA_TSTRING: /* Could be a binary string, a packed string or a named constant */
		if (ty == BF_TPACKED) {
			setpacked(L, ud, 3, ix, ep);
		} else if ((*lua_tostring(L,3) == '0') || (*lua_tostring(L,3) == '1')) {
			if (ty != BF_TBINARY) return luaL_argerror(L, 3, "Binary string not valid for this range");
			setbinary(L, ud, 3, sp, -1);
		} else {
			tx = getrange(L, 1, 3, &spc, &epc);
			if ((tx == BF_TCONST) || (tx == BF_TUCONST)) {
				if (tx == BF_TCONST) {
					if ((spc != sp) || (epc != ep))
						{return luaL_argerror(L, 3, "Named constant not valid for this range");}
					//else if (epc == (ep - sp))
					//	{return luaL_argerror(L, 3, "Named constant not the right length for range");}
				}
				setpacked(L, ud, -1, sp, ep);
				lua_pop(L, 1);
			} else {
				return luaL_argerror(L, 3, "Unable to interpret string value");
			}
		}
		break;
	default: /* Only other possibility is a boolean value */
		if (ty >= 0) if ((ty != BF_TBOOLEAN) || (ix != ep))
			return luaL_argerror(L, 3, "Boolean invalid for this range");
		setbit(ud, ix, lua_toboolean(L, 3));
		break;
	}
	return 0;
}

/* Concatenation operator metamethod for both bitfield and boolean types */
static int bf_concat(lua_State* L) {
	bf_ud* ud1 = NULL; bf_ud* ud2 = NULL; bf_ud* ud; BYTE p = 0;
	if (((lua_type(L, 1) != LUA_TBOOLEAN) && (!luaC_isclass(L, 1))) 
		|| ((lua_type(L, 2) != LUA_TBOOLEAN) && (luaC_isclass(L, 2))))
			return luaL_error(L, "Concatenation not supported for these types");
	if (lua_isuserdata(L, 1)) ud1 = (bf_ud*)lua_touserdata(L, 1);
	if (lua_isuserdata(L, 2)) ud2 = (bf_ud*)lua_touserdata(L, 2);
	ud = makeud(L, ((ud1 == NULL)? 1 : UDBITS(ud1) + 1) + ((ud2 == NULL)? 1 : UDBITS(ud2) + 1));
	if (ud2 == NULL)
		setbit(ud, p++, lua_toboolean(L, 2));
	else
		p = setbitfield(ud, ud2, 0);
	if (ud1 == NULL)
		setbit(ud, p, lua_toboolean(L, 1));
	else
		setbitfield(ud, ud1, p);
	if (luaC_gettid(L, lua_upvalueindex(1)) != 1) return luaL_error(L, "Bad Class");
	lua_setmetatable(L, -2);
	return 1;
}

/* Metamethod for equality operator */
static int bf_eq(lua_State* L) {
	bf_ud* ud1 = NULL; bf_ud* ud2 = NULL; int i;
	if ((!luaC_isclass(L, 1)) || (!luaC_isclass(L, 2))) {lua_pushboolean(L, FALSE); return 1;}
	ud1 = (bf_ud*)lua_touserdata(L, 1);
	ud2 = (bf_ud*)lua_touserdata(L, 2);
	lua_pushboolean(L, FALSE);
	if (UDBITS(ud1) != UDBITS(ud2)) return 1;
	for (i = 0; (i <= UDBITS(ud1)); i++) if (getbit(ud1,i) != getbit(ud2,i)) return 1;
	lua_pop(L, 1);
	lua_pushboolean(L, TRUE);
	return 1;
}

static int bf_Construct(lua_State* L) {
	int w, i, x, t; bf_ud* ud; BYTE sp, ep;
	int nrt = 0; int tud = 0; int lt = 2; int rt = lua_gettop(L);
	switch (lua_type(L, 1)) {
	case LUA_TNUMBER: /* Explicit width */
		w = luaL_checkunsigned(L, 1);
		if ((w < 1) || (w > 256)) return luaL_argerror(L, 1, "Width must be between 1 and 256");
		break;
	case LUA_TUSERDATA: /* Template bitfield */
		if (!luaC_isclass(L, 1)) return luaL_argerror(L, 1, "Only Bitfield objects allowed");
		w = UDBITS((bf_ud*)lua_touserdata(L, 1)) + 1;
		lua_getuservalue(L, 1);
		if (lua_istable(L, -1)) nrt = lua_gettop(L); else lua_pop(L, 1);
		tud = 1;
		break;
	case LUA_TTABLE: /* Named Range Table */
		w = checkNRT(L, 1) + 1;
		nrt = 1;
		break;
	default: /* Initialiser list only - scan initialisers for total width */
		lt = 1;
		w = 0;
		for (i = lt; (i <= rt); i++) {
			switch (lua_type(L, i)) {
			case LUA_TBOOLEAN: w++; break;
			case LUA_TSTRING:
				x = checkbinary(L, i);
				if (x < 1) return luaL_argerror(L, i, "Invalid binary string");
				w += x;
				break;
			case LUA_TUSERDATA:
				if (!luaC_isclass(L, i)) return luaL_argerror(L, 1, "Only Bitfield objects allowed");
				w += UDBITS((bf_ud*)lua_touserdata(L, i)) + 1;
				break;
			default:
				return luaL_argerror(L, i, "Cannot determine width of initialiser");
			}
		}
		if ((w < 1) || (w > 256)) return luaL_error(L, "Width must be between 1 and 256");
		break;
	}
	ud = makeud(L, w);
	if (nrt > 0) {lua_pushvalue(L, nrt); lua_setuservalue(L, -2);}
	if (tud > 0) {setbitfield(ud, (bf_ud*)lua_touserdata(L, tud), 0);}
	x = 0; for (i = rt; (i >= lt); i--) { /* Process Initialisers */
		switch (lua_type(L, i)) {
		case LUA_TBOOLEAN: setbit(ud, x++, lua_toboolean(L, i)); break;
		case LUA_TNUMBER:
			if (i != lt) return luaL_argerror(L, i, "Number initialiser only valid leftmost");
			setunsigned(L, ud, i, x, w);
			break;
		case LUA_TUSERDATA:
			if (!luaC_isclass(L, i)) return luaL_argerror(L, 1, "Only Bitfield objects allowed");
			x = setbitfield(ud, (bf_ud*)lua_touserdata(L, i), x);
			break;
		case LUA_TSTRING:
			if (checkbinary(L, i) > 0) {
				x = setbinary(L, ud, i, x, (i == lt)? w - 1 : -1);
			} else { /* Try for named constants */
				t = getrange(L, -1, i, &sp, &ep);
				if ((t != BF_TCONST) && (t != BF_TUCONST))
					return luaL_argerror(L, i, "Unrecognised named constant initialiser");
				if (t == BF_TCONST) {
					setpacked(L, ud, -1, sp, ep);
					x = ep + 1;
				} else {
					setpacked(L, ud, -1, x, x + ep);
					x = x + ep + 1;
				}
				lua_pop(L, 1);
			}
			break;
		default: return luaL_argerror(L, i, "Invalid initialiser type");
		}
	}
	if (luaC_gettid(L,0) != 1) return luaL_error(L, "Bad Class");
	lua_setmetatable(L, -2);
	return 1;
};

void bf_Create(lua_State* L)
{
	static const luaL_Reg mt[] = {
		{"__len", bf_len},
		{"__index", bf_index},
		{"__newindex", bf_newindex},
		{"__concat", bf_concat},
		{"__tostring", bf_tostring},
		{"__eq", bf_eq},
		{NULL, NULL}
	};
	luaC_newclass(L, bf_Construct, mt);
};


LUABFLIB_API int luaopen_bitfield (lua_State *L) {

	static const luaL_Reg lf[] = {
		{"bitrange", bitrange},
		{NULL, NULL}
	};

	/* Try to set metatable for boolean, but do not disturb anything already in place */
	lua_pushboolean(L, 0);
	if (lua_getmetatable(L, -1)) {
		lua_getfield(L, -1, "__concat");
		if (lua_isnil(L, -1)) {
			lua_pushcfunction(L, bf_concat);
			lua_setfield(L, -3, "__concat");
		}
		lua_pop(L, 3);
	} else {
		lua_newtable(L);
		lua_pushcfunction(L, bf_concat);
		lua_setfield(L, -2, "__concat");
		lua_setmetatable(L, -2);
		lua_pop(L, 1);
	}

	/* Return library table for bitfield library */
	luaL_newlib(L, lf);
	lua_pushstring(L, LIBVERSION);
	lua_setfield(L, -2, "_VERSION");

	bf_Create(L);
	lua_setfield(L, -2, "Bitfield");

	return 1;
}
