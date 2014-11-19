@echo off
setlocal

set VERSION=1.0.2

set OUTPUT=c:\NuGet\

nuget push %OUTPUT%Packages\MMaitre.VideoEffects.%VERSION%.nupkg
nuget push %OUTPUT%Symbols\MMaitre.VideoEffects.Symbols.%VERSION%.nupkg -Source http://nuget.gw.symbolsource.org/Public/NuGet 