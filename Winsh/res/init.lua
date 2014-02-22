--General Initialisation

--[[
Standard initialisation: 'table', 'io' and 'os' standard libraries are
not loaded by default as their features are supplied by the Winsh.lua
libraries (but they may be loaded if required by standard Lua code).
'debug' is loaded in the command console environment, but not here.

Lowercase variables represent libraries (tables of functions) whilst
capitalised symbols represent Classes (functions which return objects).
--]]

--Redefine 'tonumber' to honour '__tonumber' metamethod:
local otn = _G.tonumber
local gmt = _G.getmetatable
_G.tonumber = function(x,r)
  if type(x) == "string" or type(x) == "number" then
	return otn(x,r)
  else
    local mt = gmt(x)
	if not mt then return nil end
	local mf = mt.__tonumber
	if type(mf) ~= "function" then return nil end
	return mf(x)
  end
end

--Functions moved into 'lua' table:
_G.lua = {}
_G.lua.collectgarbage = _G.collectgarbage; _G.collectgarbage = nil
_G.lua.dofile = _G.dofile; _G.dofile = nil
_G.lua.load = _G.load; _G.load = nil
_G.lua.loadfile = _G.loadfile; _G.loadfile = nil
_G.lua.pcall = _G.pcall; _G.pcall = nil
_G.lua._VERSION = _G._VERSION; _G._VERSION = nil
_G.lua.xpcall = _G.xpcall; _G.xpcall = nil

--Standard libraries that are normally required:
string = require("string")
--Function copied to the string metatable:
string.rawlen = rawlen

math = require("math")
coroutine = require("coroutine")

--Winsh specific libraries:
winsh = require("winsh")
class = require("class")
Table = require("class").Table					-- Export standard OO stuff to global table
List = require("class").List					--
Set = require("class").Set						--
DelegateList = require("class").DelegateList	--
istype = require("class").istype				-- End
Bitfield = require("bitfield").Bitfield
bitrange = require("bitfield").bitrange
Time = require("time")
Timer = require("timer")
shell = require("shell")
task = require("task")

--Optional Winsh specific libraries:
--RegistryKey = require("registry")
--Com = require("com")

--Probably do not require these standard libraries:
--table = require("table")
--io = require("io")
--os = require("os")
--bit32 = require("bit32")
--debug = require("debug")
