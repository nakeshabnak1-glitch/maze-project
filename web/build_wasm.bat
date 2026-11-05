@echo off
chcp 65001 >nul
rem ============================================================
rem  把 src\ 里的 C++ 算法（A 的生成 + B 的求解）编译成 WebAssembly，
rem  再嵌入 index.template.html，生成可以双击打开的 index.html。
rem  用法：在 web 文件夹里双击这个文件。
rem  第一次用之前：下载 wasi-sdk 的 Windows 版并解压，把下面 WASI_SDK 改成你的解压路径。
rem ============================================================
set WASI_SDK=D:\wasi-sdk-25.0-x86_64-windows

cd /d "%~dp0"
if not exist "%WASI_SDK%\bin\clang++.exe" (
    echo 找不到 %WASI_SDK%\bin\clang++.exe ，请先修改本文件里的 WASI_SDK 路径
    pause
    exit /b 1
)

echo [1/2] 编译 C++ -^> maze.wasm ...
"%WASI_SDK%\bin\clang++.exe" --target=wasm32-wasip1 --sysroot="%WASI_SDK%\share\wasi-sysroot" ^
    -std=c++17 -Os -fwasm-exceptions -I..\src ^
    ..\src\maze_generator.cpp ..\src\maze_solver.cpp wasm_api.cpp ^
    -mexec-model=reactor -Wl,--strip-all -Wl,--gc-sections -Wl,-z,stack-size=8388608 ^
    -o maze.wasm
if errorlevel 1 (
    echo 编译失败，请看上面的错误信息
    pause
    exit /b 1
)

echo [2/2] 嵌入网页 -^> index.html ...
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0embed.ps1"
if errorlevel 1 (
    echo 生成网页失败
    pause
    exit /b 1
)

echo 完成！双击 index.html 用浏览器打开。
pause
