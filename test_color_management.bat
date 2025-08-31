@echo off
echo === Building Color Management Validation Test ===
echo.

REM Build the main project first to ensure all libraries are available
echo Building main project...
cd /d "c:\Users\braul\Desktop\Video_Editor"
cmake --build build/qt-debug --config Debug

if %ERRORLEVEL% neq 0 (
    echo ERROR: Main project build failed!
    pause
    exit /b 1
)

echo.
echo Building color management validation test...

REM Compile the color management validation test
cl.exe /std:c++20 ^
    /EHsc ^
    /I"src\media_io\include" ^
    /I"src\core\include" ^
    /I"build\qt-debug\vcpkg_installed\x64-windows\include" ^
    /D"VE_HAVE_FFMPEG=1" ^
    /D"_CRT_SECURE_NO_WARNINGS" ^
    color_management_validation_test.cpp ^
    build\qt-debug\src\media_io\Debug\ve_media_io.lib ^
    build\qt-debug\src\core\Debug\ve_core.lib ^
    /Fe:color_management_validation_test.exe

if %ERRORLEVEL% neq 0 (
    echo ERROR: Compilation failed!
    echo.
    echo Trying alternative compilation without FFmpeg dependencies...
    
    cl.exe /std:c++20 ^
        /EHsc ^
        /I"src\media_io\include" ^
        /I"src\core\include" ^
        /D"VE_HAVE_FFMPEG=0" ^
        /D"_CRT_SECURE_NO_WARNINGS" ^
        color_management_validation_test.cpp ^
        /Fe:color_management_validation_test.exe
    
    if %ERRORLEVEL% neq 0 (
        echo ERROR: Alternative compilation also failed!
        pause
        exit /b 1
    )
)

echo.
echo Color management validation test compiled successfully!
echo.

REM Run the test
echo Running color management validation test...
echo.
color_management_validation_test.exe

if %ERRORLEVEL% neq 0 (
    echo.
    echo ERROR: Color management validation test failed!
    pause
    exit /b 1
)

echo.
echo === Color Management Validation Test PASSED ===
echo All color management components are working correctly!
echo.
pause
