# Domowy Magazyn: instrukcja użytkownika (etap 3: projekty, raporty, etykiety, sklepy i ceny)

## Pomoc, F1 i dymki
- **Pomoc → Spis treści** (F1 w oknie głównym) otwiera pomoc w oknie programu, w układzie wzorowanym na pomocy Eagle'a 7.6: spis treści, odnośniki, przyciski Wstecz / Dalej. **Otwórz pomoc w przeglądarce** otwiera ten sam plik (`pomoc/pomoc_pl.htm`) w przeglądarce systemowej.
- **F1 w polu formularza** pokazuje opis tego pola z przyciskiem *Więcej w pomocy*, który otwiera właściwy rozdział. Ten sam opis pojawia się w dymku po zatrzymaniu myszy nad polem.
- Nad listami (elementy, projekty, pozycje projektu, sklepy, linki, raporty) dymek pokazuje pełne dane wiersza, także to, co nie mieści się w kolumnach.
- Opisy pól są w bazie (tabela `tbl_pomoc_pol`) i można je poprawić; ręcznie zmieniony opis nie jest nadpisywany przy aktualizacji struktury bazy. Bez bazy program używa pliku `pomoc/pola.txt`.
- Pomoc budowana jest z `pomoc/pomoc_pl.src.htm` poleceniem `python3 narzedzia/zbuduj_pomoc.py` (generuje spis treści, `pomoc_pl.htm` i `sql/pomoc_pola.sql` oraz sprawdza odnośniki).
- **Po aktualizacji programu** wykonaj *Baza danych → Utwórz / zaktualizuj strukturę bazy* (schemat wersja 4).

## Okno główne
Po lewej jest drzewo kategorii, po prawej lista elementów wybranej kategorii, na dole
dziennik zdarzeń (można go ukryć: **Widok → Pokaż dziennik zdarzeń**). Panele są aktywne
dopiero po połączeniu z bazą, która ma strukturę programu.

## Kategorie
- **Nowa** dodaje kategorię główną, **Podkategoria** dodaje kategorię w zaznaczonej.
- **Zmień nazwę** (F2) i **Usuń** (Delete) działają na zaznaczonej kategorii. Usunąć można
  tylko pustą kategorię (bez podkategorii i elementów).
- Kategorię można przenieść przeciągnięciem na inną kategorię albo na „Wszystkie elementy”
  (poziom główny).
- Liczba w nawiasie to liczba elementów w kategorii razem z podkategoriami.
- **Magazyn → Importuj kategorie startowe** dodaje brakujące kategorie z pliku
  `sql/kategorie_startowe.txt`. **Importuj kategorie z pliku...** robi to samo z własnym
  plikiem tekstowym: jedna ścieżka w linii, nazwy rozdzielone znakiem ` / ` (ze spacjami)
  albo `>`, linie z `#` są pomijane. Istniejące kategorie są pomijane, więc import można
  powtarzać.

## Elementy
- **Dodaj element** (Ctrl+N, Insert na liście) otwiera formularz. Wymagany jest tylko symbol.
  Nowy element trafia do zaznaczonej kategorii. Ilość początkowa jest zapisywana w historii
  jako przyjęcie.
- **Edytuj** (Ctrl+E albo dwukrotne kliknięcie). Zmiana ilości dostępnej jest zapisywana
  w historii magazynu jako korekta.
- **Zmień stan** (Ctrl+T): przyjęcie, wydanie albo ustawienie stanu (inwentaryzacja), z komentarzem.
  Stan nie może spaść poniżej zera.
- **Usuń** (Delete): element bez powiązań jest kasowany razem z historią ilości. Element
  używany (lista części projektu, zamienniki, lokalizacje) jest tylko ukrywany.
- Elementy poniżej stanu minimalnego są wyróżnione kolorem. Pole **Stan minimalny** = 0
  wyłącza ostrzeżenie.
- Kliknięcie nagłówka kolumny sortuje listę, ponowne kliknięcie odwraca kolejność.

## Wyszukiwanie
Pole nad listą (Ctrl+F) szuka po symbolu, symbolu producenta, producencie, opisie, wartości,
obudowie i tagach. Wielkość liter i polskie znaki nie mają znaczenia („zolty” znajdzie
„żółty”). Kilka słów oznacza, że każde musi wystąpić. Wyszukiwanie dotyczy wybranej
kategorii; **Z podkategoriami** i **Tylko niski stan** zawężają lub rozszerzają wynik.

## Eksport do CSV
**Magazyn → Eksportuj listę do CSV...** zapisuje elementy widoczne na liście (z bieżącym
filtrem i sortowaniem). Format: UTF-8 z BOM, średnik jako separator, przecinek dziesiętny,
więc plik otwiera się w polskim Excelu bez ustawień.

## Projekty i lista części (BOM)

Zakładka **Projekty** (oraz menu **Projekty**) służy do planowania projektów i pilnowania, czy masz do nich komplet części.

**Projekty.** „Nowy projekt" zakłada projekt (nazwa, status: planowany / w trakcie / wstrzymany / zakończony, opis). Na liście projektów kolor pomarańczowy oznacza, że czegoś brakuje w magazynie. „Usuń" kasuje projekt razem z listą części; jeśli wydano już do niego części, projekt jest tylko archiwizowany (pokażesz go polem „Pokaż zarchiwizowane").

**Lista części projektu.** Po prawej widać: oznaczenia (R1, R2...), symbol, wartość, obudowę, ile potrzeba, ile już wydano, ile jest w magazynie i ile brakuje. Pomarańczowy wiersz = brakuje w magazynie, zielony = wydano w całości.
- „Dodaj część..." dodaje element z magazynu (jeśli już jest w projekcie, ilość się zwiększa).
- „Edytuj pozycję..." zmienia oznaczenia, ilość, ilość zamówioną, priorytet i uwagi.
- „Wydaj z magazynu..." zdejmuje wybraną ilość ze stanu magazynu (zapis w historii jako WYDANIE dla tego projektu); „Wydaj wszystko dostępne" robi to dla całej listy naraz.
- „Lista zakupów (CSV)..." zapisuje to, czego brakuje (z uwzględnieniem tego, co już zamówiono), w formacie czytelnym dla polskiego Excela.

**Import listy części z KiCada i Eagle'a** (Ctrl+I albo „Importuj BOM..."):
- KiCad: raport BOM w CSV (Plik → Wytwórz → Lista materiałowa) albo netlista XML.
- Eagle: plik schematu `.sch` albo CSV wyeksportowany skryptem BOM.
- Pozycje o tej samej wartości i obudowie są łączone, części oznaczone DNP / „nie montować" pomijane.
- Program proponuje dla każdej pozycji element z magazynu (ten sam symbol albo ta sama wartość i obudowa); zielone = istniejący element, niebieskie = zostanie utworzony nowy (ze stanem 0), szare = pominięte. Wybór można zmienić przyciskami pod listą, zanim cokolwiek zostanie zapisane.
- „Liczba płytek" mnoży ilości. „Zastąp obecną listę" czyści pozycje, z których jeszcze nic nie wydano.
- Ponowny import tego samego pliku zwiększa ilości, nie dubluje elementów.

Uwaga: pliki `.kicad_sch` (sam schemat KiCada 6+) nie są wczytywane. Wyeksportuj BOM albo netlistę.

## Raporty (menu Raporty, Ctrl+R)

Dostępne raporty: stan magazynu (z wartością), niskie stany (z ilością do zamówienia), wartość magazynu wg kategorii głównych, braki w projektach oraz historia zmian stanów (z zakresem dat RRRR-MM-DD). Raport widać w tabeli; zapisz go jako CSV (do Excela), jako HTML albo kliknij „Otwórz do wydruku / PDF", co otwiera go w przeglądarce, gdzie można wydrukować lub zapisać jako PDF. Wartości liczone są tylko dla elementów z podaną ceną; sumy podawane są osobno dla każdej waluty.

## Etykiety z kodem kreskowym (Magazyn, Ctrl+P)

Okno drukuje etykiety dla zaznaczonego elementu albo dla wszystkich elementów z bieżącej listy (np. po przefiltrowaniu kategorią). Na etykiecie jest symbol, wartość i obudowa, producent oraz kod kreskowy z numerem elementu. Typ kodu (EAN-13, EAN-8, Code128) i prefiks ustawia się w Ustawieniach aplikacji.
- Formaty arkuszy: A4 3x8, 3x7, 5x13, 2x4 oraz etykiety pojedyncze 50x25 i 62x29 mm.
- „Zacznij od etykiety nr" pozwala dokończyć częściowo zużyty arkusz; przesunięcia korygują położenie wydruku. Na początek wydrukuj próbę z ramkami na zwykłym papierze.
- „Podgląd wydruku" i „Drukuj" korzystają z systemowego okna drukowania (tam też „Zapisz jako PDF"); „Zapisz jako PNG" zapisuje strony jako obrazy.

## Skanowanie kodów i zdjęcia

- Skaner USB (działa jak klawiatura): kliknij pole „Szukaj" i zeskanuj etykietę. Kod z naszym prefiksem od razu pokazuje element.
- „Znajdź element po zdjęciu kodu..." (menu Magazyn) odczytuje kod z pliku obrazu, np. zdjęcia etykiety zrobionego telefonem.
- W formularzu elementu przycisk „Wybierz..." przy zdjęciu kopiuje obraz do katalogu zdjęć z ustawień.
- Podgląd na żywo z kamery komputera nie jest jeszcze zrealizowany.

## Sklepy, ceny i datasheety (menu Sklepy)

**Po aktualizacji programu** wybierz *Baza danych → Utwórz / zaktualizuj strukturę bazy* (schemat wersja 3: dostawa, adresy wyszukiwania, kursy walut).

### Lista sklepów (Ctrl+Shift+S)
- *Dodaj / Zmień / Usuń*: nazwa, adres strony, **adres wyszukiwania** (z `{q}` w miejscu szukanej frazy, np. `https://sklep.pl/szukaj?q={q}`), waluta, **koszt dostawy**, **próg darmowej dostawy**, priorytet i aktywność. Sklep użyty w linkach elementów nie jest kasowany, tylko wyłączany.
- *Wczytaj listę...*: wklej wiele sklepów naraz (jeden w wierszu, pola oddzielone średnikiem lub tabulatorem, także wprost z Excela): `nazwa; adres strony; adres wyszukiwania; waluta; dostawa; darmowa od`. Sklep o tej samej nazwie jest aktualizowany. Można też wczytać plik albo wstawić bieżącą listę, poprawić ją i wczytać z powrotem.
- *Kursy walut...*: ile złotych kosztuje jednostka waluty (`USD = 4,05`). Kursy wpisuje się ręcznie, program nie pobiera ich z internetu; domyślne wartości są tylko przykładowe, **zaktualizuj je**.
- Adresy wyszukiwania sklepów startowych nie były sprawdzane na działających stronach: popraw je, jeśli sklep zmienił wyszukiwarkę.

### Sklepy, ceny i datasheet elementu (Ctrl+K, po zaznaczeniu elementu)
- Każdy nowy element dostaje automatycznie linki wyszukiwania do głównych sklepów. Żeby program mógł sprawdzać ceny, wklej w *Zmień...* adres **strony samego produktu** (przycisk *Szukaj w sklepie* otwiera wyniki wyszukiwania w przeglądarce). Cenę można też wpisać ręcznie.
- *Sprawdź ceny wszystkich / zaznaczony*: program pobiera strony po jednej, z przerwą, **tylko na Twoje polecenie**. Cenę szuka w danych strukturalnych strony (JSON-LD schema.org, znaczniki `product:price:amount`, `itemprop="price"`). Dla sklepu, który ich nie podaje, ustaw w oknie sklepu **wzorzec ceny**: wyrażenie regularne z jedną grupą w nawiasie, np. `cena-brutto">([0-9 ,.]+)`. Program niczego nie zgaduje z przypadkowych liczb na stronie. Zmiany cen trafiają do historii (*Historia cen...*).
- **Datasheet**: *Otwórz* (plik z katalogu dokumentów albo adres w przeglądarce), *Pobierz z adresu...* (sprawdza, że to naprawdę PDF; plik trafia do katalogu z Ustawień i ma nazwę z symbolu elementu), *Znajdź na stronach sklepów* (szuka odnośników do PDF na stronach linków elementu) oraz *Szukaj w internecie...* (Google, Octopart, AllDatasheet w przeglądarce).

### Najtańsze zestawienie zakupów (Ctrl+Shift+Z)
Zestawia braki wybranego projektu (albo wszystkich aktywnych) i rozdziela je między sklepy:
- *Najniższa suma z dostawą*: uwzględnia koszt dostawy i progi darmowej dostawy, więc czasem opłaca się kupić całość w jednym sklepie, choć pojedyncze części są tam droższe; okno pokazuje, ile łączenie zakupów oszczędza,
- *Najniższe ceny części*: każda część tam, gdzie jest najtańsza,
- *Wszystko w jednym sklepie*.
Ceny w obcych walutach są przeliczane wg kursów z listy sklepów. Pozycje bez ceny w żadnym sklepie są wypisane osobno. Opcja *Odejmij już zamówione* uwzględnia kolumnę „zamówione” z projektu. *Sprawdź aktualne ceny* odświeża ceny linków potrzebnych części. Wynik można skopiować jako listę zamówień (sklep → ilość x symbol) albo zapisać do CSV i HTML.

**Ograniczenia:** programu nie sprawdzano na prawdziwych sklepach (sklepy często blokują automatyczne zapytania i zmieniają strony), dlatego wzorce i adresy są do ustawienia; wiele sklepów podaje cenę dopiero po zalogowaniu albo dynamicznie (JavaScript), czego program nie odczyta. Wtedy wpisz cenę ręcznie.

## Miejsce magazynowania elementu

W formularzu elementu podajesz kontener, szufladę i opcjonalnie przegrodę, np. `Kontener 1 szufladka A1/1` albo samo `A1`. Zapis swobodny w polu kontenera program sam rozdzieli na części. Miejsce widać na liście elementów, w etykietach, w eksporcie CSV i w raporcie stanu.

## Generator szeregów E (Magazyn)

Wprowadza cały zestaw rezystorów, kondensatorów lub cewek z szeregu E6, E12, E24, E48 albo E96 w wybranym zakresie. Wartości wpisujesz zapisem `4k7`, `100n`, `2M2`. Podaj obudowę, tolerancję, ilość, minimum, miejsce (opcja: osobna szuflada na każdą dekadę), kategorię i wzór symbolu. Lista podglądu pokazuje, które elementy są nowe, a które już są w magazynie. Istniejące symbole można pominąć, uzupełnić ilością albo danymi. Zestaw można też zapisać jako CSV.

## Import elementów z pliku CSV (Magazyn)

Wczytuje dowolną tabelę elementów (rezystory, kondensatory, tranzystory itd.). Wymagana jest kolumna `Symbol`. Rozpoznawane kolumny: Wartość, Tolerancja, Obudowa, Parametry, Ilość, Minimum, Kategoria, Miejsce, Producent, Cena, Tagi i inne. Przycisk *Zapisz szablon CSV* tworzy plik wzorcowy. Brakujące kategorie są tworzone, błędne wiersze raportowane po imporcie.

Uwaga: po aktualizacji programu uruchom `Baza danych / Utwórz / zaktualizuj strukturę bazy` (struktura w wersji 5).

## Narzędzia (Ctrl+D)

*Paski kolorów i kody SMD* odczytuje paski rezystorów (3 do 6) i cewek (3 lub 4); lista kolorów zależy od roli paska, a brak paska tolerancji w kodzie czteropaskowym to ±20 %, pokazuje paski dla wpisanej wartości oraz rozpoznaje kody SMD (472, 4R7, EIA-96 jak 01A, kondensatory jak 104).

## Ikony

Ikony programu i menu są własnym, płaskim zestawem rysowanym skryptem `narzedzia/zbuduj_ikony.py` i wkompilowanym w program. Zmiana wyglądu to edycja skryptu i ponowne uruchomienie: `python3 narzedzia/zbuduj_ikony.py`.

## Zdjęcia elementów i kamera USB

Po prawej stronie listy elementów widać zdjęcie i dane zaznaczonego elementu. Przycisk *Zdjęcie z kamery* otwiera podgląd na żywo: *Zrób zdjęcie*, potem *Użyj zdjęcia* (albo *Powtórz*). Zdjęcie jest zapisywane jako JPEG w katalogu zdjęć z ustawień i przypisywane do elementu. To samo robi przycisk *Z kamery* w formularzu elementu. Zdjęcie można też wskazać z pliku, a *Usuń zdjęcie* odpina je od elementu (plik zostaje).

### Wymagania do budowania z obsługą kamery

Kamerę obsługuje OpenCV (tylko moduły `core` i `videoio`). CMake wykrywa je sam (opcja `-DDM_KAMERA=AUTO|ON|OFF`):

- MSYS2: `pacman -S mingw-w64-x86_64-opencv`
- macOS: `brew install opencv`
- Linux (Debian/Ubuntu): `sudo apt install libopencv-dev`

Bez OpenCV program buduje się i działa normalnie, a okno kamery informuje, że obsługi nie ma. Do sprawdzania okna bez kamery służy zmienna środowiskowa `DM_KAMERA_TEST=1` (generowany obraz).

## Współpraca z Eagle CAD 7.6 (menu Projekty)

- *Program ULP dla Eagle* zapisuje `dm_bom.ulp`. Uruchomiony w edytorze schematu (File / Run ULP) otwiera okno znane z standardowego `bom.ulp` (wariant montażu, lista części lub wartości). Format *DomowyMagazyn CSV* ma kolumny Reference;Value;Package;MPN;Manufacturer;Description;Qty (MPN i producent z atrybutów `MPN` i `MANUFACTURER`) i importujesz go jak każdą listę części. Skrypt nie był uruchamiany w prawdziwym Eagle 7.6 – w razie błędu podaj jego treść.
- *Skrypt atrybutów dla Eagle* zapisuje plik `.scr` dla wybranego projektu. Wykonany w edytorze schematu (File / Execute Script) ustawia w częściach atrybuty `DM_SYMBOL`, `DM_STAN`, `DM_MIEJSCE`, `MPN` i `MANUFACTURER`. Wartości są zapisywane bez polskich znaków.

Skrypty były sprawdzane testami na składnię i zgodność z importem, ale nie w samym Eagle. Jeśli polecenie `ATTRIBUTE` w Twojej wersji wymaga innej składni, daj znać.

## Katalog elementów (zakładka Katalog)
Katalog to baza wiedzy o elementach, niezależna od stanów magazynowych. Pozycje łączą się z magazynem po symbolu (symbol lub symbol producenta, bez względu na wielkość liter); pozycje, które masz w magazynie, są zielone.
- **Nowa / Edytuj**: symbol, producent, opis, obudowa, rodzina, adres datasheetu, uwagi i dowolne parametry (nazwa + wartość).
- **Import / Eksport / Szablon CSV**: kolumny Symbol, Producent, Opis, Obudowa, Rodzina, Datasheet, Uwagi, Parametry ("A=1; B=2" w cudzysłowie) – dodatkowe kolumny stają się parametrami.
- **Dodaj do magazynu**: tworzy element z danymi z katalogu (ilość, miejsce, kategoria).
- **Z katalogu...** w formularzu elementu wypełnia pola z wybranej pozycji.
- **Odczytaj parametry z PDF**: wymaga programu `pdftotext` (poppler-utils; MSYS2: `pacman -S mingw-w64-x86_64-poppler`). Program czyta tabele Absolute Maximum Ratings (przedrostek "Maks.:") i Electrical Characteristics ("Elektr.:"), w tym hFE, a opis zastosowania proponuje do pola Uwagi. Znane parametry mają polską nazwę i symbol, pozostałe wiersze są oznaczone "zgadywane" i domyślnie odznaczone. Propozycje zatwierdzasz; zapisują się ze źródłem "datasheet".


## Aktualizacje (menu Pomoc)

*Sprawdź aktualizacje* pobiera plik `DomowyMagazyn/wersja.txt` z repozytorium na GitHubie (gałąź domyślna) i porównuje numer z wersją programu. Gdy jest nowsza, pokazuje numer, opis zmian i proponuje otwarcie strony pobierania; niczego nie instaluje. Opcja *Sprawdzaj aktualizacje przy uruchomieniu* (domyślnie włączona) sprawdza po cichu, najwyżej raz dziennie.

Wydanie nowej wersji: podnieś numer w `CMakeLists.txt` (`project(... VERSION ...)`), zbuduj instalatory, a po ich opublikowaniu zmień pierwszą linię `DomowyMagazyn/wersja.txt` (opcjonalnie: druga linia z adresem pobierania zaczynającym się od `http`, dalej opis zmian) i wyślij plik do repozytorium. Inny adres pliku: `config.ini`, sekcja `[aktualizacje]`, klucz `adres`.
