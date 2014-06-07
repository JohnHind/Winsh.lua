--Command Mode Initialisation
--[[
This chunk is executed to initialise the special environment
table used to execute immediate commands.
--]]

_L = _ENV

_PROMPT = ">"

require = function(lib)
  if _G.package.loaded[lib] then return _G.package.loaded[lib] end
  return _G.require(lib)
end

debug = require("debug")

local pad = function(s, w)
  local r = #s
  if r >= w then return s .. " " end
  return s .. string.rep(" ", w - r)
end

onhelp = function()
  writehelp([=[
Detailed documentation for Winsh.lua is in the file
'Winsh.html' in the runtime distribution.
For Lua documentation, see:
  http://www.lua.org/manual/5.2/
  http://www.lua.org/pil/contents.html
Lines in the upper pane beginning with the command
prompt '>' are executed immediately on pressing ENTER.
Command lines can be repeated by placing the cursor
anywhere in a line and pressing ENTER. Prefix a
variable name or expression with '=' to display its
value.
Multi-line scripts may be entered in the lower pane,
or a file may be dropped onto this pane. This script
will be executed if it has changed prior to executing
the command in the upper pane.]=])
end

onabout = function()
  writehelp([=[
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
See web sites for detailed licence and sources.]=])
end

oninventory = function()
  writehelp("{configuration}")
  writehelp("{subsystem}")
  writehelp("====LIBRARIES")
  writehelp("(Libraries marked 'available' must be loaded using Lua 'require')")
  for k, v in Table(getinventory("libraries")) do
      if package.loaded[k] then
	    writehelp(pad(k,12) .. "[loaded]    " .. v)
	  else
	    if _L[k] then
	      writehelp(pad(k,12) .. "[cmd-only]  " .. v)
	    else
	      writehelp(pad(k,12) .. "[available] " .. v)
	    end
	  end
  end
  writehelp("====RESOURCE SCRIPTS")
  for k, v in Table(getinventory("scripts")) do
      writehelp(pad(k,12) .. "|" .. v)
  end
  writehelp("====SCRIPT FILES")
  for k, v in Table(getinventory("files")) do
      writehelp(pad(k,12) .. "|" .. v)
  end
  writehelp("====ICONS")
  for k, v in Table(getinventory("icons")) do
      writehelp(pad(k,12) .. "[" .. v .. "]")
  end
end

oncommandline = function()
  writehelp([=[
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
in quote marks.]=])
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
      dprint(l.." = {")
      for k,x in class.pairs(v) do
        dprint("["..eprint(k).."] = "..vprint(x,l,k)..";")
      end
      dprint("}")
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

local prepcommand = function(str, ev)
    str = str:gsub("^%s*(.-)%s*$", "%1")
	if str:sub(1,1) == "=" then
	  ev = true
      str = str:sub(2):gsub("^%s*(.-)%s*$", "%1")
	end
	if ev then
	  str = "return(eprint((" .. str .. "),[=[" .. str .. "]=],true))"
	end
	return str
end

local exec = function(f, e)
  if f then f, e = lua.pcall(f) end
  if not f then e = "ERROR:" .. e end
  if type(e) == "string" then dprint(e) end
  return f
end

onevaluate = function(str, st)
  local f, e, s
  if str:sub(1, #_PROMPT) == _PROMPT then
    str = prepcommand(str:sub(2))
    console('prep', str)
	f, e = console('script')
	if f ~= false then
	  dprint("**Executing script from lower pane:")
	  exec(f, e)
	end
	if #str > 0 then
	  dprint("**Executing command line:")
      exec(lua.load(str, "CommandPane", 't', _L))
	end
    console('prompt')
	return
  elseif st then
    s, st = str:match("^ERROR:%s*([^:]+):(%d+):")
	if s and st then
	  st = tonumber(st)
	  str = s
	  if str == [=[String-[ScriptPane]]=] then
		console('line', st)
		return
      elseif str == [=[[string "CommandPane"]]=] then
		console('command')
		return
	  elseif str == [=[[string "Evaluation"]]=] then
		return
	  end
	else
	  str = str:match("^%-%-%s*([^|]+)")
      str = str:gsub("^%s*(.-)%s*$", "%1")
	  if not str then return end
	end
  end
  if str:sub(1,7) == "http://" or str:sub(1,8) == "https://" or str:sub(1,4) == "www." then
    console('url',str)
	return
  end
  if not console('open', str, st) then
    str = prepcommand(str, true)
    f, e = lua.load(str, "Evaluation", 't', _L)
	if f then
	  console('prep')
	  dprint("**Evaluating selection:")
	  exec(f, e)
	  console('prompt')
	end
  end
end
