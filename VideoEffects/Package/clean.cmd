@echo off
setlocal

REM Clean up
if exist .\ARM rmdir /S /Q .\ARM
if exist .\x64 rmdir /S /Q .\x64
if exist .\Release rmdir /S /Q .\Release
if exist .\bin rmdir /S /Q .\bin
if exist .\obj rmdir /S /Q .\obj
