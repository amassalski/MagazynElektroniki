-- =====================================================================
-- DomowyMagazyn - widoki
-- Skrypt jest idempotentny (CREATE OR REPLACE). PostgreSQL 9.6+.
-- =====================================================================

-- Drzewo kategorii z pełną nazwą i liczbą elementów
CREATE OR REPLACE VIEW v_kategorie AS
SELECT
    k.id,
    k.id_rodzica,
    k.nazwa,
    k.opis,
    k.poziom,
    k.sciezka,
    (SELECT string_agg(a.nazwa, ' / ' ORDER BY a.poziom)
       FROM tbl_kategorie a
      WHERE a.sciezka @> k.sciezka) AS pelna_nazwa,
    (SELECT count(*) FROM tbl_elementy e WHERE e.id_kategorii = k.id) AS liczba_elementow,
    k.czy_aktywny
FROM tbl_kategorie k;

-- Elementy z nazwą kategorii i informacją o stanie
-- e.* zmienia się wraz ze schematem, a nowe kolumny lądują w środku widoku, więc widok trzeba odtworzyć
DROP VIEW IF EXISTS v_elementy;
CREATE VIEW v_elementy AS
SELECT
    e.*,
    k.nazwa AS kategoria,
    (SELECT string_agg(a.nazwa, ' / ' ORDER BY a.poziom)
       FROM tbl_kategorie a
      WHERE a.sciezka @> k.sciezka) AS kategoria_pelna,
    (e.ilosc_dostepna < e.ilosc_minimalna) AS niski_stan
FROM tbl_elementy e
LEFT JOIN tbl_kategorie k ON k.id = e.id_kategorii;

-- Elementy poniżej stanu minimalnego
CREATE OR REPLACE VIEW v_niskie_stany AS
SELECT
    e.id AS id_elementu,
    e.symbol,
    e.producent,
    e.ilosc_dostepna,
    e.ilosc_minimalna,
    (e.ilosc_minimalna - e.ilosc_dostepna) AS brakuje
FROM tbl_elementy e
WHERE e.czy_aktywny AND e.ilosc_dostepna < e.ilosc_minimalna
ORDER BY brakuje DESC, e.symbol;

-- Linki do sklepów dla elementów
CREATE OR REPLACE VIEW v_sklepy_elementow AS
SELECT
    e.id AS id_elementu,
    e.symbol,
    e.symbol_producenta,
    e.producent,
    s.nazwa AS nazwa_sklepu,
    s.url_bazowy,
    es.url,
    es.cena_aktualna,
    es.waluta,
    es.ostatnia_weryfikacja,
    es.status_dostepnosci,
    es.czy_domyslny
FROM tbl_elementy e
JOIN tbl_elementy_sklepy es ON e.id = es.id_elementu
JOIN tbl_sklepy s ON es.id_sklepu = s.id
WHERE e.czy_aktywny = true AND s.czy_aktywny = true
ORDER BY e.symbol, s.priorytet;

-- Ceny w sklepach wraz z rankingiem (1 = najtańszy)
CREATE OR REPLACE VIEW v_najlepsze_ceny AS
SELECT
    e.id AS id_elementu,
    e.symbol,
    e.producent,
    s.nazwa AS sklep,
    es.cena_aktualna,
    es.waluta,
    ROW_NUMBER() OVER (PARTITION BY e.id ORDER BY es.cena_aktualna ASC) AS ranking_ceny
FROM tbl_elementy e
JOIN tbl_elementy_sklepy es ON e.id = es.id_elementu
JOIN tbl_sklepy s ON es.id_sklepu = s.id
WHERE es.cena_aktualna IS NOT NULL
  AND e.czy_aktywny = true
  AND s.czy_aktywny = true;

-- Stan realizacji listy części (BOM) projektów
CREATE OR REPLACE VIEW v_bom_stan AS
SELECT
    b.id,
    b.id_projektu,
    p.nazwa AS projekt,
    b.id_elementu,
    e.symbol,
    e.producent,
    b.ilosc_wymagana,
    b.ilosc_zamowiona,
    b.ilosc_zuzyta,
    e.ilosc_dostepna,
    GREATEST(b.ilosc_wymagana - b.ilosc_zuzyta - e.ilosc_dostepna, 0) AS brakuje,
    b.priorytet,
    b.uwagi,
    b.oznaczenia
FROM tbl_bom b
JOIN tbl_projekty p ON p.id = b.id_projektu
JOIN tbl_elementy e ON e.id = b.id_elementu;
