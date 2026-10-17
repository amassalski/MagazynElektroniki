#!/bin/bash
# Wspólne wejście do budowania pakietów instalacyjnych:
#
#   narzedzia/zbuduj_instalator.sh [deb|windows|macos] [opcje skryptu docelowego]
#
# Bez argumentu wybiera system, na którym uruchomiono skrypt (każdy pakiet buduje się na swoim systemie):
#   Linux   -> deb      (zbuduj_deb.sh)      domowymagazyn_<wersja>_<arch>.deb
#   Windows -> windows  (zbuduj_windows.sh)  DomowyMagazyn-<wersja>-win64.zip i ...-instalator.exe  (MSYS2 MinGW 64)
#   macOS   -> macos    (zbuduj_macos.sh)    DomowyMagazyn-<wersja>-macos-<arch>.dmg
set -euo pipefail
KAT="$(cd "$(dirname "$0")" && pwd)"
CEL="${1:-}"
if [ -z "$CEL" ]; then
    case "$(uname -s)" in
        Linux) CEL=deb;; Darwin) CEL=macos;; MINGW*|MSYS*|CYGWIN*) CEL=windows;;
        *) echo "Nieznany system: $(uname -s)" >&2; exit 1;;
    esac
else
    shift
fi
case "$CEL" in
    deb)     exec "$KAT/zbuduj_deb.sh" "$@";;
    windows) exec "$KAT/zbuduj_windows.sh" "$@";;
    macos)   exec "$KAT/zbuduj_macos.sh" "$@";;
    *) echo "Użycie: $0 [deb|windows|macos] [opcje]" >&2; exit 1;;
esac
