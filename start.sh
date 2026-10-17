#!/bin/bash
echo "Uruchamianie DomowyMagazyn..."

# Sprawdź czy CMakeLists.txt istnieje
if [ ! -f "CMakeLists.txt" ]; then
    echo "Błąd: Nie znaleziono CMakeLists.txt"
    exit 1
fi

# Utwórz katalog build jeśli nie istnieje
if [ ! -d "build" ]; then
    echo "Tworzenie katalogu build..."
    mkdir build
fi

cd build

# Konfiguruj i kompiluj
echo "Konfigurowanie projektu..."
cmake ..

echo "Kompilowanie..."
make -j$(nproc)

# Uruchom aplikację
echo "Uruchamianie aplikacji..."
./DomowyMagazyn
