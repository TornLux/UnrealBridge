@echo off
setlocal

set "SRC=%~dp0Plugin\UnrealBridge"
set "DST=G:\UEProjects\GameplayLocomotion\Plugins\UnrealBridge"

echo Syncing UnrealBridge plugin...
echo   From: %SRC%
echo   To:   %DST%
echo.

robocopy "%SRC%" "%DST%" /MIR /XD Binaries Intermediate /XF *.pdb 2>nul

if %ERRORLEVEL% LEQ 7 (
    echo.
    echo Done.
) else (
    echo.
    echo ERROR: robocopy failed with code %ERRORLEVEL%
)

endlocal
