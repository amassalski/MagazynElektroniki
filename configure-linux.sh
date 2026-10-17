#!/bin/bash
echo "Konfiguracja projektu DomowyMagazyn dla Linux"
echo "==========================================="

# Sprawdź zależności
echo "Sprawdzanie zależności..."

if ! command -v wx-config &> /dev/null; then
    echo "wxWidgets nie jest zainstalowany!"
    echo "Ubuntu/Debian: sudo apt-get install libwxgtk3.2-dev"
    echo "Fedora: sudo dnf install wxGTK3-devel"
    exit 1
fi

if ! command -v pg_config &> /dev/null; then
    echo "PostgreSQL nie jest zainstalowany!"
    echo "Ubuntu/Debian: sudo apt-get install libpq-dev postgresql"
    echo "Fedora: sudo dnf install postgresql-devel"
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
