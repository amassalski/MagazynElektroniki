-- =====================================================================
-- DomowyMagazyn - struktura bazy (tabele, indeksy)
-- Skrypt jest idempotentny: można go uruchamiać wielokrotnie.
-- Kolejność skryptów: schema.sql -> funkcje_i_trigger.sql -> widoki.sql
--                     -> dane_startowe.sql
-- Zgodność: PostgreSQL 9.6 lub nowszy.
-- Ręcznie:  psql -U postgres -d NAZWA_BAZY -f schema.sql
-- (aplikacja robi to sama: menu Baza danych -> Utwórz/aktualizuj strukturę)
-- =====================================================================

-- Rozszerzenie ltree (drzewo kategorii). Instalacja wymaga uprawnienia CREATE
-- na bazie (od PG 13 rozszerzenie jest "trusted"; wcześniej trzeba być superuserem).
-- Jeśli użytkownik aplikacji go nie ma, jednorazowo jako administrator:
--   psql -U postgres -d NAZWA_BAZY -c "CREATE EXTENSION IF NOT EXISTS ltree;"
DO $$
BEGIN
    CREATE EXTENSION IF NOT EXISTS ltree;
EXCEPTION WHEN insufficient_privilege THEN
    RAISE EXCEPTION 'Brak uprawnień do utworzenia rozszerzenia ltree. Jako administrator bazy wykonaj: CREATE EXTENSION IF NOT EXISTS ltree;  (baza: %)', current_database();
END $$;

-- Wersja struktury bazy (do przyszłych migracji)
CREATE TABLE IF NOT EXISTS public.tbl_wersja_schematu (
    wersja integer primary key,
    opis character varying(500),
    data_zastosowania timestamp default current_timestamp
);

CREATE TABLE IF NOT EXISTS public.tbl_opis_slownika (
    id bigserial primary key,
    nazwa_slownika character varying(100),
    opis character varying(4000),
    kod_slownika character varying(6),
    character_01 character varying(100),
    character_02 character varying(100),
    character_03 character varying(100),
    character_04 character varying(100),
    character_05 character varying(100),
    character_06 character varying(100),
    character_07 character varying(100),
    character_08 character varying(100),
    character_09 character varying(100),
    character_10 character varying(100),
    character_11 character varying(2000),
    character_12 character varying(2000),
    bigint_01 character varying(100),
    bigint_02 character varying(100),
    bigint_03 character varying(100),
    bigint_04 character varying(100),
    bigint_05 character varying(100),
    bigint_06 character varying(100),
    bigint_07 character varying(100),
    bigint_08 character varying(100),
    bigint_09 character varying(100),
    bigint_10 character varying(100),
    real_01 character varying(100),
    real_02 character varying(100),
    real_03 character varying(100),
    real_04 character varying(100),
    real_05 character varying(100),
    numeric_01 character varying(100),
    numeric_02 character varying(100),
    czy_aktywny integer,
    aktywny_od date,
    wersja_aktywna real,
    widoczny integer,
    uzytkownik_mod character varying(20),
    dodano time without time zone,
    uzytkownik_dod character varying(20),
    modyfikowano timestamp without time zone,
    data_01 date,
    data_02 date,
    boolean_01 boolean DEFAULT false,
    boolean_02 boolean DEFAULT false,
    boolean_03 boolean DEFAULT false,
    uwagi character varying(6000)
);


-- tabela słowników (taka sama jak wcześniej)
CREATE TABLE IF NOT EXISTS public.tbl_slownik (
    id bigserial primary key,
    id_opis bigint references tbl_opis_slownika(id) match simple on update cascade,
    character_01 character varying(100),
    character_02 character varying(100),
    character_03 character varying(100),
    character_04 character varying(2000),
    character_05 character varying(4000),
    character_06 character varying(4000),
    character_07 character varying(100),
    character_08 character varying(100),
    character_09 character varying(100),
    character_10 character varying(100),
    character_11 character varying(2000),
    character_12 character varying(2000),
    bigint_01 bigint,
    bigint_02 bigint,
    bigint_03 bigint,
    bigint_04 bigint,
    bigint_05 bigint,
    bigint_06 bigint,
    bigint_07 bigint,
    bigint_08 bigint,
    bigint_09 bigint,
    bigint_10 bigint,
    real_01 real,
    real_02 real,
    real_03 real,
    real_04 real,
    real_05 real,
    real_06 real,
    real_07 real,
    real_08 real,
    real_09 real,
    real_10 real,
    numeric_01 numeric(10,2),
    numeric_02 numeric(10,2),
    wersja real,
    aktywny integer,
    aktywny_od date,
    widoczny integer,
    uzytkownik_mod character varying(20),
    uzytkownik_dod character varying(20),
    dodano timestamp without time zone,
    modyfikowano timestamp without time zone,
    data_01 date,
    data_02 date,
    boolean_01 boolean DEFAULT false,
    boolean_02 boolean DEFAULT false,
    boolean_03 boolean DEFAULT false,
    uwagi character varying(6000)
);


-- Pozostałe tabele bez zmian
CREATE TABLE IF NOT EXISTS public.tbl_kategorie (
    id bigserial primary key,
    id_rodzica bigint references tbl_kategorie(id),
    nazwa character varying(200) not null,
    opis character varying(2000),
    sciezka ltree,
    poziom integer not null default 0,
    czy_aktywny boolean default true,
    dodano timestamp default current_timestamp,
    uzytkownik_dod character varying(50),
    modyfikowano timestamp default current_timestamp,
    uzytkownik_mod character varying(50)
);

-- Tabela sklepów (nowa, elastyczna struktura)
CREATE TABLE IF NOT EXISTS public.tbl_sklepy (
    id bigserial primary key,
    nazwa character varying(200) not null,
    opis character varying(2000),
    url_bazowy character varying(500) not null,
    logo_url character varying(500),
    kraj character varying(100),
    waluta character varying(3),
    czy_aktywny boolean default true,
    priorytet integer default 5,
    parametr_wyszukiwania character varying(100),
    czy_wspiera_api boolean default false,
    api_klucz character varying(200),
    uzytkownik_dod character varying(50),
    dodano timestamp default current_timestamp,
    modyfikowano timestamp default current_timestamp,
    uzytkownik_mod character varying(50)
);

COMMENT ON TABLE public.tbl_sklepy IS 'Tabela sklepów z częściami elektronicznymi';
COMMENT ON COLUMN public.tbl_sklepy.parametr_wyszukiwania IS 'Parametr URL do wyszukiwania (np. MPN, kod producenta)';

-- Aktualizowana tabela z elementami (bez kolumn sklepów)
CREATE TABLE IF NOT EXISTS public.tbl_elementy (
    id bigserial primary key,
    id_kategorii bigint references tbl_kategorie(id),
    symbol character varying(200) not null,
    symbol_producenta character varying(200),
    producent character varying(200),
    opis character varying(2000),
    wartosc character varying(100),
    jednostka character varying(20),
    obudowa character varying(100),
    datasheet_url character varying(500),
    zdjecie_sciezka character varying(500),
    ilosc_dostepna integer default 0 check (ilosc_dostepna >= 0),
    ilosc_minimalna integer default 0 check (ilosc_minimalna >= 0),
    cena_jednostkowa numeric(12,4),
    waluta character varying(3) default 'PLN',
    parametry_json jsonb,
    tagi character varying(1000),
    czy_aktywny boolean default true,
    data_dodania timestamp default current_timestamp,
    data_modyfikacji timestamp default current_timestamp,
    uzytkownik_dod character varying(50),
    uzytkownik_mod character varying(50)
);

-- Tabela linków do sklepów dla elementów (nowa, elastyczna)
CREATE TABLE IF NOT EXISTS public.tbl_elementy_sklepy (
    id bigserial primary key,
    id_elementu bigint references tbl_elementy(id) on delete cascade,
    id_sklepu bigint references tbl_sklepy(id),
    url character varying(1000) not null,
    cena_aktualna numeric(12,4),
    waluta character varying(3),
    ostatnia_weryfikacja timestamp,
    status_dostepnosci character varying(50),
    minimalna_ilosc integer,
    czy_domyslny boolean default false,
    uwagi character varying(2000),
    dodano timestamp default current_timestamp,
    modyfikowano timestamp default current_timestamp,
    CONSTRAINT unikalny_element_sklep UNIQUE (id_elementu, id_sklepu)
);

CREATE INDEX IF NOT EXISTS idx_tbl_elementy_sklepy_id_elementu ON public.tbl_elementy_sklepy(id_elementu);
CREATE INDEX IF NOT EXISTS idx_tbl_elementy_sklepy_id_sklepu ON public.tbl_elementy_sklepy(id_sklepu);

-- Tabela historii cen w sklepach
CREATE TABLE IF NOT EXISTS public.tbl_historia_cen_sklepy (
    id bigserial primary key,
    id_elementu_sklepu bigint references tbl_elementy_sklepy(id) on delete cascade,
    cena numeric(12,4) not null,
    waluta character varying(3),
    data_ceny timestamp default current_timestamp,
    zrodlo character varying(100),
    czy_aktualna boolean default false
);

CREATE INDEX IF NOT EXISTS idx_tbl_historia_cen_sklepy_id_elementu_sklepu ON public.tbl_historia_cen_sklepy(id_elementu_sklepu);
CREATE INDEX IF NOT EXISTS idx_tbl_historia_cen_sklepy_data_ceny ON public.tbl_historia_cen_sklepy(data_ceny);



CREATE TABLE IF NOT EXISTS public.tbl_lokalizacje (
    id bigserial primary key,
    nazwa character varying(200) not null,
    opis character varying(1000),
    sciezka ltree,
    czy_aktywna boolean default true,
    dodano timestamp default current_timestamp
);

CREATE TABLE IF NOT EXISTS public.tbl_elementy_lokalizacje (
    id_elementu bigint references tbl_elementy(id),
    id_lokalizacji bigint references tbl_lokalizacje(id),
    ilosc integer not null,
    primary key (id_elementu, id_lokalizacji)
);

CREATE TABLE IF NOT EXISTS public.tbl_projekty (
    id bigserial primary key,
    nazwa character varying(200) not null,
    opis character varying(4000),
    status character varying(50) default 'W_TRAKCIE',
    data_utworzenia timestamp default current_timestamp,
    data_modyfikacji timestamp default current_timestamp,
    uzytkownik_dod character varying(50),
    czy_aktywny boolean default true
);

CREATE TABLE IF NOT EXISTS public.tbl_bom (
    id bigserial primary key,
    id_projektu bigint references tbl_projekty(id),
    id_elementu bigint references tbl_elementy(id),
    ilosc_wymagana integer not null,
    ilosc_zamowiona integer default 0,
    ilosc_zuzyta integer default 0,
    priorytet integer default 5,
    uwagi character varying(2000),
    dodano timestamp default current_timestamp,
    modyfikowano timestamp default current_timestamp
);

CREATE TABLE IF NOT EXISTS public.tbl_transakcje (
    id bigserial primary key,
    typ_transakcji character varying(20) not null,
    id_elementu bigint references tbl_elementy(id),
    id_projektu bigint references tbl_projekty(id),
    id_lokalizacji bigint references tbl_lokalizacje(id),
    ilosc_zmiana integer not null,
    ilosc_przed integer,
    ilosc_po integer,
    cena_jednostkowa numeric(12,4),
    data_transakcji timestamp default current_timestamp,
    uzytkownik character varying(50),
    komentarz character varying(2000)
);

-- Nowa tabela dla zamienników elementów
CREATE TABLE IF NOT EXISTS public.tbl_zamienniki (
    id_elementu_podstawowego bigint references tbl_elementy(id),
    id_elementu_zamiennego bigint references tbl_elementy(id),
    stopien_zgodnosci integer not null default 100,
    uwagi character varying(2000),
    data_dodania timestamp default current_timestamp,
    dodano_przez character varying(50),
    primary key (id_elementu_podstawowego, id_elementu_zamiennego),
    CHECK (id_elementu_podstawowego <> id_elementu_zamiennego)
);

COMMENT ON TABLE public.tbl_zamienniki IS 'Tabela przechowująca informacje o zamiennikach elementów';
COMMENT ON COLUMN public.tbl_zamienniki.stopien_zgodnosci IS 'Stopień zgodności w procentach (0-100)';

-- Tabela dla zdjęć elementów (wielość zdjęć)
CREATE TABLE IF NOT EXISTS public.tbl_zdjecia_elementow (
    id bigserial primary key,
    id_elementu bigint references tbl_elementy(id) on delete cascade,
    sciezka_pliku character varying(500) not null,
    opis character varying(1000),
    czy_glowne boolean default false,
    data_dodania timestamp default current_timestamp,
    dodano_przez character varying(50)
);

CREATE INDEX IF NOT EXISTS idx_tbl_zdjecia_elementow_id_elementu ON public.tbl_zdjecia_elementow(id_elementu);

-- Dodatkowe indeksy i ograniczenia ------------------------------------
CREATE UNIQUE INDEX IF NOT EXISTS uq_tbl_kategorie_rodzic_nazwa
    ON public.tbl_kategorie (COALESCE(id_rodzica, 0), lower(nazwa));
CREATE INDEX IF NOT EXISTS idx_tbl_kategorie_sciezka
    ON public.tbl_kategorie USING gist (sciezka);
CREATE INDEX IF NOT EXISTS idx_tbl_kategorie_id_rodzica
    ON public.tbl_kategorie (id_rodzica);

CREATE UNIQUE INDEX IF NOT EXISTS uq_tbl_sklepy_nazwa
    ON public.tbl_sklepy (lower(nazwa));

CREATE INDEX IF NOT EXISTS idx_tbl_elementy_id_kategorii
    ON public.tbl_elementy (id_kategorii);
CREATE INDEX IF NOT EXISTS idx_tbl_elementy_symbol
    ON public.tbl_elementy (lower(symbol));
CREATE INDEX IF NOT EXISTS idx_tbl_elementy_symbol_producenta
    ON public.tbl_elementy (lower(symbol_producenta));

CREATE INDEX IF NOT EXISTS idx_tbl_bom_id_projektu ON public.tbl_bom (id_projektu);
CREATE INDEX IF NOT EXISTS idx_tbl_bom_id_elementu ON public.tbl_bom (id_elementu);

CREATE INDEX IF NOT EXISTS idx_tbl_transakcje_id_elementu ON public.tbl_transakcje (id_elementu);
CREATE INDEX IF NOT EXISTS idx_tbl_transakcje_data ON public.tbl_transakcje (data_transakcji);

CREATE INDEX IF NOT EXISTS idx_tbl_elementy_lokalizacje_lok
    ON public.tbl_elementy_lokalizacje (id_lokalizacji);

-- Wersja 2: oznaczenia części (np. "R1, R2, R5") w pozycjach listy części projektu
ALTER TABLE public.tbl_bom ADD COLUMN IF NOT EXISTS oznaczenia character varying(2000);

INSERT INTO public.tbl_wersja_schematu (wersja, opis)
SELECT 1, 'Struktura początkowa'
WHERE NOT EXISTS (SELECT 1 FROM public.tbl_wersja_schematu WHERE wersja = 1);

INSERT INTO public.tbl_wersja_schematu (wersja, opis)
SELECT 2, 'Projekty: oznaczenia części w liście BOM'
WHERE NOT EXISTS (SELECT 1 FROM public.tbl_wersja_schematu WHERE wersja = 2);

-- ---------------------------------------------------------------------
-- Wersja 3: sklepy (ceny, dostawa), linki elementów, kursy walut
-- ---------------------------------------------------------------------
ALTER TABLE public.tbl_sklepy ADD COLUMN IF NOT EXISTS szablon_wyszukiwania character varying(500);
ALTER TABLE public.tbl_sklepy ADD COLUMN IF NOT EXISTS koszt_dostawy numeric(10,2) DEFAULT 0;
ALTER TABLE public.tbl_sklepy ADD COLUMN IF NOT EXISTS darmowa_dostawa_od numeric(10,2);
ALTER TABLE public.tbl_sklepy ADD COLUMN IF NOT EXISTS regex_ceny character varying(500);
ALTER TABLE public.tbl_elementy_sklepy ADD COLUMN IF NOT EXISTS kod_w_sklepie character varying(100);

COMMENT ON COLUMN public.tbl_sklepy.szablon_wyszukiwania IS 'Adres wyszukiwania z {q} w miejscu szukanej frazy';
COMMENT ON COLUMN public.tbl_sklepy.koszt_dostawy IS 'Koszt dostawy w walucie sklepu';
COMMENT ON COLUMN public.tbl_sklepy.darmowa_dostawa_od IS 'Wartość zamówienia, od której dostawa jest darmowa (NULL = nigdy)';
COMMENT ON COLUMN public.tbl_sklepy.regex_ceny IS 'Własne wyrażenie regularne wyciągające cenę ze strony (pierwsza grupa)';

CREATE TABLE IF NOT EXISTS public.tbl_kursy_walut (
    waluta character varying(3) primary key,
    kurs_pln numeric(12,4) not null check (kurs_pln > 0),
    zmieniono timestamp default current_timestamp
);

INSERT INTO public.tbl_wersja_schematu (wersja, opis)
SELECT 3, 'Sklepy: szablon wyszukiwania, dostawa, kursy walut'
WHERE NOT EXISTS (SELECT 1 FROM public.tbl_wersja_schematu WHERE wersja = 3);

-- ---------------------------------------------------------------------
-- Wersja 4: opisy pól formularzy (pomoc kontekstowa F1 i dymki)
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS public.tbl_pomoc_pol (
    klucz character varying(80) primary key,
    tytul character varying(200) not null,
    opis text not null,
    kotwica character varying(80),
    zmienione boolean not null default false,
    zmieniono timestamp default current_timestamp
);
COMMENT ON TABLE public.tbl_pomoc_pol IS 'Opisy pól formularzy; zmienione = true chroni opis przed nadpisaniem przy aktualizacji';

INSERT INTO public.tbl_wersja_schematu (wersja, opis)
SELECT 4, 'Pomoc kontekstowa: tabela opisów pól'
WHERE NOT EXISTS (SELECT 1 FROM public.tbl_wersja_schematu WHERE wersja = 4);

-- ---------------------------------------------------------------------
-- Wersja 5: miejsce magazynowania elementu (kontener, szuflada, przegroda)
-- ---------------------------------------------------------------------
ALTER TABLE public.tbl_elementy ADD COLUMN IF NOT EXISTS kontener character varying(100);
ALTER TABLE public.tbl_elementy ADD COLUMN IF NOT EXISTS szuflada character varying(50);
ALTER TABLE public.tbl_elementy ADD COLUMN IF NOT EXISTS przegroda character varying(50);
ALTER TABLE public.tbl_elementy ADD COLUMN IF NOT EXISTS tolerancja character varying(20);
ALTER TABLE public.tbl_elementy ADD COLUMN IF NOT EXISTS parametry character varying(300);
COMMENT ON COLUMN public.tbl_elementy.tolerancja IS 'Tolerancja wartości, np. "1%"';
COMMENT ON COLUMN public.tbl_elementy.parametry IS 'Pozostałe parametry techniczne, np. "0,1 W; 50 V; X7R"';
COMMENT ON COLUMN public.tbl_elementy.kontener IS 'Kontener / szafka, np. "Kontener 1"';
COMMENT ON COLUMN public.tbl_elementy.szuflada IS 'Oznaczenie szuflady, np. "A1"';
COMMENT ON COLUMN public.tbl_elementy.przegroda IS 'Przegroda w szufladzie (opcjonalnie), np. "1"';
CREATE INDEX IF NOT EXISTS idx_tbl_elementy_miejsce ON public.tbl_elementy (kontener, szuflada, przegroda);

INSERT INTO public.tbl_wersja_schematu (wersja, opis)
SELECT 5, 'Miejsce magazynowania, tolerancja i parametry elementu'
WHERE NOT EXISTS (SELECT 1 FROM public.tbl_wersja_schematu WHERE wersja = 5);

-- ---------------------------------------------------------------------
-- Wersja 6: katalog elementów (wzorce z parametrami i datasheetem)
-- Powiązanie z magazynem jest liczone po symbolu (lub symbolu producenta) elementu.
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS public.tbl_katalog (
    id bigserial primary key,
    symbol character varying(120) not null,
    producent character varying(150) not null default '',
    opis character varying(1000),
    obudowa character varying(100),
    rodzina character varying(150),
    datasheet_url character varying(500),
    datasheet_plik character varying(500),
    uwagi text,
    dodano timestamp default current_timestamp,
    zmieniono timestamp default current_timestamp
);
CREATE UNIQUE INDEX IF NOT EXISTS uq_tbl_katalog_symbol_producent
    ON public.tbl_katalog (lower(symbol), lower(producent));
CREATE INDEX IF NOT EXISTS idx_tbl_katalog_rodzina ON public.tbl_katalog (lower(rodzina));

CREATE TABLE IF NOT EXISTS public.tbl_katalog_parametry (
    id bigserial primary key,
    id_katalogu bigint not null references public.tbl_katalog(id) on delete cascade,
    nazwa character varying(100) not null,
    wartosc character varying(300) not null,
    zrodlo character varying(20) not null default 'reczny',
    kolejnosc integer not null default 0
);
CREATE UNIQUE INDEX IF NOT EXISTS uq_tbl_katalog_parametry_nazwa
    ON public.tbl_katalog_parametry (id_katalogu, lower(nazwa));

COMMENT ON TABLE public.tbl_katalog IS 'Katalog elementów: wzorce z opisem, parametrami i datasheetem';
COMMENT ON COLUMN public.tbl_katalog.rodzina IS 'Swobodna nazwa grupy, np. "Wzmacniacze operacyjne"';
COMMENT ON COLUMN public.tbl_katalog_parametry.zrodlo IS 'reczny, csv albo datasheet (odczytane z karty katalogowej)';

INSERT INTO public.tbl_wersja_schematu (wersja, opis)
SELECT 6, 'Katalog elementów z parametrami'
WHERE NOT EXISTS (SELECT 1 FROM public.tbl_wersja_schematu WHERE wersja = 6);
