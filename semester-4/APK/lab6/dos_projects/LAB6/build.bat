@echo off
set PATH=C:\BORLANDC\BIN;%PATH%
cd \LAB6

if exist LAB6.EXE del LAB6.EXE
if exist LAB6.OBJ del LAB6.OBJ
if exist LAB6.MAP del LAB6.MAP

C:\BORLANDC\BIN\TASM.EXE /m2 LAB6.ASM
if errorlevel 1 goto end

C:\BORLANDC\BIN\TLINK.EXE /3 LAB6.OBJ,LAB6.EXE,,,

:end
