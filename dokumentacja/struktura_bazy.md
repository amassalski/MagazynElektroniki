# Struktura bazy

Skrypty w katalogu `sql/` (kolejność wykonania):

| Plik | Zawartość |
|------|-----------|
| `schema.sql` | tabele, indeksy, tabela wersji struktury (`tbl_wersja_schematu`) |
| `funkcje_i_trigger.sql` | funkcje i triggery |
| `widoki.sql` | widoki |
| `dane_startowe.sql` | sklepy i kategorie startowe |

Wszystkie skrypty są idempotentne (można je uruchamiać wielokrotnie).

## Funkcje

- `zmien_ilosc_elementu(id_elementu, zmiana, typ, id_projektu, id_lokalizacji, komentarz, uzytkownik, cena)` zmienia stan, zapisuje transakcję i pilnuje, by stan nie spadł poniżej zera.
- `szukaj_elementow(tekst, limit)` szuka po symbolu, producencie, opisie, wartości, obudowie i tagach, bez rozróżniania wielkości liter i polskich znaków.
- `dodaj_kategorie_jesli_brak(id_rodzica, nazwa, opis)`.
- `utworz_link_sklepu(id_sklepu, kod_producenta, symbol)`, `fn_url_encode(tekst)`.

## Triggery

- ścieżka (`ltree`) i poziom kategorii są liczone automatycznie, także po zmianie rodzica; zabezpieczenie przed cyklami,
- nowy element dostaje linki do 5 najważniejszych sklepów,
- najwyżej jeden domyślny link sklepu na element,
- każda zmiana ceny w sklepie trafia do `tbl_historia_cen_sklepy`,
- automatyczne daty modyfikacji.

## Widoki

`v_kategorie`, `v_elementy`, `v_niskie_stany`, `v_sklepy_elementow`, `v_najlepsze_ceny`, `v_bom_stan`.

## Wersja 3 (sklepy i ceny)

- `tbl_sklepy`: `szablon_wyszukiwania` (adres z `{q}`), `koszt_dostawy`, `darmowa_dostawa_od`, `regex_ceny`.
- `tbl_elementy_sklepy`: `kod_w_sklepie`. Historię cen zapisuje wyzwalacz `fn_historia_cen` (źródło z ustawienia sesji `dm.zrodlo_ceny`: `reczna`, `strona`).
- `tbl_kursy_walut`: kursy do złotego, wpisywane ręcznie.
- `utworz_link_sklepu` używa `szablon_wyszukiwania`, a bez niego dawnej pary `url_bazowy` + `parametr_wyszukiwania`.

## Wersja 4 (pomoc kontekstowa)

- `tbl_pomoc_pol`: opisy pól formularzy (`klucz`, `tytul`, `opis`, `kotwica` w pliku pomocy). Zasilana skryptem `sql/pomoc_pola.sql`, generowanym z `pomoc/pola.txt` przez `narzedzia/zbuduj_pomoc.py`.
- Kolumna `zmienione`: wyzwalacz `fn_pomoc_pol_zmiana` ustawia ją na `true` przy ręcznej zmianie wiersza. Takiego opisu aktualizacja struktury nie nadpisuje. Żeby wrócić do opisu fabrycznego, usuń wiersz i uruchom aktualizację struktury.

## Wersja 5 (miejsce, tolerancja, parametry)

- `tbl_elementy`: `kontener`, `szuflada`, `przegroda` (miejsce magazynowania, każda część opcjonalna), `tolerancja` (np. `5%`) i `parametry` (swobodny tekst, np. `0,1 W; X7R`).
- Widoki `v_elementy`, `v_niskie_stany` i raport stanu magazynu zawierają nowe kolumny. Każda zmiana używa `ADD COLUMN IF NOT EXISTS`, więc aktualizacja struktury jest bezpieczna dla istniejących danych.

## Wersja 6: katalog elementów
- `tbl_katalog` (symbol, producent, opis, obudowa, rodzina, datasheet_url, datasheet_plik, uwagi); unikalny klucz (lower(symbol), lower(producent)).
- `tbl_katalog_parametry` (id_katalogu → kaskadowo, nazwa, wartosc, zrodlo: reczny/csv/datasheet, kolejnosc); nazwa unikalna w obrębie pozycji.
- Powiązanie z `tbl_elementy` po symbolu / symbolu producenta (bez klucza obcego).
