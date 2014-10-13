--Lua Part for 'registry' library
local T = ...

HKEY = {			-- Registry Root Keys (added internally to 0x80000000)
  LOCAL_MACHINE = 2;
  CLASSES_ROOT = 0;
  USERS = 3;
  CURRENT_CONFIG = 5;
  PERFORMANCE_DATA = 4;
  PERFORMANCE_NLSTEXT = 0x60;
  PERFORMANCE_TEXT = 0x50;
  CURRENT_USER = 1;
}
