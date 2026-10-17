#!/bin/bash
# Buduje wersję dla macOS: pakiet DomowyMagazyn.app (z dołączonymi bibliotekami) i obraz dysku .dmg.
# Uruchom na Macu, z katalogu projektu:
#
#   narzedzia/zbuduj_macos.sh [--bez-bibliotek] [katalog_budowania]
#
# Wymaga: Xcode Command Line Tools, Homebrew: cmake wxwidgets postgresql@15 (lub 14) dylibbundler;
# opcjonalnie: opencv (kamera), poppler (pdftotext, odczyt parametrów z datasheetów), gettext (tłumaczenia).
#   brew install cmake wxwidgets postgresql@15 dylibbundler poppler
# MacPorts zamiast Homebrew: sudo port install cmake wxWidgets-3.2 postgresql15 dylibbundler poppler opencv4
#   (CMake szuka OpenCV także w /opt/local; opencv3 też zadziała). Kamera jest opcjonalna: bez OpenCV program się zbuduje.
# --bez-bibliotek: nie dołącza bibliotek Homebrew (aplikacja zadziała tylko na tym Macu, ale nie potrzeba dylibbundler).
# Aplikacja jest podpisana "ad hoc" (bez konta Apple Developer), więc po pobraniu z internetu macOS zapyta o zgodę:
# prawy klik na aplikacji -> Otwórz.
set -euo pipefail

BIBLIOTEKI=1
if [ "${1:-}" = "--bez-bibliotek" ]; then BIBLIOTEKI=0; shift; fi
[ "$(uname)" = "Darwin" ] || { echo "Ten skrypt działa tylko na macOS." >&2; exit 1; }
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
if [ -n "${1:-}" ]; then B="$(python3 -c 'import os,sys;print(os.path.abspath(sys.argv[1]))' "$1")"; else B="$ROOT/build-mac"; fi
WERSJA="$(sed -n 's/^project(DomowyMagazyn VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt | head -1)"
ARCH="$(uname -m)"
[ -n "$WERSJA" ] || { echo "Nie znaleziono wersji w CMakeLists.txt" >&2; exit 1; }
if [ "$BIBLIOTEKI" = 1 ] && ! command -v dylibbundler >/dev/null; then
    echo "Brak dylibbundler (brew install dylibbundler) - albo użyj opcji --bez-bibliotek." >&2; exit 1
fi

if [ ! -x "$B/DomowyMagazyn" ]; then
    cmake -S . -B "$B" -DCMAKE_BUILD_TYPE=Release -DUSE_SYSTEM_WX=ON
    cmake --build "$B" -j"$(sysctl -n hw.ncpu)"
fi
command -v python3 >/dev/null && python3 narzedzia/zbuduj_pomoc.py >/dev/null || true

OUT="$B/instalator"
APP="$OUT/DomowyMagazyn.app"
rm -rf "$OUT"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources" "$APP/Contents/Frameworks"

install -m 755 "$B/DomowyMagazyn" "$APP/Contents/MacOS/DomowyMagazyn"
R="$APP/Contents/Resources"
cp -r sql "$R/sql"
mkdir -p "$R/pomoc" && cp pomoc/pomoc_pl.htm pomoc/pola.txt "$R/pomoc/"
if   [ -f "$B/pl/LC_MESSAGES/DomowyMagazyn.mo" ]; then mkdir -p "$R/pl/LC_MESSAGES"; cp "$B/pl/LC_MESSAGES/DomowyMagazyn.mo" "$R/pl/LC_MESSAGES/"
elif [ -f lang/pl.mo ];                            then mkdir -p "$R/pl/LC_MESSAGES"; cp lang/pl.mo "$R/pl/LC_MESSAGES/DomowyMagazyn.mo"; fi
[ -d assets/etykiety ] && { mkdir -p "$R/assets"; cp -r assets/etykiety "$R/assets/"; }

# Ikona: .icns z PNG (sips i iconutil są w systemie)
ZESTAW="$OUT/AppIcon.iconset"; mkdir -p "$ZESTAW"
for r in 16 32 128 256 512; do
    sips -z $r $r assets/ikony/app_256.png --out "$ZESTAW/icon_${r}x${r}.png" >/dev/null
    sips -z $((r*2)) $((r*2)) assets/ikony/app_256.png --out "$ZESTAW/icon_${r}x${r}@2x.png" >/dev/null
done
iconutil -c icns "$ZESTAW" -o "$R/AppIcon.icns"

cat > "$APP/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key><string>Domowy Magazyn</string>
    <key>CFBundleDisplayName</key><string>Domowy Magazyn</string>
    <key>CFBundleIdentifier</key><string>pl.domowymagazyn.app</string>
    <key>CFBundleExecutable</key><string>DomowyMagazyn</string>
    <key>CFBundleIconFile</key><string>AppIcon</string>
    <key>CFBundlePackageType</key><string>APPL</string>
    <key>CFBundleShortVersionString</key><string>$WERSJA</string>
    <key>CFBundleVersion</key><string>$WERSJA</string>
    <key>CFBundleDevelopmentRegion</key><string>pl</string>
    <key>LSMinimumSystemVersion</key><string>10.15</string>
    <key>LSApplicationCategoryType</key><string>public.app-category.utilities</string>
    <key>NSHighResolutionCapable</key><true/>
    <key>NSCameraUsageDescription</key><string>Kamera służy do robienia zdjęć elementów i czytania kodów kreskowych.</string>
</dict>
</plist>
PLIST
plutil -lint "$APP/Contents/Info.plist" >/dev/null

# pdftotext (opcjonalnie): obok programu, żeby odczyt datasheetów działał bez Homebrew
if PDFT="$(command -v pdftotext 2>/dev/null)"; then
    cp -f "$PDFT" "$APP/Contents/MacOS/pdftotext"
    [ "$BIBLIOTEKI" = 1 ] && dylibbundler -of -b -cd -x "$APP/Contents/MacOS/pdftotext" \
        -d "$APP/Contents/Frameworks" -p @executable_path/../Frameworks/ >/dev/null || true
fi

if [ "$BIBLIOTEKI" = 1 ]; then
    dylibbundler -of -b -cd -x "$APP/Contents/MacOS/DomowyMagazyn" \
        -d "$APP/Contents/Frameworks" -p @executable_path/../Frameworks/ >/dev/null
    echo "Dołączono $(ls "$APP/Contents/Frameworks" | wc -l | tr -d ' ') bibliotek"
fi

# Podpis "ad hoc": bez niego aplikacje na Apple Silicon w ogóle się nie uruchomią
codesign --force --deep --sign - "$APP"

# Obraz dysku
DMG="$ROOT/DomowyMagazyn-$WERSJA-macos-$ARCH.dmg"
rm -f "$DMG"
DYSK="$OUT/dysk"; mkdir -p "$DYSK"
cp -R "$APP" "$DYSK/"; ln -s /Applications "$DYSK/Applications"
hdiutil create -volname "Domowy Magazyn" -srcfolder "$DYSK" -ov -format UDZO "$DMG" >/dev/null
echo "Gotowe: $DMG"
