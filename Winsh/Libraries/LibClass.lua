--Lua Part for 'class' library

local T = ...  -- This table becomes the 'Class' library
--T.gettid     -- Defined in 'C'
--Functions copied into 'class' table:
T.type = _G.type
T.tostring = _G.tostring
T.tonumber = _G.tonumber
--Functions moved into 'class' table:
T.getmetatable = _G.getmetatable; _G.getmetatable = nil
T.setmetatable = _G.setmetatable; _G.setmetatable = nil
T.pairs = _G.pairs; _G.pairs = nil
T.next = _G.next; _G.next = nil
T.ipairs = _G.ipairs; _G.ipairs = nil
T.rawget = _G.rawget; _G.rawget = nil
T.rawset = _G.rawset; _G.rawset = nil
T.rawlen = _G.rawlen; _G.rawlen = nil
T.rawequal = _G.rawequal; _G.rawequal = nil

T.istype = function(ob, ty)
  local tob = T.type(ob)
  local tid1, isc = T.gettid(ob)
  if T.type(ty) == 'string' and ty ~= "" then
    if ty == "class" then
	  return isc == 'class'
	elseif ty == "object" then
	  return isc ~= 'type'
	elseif ty == "rawtable" then
	  if tob ~= "table" then return false end
	  return T.getmetatable(ob) == nil
	elseif ty == "callable" then
	  if tob == "function" then return true end
	  local m = T.getmetatable(ob)
	  if not m then return false end
	  return T.type(m.__call) == "function"
	else
	  return tob == ty
	end
  end
  local tid2 = T.gettid(ty)
  if tid2 == tob then return true end
  if T.type(tid1) == 'function' and tid1 == tid2 then return not not (tid1(ob, ty)) end
  if T.type(tid1) == 'table' and tid1[1] then
    for i=1, #tid1 do
	  if tid2 == tid1[i] then return true end
	end
  end
  if isc == 'object_mt' and T.type(tid2) == 'table' then
    repeat
	  if tid2 == tid1 then return true end
	  tid1 = T.getmetatable(tid1)
	until not tid1
  end
  return tid1 == tid2
end

T.checkmethod = function(sl, cl)
	if not T.istype(sl, cl) then error("Bad method call") end
end

T.newmeta = function(pcl)
    local mt = {}
	if pcl then
		if not T.istype(pcl,'class') then error("Bad Parent Class") end
		local pmt = T.gettid(pcl)
		if not T.istype(pmt, 'table') then error("Bad Parent Class") end
		T.setmetatable(mt, pmt)
	else
		T.setmetatable(mt, nil)
	end
	T.rawset(mt, "__index", mt)
	return mt, pcl
end

T.newproto = function(...)
  local _TID, _PRT, pmt = {}, {}, nil
  for i, t in T.ipairs{...} do
    if T.type(t) ~= 'table' then error("prototype must be table") end
    pmt = T.getmetatable(t)
    for k, v in T.pairs(t) do _PRT[k] = v end
    if pmt then 
	  for k, v in T.pairs(pmt) do _TID[k] = v end
	  _TID[#_TID + 1] = pmt
	end
  end
  local _C = function(init)
    local tid, prt, ob = _TID, _PRT, {}
    for k, v in T.pairs(prt) do ob[k] = v end
    if init then for k, v in T.pairs(init) do
      if ob[k] == nil then
	    error("attempt to initialise non-existant field: " .. k)
	  end
	  if T.type(k) ~= 'string' or k:sub(1,1) == '_' then
	    error("attempt to initialise private field: " .. k)
	  end
      ob[k] = v
    end end
    T.setmetatable(ob, tid)
    return ob
  end
  return _C, _PRT, _TID
end

do  -- Class 'Table'
	local _TID = T.newmeta()
	local _C = function(s)
		local o
		if T.istype(s,'rawtable') then o = s else o = {} end
		if T.getmetatable(s) == _TID then for k,v in T.pairs(s) do o[k] = v end end
		T.setmetatable(o, _TID)
		return o
	end
	function _TID:count(lim)
		T.checkmethod(self, _C)
		if lim and not T.istype(lim,"number") then error("Parameter must be number type") end
		local c = 0
		for k in T.pairs(self) do
			c = c + 1
			if lim and c >= lim then break end
		end
		return c
	end
	function _TID:invert(dup)
		T.checkmethod(self, _C)
		if dup and not T.istype(dup,"table") then error("Parameter must be table type") end
		local o = _C{}
		for k,v in T.pairs(self) do
		    if T.istype(o[v],'nil') then
			    o[v] = k
			elseif dup then
			    dup[#dup+1] = v
			end
		end
		return o
	end
	function _TID:merge(tab,ovr)
		T.checkmethod(self, _C)
		if not T.istype(tab,"table") then error("Parameter must be table type") end
		if ovr and not T.istype(ovr,"table") then error("Parameter must be table type") end
		for k,v in T.pairs(tab) do
			if ovr and self[k] then ovr[k] = self[k] end
		    self[k] = v
		end
	end
	function _TID:__tostring()
		return "Table{ ... }"
	end
	_TID.__iter = T.pairs
	_TID.pairs  = T.pairs
	_TID.rawget = T.rawget
	_TID.rawset = T.rawset
	T.Table = _C
end -- Class 'Table'

do  -- Class 'List' (extends partial implementation in C)
    local _C = T.List
	local _TID = T.gettid(_C)
	-- _TID:unpack -- already present from 'C'
	-- _TID:sort   -- already present from 'C'
	function _TID:add(v)
		T.checkmethod(self, _C)
		self[#self+1] = v
	end
	function _TID:insert(ip, v)
		T.checkmethod(self, _C)
		if v == nil then return end
		local t = #self + 1
		ip = T.tonumber(ip)
		if ip < 0 then ip = t + ip end
		if ip == 0 then ip = t end
		if ip < 1 or ip > t then error("Insert: index out of range") end
		if T.istype(v, _C) and not v.__whole then
			local sv = #v
			local p = t - ip
			if sv <= p then
				-- Insert size <= above part (partition the above part)
				p = t - sv
				for i=0, t-p-1 do self[t+i] = self[p+i] end
				p = ip + sv
				for i=t-ip-sv-1,0,-1 do self[p+i] = self[ip+i] end
				for i=0, sv-1 do self[ip+i] = v[1+i] end
			else
				-- Insert size > above part (partition the insert)
				p = p + 1
				for i=0, sv-p do self[t+i] = v[p+i] end
				if t > ip then
					local b = t + (sv - p)
					for i=0, t-ip do self[b+i] = self[ip+i]; self[ip+i] = v[i+1] end
				end
			end
		else
			-- Insert size == 1
			for i = t-1, ip, -1 do self[i+1] = self[i] end
			self[ip] = v
		end
	end
	function _TID:remove(p1, p2)
		T.checkmethod(self, _C)
		local t = #self + 1
		p1 = T.tonumber(p1 or t-1)
		if p1 < 0 then p1 = t + p1 end
		if p1 < 1 or p1 >= t then error("Remove: index out of range") end
		p2 = T.tonumber(p2 or p1)
		if p2 < 0 then p2 = t + p2 end
		if p2 < p1 then p2 = p1 end
		local d = p2 - p1 + 1
		for i=p2+1, t-1 do self[i-d] = self[i] end
		for i=t-1, t-d, -1 do self[i] = nil end
	end
	function _TID:reverse()
		T.checkmethod(self, _C)
		local e = #self
		for i=1, e/2 do
			self[i], self[e] = self[e], self[i]
			e = e - 1
		end
	end
	function _TID:__newindex(k, v)
		T.checkmethod(self, _C)
		if T.istype(k,'number') and k == #self+1 then
			T.rawset(self, k, v)
		else
			error("Invalid key: "..T.tostring(k).." for List object")
		end
	end
	function _TID:__iter()
		local ix = 0
		return function() ix = ix + 1; return self[ix] end
	end
	function _TID:concat(sep, i, j)
	    T.checkmethod(self, _C)
		local s = ""
		sep = T.tostring(sep or " ")
		i = T.tonumber(i or 1)
		j = T.tonumber(j or #self)
		if i < 1 then i = #self + i + 1 end
		if j < 1 then j = #self + j + 1 end
		if i < 1 then i = 1 end
		if i > #self then i = #self end
		if j < i then j = i end
		for k = i, j do
			if #s > 0 then s = s .. sep end
			s = s .. T.tostring(self[k])
		end
		return s
	end
	function _TID:__tostring()
		return "List{ " .. #self .. " items }"
	end
end -- Class 'List'

do  -- Class 'DelegateList' (Inherits List)
	local _TID, _PC = T.newmeta(T.List)
	local _C = function(...)
		local o = _PC(...)
		for i=1, #o do if not T.istype(o[i], "callable") then error("Invalid DelegateList member (must be callable)") end end 
		T.setmetatable(o, _TID)
		return o
	end
	T.rawset(_TID, "__call", function(self, ...)
		T.checkmethod(self, _C)
		local r, f
		for i=1, #self do
			f = self[i]
			if T.istype(f,'callable') then
				r = f(...)
				if not T.istype(r,'nil') then return r end
			end
		end
	end)
	T.rawset(_TID, "__newindex", function(self, k, v)
		T.checkmethod(self, _C)
		if T.istype(k,'number') and k == #self+1 then
			if T.istype(v, "callable") then
				T.rawset(self, k, v)
			else
				error("DelegateList values must be callable")
			end
		else
			error("Invalid key for List object")
		end
	end)
	T.rawset(_TID, "__tostring", function(self)
		return "DelegateList{ " .. #self .. " callables }"
	end)
	T.DelegateList = _C
end -- Class 'DelegateList'

do  -- Class 'Set'
	local _TID = T.newmeta()
	local _C = function(...)
		local o
		local t,n = ...
		local p = true
		if n == nil then
			if T.istype(t,'table') then
				if not T.getmetatable(t) then p = false; o = t end
				if T.getmetatable(t) == _TID then p = false; o = {}; for k in T.pairs(t) do o[k] = true end end
			end
		end
		o = o or {}
		if p then for k,v in T.pairs{...} do o[v] = true end end
		for k,v in T.pairs(o) do if v ~= true then error("Invalid Set member") end end
		T.setmetatable(o, _TID)
		return o
	end
	function _TID.__add(p1, p2)
		T.checkmethod(p1, _C)
		local rv = _C{}
		for k,v in T.pairs(p1) do if v then rv[k] = true end end
		if T.istype(p2, _C) then
			for k,v in T.pairs(p2) do if v then rv[k] = true end end
		else
			rv[p2] = true
		end
		return rv
	end
	function _TID.__sub(p1, p2)
		T.checkmethod(p1, _C)
		local rv = _C{}
		if T.istype(p2, _C) then
			for k,v in T.pairs(p1) do if v and not p2[k] then rv[k] = true end end
		else
			rv[p2] = nil
		end
		return rv
	end
	function _TID.__eq(p1, p2)
		T.checkmethod(p1, _C)
		T.checkmethod(p2, _C)
		for k,v in T.pairs(p1) do if v and not p2[k] then return false end end
		for k,v in T.pairs(p2) do if v and not p1[k] then return false end end
		return true
	end
	function _TID.__lt(p1, p2)
		T.checkmethod(p1, _C)
		T.checkmethod(p2, _C)
		for k,v in T.pairs(p1) do if v and not p2[k] then return false end end
		for k,v in T.pairs(p2) do if v and not p1[k] then return true end end
		return false
	end
	function _TID.__le(p1, p2)
		T.checkmethod(p1, _C)
		T.checkmethod(p2, _C)
		for k,v in T.pairs(p1) do if v and not p2[k] then return false end end
		return true
	end
	function _TID:__newindex(k, v)
		T.checkmethod(self, _C)
		if v then T.rawset(self, k, true) end
	end
	function _TID:__iter()
		local k = nil
		return function() k = T.next(self, k); return k end
	end
	function _TID:concat(sep)
	    T.checkmethod(self, _C)
		local s = ""
		sep = T.tostring(sep or " ")
		for k,v in T.pairs(self) do
			if v then
				if #s > 0 then s = s .. sep end
				s = s .. k
			end
		end
		return s
	end
	function _TID:__tostring()
		return "Set{ ... }"
	end
	T.Set = _C
end -- Class 'Set'
