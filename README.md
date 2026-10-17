# DomowyMagazyn - aplikacja do zarządzania częściami elektronicznymi
## Wymagania
- wxWidgets 3.2+
- PostgreSQL 9.6+
- CMake 3.16+
----
# Budowanie programu (Windows, MSYS2)

Cały program buduje się w powłoce **MSYS2 MINGW64** (nie MSYS, nie UCRT64).

## Pakiety

```bash
pacman -S --needed mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja \
  mingw-w64-x86_64-wxwidgets3.2-msw mingw-w64-x86_64-postgresql mingw-w64-x86_64-gettext-tools
```

## Budowanie

```bash
cd /g/Massaada/Develop/DomowyMagazyn
rm -rf build && mkdir build && cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release          # -DDM_CONSOLE=ON dodaje konsolę do debugowania
ninja
./DomowyMagazyn.exe
```

Po zbudowaniu CMake kopiuje obok exe katalog `sql/` oraz (w MSYS2) wymagane DLL-ki z `/mingw64`.
Nie kopiuj nic z innych instalacji wxWidgets/PostgreSQL, bo pomieszane wersje DLL
dają błąd 0xc0000139.

## Konfiguracja

Program szuka `config.ini` obok exe, potem w katalogu bieżącym i katalog wyżej.
Jeśli pliku nie ma, zapisze nowy obok exe. Połączenie ustawiasz w programie:
**Baza danych → Ustawienia połączenia...**. Hasło można zapisać w pliku (zwykły tekst)
albo nie zapamiętywać; wtedy program zapyta o nie przy starcie.

## Pierwsze uruchomienie bazy

Po połączeniu z pustą bazą program zaproponuje utworzenie struktury. Można też zrobić to
później: **Baza danych → Utwórz / zaktualizuj strukturę bazy...** (skrypty z katalogu `sql/`
można uruchamiać wielokrotnie, nie kasują danych). Ręcznie, kolejno:
`schema.sql`, `funkcje_i_trigger.sql`, `widoki.sql`, `dane_startowe.sql`.
Wymagany PostgreSQL 9.6 lub nowszy i rozszerzenie `ltree`. Jeśli użytkownik aplikacji
nie ma uprawnienia CREATE na bazie, administrator wykonuje jednorazowo:
`psql -U postgres -d NAZWA_BAZY -c "CREATE EXTENSION IF NOT EXISTS ltree;"`.

## Obsługa programu
Opis okien i funkcji: `instrukcja_uzytkownika.md`.

## Testy

```bash
cmake .. -G Ninja -DDM_BUILD_TESTS=ON
ninja
# osobna, pusta baza testowa! Test czyści schemat public.
DM_TEST_DB=dm_test DM_TEST_USER=postgres DM_TEST_PASSWORD=... ctest --output-on-failure
```

## Pakiet DEB (Debian, Ubuntu, Mint)
Budowanie: `narzedzia/zbuduj_deb.sh` (wymaga cmake, g++, libwxgtk3.2-dev, libpq-dev, dpkg-dev; kamera: libopencv-dev). Wynik: `domowymagazyn_<wersja>_amd64.deb`.

Instalacja: `sudo apt install ./domowymagazyn_1.0.0_amd64.deb` (apt doinstaluje wymagane biblioteki). Uruchomienie: polecenie `domowymagazyn` albo ikona w menu aplikacji.

Program leży w `/usr/lib/domowymagazyn`. Ponieważ ten katalog jest tylko do odczytu, konfiguracja trafia do `~/.config/DomowyMagazyn/config.ini`, a zdjęcia i datasheety (jeśli nie ustawisz własnych ścieżek w Ustawieniach) do `~/.local/share/DomowyMagazyn/`. Serwer PostgreSQL nie jest instalowany razem z pakietem (może działać na innym komputerze); po pierwszym uruchomieniu wpisz dane połączenia i wybierz "Baza danych / Utwórz / zaktualizuj strukturę bazy". Do odczytu parametrów z datasheetów przyda się `poppler-utils` (pakiet go poleca).

## Instalatory dla Windows i macOS
Każdy pakiet buduje się na swoim systemie. Wspólne wejście: `narzedzia/zbuduj_instalator.sh [deb|windows|macos]` (bez argumentu wybiera bieżący system).
- **Windows** (powłoka MSYS2 MinGW 64-bit): `narzedzia/zbuduj_windows.sh`. Wynik: `DomowyMagazyn-<wersja>-win64.zip` (wersja przenośna, z bibliotekami DLL) oraz `DomowyMagazyn-<wersja>-instalator.exe`, jeśli jest NSIS (`pacman -S mingw-w64-x86_64-nsis`; do ikony `mingw-w64-x86_64-imagemagick`; do odczytu datasheetów `mingw-w64-x86_64-poppler`). Instalator kopiuje program do Program Files, tworzy skróty i wpis w „Aplikacje i funkcje". Konfiguracja użytkownika trafia do `%APPDATA%\DomowyMagazyn`.
- **macOS**: `narzedzia/zbuduj_macos.sh` (potrzebne: `brew install cmake wxwidgets postgresql@15 dylibbundler`, opcjonalnie `poppler`). Wynik: `DomowyMagazyn-<wersja>-macos-<arch>.dmg` z `DomowyMagazyn.app` z dołączonymi bibliotekami (dylibbundler). Aplikacja jest podpisana tylko „ad hoc", więc po pobran

- 05_okno_z_danymi (1260, 780)
21_zakladka_katalog (1400, 1000)
09_projekty (1260, 780)
17d_wartosc6 (1400, 1000)
16_generator_szeregow (1400, 1000)
total 564
drwxr-xr-x 2 root root   4096 Oct 17 06:49 .
drwxr-xr-x 3 root root   4096 Oct 17 06:49 ..
-rw-r--r-- 1 root root 164229 Oct 17 06:49 generator.png
-rw-r--r-- 1 root root  90994 Oct 17 06:49 katalog.png
-rw-r--r-- 1 root root  96425 Oct 17 06:49 magazyn.png
-rw-r--r-- 1 root root 107859 Oct 17 06:49 narzedzia.png
-rw-r--r-- 1 root root  96425 Oct 17 06:49 projekty.png
# Domowy Magazyn: instrukcja użytkownika (etap 3: projekty, raporty, etykiety, sklepy i ceny)

## Pomoc, F1 i dymki
- **Pomoc → Spis treści** (F1 w oknie głównym) otwiera pomoc w oknie programu, w układzie wzorowanym na pomocy Eagle'a 7.6: spis treści, odnośniki, przy
- **F1 w polu formularza** pokazuje opis tego pola z przyciskiem *Więcej w pomocy*, który otwiera właściwy rozdział. Ten sam opis pojawia się w dymku po z
- Nad listami (elementy, projekty, pozycje projektu, sklepy, linki, raporty) dymek pokazuje pełne dane wiersza, także to, co nie mieści się w kolumnach.
- Opisy pól są w bazie (tabela `tbl_pomoc_pol`) i można je poprawić; ręcznie zmieniony opis nie jest nadpisywany przy aktualizacji struktury bazy. Bez bazy
- Pomoc budowana jest z `pomoc/pomoc_pl.src.htm` poleceniem `python3 narzedzia/zbuduj_pomoc.py` (generuje spis treści, `pomoc_pl.htm` i `sql/pomoc_pola.sql` or
- **Po aktualizacji programu** wykonaj *Baza danych → Utwórz / zaktualizuj strukturę bazy* (schemat wersja 4).

## Okno główne
Po lewej jest drzewo kategorii, po prawej lista elementów wybranej kategorii, na dole
dziennik zdarzeń (można go ukryć: **Widok → Pokaż dziennik zdarzeń**). Panele są aktywne
dopiero po połączeniu z bazą, która ma strukturę programu.
