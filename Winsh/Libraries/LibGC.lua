--Lua Part for 'winsh' library
local T = ...
local C = require("class")

do  -- Class 'Menu' (inherits List methods, but is not a list)
	local _TID, _PC = C.newmeta(C.List)
	local _C = function(...)
	    local o = _PC()
		C.rawset(o, "disabled", false)
		for i=1, 3 do
			local x = select(i,...)
			if not o.title and C.istype(x,'string') then
			    C.rawset(o, "title", x)
			elseif not o.onshow and C.istype(x,'callable') then
			    C.rawset(o, "onshow", x)
			elseif C.type(x) ~= 'nil' then
			    C.rawset(o, "gutter", not not x)
			end
		end
		C.setmetatable(o, _TID)
		return o
	end
	C.rawset(_TID, "__tostring", function(self)
		return "Menu:" .. (self.title or "anon")
	end)
	C.rawset(_TID, "__iter", C.gettid(_PC).__iter)
	C.rawset(_TID, "__newindex", C.gettid(_PC).__newindex)
	C.rawset(_TID, "__whole", true)
	T.Menu = _C
end -- Class 'Menu'

do  -- Class 'Command'
	local _TID = C.newmeta()
	local _C = function(p1,p2,p3)
		local o
		if C.istype(p1,'rawtable') then o = p1 else o = {} end
		if C.istype(p1,'string') then o.title = p1 end
		if C.istype(p2,'callable') then o.action = p2 end
		if not C.istype(p3,'nil') then o.value = p3 end
		C.setmetatable(o, _TID)
		return o
	end
	function _TID:__call(x,y,z)
		C.checkmethod(self, _C)
		if self.disabled then return end
		if C.istype(self.value,'boolean') then self.value = not self.value end
		if self.action then self.action(self.value,z) end
	end
	function _TID:__tostring()
		return "Command:" .. (self.title or "anon")
	end
	_TID.__iter = C.pairs
	T.Command = _C
end -- Class 'Command'

GT = {					--Winsh message codes scripts may want to capture.
						--These are the ones defined by the runtime and the Winsh library
						--Other libraries may add to this table.
	STARTUP = 1024;
	ERROR = 1025;
	NOTIFY_LCLICK = 1027;
	NOTIFY_RCLICK = 1028;
	REPORT_RCLICK = 1029;
	REPORT_DROP = 1030;
}

WM = {					--Windows Message codes scripts may want to capture.
						--Generally only messages broadcast to all top-level windows are useful.
	ENDSESSION = 0x0016;
	DEVICECHANGE = 0x0219;
	POWERBROADCAST = 0x0218;
	DISPLAYCHANGE = 0x007E;
	TIMECHANGE = 0x001E;
	USERCHANGED = 0x0054;
	SETTINGCHANGE = 0x001A;
	THEMECHANGED = 0x031A;
	SPOOLERSTATUS = 0x002A;
	FONTCHANGE = 0x001D;
	DEVMODECHANGE = 0x001B;
}

DBT = {					--Wparam Message sub-codes for WM.DEVICECHANGE message.
	CONFIGCHANGED = 0x0018;
	DEVICEARRIVAL = 0x8000;
	DEVICEPENDING = 0x8003;
	DEVICEREMOVED = 0x8004;
}

PBT = {					--Wparam Message sub-codes for WM.POWERBROADCAST message.
	BATTERYLOW = 0x0009;
	POWERSTATUS = 0x000A;
	SUSPEND = 0x0004;
	RESUMEAUTO = 0x0012;
	RESUMECRITICAL = 0x0006;
	RESUMESUSPEND = 0x0007;
}

VK = {					--Virtual Key codes for winsh.hotkey.
	LBUTTON = 0x01;
	RBUTTON = 0x02;
	CANCEL = 0x03;
	MBUTTON = 0x04;
	XBUTTON1 = 0x05;
	XBUTTON2 = 0x06;
	BACK = 0x08;
	TAB = 0x09;
	CLEAR = 0x0C;
	RETURN = 0x0D;
	SHIFT = 0x10;
	CONTROL = 0x11;
	MENU = 0x12;
	PAUSE = 0x13;
	CAPITAL = 0x14;
	ESCAPE = 0x1B;
	SPACE = 0x20;
	PRIOR = 0x21;
	NEXT = 0x22;
	END = 0x23;
	HOME = 0x24;
	LEFT = 0x25;
	UP = 0x26;
	RIGHT = 0x27;
	DOWN = 0x28;
	SELECT = 0x29;
	PRINT = 0x2A;
	EXECUTE = 0x2B;
	SNAPSHOT = 0x2C;
	INSERT = 0x2D;
	DELETE = 0x2E;
	HELP = 0x2F;
	SLEEP = 0x5F;
	NUMPAD0 = 0x60;
	NUMPAD1 = 0x61;
	NUMPAD2 = 0x62;
	NUMPAD3 = 0x63;
	NUMPAD4 = 0x64;
	NUMPAD5 = 0x65;
	NUMPAD6 = 0x66;
	NUMPAD7 = 0x67;
	NUMPAD8 = 0x68;
	NUMPAD9 = 0x69;
	MULTIPLY = 0x6A;
	ADD = 0x6B;
	SEPARATOR = 0x6C;
	SUBTRACT = 0x6D;
	DECIMAL = 0x6E;
	DIVIDE = 0x6F;
	F1 = 0x70;
	F2 = 0x71;
	F3 = 0x72;
	F4 = 0x73;
	F5 = 0x74;
	F6 = 0x75;
	F7 = 0x76;
	F8 = 0x77;
	F9 = 0x78;
	F10 = 0x79;
	F11 = 0x7A;
	F12 = 0x7B;
	F13 = 0x7C;
	F14 = 0x7D;
	F15 = 0x7E;
	F16 = 0x7F;
	F17 = 0x80;
	F18 = 0x81;
	F19 = 0x82;
	F20 = 0x83;
	F21 = 0x84;
	F22 = 0x85;
	F23 = 0x86;
	F24 = 0x87;
	NUMLOCK = 0x90;
	SCROLL = 0x91;
	BROWSER_BACK = 0xA6;
	BROWSER_FORWARD = 0xA7;
	BROWSER_REFRESH = 0xA8;
	BROWSER_STOP = 0xA9;
	BROWSER_SEARCH = 0xAA;
	BROWSER_FAVORITES = 0xAB;
	BROWSER_HOME = 0xAC;
	VOLUME_MUTE = 0xAD;
	VOLUME_DOWN = 0xAE;
	VOLUME_UP = 0xAF;
	MEDIA_NEXT_TRACK = 0xB0;
	MEDIA_PREV_TRACK = 0xB1;
	MEDIA_STOP = 0xB2;
	MEDIA_PLAY_PAUSE = 0xB3;
	LAUNCH_MAIL = 0xB4;
	LAUNCH_MEDIA_SELECT = 0xB5;
	LAUNCH_APP1 = 0xB6;
	LAUNCH_APP2 = 0xB7;
}
