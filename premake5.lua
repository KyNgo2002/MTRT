-- premake5.lua
workspace "MTRT"
   architecture "x64"
   configurations { "Debug", "Release", "Dist" }
   startproject "MTRT"

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
include "Walnut/WalnutExternal.lua"

include "MTRT"