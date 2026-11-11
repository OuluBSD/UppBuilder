@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

echo ==> UppBuilder: Compiling standalone binary on Windows...

where cl.exe >nul 2>&1
if %ERRORLEVEL% equ 0 (
    echo Using MSVC cl.exe...
    cl.exe /std:c++17 /O2 /EHsc /W3 ^
        UppBuilder.cpp Bridge.cpp UppParser.cpp DepGraph.cpp HeaderScanner.cpp UmkDriver.cpp RuntimeDeps.cpp FlagAuditor.cpp UseModel.cpp PolicyEngine.cpp EscBuildParser.cpp ProfileManager.cpp ProviderPlanner.cpp OverlayManager.cpp EmergeEngine.cpp VdbManager.cpp BinPkgManager.cpp main.cpp ^
        /Fe:uppbuilder.exe
    goto :done
)

where clang++.exe >nul 2>&1
if %ERRORLEVEL% equ 0 (
    echo Using clang++.exe...
    clang++.exe -std=c++17 -O2 -Wall ^
        UppBuilder.cpp Bridge.cpp UppParser.cpp DepGraph.cpp HeaderScanner.cpp UmkDriver.cpp RuntimeDeps.cpp FlagAuditor.cpp UseModel.cpp PolicyEngine.cpp EscBuildParser.cpp ProfileManager.cpp ProviderPlanner.cpp OverlayManager.cpp EmergeEngine.cpp VdbManager.cpp BinPkgManager.cpp main.cpp ^
        -o uppbuilder.exe
    goto :done
)

where g++.exe >nul 2>&1
if %ERRORLEVEL% equ 0 (
    echo Using g++.exe...
    g++.exe -std=c++17 -O2 -Wall ^
        UppBuilder.cpp Bridge.cpp UppParser.cpp DepGraph.cpp HeaderScanner.cpp UmkDriver.cpp RuntimeDeps.cpp FlagAuditor.cpp UseModel.cpp PolicyEngine.cpp EscBuildParser.cpp ProfileManager.cpp ProviderPlanner.cpp OverlayManager.cpp EmergeEngine.cpp VdbManager.cpp BinPkgManager.cpp main.cpp ^
        -o uppbuilder.exe
    goto :done
)

echo Error: Neither cl.exe, clang++, nor g++ found in PATH.
exit /b 1

:done
echo ==> UppBuilder: Successfully built uppbuilder.exe
uppbuilder.exe --version
