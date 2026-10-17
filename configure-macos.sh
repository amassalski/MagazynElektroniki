#!/bin/bash
echo "Konfiguracja projektu DomowyMagazyn dla macOS"
echo "==========================================="

# Sprawdź czy Homebrew jest zainstalowany
if ! command -v brew &> /dev/null; then
    echo "Homebrew nie jest zainstalowany!"
    echo "Zainstaluj z: https://brew.sh/"
    exit 1
fi

# Sprawdź zależności
echo "Sprawdzanie zależności..."

if ! brew list wxwidgets &> /dev/null; then
    echo "wxWidgets nie jest zainstalowany!"
    echo "Zainstaluj: brew install wxwidgets"
    exit 1
fi

if ! brew list postgresql@15 &> /dev/null && ! brew list postgresql@14 &> /dev/null; then
    echo "PostgreSQL nie jest zainstalowany!"
    echo "Zainstaluj: brew install postgresql@15"
    exit 1
fi

# Utwórz katalog build
mkdir -p build
cd build

# Konfiguruj
echo "Konfigurowanie projektu..."
cmake -DUSE_SYSTEM_WX=ON ..

if [ $? -ne 0 ]; then
    echo "Błąd konfiguracji!"
    exit 1
fi

echo ""
echo "Konfiguracja zakończona pomyślnie!"
echo "Uruchom 'make' aby skompilować projekt."
