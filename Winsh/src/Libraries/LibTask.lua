--Lua Part for 'task' library
local T = ...
local C = require("class")

WM = WM or {}
WM["CLIPBOARDUPDATE"] = 0x031D

VK = VK or {}
VK["DELAY"] = -1

do  -- Class 'Frame'
	local _TID = C.newmeta()
	-- Constructor:
	local _F = function(l, t, w, h)
		local o = {}
		o._l = l or 0; o._t = t or 0
		local h = h or w
		if w and w > 0 and h and h > 0 then
		  o._w = w; o._h = h
		end
		C.setmetatable(o, _TID)
		return o
	end

	-- Methods:
	function _TID:position()
	    C.checkmethod(self, _F)
	    return self._l, self._t
	end
	function _TID:size()
	    C.checkmethod(self, _F)
	    if self._w and self._h then return self._w, self._h end
		return nil
	end
	local pl1 = function(x, y, z) return x end
	local pl2 = function(x, y, z) return x + (y / 2) - (z / 2) end
	local pl3 = function(x, y, z) return x + y - z end
	local _V1 = {lefttop=pl1, leftcenter=pl1, leftbottom=pl1,
	             centertop=pl2, center=pl2, centerbottom=pl2,
				 righttop=pl3, rightcenter=pl3, rightbottom=pl3}
	local _V2 = {lefttop=pl1, centertop=pl1, righttop=pl1,
	             leftcenter=pl2, center=pl2, rightcenter=pl2,
				 leftbottom=pl3, centerbottom=pl3, rightbottom=pl3}
	function _TID:place(ke, fr)
	    C.checkmethod(self, _F)
		if not (self._w and self._h) then error("place operation requires an area") end
		local l, t, w, h
		local fl, ft, fw, fh
		local ke = ke or "center"
		if fr then
		  C.checkmethod(fr, _F)
		  fl, ft = fr._l, fr._t
		  if fr._w and fr._h then fw, fh = fr._w, fr._h end
		else
		  fl, ft = 0, 0
		end
		if fw and fh then
	        if fw > self._w then w = self._w else w = fw end
	        if fh > self._h then h = self._h else h = fh end
		else
		    w, h = 0, 0
		end
		local x = _V1[ke]; if not x then error("bad place key") end
		l = x(self._l, self._w, w)
		x = _V2[ke]; if not x then error("bad place key") end
		t = x(self._t, self._h, h)
		return _F(l, t, w, h)
	end

    -- Metamethods:
	function _TID:__tostring()
	    C.checkmethod(self, _F)
		local s = "Frame: (" .. self._l .. ", " .. self._t .. ")"
		if self._w and self._h then
		  s = s .. " " .. self._w .. " x " .. self._h
		end
		return s
	end

	T.Frame = _F
end -- Class 'Frame'
