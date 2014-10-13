X = require "ComLink"

printtab = function(t,h,p)
  if not p then p = "--" end
  if h then print(h) end
  local s = ""
  for k,v in t do
    s = p .. tostring(k)
    if v ~= true then 
      if type(v) == "string" then
        s = s .. [[="]] .. v .. [["]]
      else
        s = s .. "=" .. tostring(v)
      end
    end
    print(s)
  end
end

printit = function(v)
  if type(v) == nil then
    print("nil")
  elseif type(v) == "boolean" then
    if v then print "true" else print "false" end
  elseif type(v) == "string" then
    print([["]] .. v .. [["]])
  else
    print(tostring(v))
  end
end

local er = 0

print("Enumerating available ports:")
ix = 0
for i, v in X.ports() do
  printtab(v, "Port " .. i)
  if ix == 0 then ix = i end
end
print("")

if ix > 0 then
  print("Testing using first Com device at index " .. ix)
  print("Ensure TX/RX loopback link fitted")
  print("Ensure RTS/CTS loopback link fitted")
else
  print("No Com ports found")
  return
end
print("")

d, err = X.Port{index=ix}
if not d then print("FAILED to open Port using index '" .. ix .. "' Error: " .. err) return end

if d then
  print("Open device by Index '" .. ix .. "' OK")
  printtab(d:info(), "Device Information '"..ix.."'")
  print("Testing fixed 4 second wait")
  d:write(X.op('dly', 4000))
  print("Done!")
  if d:close() then
    print("Close device OK")
  else
    print("Close device FAILED")
    er = er + 1
  end
else
  print("Open device by Index '" .. ix .. "'' FAILED")
  er = er + 1
end

print("")
d,err = X.Port{index=ix;baudrate=2400;wordlength=8;stopbits=1;parity="none";flowcontrol="none"}
if d then
  print("Open and configure (UART) OK")
else
  print("Open and configure (UART) FAILED: " .. err)
  er = er + 1
end

ts = "Loopback Test"

d:write(X.op('prx'), X.op('ptx'), ts)
c = 0

print("")
print("Fixed size loopback test with short timeout")
d:read(1, #ts)
repeat
  rs, rc = d:read(2)
  if rc then print("Characters received so far: " .. rc) end
  c = c + 1
until rs or c > 40
if rs == ts then
  print("Fixed size loopback test OK")
else
  print("Fixed size loopback test FAILED (is RX connected to TX?)")
  er = er + 1
end

print("")
print("Terminated loopback test with long timeout")
d:write(X.op('prx'), X.op('ptx'), ts .. "]=]", X.op('dly',100))
rs = d:read(200, #ts+10, "]=]")
print(rs)
if rs == ts then
  print("Terminated loopback test OK")
else
  print("Terminated loopback test FAILED")
  er = er + 1
end

print("")
s0 = d:status('cts')
d:write(X.op('rts', false), X.op('dly',100))
s1 = d:status('cts')
d:write(X.op('rts', true), X.op('dly',100))
s2 = d:status('cts')
d:write(X.op('rts', false), X.op('dly',100))
s3 = d:status('cts')
print(tostring(s0), tostring(s1), tostring(s2), tostring(s3))
if s2 and not (s1 or s3) then
  print("RTS/CTS Loopback Test OK")
else
  print("RTS/CTS Loopback Test FAILED (is RTS connected to CTS?)")
  er = er + 1
end

d:write(X.op('rts', true), X.op('dly',100))
if d:status('cts') then
  winsh.messagebox("Press OK when ready to remove CTS/RTS link")
  print("")
  print("Testing wait for modem signal change with 4 second timeout")
  print("Remove CTS/RTS link now to cut wait short")
  if d:write(X.op('cts', false, 4095)) then
    print("Wait cut short by modem signal change - OK")
  else
    print("Timeout expired FAILED (did you remove the link?)")
    er = er + 1
  end
end
d:write(X.op('rts', false), X.op('dly',100))

cb = function(dev)
  local d = dev:read()
  print("Data received asynchronously OK: " .. d)
  er = er - 1
  dev:close()
  print("")
  print("Testing Complete. Errors: " .. er)
end

print("")
print("Testing Asynchronous reception")
ap = d:read(cb, #ts)
d:write(X.op('prx'), X.op('ptx'), ts, X.op('dly', 100))
er = er + 1
tim = Time{seconds=10}
tim:alarm(ap, true)
print("Main routine terminated, waiting for timer reception")
