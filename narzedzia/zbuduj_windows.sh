#!/bin/bash
# Buduje wersję dla Windows: folder z programem i bibliotekami, archiwum ZIP (wersja przenośna)
# oraz instalator .exe (jeśli jest NSIS). Uruchom w powłoce MSYS2 MinGW 64-bit, z katalogu projektu:
#
#   narzedzia/zbuduj_windows.sh [katalog_budowania]
#
# Wymaga: pakietów mingw-w64-x86_64: toolchain, cmake, ninja, wxWidgets, postgresql;
# opcjonalnie: nsis (instalator), imagemagick (ikona), opencv (kamera), poppler (pdftotext), gettext (tłumaczenia):
#   pacman -S mingw-w64-x86_64-nsis mingw-w64-x86_64-imagemagick mingw-w64-x86_64-poppler
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
case "${MSYSTEM:-}" in MINGW64|UCRT64|CLANG64) ;; *)
    echo "Uruchom ten skrypt w powłoce MSYS2 MinGW 64-bit (MSYSTEM=$MSYSTEM)." >&2; exit 1;; esac
B="$(realpath -m "${1:-$ROOT/build-win}")"
WERSJA="$(sed -n 's/^project(DomowyMagazyn VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt | head -1)"
[ -n "$WERSJA" ] || { echo "Nie znaleziono wersji w CMakeLists.txt" >&2; exit 1; }

if [ ! -f "$B/DomowyMagazyn.exe" ]; then
    GEN="MSYS Makefiles"; command -v ninja >/dev/null && GEN="Ninja"
    cmake -S . -B "$B" -G "$GEN" -DCMAKE_BUILD_TYPE=Release -DUSE_SYSTEM_WX=ON
    cmake --build "$B" -j"$(nproc)"
fi
command -v python3 >/dev/null && python3 narzedzia/zbuduj_pomoc.py >/dev/null || true

NAZWA="DomowyMagazyn-$WERSJA-win64"
OUT="$B/instalator"
STAGE="$OUT/$NAZWA"
rm -rf "$OUT"; mkdir -p "$STAGE"

install -m 755 "$B/DomowyMagazyn.exe" "$STAGE/"
strip --strip-unneeded "$STAGE/DomowyMagazyn.exe" 2>/dev/null || true
cp -r sql "$STAGE/sql"
mkdir -p "$STAGE/pomoc" && cp pomoc/pomoc_pl.htm pomoc/pola.txt "$STAGE/pomoc/"
if   [ -f "$B/pl/LC_MESSAGES/DomowyMagazyn.mo" ]; then mkdir -p "$STAGE/pl/LC_MESSAGES"; cp "$B/pl/LC_MESSAGES/DomowyMagazyn.mo" "$STAGE/pl/LC_MESSAGES/"
elif [ -f lang/pl.mo ];                            then mkdir -p "$STAGE/pl/LC_MESSAGES"; cp lang/pl.mo "$STAGE/pl/LC_MESSAGES/DomowyMagazyn.mo"; fi
[ -d assets/etykiety ] && { mkdir -p "$STAGE/assets"; cp -r assets/etykiety "$STAGE/assets/"; }
mkdir -p "$STAGE/dokumentacja" && cp -r dokumentacja/. "$STAGE/dokumentacja/" && cp README.md "$STAGE/"

# Biblioteki DLL: wszystko, co program (i zależności zależności) bierze z /mingw64 lub /ucrt64
zbierz_dll() {
    ldd "$1" | awk '/=>/ {print $3}' | grep -iE '^/(mingw64|ucrt64|clang64)/' | sort -u | while read -r d; do
        cp -f "$d" "$STAGE/"
    done
}
zbierz_dll "$STAGE/DomowyMagazyn.exe"
# pdftotext (odczyt parametrów z datasheetów), jeśli zainstalowany: leży obok programu i znajdzie swoje DLL
if PDFT="$(command -v pdftotext.exe 2>/dev/null)"; then
    cp -f "$PDFT" "$STAGE/"; zbierz_dll "$STAGE/pdftotext.exe"
fi
echo "Skopiowano $(ls "$STAGE"/*.dll 2>/dev/null | wc -l) bibliotek DLL"

# Wersja przenośna: ZIP
ZIP="$ROOT/$NAZWA.zip"
rm -f "$ZIP"
if   command -v zip >/dev/null; then ( cd "$OUT" && zip -qr "$ZIP" "$NAZWA" )
elif command -v 7z  >/dev/null; then ( cd "$OUT" && 7z a -tzip -bso0 "$ZIP" "$NAZWA" )
else powershell.exe -NoProfile -Command "Compress-Archive -Path '$(cygpath -w "$STAGE")' -DestinationPath '$(cygpath -w "$ZIP")'"; fi
echo "Gotowe (przenośna): $ZIP"

# Instalator: NSIS
if command -v makensis >/dev/null; then
    IKONA=""
    if command -v magick >/dev/null || command -v convert >/dev/null; then
        IM="magick"; command -v magick >/dev/null || IM="convert"
        if $IM assets/ikony/app_256.png -define icon:auto-resize=256,128,64,48,32,16 "$OUT/domowymagazyn.ico" 2>/dev/null; then
            IKONA="$OUT/domowymagazyn.ico"
        fi
    fi
    W() { if command -v cygpath >/dev/null; then cygpath -w "$1"; else echo "$1"; fi; }
    EXE="$ROOT/DomowyMagazyn-$WERSJA-instalator.exe"
    ARGS=(-V2 "-DWERSJA=$WERSJA" "-DSTAGE=$(W "$STAGE")" "-DWYNIK=$(W "$EXE")")
    [ -n "$IKONA" ] && ARGS+=("-DIKONA=$(W "$IKONA")")
    makensis "${ARGS[@]}" "$(W "$ROOT/narzedzia/instalator/windows.nsi")"
    echo "Gotowe (instalator): $EXE"
else
    echo "Brak makensis - pomijam instalator (pacman -S mingw-w64-x86_64-nsis). Wersja przenośna jest gotowa."
fi
