@echo off

:: 1. Initialize the Visual Studio x64 Developer Environment
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

:: 2. Compile the program using Clang
clang -O3 -ffast-math -mavx2 -DNDEBUG main.c -I. -Llib -lraylib -lopengl32 -lgdi32 -lwinmm -luser32 -lshell32 -lucrt -Xlinker /NODEFAULTLIB:libucrt -Xlinker /NODEFAULTLIB:msvcrt -o main.exe

:: 3. Keep the window open if there are errors, or let them know it finished
if %ERRORLEVEL% EQU 0 (
    echo Build Successful!
) else (
    echo Build Failed!
)
:: pause