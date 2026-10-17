-- =====================================================================
-- DomowyMagazyn - dane startowe (sklepy, kategorie)
-- Skrypt jest idempotentny: nic nie dubluje i nie nadpisuje zmian.
-- =====================================================================

INSERT INTO tbl_sklepy (nazwa, opis, url_bazowy, kraj, waluta, priorytet, parametr_wyszukiwania)
SELECT v.nazwa, v.opis, v.url_bazowy, v.kraj, v.waluta, v.priorytet, v.parametr
FROM (VALUES
    ('TME', 'Transfer Multisort Elektronik', 'https://www.tme.eu/pl/katalog/', 'Polska', 'PLN', 1, 'szukaj={KOD}/'),
    ('Allegro', 'Allegro.pl', 'https://allegro.pl/listing?string=', 'Polska', 'PLN', 2, '{KOD}'),
    ('AliExpress', 'AliExpress', 'https://pl.aliexpress.com/wholesale?catId=0&SearchText=', 'China', 'USD', 3, '{KOD}'),
    ('Farnell', 'Farnell/Element14', 'https://pl.farnell.com/search?st=', 'Polska', 'PLN', 4, '{KOD}'),
    ('Mouser', 'Mouser Electronics', 'https://www.mouser.pl/Search/Refine?Keyword=', 'USA', 'USD', 5, '{KOD}'),
    ('DigiKey', 'Digi-Key Electronics', 'https://www.digikey.pl/products/en?keywords=', 'USA', 'USD', 6, '{KOD}'),
    ('RS Components', 'RS Online', 'https://pl.rs-online.com/web/c/', 'Polska', 'PLN', 7, '{KOD}'),
    ('Conrad', 'Conrad Electronic', 'https://www.conrad.pl/', 'Polska', 'PLN', 8, '{KOD}'),
    ('Botland', 'Botland', 'https://botland.com.pl/', 'Polska', 'PLN', 9, '{KOD}'),
    ('AVT', 'AVT Kits', 'https://sklep.avt.pl/', 'Polska', 'PLN', 10, '{KOD}')
) AS v(nazwa, opis, url_bazowy, kraj, waluta, priorytet, parametr)
WHERE NOT EXISTS (SELECT 1 FROM tbl_sklepy s WHERE lower(s.nazwa) = lower(v.nazwa));

-- Uwaga: adresy wyszukiwania sklepów (url_bazowy + parametr_wyszukiwania) pochodzą
-- z pierwotnego schematu i nie były weryfikowane. Popraw je w tabeli tbl_sklepy.

-- Kategorie startowe
DO $$
DECLARE
    r bigint;
    p bigint;
BEGIN
    r := dodaj_kategorie_jesli_brak(NULL, 'Elementy bierne');
        p := dodaj_kategorie_jesli_brak(r, 'Rezystory');
            PERFORM dodaj_kategorie_jesli_brak(p, 'Rezystory THT');
            PERFORM dodaj_kategorie_jesli_brak(p, 'Rezystory SMD');
            PERFORM dodaj_kategorie_jesli_brak(p, 'Potencjometry i trymery');
            PERFORM dodaj_kategorie_jesli_brak(p, 'Termistory i warystory');
        p := dodaj_kategorie_jesli_brak(r, 'Kondensatory');
            PERFORM dodaj_kategorie_jesli_brak(p, 'Elektrolityczne');
            PERFORM dodaj_kategorie_jesli_brak(p, 'Ceramiczne');
            PERFORM dodaj_kategorie_jesli_brak(p, 'Foliowe');
            PERFORM dodaj_kategorie_jesli_brak(p, 'Tantalowe');
        p := dodaj_kategorie_jesli_brak(r, 'Cewki i dławiki');
        p := dodaj_kategorie_jesli_brak(r, 'Kwarce i rezonatory');
        p := dodaj_kategorie_jesli_brak(r, 'Bezpieczniki');

    r := dodaj_kategorie_jesli_brak(NULL, 'Półprzewodniki');
        p := dodaj_kategorie_jesli_brak(r, 'Diody');
            PERFORM dodaj_kategorie_jesli_brak(p, 'Diody prostownicze');
            PERFORM dodaj_kategorie_jesli_brak(p, 'Diody Zenera');
            PERFORM dodaj_kategorie_jesli_brak(p, 'Diody Schottky''ego');
            PERFORM dodaj_kategorie_jesli_brak(p, 'Diody LED');
        p := dodaj_kategorie_jesli_brak(r, 'Tranzystory');
            PERFORM dodaj_kategorie_jesli_brak(p, 'Bipolarne');
            PERFORM dodaj_kategorie_jesli_brak(p, 'MOSFET');
            PERFORM dodaj_kategorie_jesli_brak(p, 'IGBT');
        p := dodaj_kategorie_jesli_brak(r, 'Tyrystory i triaki');
        p := dodaj_kategorie_jesli_brak(r, 'Transoptory');

    r := dodaj_kategorie_jesli_brak(NULL, 'Układy scalone');
        PERFORM dodaj_kategorie_jesli_brak(r, 'Mikrokontrolery');
        PERFORM dodaj_kategorie_jesli_brak(r, 'Stabilizatory napięcia');
        PERFORM dodaj_kategorie_jesli_brak(r, 'Wzmacniacze operacyjne');
        PERFORM dodaj_kategorie_jesli_brak(r, 'Logika');
        PERFORM dodaj_kategorie_jesli_brak(r, 'Pamięci');
        PERFORM dodaj_kategorie_jesli_brak(r, 'Interfejsy i komunikacja');
        PERFORM dodaj_kategorie_jesli_brak(r, 'Przetworniki ADC/DAC');

    r := dodaj_kategorie_jesli_brak(NULL, 'Złącza i elektromechanika');
        PERFORM dodaj_kategorie_jesli_brak(r, 'Złącza i listwy');
        PERFORM dodaj_kategorie_jesli_brak(r, 'Przełączniki i przyciski');
        PERFORM dodaj_kategorie_jesli_brak(r, 'Przekaźniki');
        PERFORM dodaj_kategorie_jesli_brak(r, 'Gniazda i podstawki');
        PERFORM dodaj_kategorie_jesli_brak(r, 'Przewody i kable');

    r := dodaj_kategorie_jesli_brak(NULL, 'Moduły i płytki');
        PERFORM dodaj_kategorie_jesli_brak(r, 'Płytki rozwojowe');
        PERFORM dodaj_kategorie_jesli_brak(r, 'Moduły czujników');
        PERFORM dodaj_kategorie_jesli_brak(r, 'Moduły zasilające');
        PERFORM dodaj_kategorie_jesli_brak(r, 'Wyświetlacze');

    r := dodaj_kategorie_jesli_brak(NULL, 'Zasilanie');
        PERFORM dodaj_kategorie_jesli_brak(r, 'Baterie i akumulatory');
        PERFORM dodaj_kategorie_jesli_brak(r, 'Zasilacze');
        PERFORM dodaj_kategorie_jesli_brak(r, 'Transformatory');

    r := dodaj_kategorie_jesli_brak(NULL, 'Mechanika i obudowy');
        PERFORM dodaj_kategorie_jesli_brak(r, 'Obudowy');
        PERFORM dodaj_kategorie_jesli_brak(r, 'Śrubki, dystanse, wsporniki');
        PERFORM dodaj_kategorie_jesli_brak(r, 'Radiatory');

    PERFORM dodaj_kategorie_jesli_brak(NULL, 'Narzędzia i materiały');
    PERFORM dodaj_kategorie_jesli_brak(NULL, 'Inne');
END
$$;

-- Wersja 3: szablony wyszukiwania sklepów ({q} = szukana fraza) i przykładowe kursy.
-- Adresy NIE były sprawdzane na działających sklepach (sklepy zmieniają strony) - popraw w oknie Sklepy.
UPDATE tbl_sklepy SET szablon_wyszukiwania = v.szablon
FROM (VALUES
    ('TME', 'https://www.tme.eu/pl/katalog/?search={q}'),
    ('Allegro', 'https://allegro.pl/listing?string={q}'),
    ('AliExpress', 'https://pl.aliexpress.com/wholesale?SearchText={q}'),
    ('Farnell', 'https://pl.farnell.com/search?st={q}'),
    ('Mouser', 'https://www.mouser.pl/c/?q={q}'),
    ('DigiKey', 'https://www.digikey.pl/pl/products/result?keywords={q}'),
    ('RS Components', 'https://pl.rs-online.com/web/c/?searchTerm={q}'),
    ('Conrad', 'https://www.conrad.pl/pl/search.html?search={q}'),
    ('Botland', 'https://botland.com.pl/szukaj?controller=search&s={q}'),
    ('AVT', 'https://sklep.avt.pl/szukaj.html?q={q}')
) AS v(nazwa, szablon)
WHERE lower(tbl_sklepy.nazwa) = lower(v.nazwa) AND tbl_sklepy.szablon_wyszukiwania IS NULL;

-- Kursy tylko przykładowe - zaktualizuj w oknie Sklepy (przycisk Kursy walut)
INSERT INTO tbl_kursy_walut (waluta, kurs_pln)
SELECT v.w, v.k FROM (VALUES ('PLN', 1.0), ('EUR', 4.30), ('USD', 4.00), ('GBP', 5.00)) AS v(w, k)
WHERE NOT EXISTS (SELECT 1 FROM tbl_kursy_walut k WHERE k.waluta = v.w);
