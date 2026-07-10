-- premake5.lua
workspace "RayStalker"
   architecture "x64"
   configurations { "Debug", "Release", "Dist" }
   startproject "RayStalker"

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
include "Walnut/WalnutExternal.lua"

include "RayStalker"