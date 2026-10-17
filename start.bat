@echo off
echo Uruchamianie DomowyMagazyn...
if not exist build (
    echo Tworzenie katalogu build...
    mkdir build
)
cd build
cmake -G "MinGW Makefiles" ..
mingw32-make
DomowyMagazyn.exe
