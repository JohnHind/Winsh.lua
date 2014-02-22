#include "stdafx.h"
#define LUATMLIB_BUILDING
#include "LibClass.h"
#include "LibTime.h"
#include "..\LuaLibIF.h"
#include "..\resource.h"
#include <math.h>

#ifdef LUATMLIB_DLL
HMODULE hM;
LUATMLIB_API BOOL APIENTRY DllMain( HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved )
{
	hM = hModule;
    return TRUE;
}
#endif

int time_cmd = 0;
#define TIME_ALARMS (16)

// Time Class.
// ===========

#define TudTypAbs(x) ((int)(x->typ) < 6)
#define TudTypTod(x) (x->typ == tud_tod)
#define TudTypDur(x) (x->typ == tud_dur)

BOOL TimeSplit(DOUBLE t, SYSTEMTIME* st, double tzo = 100.0)
{
	DOUBLE d = t;
	d *= (60 * 60 * 24);
	d *= 10000000;
	ULARGE_INTEGER u; u.QuadPart = (ULONGLONG)d;
	FILETIME ft; ft.dwHighDateTime = u.HighPart; ft.dwLowDateTime = u.LowPart;
	SYSTEMTIME stu;
	TIME_ZONE_INFORMATION tz;
	if (tzo > 99.9) {
		GetTimeZoneInformation(&tz);
	} else {
		tz.Bias = (int)(tzo * 60.0);
		tz.DaylightDate.wMonth = tz.StandardDate.wMonth = 0;
		tz.DaylightBias = tz.StandardBias = 0;
		tz.DaylightName[0] = tz.StandardName[0] = 0;
	}
	if (FileTimeToSystemTime(&ft, &stu))
	{
		if (SystemTimeToTzSpecificLocalTime(&tz, &stu, st)) return TRUE;
	}
	return FALSE;
}

DOUBLE TimeMake(SYSTEMTIME* stl, double tzo = 100.0)
{
	SYSTEMTIME stu;
	FILETIME ft;
	TIME_ZONE_INFORMATION tz;
	DOUBLE t = 0.0;
	if (tzo > 99.9) {
		GetTimeZoneInformation(&tz);
	} else {
		tz.Bias = (int)(tzo * 60.0);
		tz.DaylightDate.wMonth = tz.StandardDate.wMonth = 0;
		tz.DaylightBias = tz.StandardBias = 0;
		tz.DaylightName[0] = tz.StandardName[0] = 0;
	}
	if (TzSpecificLocalTimeToSystemTime(&tz, stl, &stu))
	{
		if (SystemTimeToFileTime(&stu, &ft))
		{
			ULARGE_INTEGER u; u.HighPart = ft.dwHighDateTime; u.LowPart = ft.dwLowDateTime;
			t = (DOUBLE)u.QuadPart;
			t /= 10000000;
			t /= (60 * 60 * 24);
		}
	}
	return t;
}

DOUBLE TimeEndAt(DOUBLE st, TudTyp res)
{
	SYSTEMTIME t;
	switch (res)
	{
	case tud_abs_min:
		return st + (1.0 / (24 * 60));
	case tud_abs_hour:
		return st + (1.0 / 24);
	case tud_abs_day:
		return st + 1.0;
	case tud_abs_month:
		TimeSplit(st, &t, 0.0);
		t.wMonth++; if (t.wMonth > 12) t.wYear++;
		return TimeMake(&t, 0.0);
	case tud_abs_year:
		TimeSplit(st, &t, 0.0);
		t.wYear++;
		return TimeMake(&t, 0.0);
	default:
		return st;
	}
}

DOUBLE TimeRound(DOUBLE d, int res)
{
	DOUBLE fact = 1.0;
	switch (res)
	{
	case tud_abs_min:
		fact = (24 * 60);
		break;
	case tud_abs_hour:
		fact = 24;
		break;
	case tud_abs_day:
		fact = 1;
		break;
	default:
		return d;
	}
	DOUBLE dd = d * fact;
	DOUBLE ddd;
	modf(dd, &ddd);
	return ddd / fact;
}

LUATMLIB_API BOOL luaTM_TimeFromFiletime(lua_State* L, FILETIME ft)
{
	if (!luaC_isclass(L, -1, "time.Time")) return FALSE;
	timeud* ud = (timeud*)lua_touserdata(L, -1);
	ud->alm = 0;
	ud->typ = tud_abs;
	ULARGE_INTEGER u; u.HighPart = ft.dwHighDateTime; u.LowPart = ft.dwLowDateTime;
	ud->val = (DOUBLE)u.QuadPart;
	ud->val /= 10000000;
	ud->val /= (60 * 60 * 24);
	return TRUE;
}

LUATMLIB_API BOOL luaTM_FiletimeFromTime(lua_State* L, int inx, FILETIME* pft)
{
	if (!luaC_isclass(L, inx, "time.Time")) return FALSE;
	timeud* ud = (timeud*)lua_touserdata(L, inx);
	DOUBLE d = ud->val;
	d *= (60 * 60 *24);
	d *= 10000000;
	ULARGE_INTEGER u;
	u.HighPart = pft->dwHighDateTime;
	u.LowPart = pft->dwLowDateTime;
	if (ud->typ == tud_dur)
	{
		u.QuadPart += (ULONGLONG)d;
	}
	else if (ud->typ == tud_tod)
	{
		u.QuadPart /= 10000000;
		u.QuadPart /= (60 * 60 * 24);
		u.QuadPart *= (60 * 60 * 24);
		u.QuadPart *= 10000000;
		u.QuadPart += (ULONGLONG)d;
	}
	else
	{
		u.QuadPart = (ULONGLONG)d;
	}
	pft->dwHighDateTime = u.HighPart;
	pft->dwLowDateTime = u.LowPart;
	return TRUE;
}

//R1 (String) "absolute", "day" or "duration" (The type of the Time object).
//R2 (String or nil) "minute", "hour", "day", "month", "year" (Resolution of an absolute Time object).
int LuaTimeType(lua_State* L)
{
	luaC_checkmethod(L, 1);
	timeud* p1 = (timeud*)lua_touserdata(L, 1);
	switch (p1->typ)
	{
	case tud_abs:
		lua_pushstring(L, "absolute");
		lua_pushstring(L, "");
		return 2;
	case tud_abs_min:
		lua_pushstring(L, "absolute");
		lua_pushstring(L, "minute");
		return 2;
	case tud_abs_hour:
		lua_pushstring(L, "absolute");
		lua_pushstring(L, "hour");
		return 2;
	case tud_abs_day:
		lua_pushstring(L, "absolute");
		lua_pushstring(L, "day");
		return 2;
	case tud_abs_month:
		lua_pushstring(L, "absolute");
		lua_pushstring(L, "month");
		return 2;
	case tud_abs_year:
		lua_pushstring(L, "absolute");
		lua_pushstring(L, "year");
		return 2;
	case tud_tod:
		lua_pushstring(L, "day");
		return 1;
	case tud_dur:
		lua_pushstring(L, "duration");
		return 1;
	}
	return 0;
}

double opttzo(lua_State* L, int ix, double def = 100.0)
{
	double tzo = luaL_optnumber(L, ix, def);
	// Timezones currently range -13 to +12 hours, but also allow for DST.
	// Purpose of test mainly to catch incorrect specification in minutes.
	if ((tzo != 100.0) && ((tzo < -14.0) || (tzo > 14.0)))
		return luaL_argerror(L, ix, "Invalid Timezone Offset");
	return tzo;
}

//P1 (Number, optional) Time zone offset in hours, defaults to current locale.
//R1 (Number) Hour (0 .. 23)
//R2 (Number) Minute (0 .. 59)
//R3 (Number) Second and fractions of a second (0.00 .. 59.99)
static int LuaTimeTime(lua_State* L)
{
	luaC_checkmethod(L, 3);
	timeud* p1 = (timeud*)lua_touserdata(L, 1);
	if (!(TudTypTod(p1) || (p1->typ < tud_abs_day)))
		return luaL_argerror(L, 1, "Object does not have absolute time.");
	double tzo = opttzo(L, 2);
	SYSTEMTIME st;
	if (!TimeSplit(p1->val, &st, tzo)) return luaL_argerror(L, 1, "Invalid Time.");
	DOUBLE n = (DOUBLE)st.wSecond + (DOUBLE)st.wMilliseconds / 1000;
	lua_pushinteger(L, st.wHour);
	lua_pushinteger(L, st.wMinute);
	lua_pushnumber(L, n);
	return 3;
}

//P1 (Number, optional) Time zone offset in hours, defaults to current locale.
//R1 (Number) Year (Whole number, full four-digit year)
//R2 (Number) Month (1 .. 12).
//R3 (Number) Day of month (1 .. 31).
static int LuaTimeDate(lua_State* L)
{
	luaC_checkmethod(L, 3);
	timeud* p1 = (timeud*)lua_touserdata(L, 1);
	if (!TudTypAbs(p1)) return luaL_argerror(L, 1, "Object does not have date.");
	double tzo = opttzo(L, 2);
	SYSTEMTIME st;
	if (!TimeSplit(p1->val, &st, tzo)) return luaL_argerror(L, 1, "Invalid Time.");
	lua_pushinteger(L, st.wYear);
	lua_pushinteger(L, st.wMonth);
	lua_pushinteger(L, st.wDay);
	return 3;
}

//P1 (Number, optional) Time zone offset in hours, defaults to current locale.
//R1 (Number) Day-of-week (0 .. 6; 0 = Sunday).
static int LuaTimeDay(lua_State* L)
{
	luaC_checkmethod(L, 1);
	timeud* p1 = (timeud*)lua_touserdata(L, 1);
	if (!TudTypAbs(p1)) return luaL_argerror(L, 1, "Object does not have date.");
	double tzo = opttzo(L, 2);
	SYSTEMTIME st;
	if (!TimeSplit(p1->val, &st, tzo)) return luaL_argerror(L, 1, "Invalid Time.");
	lua_pushinteger(L, st.wDayOfWeek);
	return 1;
}

//R1 (Number) Real number of days and fractions of a day.
static int LuaTimeValue(lua_State* L)
{
	luaC_checkmethod(L, 1);
	timeud* p1 = (timeud*)lua_touserdata(L, 1);
	lua_pushnumber(L, p1->val);
	return 1;
}

//R1 (Number) Time zone offset in hours including DST applicable at the time and date.
//R2 (String) Name of the time zone.
//R3 (Number) DST offset in hours applicable at the time or date.
int LuaTimeZone(lua_State* L)
{
	luaC_checkmethod(L, 3);
	timeud* p1 = (timeud*)lua_touserdata(L, 1);
	if (!(p1->typ < tud_abs_hour)) return luaL_argerror(L, 1, "Only applies to an absolute Time with at least minute resolution.");
	TIME_ZONE_INFORMATION tz;
	GetTimeZoneInformation(&tz);

	SYSTEMTIME st;
	TimeSplit(p1->val, &st);
	DOUBLE loc = TimeMake(&st, 0.0) * (24 * 60);
	int ofs = (int)((p1->val * (24 * 60)) - loc);

	lua_pushnumber(L, (double)ofs / 60.0);
	luaX_pushstring(L, (ofs == tz.Bias)? CString(tz.StandardName) : CString(tz.DaylightName));
	lua_pushnumber(L, (double)((ofs == tz.Bias)? tz.StandardBias : tz.DaylightBias) / 60.0);
	return 3;
}

UINT time_ToNextAlarm(lua_State* L, DOUBLE tim, TudTyp typ)
{
	SYSTEMTIME st;
	FILETIME ft;
	LARGE_INTEGER dt;
	DOUBLE dd;
	DOUBLE dr;
	GetSystemTime(&st);
	SystemTimeToFileTime(&st, &ft);
	dt.HighPart = ft.dwHighDateTime; dt.LowPart = ft.dwLowDateTime;
	dd = (DOUBLE)dt.QuadPart;
	dd /= 10000000.0;
	dd /= (60 * 60 * 24);
	switch (typ)
	{
	case tud_tod:
		dr = floor(dd) + tim;
		if (dr < dd) dr += 1;
		dd = dr - dd;
		break;
	case tud_dur:
		dd = tim;
		break;
	default:
		dd = tim - dd;
		if (dd < 0) dd = 0;
		break;
	}
	dd *= (60 * 60 * 24);
	dd *= 1000.0;
	dt.QuadPart = (LONGLONG)dd;
	if (dt.HighPart != 0) return luaL_error(L, "Alarm Interval too long (execeeds ~49 days)");
	return dt.LowPart;
}

// Function for alarm closure:
// U1 (Function) The notifier function to be called when the alarm expires.
// U2 (Time)     The user-data based Time object this applies to.
// U3 (Number)   The remaining number of times the alarm should be triggered.
int LuaTimeCF(lua_State* L)
{
	WINSH_LUA(2)
	int ct = lua_tointeger(L, lua_upvalueindex(3));
	lua_pushvalue(L, lua_upvalueindex(1));
	lua_pushvalue(L, lua_upvalueindex(2));
	int r = lua_pcall(L, 1, 0, 0);
	if (r != 0)
	{
		H->WriteError(CString("Time alarm callback: ") + CString(lua_tostring(L, -1)));
		lua_pop(L, 1);
	}
	if (ct > 0)
	{
		lua_pushinteger(L, --ct);
		lua_replace(L, lua_upvalueindex(3));
	}
	timeud* t = (timeud*)lua_touserdata(L, lua_upvalueindex(2));
	if (ct == 0)
		H->SetTimerMessage(t->alm);
	else
		H->SetTimerMessage(t->alm, time_ToNextAlarm(L, t->val, t->typ));
	return 0;
}

// P1 (Function/Table/Userdata, opt) The object is called when the alarm triggers. If absent any alarm is cancelled.
// P2 (Number, Boolean, opt) Repeat strategy. If absent repeats once. If boolean true, repeats
//    indefinitely. If a number, repeats that number of times. (Repeats apply to Interval or ToD only).
int LuaTimeAlarm(lua_State* L)
{
	WINSH_LUA(4);
	luaC_checkmethod(L, 4);
	timeud* p1 = (timeud*)lua_touserdata(L, 1);
	int cnt = 1;

	if (p1->alm != 0)
	{
		H->SetTimerMessage(p1->alm);
		lua_pushnil(L);
		H->SetLuaMessageHandler(p1->alm);
		lua_pop(L, 1);
	}
	p1->alm = 0;

	if (lua_gettop(L) < 2) return 0;

	if (!luaX_iscallable(L, 2)) luaL_error(L, "Alarm Binding must be a callable object.");
	if (lua_type(L, 3) == LUA_TBOOLEAN) if (lua_toboolean(L, 3)) cnt = -1;
	if (lua_type(L, 3) == LUA_TNUMBER) {cnt = lua_tointeger(L, 3); if (cnt < 1) cnt = 1;}
	if (p1->typ == tud_tod) cnt = 1;
	lua_settop(L, 2);

	lua_pushvalue(L, 2);
	lua_pushvalue(L, 1);
	lua_pushinteger(L, cnt);
	lua_pushcclosure(L, LuaTimeCF, 3);
	p1->alm = H->FindFreeLuaMessage(time_cmd, TIME_ALARMS);
	if (p1->alm < 1) return luaL_error(L, "Too many alarms");
	H->SetLuaMessageHandler(p1->alm);
	H->SetTimerMessage(p1->alm, time_ToNextAlarm(L, p1->val, p1->typ));
	return 0;
}

static int LuaTimeGc(lua_State* L)
{
	WINSH_LUA(1);
	luaC_checkmethod(L, 1);
	timeud* p1 = (timeud*)lua_touserdata(L, 1);
	if (p1->alm != 0)
	{
		H->SetTimerMessage(p1->alm);
		H->SetLuaMessageHandler(p1->alm);
	}
	return 0;
}

static int LuaTimeAdd(lua_State* L)
{
	WINSH_LUA(2)
	DOUBLE d;

	if ((!luaC_isclass(L, 1))||(!luaC_isclass(L, 2))) return luaL_error(L, "Cannot add these types");
	lua_settop(L, 2);

	timeud* p1 = (timeud*)lua_touserdata(L, 1);
	timeud* p2 = (timeud*)lua_touserdata(L, 2);

	if (TudTypAbs(p1) && TudTypDur(p2)) //A = A + D
	{
		if ((p1->typ == tud_abs_month) || (p1->typ == tud_abs_year))
			return luaL_error(L, "Cannot add to Time object with month or year resolution.");
		timeud* pr = (timeud*)lua_newuserdata(L, sizeof(timeud));
		pr->alm = 0;
		pr->typ = p1->typ;
		pr->val = p1->val + TimeRound(p2->val, p1->typ);
	}
	else if (TudTypAbs(p2) && TudTypDur(p1)) //A = D + A
	{
		if ((p2->typ == tud_abs_month) || (p2->typ == tud_abs_year))
			return luaL_error(L, "Cannot add to Time object with month or year resolution.");
		timeud* pr = (timeud*)lua_newuserdata(L, sizeof(timeud));
		pr->alm = 0;
		pr->typ = p2->typ;
		pr->val = p2->val + TimeRound(p1->val, p2->typ);
	}
	else if (TudTypDur(p1) && TudTypDur(p2)) //D = D + D
	{
		timeud* pr = (timeud*)lua_newuserdata(L, sizeof(timeud));
		pr->alm = 0;
		pr->val = p1->val + p2->val;
		pr->typ = tud_dur;
	}
	else if ((TudTypTod(p1) && TudTypDur(p2)) || (TudTypTod(p2) && TudTypDur(p1))) //T = T + D or T = T + D
	{
		timeud* pr = (timeud*)lua_newuserdata(L, sizeof(timeud));
		pr->alm = 0;
		pr->val = p1->val + p2->val;
		pr->val = modf(pr->val, &d);
		pr->typ = tud_tod;
	}
	else
	{
		return luaL_argerror(L, 2, "Cannot add these Time objects.");
	}
	if (luaC_gettid(L, lua_upvalueindex(1)) != 1) return luaL_error(L, "Bad Class");
	lua_setmetatable(L, -2);									//|T|
	return 1;
}

static int LuaTimeSub(lua_State* L)
{
	WINSH_LUA(2)
	DOUBLE d;

	if ((!luaC_isclass(L, 1))||(!luaC_isclass(L, 2))) return luaL_error(L, "Cannot subtract these types");
	lua_settop(L, 2);

	timeud* p1 = (timeud*)lua_touserdata(L, 1);
	timeud* p2 = (timeud*)lua_touserdata(L, 2);

	if (TudTypAbs(p1) && TudTypDur(p2)) //A = A - D
	{
		if ((p1->typ == tud_abs_month) || (p1->typ == tud_abs_year))
			return luaL_error(L, "Cannot subtract from Time object with month or year resolution.");
		timeud* pr = (timeud*)lua_newuserdata(L, sizeof(timeud));
		pr->alm = 0;
		pr->typ = p1->typ;
		pr->val = p1->val - TimeRound(p2->val, p1->typ);
	}
	else if (TudTypDur(p1) && TudTypDur(p2)) //D = D - D
	{
		timeud* pr = (timeud*)lua_newuserdata(L, sizeof(timeud));
		pr->alm = 0;
		pr->val = p1->val - p2->val;
		pr->typ = tud_dur;
	}
	else if (TudTypAbs(p1) && TudTypAbs(p2)) //D = A - A
	{
		timeud* pr = (timeud*)lua_newuserdata(L, sizeof(timeud));
		pr->alm = 0;
		pr->val = p1->val - p2->val;
		pr->typ = tud_dur;
	}
	else if (TudTypTod(p1) && TudTypDur(p2)) //T = T - D
	{
		timeud* pr = (timeud*)lua_newuserdata(L, sizeof(timeud));
		pr->alm = 0;
		pr->val = p1->val - p2->val;
		pr->val = modf(pr->val, &d);
		pr->typ = tud_tod;
	}
	else
	{
		return luaL_error(L, "Cannot subtract these Time objects.");
	}
	if (luaC_gettid(L, lua_upvalueindex(1)) != 1) return luaL_error(L, "Bad Class");
	lua_setmetatable(L, -2);									//|T|
	return 1;
}

static int LuaTimeUnm(lua_State* L)
{
	luaC_checkmethod(L, 2);
	lua_settop(L, 1);
	timeud* p1 = (timeud*)lua_touserdata(L, 1);
	if (TudTypDur(p1))	//D = -D
	{
		timeud* pr = (timeud*)lua_newuserdata(L, sizeof(timeud));
		pr->alm = 0;
		pr->val = p1->val * -1.0;
		pr->typ = tud_dur;
	}
	else
	{
		return luaL_error(L, "Cannot negate this Time object.");
	}
	if (luaC_gettid(L, lua_upvalueindex(1)) != 1) return luaL_error(L, "Bad Class");
	lua_setmetatable(L, -2);									//|T|
	return 1;
}

static int LuaTimeMul(lua_State* L)
{
	luaC_checkmethod(L, 2);
	luaL_checknumber(L, 2);
	lua_settop(L, 2);
	timeud* p1 = (timeud*)lua_touserdata(L, 1);
	if (!TudTypDur(p1)) return luaL_error(L, "Can only multiply duration Time objects.");
	timeud* pr = (timeud*)lua_newuserdata(L, sizeof(timeud));
	pr->alm = 0;
	pr->val = p1->val * lua_tonumber(L, 2);
	pr->typ = tud_dur;
	if (luaC_gettid(L, lua_upvalueindex(1)) != 1) return luaL_error(L, "Bad Class");
	lua_setmetatable(L, -2);									//|T|
	return 1;
}

static int LuaTimeDiv(lua_State* L)
{
	luaC_checkmethod(L, 2);
	lua_settop(L, 2);
	timeud* p1 = (timeud*)lua_touserdata(L, 1);
	if (!TudTypDur(p1)) return luaL_error(L, "Can only divide duration Time objects.");

	if (lua_isnumber(L, 2)) //D = D / N
	{
		DOUBLE d = lua_tonumber(L, 2);
		timeud* pr = (timeud*)lua_newuserdata(L, sizeof(timeud));
		pr->alm = 0;
		pr->val = p1->val / d;
		pr->typ = tud_dur;
		if (luaC_gettid(L, lua_upvalueindex(1)) != 1) return luaL_error(L, "Bad Class");
		lua_setmetatable(L, -2);									//|T|
	}
	else
	{
		if (!luaC_isclass(L, 2)) return luaL_error(L, "Can only divide by number or Time");
		timeud* p2 = (timeud*)lua_touserdata(L, 2);
		if (TudTypDur(p2))	//N = D / D
		{
			lua_pushnumber(L, (p1->val / p2->val));
		}
		else
		{
			return luaL_error(L, "Can only divide by another duration Time object or a Number.");
		}
	}
	return 1;
}

static int LuaTimeEq(lua_State* L)
{
	if ((!luaC_isclass(L, 1))||(!luaC_isclass(L, 2))) {lua_pushboolean(L, FALSE); return 1;}
	lua_settop(L, 2);
	timeud* p1 = (timeud*)lua_touserdata(L, 1);
	timeud* p2 = (timeud*)lua_touserdata(L, 2);

	if (p1->typ == p2->typ)
	{
		lua_pushboolean(L, (p1->val == p2->val));
	}
	else if (TudTypAbs(p1) && TudTypAbs(p2))
	{
		DOUBLE e1 = TimeEndAt(p1->val, p1->typ);
		DOUBLE e2 = TimeEndAt(p2->val, p2->typ);
		if (p1->typ < p2->typ)
			lua_pushboolean(L, ((p2->val <= p1->val) && (e2 >= e1))); //2 wider
		else
			lua_pushboolean(L, ((p1->val <= p2->val) && (e1 >= e2))); //1 wider
	}
	else
	{
		lua_pushboolean(L, FALSE);
	}
	return 1;
}

static int LuaTimeLt(lua_State* L)
{
	if ((!luaC_isclass(L, 1))||(!luaC_isclass(L, 2))) {lua_pushboolean(L, FALSE); return 1;}
	lua_settop(L, 2);
	timeud* p1 = (timeud*)lua_touserdata(L, 1);
	timeud* p2 = (timeud*)lua_touserdata(L, 2);

	if (p1->typ = p2->typ)
	{
		lua_pushboolean(L, (p1->val < p2->val));
	}
	else if (TudTypAbs(p1) && TudTypAbs(p2))
	{
		DOUBLE e1 = TimeEndAt(p1->val, p1->typ);
		lua_pushboolean(L, (e1 < p2->val));
	}
	else
	{
		lua_pushboolean(L, FALSE);
	}
	return 1;
}

static int LuaTimeLe(lua_State* L)
{
	if ((!luaC_isclass(L, 1))||(!luaC_isclass(L, 2))) {lua_pushboolean(L, FALSE); return 1;}
	lua_settop(L, 2);
	timeud* p1 = (timeud*)lua_touserdata(L, 1);
	timeud* p2 = (timeud*)lua_touserdata(L, 2);

	if (p1->typ == p2->typ)
	{
		lua_pushboolean(L, (p1->val <= p2->val));
	}
	else if (TudTypAbs(p1) && TudTypAbs(p2))
	{
		DOUBLE e1 = TimeEndAt(p1->val, p1->typ);
		DOUBLE e2 = TimeEndAt(p2->val, p2->typ);
		lua_pushboolean(L, (e1 <= e2));
	}
	else
	{
		lua_pushboolean(L, FALSE);
	}
	return 1;
}

void TimeFormat(CString &s, DOUBLE v, LPCWSTR k, LPCWSTR f)
{
	int x = s.Find(k);
	if (x >= 0)
	{
		CString vv(""); vv.Format(f, v);
		s = s.Left(x) + vv + s.Mid(x + 2);
	}
}

//P1 (String, opt) String containing format wildcards (a 'sensible' default is provided if absent).
//P2 (Number, opt) Time zone offset in hours, defaults to current locale.
//R1 (String) The formatted string.
static int LuaTimeFormat(lua_State* L)
{
	luaC_checkmethod(L, 2);
	timeud* ud = (timeud*)lua_touserdata(L, 1);
	lua_settop(L, 3);
	CString fmt("");
	double tzo = 0.0;
	if (lua_isstring(L, 2))
		fmt = CString(lua_tostring(L, 2));
	else if (lua_isnumber(L, 2))
		tzo = opttzo(L, 2);
	else if (!lua_isnil(L, 2))
		return luaL_argerror(L, 2, "");
	tzo = opttzo(L, 3, tzo);

	CString txt("");
	if (TudTypDur(ud))
	{
		if (fmt.GetLength() < 1)
		{
			if (abs(ud->val) > 1)
				txt = CString(TEXT("%D Days"));
			else if (abs(ud->val * (24 * 60)) > 1)
				txt = CString(TEXT("%M Minutes"));
			else
				txt = CString(TEXT("%S Seconds"));
		}
		else
		{
			txt = fmt;
		}
		TimeFormat(txt, ud->val / 7.0, TEXT("%W"), TEXT("%g"));
		TimeFormat(txt, ud->val / 7.0, TEXT("%w"), TEXT("%.0f"));
		TimeFormat(txt, ud->val, TEXT("%D"), TEXT("%g"));
		TimeFormat(txt, ud->val, TEXT("%d"), TEXT("%.0f"));
		TimeFormat(txt, ud->val * 24, TEXT("%H"), TEXT("%g"));
		TimeFormat(txt, ud->val * 24, TEXT("%h"), TEXT("%.0f"));
		TimeFormat(txt, ud->val * (24 * 60), TEXT("%M"), TEXT("%g"));
		TimeFormat(txt, ud->val * (24 * 60), TEXT("%m"), TEXT("%.0f"));
		TimeFormat(txt, ud->val * (24 * 60 * 60), TEXT("%S"), TEXT("%g"));
		TimeFormat(txt, ud->val * (24 * 60 * 60), TEXT("%s"), TEXT("%.0f"));
	}
	else
	{
		tzo = opttzo(L, 3);
		lua_settop(L, 0);

		if (fmt.GetLength() < 1)
		{
			switch (ud->typ)
			{
			case tud_tod:
				fmt += CString("%H:%M:%S");
				break;
			case tud_abs_year:
				fmt += CString("%Y");
				break;
			case tud_abs_month:
				fmt += CString("%B %Y");
				break;
			case tud_abs_day:
				fmt += CString("%d %B %Y");
				break;
			case tud_abs_hour:
				fmt += CString("Hour %H on %d %B %Y");
				break;
			case tud_abs_min:
				fmt += CString("%H:%M on %d %B %Y");
				break;
			default:
				fmt += CString("%H:%M:%S on %d %B %Y");
				break;
			}
		}

		SYSTEMTIME stl;
		tm tms;
		if (TimeSplit(ud->val, &stl, tzo))
		{
			tms.tm_hour = stl.wHour; tms.tm_min = stl.wMinute; tms.tm_sec = stl.wSecond;
			tms.tm_year = stl.wYear - 1900; tms.tm_mon = stl.wMonth - 1; tms.tm_mday = stl.wDay;
			tms.tm_isdst = 0; tms.tm_wday = stl.wDayOfWeek; tms.tm_yday = 0;
			wcsftime(txt.GetBuffer(128), 128, fmt, &tms);
			txt.ReleaseBuffer();
		}
	}
	luaX_pushstring(L, txt);
	return 1;
}

static int LuaTimeToString(lua_State* L)
{
	WINSH_LUA(1)
	lua_settop(L, 1);
	return LuaTimeFormat(L);
}

static int time_Construct(lua_State* L)
{
	WINSH_LUA(2)
	SYSTEMTIME stl;
	DOUBLE dt;
	double tzo = 100.0;

	lua_settop(L, 1);
	timeud* ud = (timeud*)lua_newuserdata(L, sizeof(timeud));
	ud->alm = 0;
	ud->typ = tud_abs;
	ud->val = 0.0;
	if (luaC_isclass(L, 1))
	{
		timeud* ud1 = (timeud*)lua_touserdata(L, 1);
		ud->typ = ud1->typ;
		ud->val = ud1->val;
	}
	else
	{
		GetLocalTime(&stl);
		if (lua_istable(L, 1))
		{
			int min = 6;
			BOOL tod = TRUE;
			lua_getfield(L, 1, "year");
			if (!lua_isnil(L, 3)) {min = 5; tod = FALSE;}
			stl.wYear = luaL_optinteger(L, 3, stl.wYear);
			lua_pop(L, 1);
			lua_getfield(L, 1, "month");
			if (!lua_isnil(L, 3)) {min = 4; tod = FALSE;}
			stl.wMonth = luaL_optinteger(L, 3, stl.wMonth);
			lua_pop(L, 1);
			lua_getfield(L, 1, "day");
			if (!lua_isnil(L, 3)) {min = 3; tod = FALSE;}
			stl.wDay = luaL_optinteger(L, 3, stl.wDay);
			lua_pop(L, 1);
			lua_getfield(L, 1, "hour");
			if (!lua_isnil(L, 3)) min = 2;
			stl.wHour = luaL_optinteger(L, 3, stl.wHour);
			lua_pop(L, 1);
			lua_getfield(L, 1, "minute");
			if (!lua_isnil(L, 3)) min = 1;
			stl.wMinute = luaL_optinteger(L, 3, stl.wMinute);
			lua_pop(L, 1);
			lua_getfield(L, 1, "second");
			if (!lua_isnil(L, 3)) min = 0;
			dt = luaL_optnumber(L, 3, (DOUBLE)stl.wSecond + (DOUBLE)stl.wMilliseconds / 1000.0);
			stl.wSecond = (int)dt;
			stl.wMilliseconds = (WORD)((dt - stl.wSecond) * 1000.0);
			lua_pop(L, 1);
			lua_getfield(L, 1, "timezone");
			tzo = opttzo(L, 3);
			lua_pop(L, 1);
			lua_getfield(L, 1, "time");
			if (lua_toboolean(L, -1)) {tod = TRUE; min = 0;}
			lua_pop(L, 1);
			lua_getfield(L, 1, "date");
			if (lua_toboolean(L, -1)) {min = 2; tod = FALSE;}
			lua_pop(L, 1);
			if (min < 6)
			{
				if (min > 0) {stl.wSecond = 0; stl.wMilliseconds = 0;}
				if (min > 1) stl.wMinute = 0;
				if (min > 2) stl.wHour = 0;
				if (min > 3) stl.wDay = 1;
				if (min > 4) stl.wMonth = 1;
				if (tod)
				{
					ud->val = (DOUBLE)stl.wHour + (DOUBLE)stl.wMinute / 60 + (DOUBLE)stl.wSecond / (60 * 60) + (DOUBLE)(stl.wMilliseconds + 1) / (60 * 60 * 1000);
					ud->val /= 24;
					ud->typ = tud_tod;
				}
				else
				{
					ud->val = TimeMake(&stl, tzo);
					ud->typ = (TudTyp)min;
				}
			}
			else
			{
				ud->val = 0.0;
				ud->typ = tud_dur;
				DOUBLE d;
				lua_getfield(L, 1, "seconds");
				d = luaL_optnumber(L, 3, 0.0);
				lua_pop(L, 1);
				ud->val += d / (24 * 60 * 60);
				lua_getfield(L, 1, "minutes");
				d = luaL_optnumber(L, 3, 0.0);
				lua_pop(L, 1);
				ud->val += d / (24 * 60);
				lua_getfield(L, 1, "hours");
				d = luaL_optnumber(L, 3, 0.0);
				lua_pop(L, 1);
				ud->val += d / 24;
				lua_getfield(L, 1, "days");
				d = luaL_optnumber(L, 3, 0.0);
				lua_pop(L, 1);
				ud->val += d;
				lua_getfield(L, 1, "weeks");
				d = luaL_optnumber(L, 3, 0.0);
				lua_pop(L, 1);
				ud->val += d * 7;
			}
		}
		else
		{
			ud->val = TimeMake(&stl);
		}
	}
	if (luaC_gettid(L,0) != 1) return luaL_error(L, "Bad Class");
	lua_setmetatable(L, -2);									//|T|
	return 1;
}

void time_Create(lua_State* L)
{
	static const struct luaL_Reg ml [] = {
		{"type", LuaTimeType},
		{"time", LuaTimeTime},
		{"date", LuaTimeDate},
		{"day", LuaTimeDay},
		{"value", LuaTimeValue},
		{"zone", LuaTimeZone},
		{"alarm", LuaTimeAlarm},
		{"format", LuaTimeFormat},
		{"__tostring", LuaTimeToString},
		{"__add", LuaTimeAdd},
		{"__sub", LuaTimeSub},
		{"__mul", LuaTimeMul},
		{"__div", LuaTimeDiv},
		{"__unm", LuaTimeUnm},
		{"__eq", LuaTimeEq},
		{"__lt", LuaTimeLt},
		{"__le", LuaTimeLe},
		{"__gc", LuaTimeGc},
		{NULL, NULL}
	};
	luaC_newclass(L, time_Construct, ml);
}

// =================================================================================================

LUATMLIB_API int LUATMLIB_NGEN(luaopen_)(lua_State* L)
{
	WINSH_LUA(6)
	H->Require(CString("class"));

	// Allocate pool of messages for alarms:
	time_cmd = H->AllocLuaMessages(TIME_ALARMS);

	time_Create(L);

	return 1; // Return the Time class NOT a library table!
}
