@echo off
echo Konfiguracja projektu DomowyMagazyn dla Windows
echo ==============================================

setlocal

rem Sprawdź czy CMake jest dostępny
where cmake >nul 2>nul
if errorlevel 1 (
    echo Blad: Nie znaleziono CMake!
    echo Zainstaluj CMake z: https://cmake.org/download/
    pause
    exit /b 1
)

rem Sprawdź lokalizację wxWidgets
if "%WXWIDGETS_ROOT%"=="" (
    if exist "C:\wxWidgets-3.2.9" (
        set WXWIDGETS_ROOT=C:\wxWidgets-3.2.9
    ) else if exist "C:\wxWidgets-3.2.4" (
        set WXWIDGETS_ROOT=C:\wxWidgets-3.2.4
    ) else (
        echo Blad: Nie znaleziono wxWidgets!
        echo Ustaw zmienna srodowiskowa WXWIDGETS_ROOT lub zainstaluj wxWidgets.
        echo Pobierz z: https://www.wxwidgets.org/downloads/
        pause
        exit /b 1
    )
)

echo Uzycie wxWidgets: %WXWIDGETS_ROOT%

rem Sprawdź czy MSYS2/MinGW jest dostępny
where gcc >nul 2>nul
if errorlevel 1 (
    echo Ostrzeżenie: Nie znaleziono GCC (MinGW).
    echo Zalecane: Zainstaluj MSYS2 z: https://www.msys2.org/
    echo Uruchom ponownie skrypt po instalacji.
    pause
)

rem Utwórz katalog build jeśli nie istnieje
if not exist "build" mkdir build

cd build

rem Konfiguruj projekt
echo.
echo Konfigurowanie projektu...
cmake -G "MinGW Makefiles" -DWXWIDGETS_ROOT="%WXWIDGETS_ROOT%" ..

if errorlevel 1 (
    echo.
    echo Blad konfiguracji!
    echo Sprawdz:
    echo 1. Czy wxWidgets jest skompilowany? (najlepiej wersja Unicode Debug/Release)
    echo 2. Czy PostgreSQL jest zainstalowany?
    echo 3. Czy MSYS2/MinGW jest w PATH?
    pause
    exit /b 1
)

echo.
echo Konfiguracja zakonczona pomyslnie!
echo Uruchom 'mingw32-make' aby skompilowac projekt.
echo.

pause
