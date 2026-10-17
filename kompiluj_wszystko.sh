# build_all.sh
#!/bin/bash

cd /c/Users/massaada/Programowanie/DomowyMagazyn

# Skompiluj tłumaczenia jeśli gettext jest dostępny
if command -v msgfmt &> /dev/null; then
    echo "Kompilacja tłumaczeń..."
    msgfmt lang/pl.po -o lang/pl.mo 2>/dev/null || echo "Brak pliku pl.po"
else
    echo "Gettext nie zainstalowany - pomijam tłumaczenia"
fi

# Skompiluj projekt
echo "Kompilacja projektu..."
rm -rf build
cmake -B build -G "Ninja"
cmake --build build --config Release

# Skopiuj pliki tłumaczeń do build
if [ -f lang/pl.mo ]; then
    mkdir -p build/lang
    cp lang/pl.mo build/lang/
    echo "Pliki tłumaczeń skopiowane"
fi

echo "Budowanie zakończone!"
echo "Uruchom: ./build/DomowyMagazyn.exe"