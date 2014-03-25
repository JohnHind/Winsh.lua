##Introduction

Winsh.lua is an advanced runtime for the Lua scripting language on the Windows platform (XP and later). It delivers an innovative set of script packaging options and a comprehensive range of shell libraries.

##Scope

Winsh.lua is optimised for the creation of shell automation utilities such as backup routines, build procedures, test and benchmark processes and portable application menus. It can create notification icons with script-defined context menus; windows with scrolling text reports, progress animations and result indication; and dialog boxes for file and folder selection or confirmation. It can also create console-mode command-line utilities.

Winsh.lua's comprehensive libraries cover the shell object hierarchy (broadly the functionality of Windows Explorer) including management of removable drives and media; the registry; serial ports; comprehensive application interaction including keystroke simulation, clipboard automation, multi-monitor window control, and termination detection; and time scheduling including precision benchmarking timers.

Packing options are designed to avoid the need for installation and to be location independent. The runtime can be freely copied and renamed and is compact enough to permit one copy per application. Lua scripts and libraries can be stored in the runtime executable as resources for single-file applications. Alternatively scripts and libraries (including object code libraries) can be contained with the runtime in a relocatable application folder. Conventional commandline script specification compatible with the standard Lua runtime is also supported.

##Getting Started

A runtime package containing several alternative pre-built binaries and documentation can be downloaded here:

The sources on this site can be built using Visual Studio 2010 or later, or probably in any C++ environment with some work. The sources comprise the following:

Patched Lua sources and a project to build these into an object code library.

Windows Template Library (WTL) sources unchanged from the WTL distribution, and the specific Grunt sources with a project to build the executables.

Lua is pure C code, WTL is C++ templates, Grunt is mostly C++ with some Lua embedded as resources.
