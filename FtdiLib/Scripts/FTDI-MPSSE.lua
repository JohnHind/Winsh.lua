local FT = require('FtdiLink')

mpsse = {
  -- Read or write DBUS and CBUS as GPIO.
  SET_DBUS = string.char(0x80);         -- [Data][Inp]
  SET_CBUS = string.char(0x82);         -- Inp: set bit for input
  GET_DBUS = string.char(0x81);         -- Returns Data
  GET_CBUS = string.char(0x83);
  -- Loopback DBUS1 (DO) to DBUS2 (DI)
  LOOPBACK_ON = string.char(0x83);
  LOOPBACK_OFF = string.char(0x83);
  -- Shift Clock Control
  CLOCK_DIV = string.char(0x86);        -- [ValL][ValH]
  CLOCK_DIV5 = string.char(0x8A);       -- Divide clock by 5
  CLOCK_DIV1 = string.char(0x8B);       -- Do not divide clock by 5
  CLOCK_3P = string.char(0x8C);         -- Enable 3-Phase clocking
  CLOCK_1P = string.char(0x8D);         -- Disable 3-Phase clocking
  CLOCK_ADON = string.char(0x96);       -- Clocks waits for DBUS7 to match.
  CLOCK_ADOFF = string.char(0x97);      -- Clock insensitive to DBUS7.
  CLOCK_BITS = string.char(0x8E);       -- [Bits] (0..7) - 1 to 8 clocks.
  CLOCK_BYTES = string.char(0x8F);      -- [LenL][LenH]. 0 does 8 clocks etc.
  CLOCK_TOH = string.char(0x94);        -- Clock until DBUS5 goes high.
  CLOCK_TOL = string.char(0x95);        -- Clock until DBUS5 goes low.
  CLOCK_TOHC = string.char(0x9C);       -- [LenL][LenH]. OR DBUS5 high.
  CLOCK_TOLC = string.char(0x9D);       -- [LenL][LenH]. OR DBUS5 low.
  -- MCU Mode
  MCU_RD_SA = string.char(0x90);        -- [Addr] (Read using 8-bit address)
  MCU_RD_EA = string.char(0x91);        -- [AddrH][AddrL] (Read using 16-bit address)
  MCU_WR_SA = string.char(0x92);        -- [Addr][Data]
  MCU_WR_EA = string.char(0x93);        -- [AddrH][AddrL][Data]
  -- Miscellaneous commands
  FLUSH = string.char(0x87);            -- Flush data buffer to PC.
  WAIT_H = string.char(0x88);           -- Hold processing until DBUS5 is high.
  WAIT_L = string.char(0x89);           -- Hold processing until DBUS5 is low.
  TRIS = string.char(0x9E);             -- [DbitL][DbitH] (FT232H Only).
  -- Bad command processing
  BAD_COMMAND = string.char(0xAA);      -- A sample invalid command.
  BAD_RESPONSE = string.char(0xFA);     -- {BAD_RESPONSE}{Command}
}


mpsse.open = function(index, mode)
  local set = {}
  set.index = tonumber(index) or 1
  set.mode = 'mpsse'
  if mode == 'mcu' then set.mode = 'mcu' end
  set.flowcontrol = 'rtscts'
  local DV = FT.Port(set)
  if not DV then return false end
  -- Synchronise MPSSE:
  if DV:write(mpsse.FLUSH, FT.op('prx'), mpsse.BAD_COMMAND) then
    local r =DV:read(100, 2)
    if r == mpsse.BAD_RESPONSE..mpsse.BAD_COMMAND then
      return DV
    end
  end
  DV:close()
  return false
end

mpsse.setclock = function(div, div5, phase3, adapt)
  local cmd = mpsse.CLOCK_DIV .. string.char(math.floor(count / 256)) .. string.char(math.fmod(count,256))
  if div5 then
    cmd = cmd .. mpsse.CLOCK_DIV5
  else
    cmd = cmd .. mpsse.CLOCK_DIV1
  end
  if phase3 then
    cmd = cmd .. mpsse.CLOCK_3P
  else
    cmd = cmd .. mpsse.CLOCK_1P
  end
  if adapt then
    cmd = cmd .. mpsse.CLOCK_ADON
  else
    cmd = cmd .. mpsse.CLOCK_ADOFF
  end
  return cmd
end

mpsse.shiftdata = function(...)
-- Assembles a Data Shift command and returns it as a string.
-- If data is being shifted out, first parameter should be the data string.
-- Next parameter must be a number, the bit or byte count (must be 1 or greater).
-- Remaining parameters must be string keys from the following set:
--  'TDI' or 'DO' - Shift out to DBUS1.
--  'TDO' or 'DI' - Shift in from DBUS2.
--  'TMS'         - Shift out to DBUS3.
--  'WRITEFALL'   - Write data on clock falling edge (else on rising edge).
--  'READFALL'    - Read data on clock falling edge (else on rising edge).
--  'BIT'         - Clocks count = 1..8 bits (else count = 1..65536 bytes).
--  'LSB'         - Data is clocked LS bit first (else MS bit first).
  p = {...}
  local data = ""
  local count = 0
  local cmd = 0
  local wr = false
  local bit = false
  for i=1, #p do
    if i == 1 and type(p[i]) == 'string' then 
      data = p[i]
    elseif i < 3 and type(p[i]) == 'number' then
      count = p[i]
    elseif p[i] == 'DO' or p[i] == 'TDI' then
      cmd = cmd + 16
      wr = true
    elseif p[i] == 'DI' or p[i] == 'TDO' then
      cmd = cmd + 32
    elseif p[i] == 'TMS' then
      cmd = cmd + 64
      wr = true
    elseif p[i] == 'WRITEFALL' then
      cmd = cmd + 1
    elseif p[i] == 'READFALL' then
      cmd = cmd + 4
    elseif p[i] == 'BIT' then
      cmd = cmd + 2
      bit = true
    elseif p[i] == 'LSB' then
      cmd = cmd + 8
    else
      error("Invalid Parameter")
    end
  end
  if (count < 1) or (count > 65536) or (bit and count > 8) or (cmd > 127) then
    error("Invalid Parameter Combination")
  end
  if (bit and #data ~= 1) or (not bit and #data ~= count) then
    error("Incorrect length of data string")
  end
  count = count - 1
  local rs = string.char(cmd) .. string.char(math.fmod(count,256))
  if not bit then rs = rs .. string.char(math.floor(count / 256)) end
  if wr then rs = rs .. data end
  return rs
end

local MT = class.gettid(FT.Port)

MT.setdbus = function(dev, state, dir)
  if state < 0 or state > 255 then error("Bad Parameter") end
  if dir and (dir < 0 or dir > 255) then error("Bad Parameter") end
  local inst = dev:instancetable()
  inst.dstate = state
  if dir then
    inst.ddir = dir
  else
    dir = inst.ddir
  end
  dev:write(mpsse.SET_DBUS, string.char(state), string.char(dir))
end

MT.getdbus = function(dev)
  dev:write(mpsse.FLUSH, FT.op('prx'), mpsse.GET_DBUS)
  return dev:read(100, 1)
end

MT.setcbus = function(dev, state, dir)
  if state < 0 or state > 255 then error("Bad Parameter") end
  if dir and (dir < 0 or dir > 255) then error("Bad Parameter") end
  local inst = dev:instancetable()
  inst.cstate = state
  if dir then
    inst.cdir = dir
  else
    dir = inst.cdir
  end
  dev:write(mpsse.SET_CBUS, string.char(state), string.char(dir))
end

MT.getdbus = function(dev)
  dev:write(mpsse.FLUSH, FT.op('prx'), mpsse.GET_CBUS)
  return dev:read(100, 1)
end
