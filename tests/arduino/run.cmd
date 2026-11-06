@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist "%~dp0..\..\build\native-tests" mkdir "%~dp0..\..\build\native-tests"
cl /nologo /EHsc /std:c++17 /W3 /D_CRT_SECURE_NO_WARNINGS /I"%~dp0." "%~dp0regression.cpp" /Fe:"%~dp0..\..\build\native-tests\regression.exe" /Fo:"%~dp0..\..\build\native-tests\regression.obj"
if errorlevel 1 exit /b 1
"%~dp0..\..\build\native-tests\regression.exe"
