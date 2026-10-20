@echo off
if "%~1"=="" (premake5 vs2022) else (premake5 %*)
exit /b %errorlevel%
