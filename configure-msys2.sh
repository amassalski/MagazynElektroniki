#!/bin/bash
echo "Konfiguracja projektu DomowyMagazyn dla MSYS2"
echo "==========================================="

# Sprawdź czy jesteśmy w MSYS2
if [[ "$MSYSTEM" != "MINGW64" ]] && [[ "$MSYSTEM" != "MINGW32" ]]; then
    echo "Uruchom ten skrypt w MSYS2 MinGW 64-bit lub 32-bit!"
    exit 1
fi

# Sprawdź czy wxWidgets jest zainstalowany
if ! pacman -Q mingw-w64-x86_64-wxWidgets 2>/dev/null; then
    echo "wxWidgets nie jest zainstalowany!"
    echo "Zainstaluj: pacman -S mingw-w64-x86_64-wxWidgets"
    exit 1
fi

# Sprawdź czy PostgreSQL jest zainstalowany
if ! pacman -Q mingw-w64-x86_64-postgresql 2>/dev/null; then
    echo "PostgreSQL nie jest zainstalowany!"
    echo "Zainstaluj: pacman -S mingw-w64-x86_64-postgresql"
    exit 1
fi

# Utwórz katalog build
mkdir -p build
cd build

# Konfiguruj z użyciem systemowych bibliotek
echo "Konfigurowanie projektu..."
cmake -G "MSYS Makefiles" -DUSE_SYSTEM_WX=ON ..

if [ $? -ne 0 ]; then
    echo "Błąd konfiguracji!"
    exit 1
fi

echo ""
echo "Konfiguracja zakończona pomyślnie!"
echo "Uruchom 'make' aby skompilować projekt."
