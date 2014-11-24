@echo off
setlocal

set VERSION=1.0.3

set OUTPUT=c:\NuGet\

%OUTPUT%nuget push %OUTPUT%Packages\MMaitre.VideoEffects.%VERSION%.nupkg
%OUTPUT%nuget push %OUTPUT%Symbols\MMaitre.VideoEffects.Symbols.%VERSION%.nupkg -Source http://nuget.gw.symbolsource.org/Public/NuGet 