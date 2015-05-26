@echo off
setlocal enableextensions

set VERSION=2.3.0

set OUTPUT=c:\NuGet\

if exist "%ProgramFiles%\MSBuild\12.0\Bin\msbuild.exe" (
    set BUILD="%ProgramFiles%\MSBuild\12.0\Bin\msbuild.exe"
)
if exist "%ProgramFiles(x86)%\MSBuild\12.0\Bin\msbuild.exe" (
    set BUILD="%ProgramFiles(x86)%\MSBuild\12.0\Bin\msbuild.exe"
)

if exist "%ProgramFiles%\Git\cmd\git.exe" (
    set GIT="%ProgramFiles%\Git\cmd\git.exe"
)
if exist "%ProgramFiles(x86)%\Git\cmd\git.exe" (
    set GIT="%ProgramFiles(x86)%\Git\cmd\git.exe"
)

REM Clean
call .\clean.cmd

REM Build
%BUILD% .\pack.sln /maxcpucount /target:build /nologo /p:Configuration=Release /p:Platform=Win32
if %ERRORLEVEL% NEQ 0 goto eof
%BUILD% .\pack.sln /maxcpucount /target:build /nologo /p:Configuration=Release /p:Platform=x64
if %ERRORLEVEL% NEQ 0 goto eof
%BUILD% .\pack.sln /maxcpucount /target:build /nologo /p:Configuration=Release /p:Platform=ARM
if %ERRORLEVEL% NEQ 0 goto eof

REM Pack
%OUTPUT%nuget.exe pack MMaitre.VideoEffects.nuspec -OutputDirectory %OUTPUT%Packages -Prop NuGetVersion=%VERSION% -NoPackageAnalysis
if %ERRORLEVEL% NEQ 0 goto eof
%OUTPUT%nuget.exe pack MMaitre.VideoEffects.Symbols.nuspec -OutputDirectory %OUTPUT%Symbols -Prop NuGetVersion=%VERSION% -NoPackageAnalysis
if %ERRORLEVEL% NEQ 0 goto eof

REM Tag
%GIT% tag v%VERSION%