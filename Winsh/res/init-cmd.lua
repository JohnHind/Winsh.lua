--Command Mode Initialisation
--[[
This chunk is executed to initialise the special environment
table used to execute immediate commands. This table has a
metatable indexing the Global Environment so it has read,
but not direct write access to the Global Environment.
Some specific fields may be defined as follows:
'onhelp', 'onabout', 'oninventory', 'oncommandline': These
must be functions which are run on selection of Help menu
items.
'onreportclear': Must be a function. Called when the report
pane (lower pane) on the console is cleared.
'_EQUAL': Must be a string. This overwrites the default
handling of command lines beginning with '='. The string
may include the token '!!' (possibly more than once) which
gets replaced by the text following the '='. After this
substitution, the string must be valid Lua code.
'_PROMPT': Must be a string which overwrites the default
command prompt ('>').
--]]

_L = _ENV

require = function(lib)
  if package.loaded[lib] then return package.loaded[lib] end
  local dl = package.preload[lib]
  if type(dl) == "function" then
    _L[lib] = dl(lib)
    _G[lib] = nil
    _G.package.loaded[lib] = nil
  end
  return _L[lib]
end

debug = require("debug")

local pad = function(s,w)
  local r = #s
  if r >= w then return s .. "\t" end
  r = w - r
  local n = s
  while r > 0 do
	s = s .. "\t"
	r = r - 4
  end
  return s
end

onhelp = function()
  local h = [=[
Detailed documentation for Winsh.lua is in the file
'Winsh.html' in the runtime distribution.
For Lua documentation, see:
http://www.lua.org/manual/5.2/
http://www.lua.org/pil/contents.html
Lines beginning with the command prompt '>' are executed
immediately on pressing ENTER. Command lines can be
repeated by placing the cursor anywhere in a line and
pressing ENTER. Prefix a variable name or expression
with '=' to display its value.
The command prompt may be deleted to enter multi-line
code. Press F5 to execute selected code or the entire
contents if there is no selection.]=]
  _G.print(expandhelp(h))
end

onabout = function()
  local a = [=[
{productname}
Version: {productversion} {specialbuild} {privatebuild}
{legalcopyright}
https://github.com/JohnHind/Winsh.lua

Based on the Lua Programming Language
By: {luaauthors}
{luacopyright}
http://www.lua.org/

Uses Windows Template Library - WTL Version 9.0
Copyright © 2014 Microsoft Corporation, WTL Team.
http://sourceforge.net/projects/wtl/

Open Source, free of charge and without warranty.
See web sites for detailed licence and sources.]=]
  _G.print(expandhelp(a))
end

oninventory = function()
  _G.print(expandhelp("{configuration}"))
  _G.print(expandhelp("{subsystem}"))
  _G.print("====LIBRARIES")
  _G.print("(Libraries marked 'available' must be loaded using Lua 'require')")
  for k, v in Table(getinventory("libraries")) do
      if package.loaded[k] then
	    _G.print(pad(k,12) .. "[loaded]    " .. v)
	  else
	    if _L[k] then
	      _G.print(pad(k,12) .. "[cmd-only]  " .. v)
	    else
	      _G.print(pad(k,12) .. "[available] " .. v)
	    end
	  end
  end
  _G.print("====SCRIPTS")
  for k, v in Table(getinventory("scripts")) do
      if type(v) == "string" then
          _G.print(pad(k,12) .. v)
      else
          _G.print(k)
      end
  end
  _G.print("====ICONS")
  for k, v in Table(getinventory("icons")) do
      _G.print(pad(k,12) .. v)
  end
end

oncommandline = function()
  local h = [=[
{exename} [options] [scriptname [args]]
OPTIONS:
-e'lua':  Compiles and executes 'lua' as a source code string.
-l'mod':  Compiles and executes 'mod = require(mod)'.
-i     :  Enters interactive mode after running the script.
-v     :  Prints version information.
--     :  Ignore any further options.
SCRIPT:
'scriptname' may be the name of a LUA resource in this exe file,
the name of a file or folder in the same folder as this exe
file, or the full path to a file or folder. If a folder, it must
contain a file '{startname}{luaext}'. Note that any '-l' text is
executed first, then 'scriptname', then any '-e' text.
ARGUMENTS:
Any further arguments after the scriptname are passed through to
the script using the Lua varargs mechanism. A parameter that is
valid as a number string becomes a number, 'true' or 'false'
becomes a boolean, 'nil' becomes nil and anything else becomes
a string. Interpretation as a string may be forced by enclosing
in quote marks.]=]
  _G.print(expandhelp(h))
end

onreportclear = function()
  tab = nil
end

local vprint = function(v,l,k)
  if (type(v) == "table") and l then
    if (type(k) == "string") then
	  return l..[=[["]=]..k..[=["]]=]
	elseif (type(k) == "number") then
	  return l..[=[[]=]..k..[=[]]=]
	elseif (type(k) == "boolean") then
	  if k then
	    return l..[=[[true]]=]
	  else
	    return l..[=[[false]]=]
	  end
	else
      return eprint(v)
	end
  else
    return eprint(v)
  end
end

eprint = function(v,l,r)
  local s = "" local t = type(v)
  if t == "number" then
    s = tostring(v)
  elseif t == "string" then
    s = [["]]..tostring(v)..[["]]
  elseif t == "boolean" then
	if v then s = "true" else s = "false" end
  elseif (t == "table") and not (v == tab) then
    if r then
      print(l.." = {")
      for k,x in class.pairs(v) do
        print("["..eprint(k).."] = "..vprint(x,l,k)..";")
      end
      print("}")
      return
    else
	  if not tab then tab = {} end
	  local tr = #tab + 1
	  tab[tr] = v
	  s = "tab["..tr.."]"
	end
  else
    s = tostring(v) or "<"..t..">"
  end
  if l then return l.." = "..s else return s end
end

_EQUAL = [==[return eprint((!!),[=[!!]=],true)]==]
