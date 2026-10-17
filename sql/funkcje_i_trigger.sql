-- =====================================================================
-- DomowyMagazyn - funkcje i triggery
-- Skrypt jest idempotentny. Zgodność: PostgreSQL 9.6 lub nowszy.
-- =====================================================================

-- ---------------------------------------------------------------------
-- Pomocnicze
-- ---------------------------------------------------------------------

-- Kodowanie procentowe tekstu do użycia w adresach URL (UTF-8)
CREATE OR REPLACE FUNCTION fn_url_encode(p_tekst text)
RETURNS text AS $$
DECLARE
    v_bajty bytea;
    v_wynik text := '';
    v_bajt integer;
    i integer;
BEGIN
    IF p_tekst IS NULL THEN
        RETURN NULL;
    END IF;
    v_bajty := convert_to(p_tekst, 'UTF8');
    FOR i IN 0 .. length(v_bajty) - 1 LOOP
        v_bajt := get_byte(v_bajty, i);
        IF (v_bajt BETWEEN 48 AND 57) OR (v_bajt BETWEEN 65 AND 90)
           OR (v_bajt BETWEEN 97 AND 122) OR v_bajt IN (45, 46, 95, 126) THEN
            v_wynik := v_wynik || chr(v_bajt);
        ELSE
            v_wynik := v_wynik || '%' || upper(lpad(to_hex(v_bajt), 2, '0'));
        END IF;
    END LOOP;
    RETURN v_wynik;
END;
$$ LANGUAGE plpgsql IMMUTABLE;

-- Ustawia pole "modyfikowano" (tabele z taką kolumną)
CREATE OR REPLACE FUNCTION fn_ts_modyfikowano()
RETURNS TRIGGER AS $$
BEGIN
    NEW.modyfikowano := CURRENT_TIMESTAMP;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- Ustawia pole "data_modyfikacji" (tabele z taką kolumną)
CREATE OR REPLACE FUNCTION fn_ts_data_modyfikacji()
RETURNS TRIGGER AS $$
BEGIN
    NEW.data_modyfikacji := CURRENT_TIMESTAMP;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

DROP TRIGGER IF EXISTS trigger_aktualizuj_date_modyfikacji_sklepu ON tbl_sklepy;
CREATE TRIGGER trigger_aktualizuj_date_modyfikacji_sklepu
    BEFORE UPDATE ON tbl_sklepy
    FOR EACH ROW EXECUTE PROCEDURE fn_ts_modyfikowano();

DROP TRIGGER IF EXISTS trigger_aktualizuj_date_modyfikacji_element_sklep ON tbl_elementy_sklepy;
CREATE TRIGGER trigger_aktualizuj_date_modyfikacji_element_sklep
    BEFORE UPDATE ON tbl_elementy_sklepy
    FOR EACH ROW EXECUTE PROCEDURE fn_ts_modyfikowano();

DROP TRIGGER IF EXISTS trigger_ts_kategorie ON tbl_kategorie;
CREATE TRIGGER trigger_ts_kategorie
    BEFORE UPDATE ON tbl_kategorie
    FOR EACH ROW EXECUTE PROCEDURE fn_ts_modyfikowano();

DROP TRIGGER IF EXISTS trigger_ts_bom ON tbl_bom;
CREATE TRIGGER trigger_ts_bom
    BEFORE UPDATE ON tbl_bom
    FOR EACH ROW EXECUTE PROCEDURE fn_ts_modyfikowano();

DROP TRIGGER IF EXISTS trigger_ts_elementy ON tbl_elementy;
CREATE TRIGGER trigger_ts_elementy
    BEFORE UPDATE ON tbl_elementy
    FOR EACH ROW EXECUTE PROCEDURE fn_ts_data_modyfikacji();

DROP TRIGGER IF EXISTS trigger_ts_projekty ON tbl_projekty;
CREATE TRIGGER trigger_ts_projekty
    BEFORE UPDATE ON tbl_projekty
    FOR EACH ROW EXECUTE PROCEDURE fn_ts_data_modyfikacji();

-- ---------------------------------------------------------------------
-- Kategorie: ścieżka ltree i poziom liczone automatycznie
-- ---------------------------------------------------------------------

CREATE OR REPLACE FUNCTION fn_kategorie_sciezka()
RETURNS TRIGGER AS $$
DECLARE
    v_sciezka_rodzica ltree;
    v_poziom_rodzica integer;
BEGIN
    IF NEW.id_rodzica IS NULL THEN
        NEW.poziom := 0;
        NEW.sciezka := text2ltree('k' || NEW.id);
        RETURN NEW;
    END IF;

    IF NEW.id_rodzica = NEW.id THEN
        RAISE EXCEPTION 'Kategoria nie może być swoim własnym rodzicem';
    END IF;

    SELECT sciezka, poziom INTO v_sciezka_rodzica, v_poziom_rodzica
    FROM tbl_kategorie WHERE id = NEW.id_rodzica;

    IF NOT FOUND THEN
        RAISE EXCEPTION 'Nie istnieje kategoria nadrzędna o id %', NEW.id_rodzica;
    END IF;

    -- nie wolno przenieść kategorii do jej własnego poddrzewa
    IF TG_OP = 'UPDATE' AND OLD.sciezka IS NOT NULL
       AND v_sciezka_rodzica <@ OLD.sciezka THEN
        RAISE EXCEPTION 'Nie można przenieść kategorii do jej własnej podkategorii';
    END IF;

    NEW.poziom := v_poziom_rodzica + 1;
    NEW.sciezka := v_sciezka_rodzica || text2ltree('k' || NEW.id);
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION fn_kategorie_potomkowie()
RETURNS TRIGGER AS $$
BEGIN
    IF OLD.sciezka IS NOT NULL AND OLD.sciezka IS DISTINCT FROM NEW.sciezka THEN
        UPDATE tbl_kategorie
        SET sciezka = NEW.sciezka || subpath(sciezka, nlevel(OLD.sciezka)),
            poziom  = poziom + (NEW.poziom - OLD.poziom)
        WHERE sciezka <@ OLD.sciezka AND id <> NEW.id;
    END IF;
    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

DROP TRIGGER IF EXISTS trigger_kategorie_sciezka ON tbl_kategorie;
CREATE TRIGGER trigger_kategorie_sciezka
    BEFORE INSERT OR UPDATE OF id_rodzica ON tbl_kategorie
    FOR EACH ROW EXECUTE PROCEDURE fn_kategorie_sciezka();

DROP TRIGGER IF EXISTS trigger_kategorie_potomkowie ON tbl_kategorie;
CREATE TRIGGER trigger_kategorie_potomkowie
    AFTER UPDATE OF id_rodzica ON tbl_kategorie
    FOR EACH ROW EXECUTE PROCEDURE fn_kategorie_potomkowie();

-- Dodaje kategorię, jeśli jeszcze nie istnieje; zwraca jej id
CREATE OR REPLACE FUNCTION dodaj_kategorie_jesli_brak(
    p_id_rodzica bigint,
    p_nazwa character varying,
    p_opis character varying DEFAULT NULL
) RETURNS bigint AS $$
DECLARE
    v_id bigint;
BEGIN
    SELECT id INTO v_id FROM tbl_kategorie
    WHERE COALESCE(id_rodzica, 0) = COALESCE(p_id_rodzica, 0)
      AND lower(nazwa) = lower(p_nazwa);
    IF v_id IS NULL THEN
        INSERT INTO tbl_kategorie (id_rodzica, nazwa, opis)
        VALUES (p_id_rodzica, p_nazwa, p_opis)
        RETURNING id INTO v_id;
    END IF;
    RETURN v_id;
END;
$$ LANGUAGE plpgsql;

-- ---------------------------------------------------------------------
-- Linki do sklepów
-- ---------------------------------------------------------------------

-- Buduje adres wyszukiwania elementu w sklepie
CREATE OR REPLACE FUNCTION utworz_link_sklepu(
    p_id_sklepu bigint,
    p_kod_producenta character varying,
    p_symbol character varying
) RETURNS character varying AS $$
DECLARE
    v_url_bazowy character varying;
    v_parametr character varying;
    v_szablon character varying;
    v_kod character varying;
BEGIN
    SELECT url_bazowy, COALESCE(NULLIF(parametr_wyszukiwania, ''), '{KOD}'), NULLIF(szablon_wyszukiwania, '')
    INTO v_url_bazowy, v_parametr, v_szablon
    FROM tbl_sklepy
    WHERE id = p_id_sklepu AND czy_aktywny = true;

    IF v_url_bazowy IS NULL THEN
        RETURN NULL;
    END IF;

    v_kod := COALESCE(NULLIF(p_kod_producenta, ''), NULLIF(p_symbol, ''));
    IF v_kod IS NULL THEN
        RETURN v_url_bazowy;
    END IF;

    -- nowy sposób (schemat 3): pełny adres wyszukiwania z {q}
    IF v_szablon IS NOT NULL THEN
        RETURN replace(v_szablon, '{q}', fn_url_encode(v_kod));
    END IF;
    RETURN v_url_bazowy || replace(v_parametr, '{KOD}', fn_url_encode(v_kod));
END;
$$ LANGUAGE plpgsql;

-- Po dodaniu elementu tworzy linki do najważniejszych sklepów (maks. 5)
CREATE OR REPLACE FUNCTION tworz_domyslne_linki_sklepow()
RETURNS TRIGGER AS $$
DECLARE
    v_sklep RECORD;
    v_url character varying;
BEGIN
    FOR v_sklep IN SELECT id FROM tbl_sklepy
                   WHERE czy_aktywny = true
                   ORDER BY priorytet, id
                   LIMIT 5
    LOOP
        v_url := utworz_link_sklepu(v_sklep.id, NEW.symbol_producenta, NEW.symbol);
        IF v_url IS NOT NULL THEN
            INSERT INTO tbl_elementy_sklepy (id_elementu, id_sklepu, url, czy_domyslny)
            VALUES (NEW.id, v_sklep.id, v_url, false)
            ON CONFLICT (id_elementu, id_sklepu) DO NOTHING;
        END IF;
    END LOOP;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

DROP TRIGGER IF EXISTS trigger_tworz_domyslne_linki_sklepow ON tbl_elementy;
CREATE TRIGGER trigger_tworz_domyslne_linki_sklepow
    AFTER INSERT ON tbl_elementy
    FOR EACH ROW EXECUTE PROCEDURE tworz_domyslne_linki_sklepow();

-- Najwyżej jeden domyślny link sklepu na element
CREATE OR REPLACE FUNCTION fn_jeden_domyslny_sklep()
RETURNS TRIGGER AS $$
BEGIN
    IF NEW.czy_domyslny THEN
        UPDATE tbl_elementy_sklepy SET czy_domyslny = false
        WHERE id_elementu = NEW.id_elementu AND id <> NEW.id AND czy_domyslny;
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

DROP TRIGGER IF EXISTS trigger_jeden_domyslny_sklep ON tbl_elementy_sklepy;
CREATE TRIGGER trigger_jeden_domyslny_sklep
    AFTER INSERT OR UPDATE OF czy_domyslny ON tbl_elementy_sklepy
    FOR EACH ROW WHEN (NEW.czy_domyslny)
    EXECUTE PROCEDURE fn_jeden_domyslny_sklep();

-- Historia cen: każda zmiana ceny aktualnej trafia do tbl_historia_cen_sklepy
CREATE OR REPLACE FUNCTION fn_historia_cen()
RETURNS TRIGGER AS $$
BEGIN
    IF NEW.cena_aktualna IS NOT NULL
       AND (TG_OP = 'INSERT' OR OLD.cena_aktualna IS DISTINCT FROM NEW.cena_aktualna) THEN
        UPDATE tbl_historia_cen_sklepy SET czy_aktualna = false
        WHERE id_elementu_sklepu = NEW.id AND czy_aktualna;

        INSERT INTO tbl_historia_cen_sklepy
            (id_elementu_sklepu, cena, waluta, data_ceny, zrodlo, czy_aktualna)
        VALUES (NEW.id, NEW.cena_aktualna, NEW.waluta, CURRENT_TIMESTAMP,
                COALESCE(NULLIF(current_setting('dm.zrodlo_ceny', true), ''), 'trigger'), true);
    END IF;
    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

DROP TRIGGER IF EXISTS trigger_historia_cen ON tbl_elementy_sklepy;
CREATE TRIGGER trigger_historia_cen
    AFTER INSERT OR UPDATE OF cena_aktualna ON tbl_elementy_sklepy
    FOR EACH ROW EXECUTE PROCEDURE fn_historia_cen();

-- ---------------------------------------------------------------------
-- Stany magazynowe
-- ---------------------------------------------------------------------

-- Zmienia ilość elementu i zapisuje transakcję. Zwraca id transakcji.
-- p_zmiana > 0 przyjęcie, < 0 wydanie. Nie pozwala zejść poniżej zera.
CREATE OR REPLACE FUNCTION zmien_ilosc_elementu(
    p_id_elementu bigint,
    p_zmiana integer,
    p_typ character varying DEFAULT 'KOREKTA',
    p_id_projektu bigint DEFAULT NULL,
    p_id_lokalizacji bigint DEFAULT NULL,
    p_komentarz character varying DEFAULT NULL,
    p_uzytkownik character varying DEFAULT NULL,
    p_cena numeric DEFAULT NULL
) RETURNS bigint AS $$
DECLARE
    v_przed integer;
    v_po integer;
    v_ilosc_lok integer;
    v_id_transakcji bigint;
BEGIN
    SELECT ilosc_dostepna INTO v_przed
    FROM tbl_elementy WHERE id = p_id_elementu FOR UPDATE;

    IF NOT FOUND THEN
        RAISE EXCEPTION 'Nie istnieje element o id %', p_id_elementu;
    END IF;

    v_po := COALESCE(v_przed, 0) + p_zmiana;
    IF v_po < 0 THEN
        RAISE EXCEPTION 'Niewystarczająca ilość elementu % (jest %, wymagane %)',
            p_id_elementu, COALESCE(v_przed, 0), -p_zmiana;
    END IF;

    UPDATE tbl_elementy SET ilosc_dostepna = v_po WHERE id = p_id_elementu;

    IF p_id_lokalizacji IS NOT NULL THEN
        SELECT ilosc INTO v_ilosc_lok FROM tbl_elementy_lokalizacje
        WHERE id_elementu = p_id_elementu AND id_lokalizacji = p_id_lokalizacji
        FOR UPDATE;

        IF NOT FOUND THEN
            v_ilosc_lok := 0;
        END IF;
        IF v_ilosc_lok + p_zmiana < 0 THEN
            RAISE EXCEPTION 'Niewystarczająca ilość w lokalizacji % (jest %, wymagane %)',
                p_id_lokalizacji, v_ilosc_lok, -p_zmiana;
        END IF;

        INSERT INTO tbl_elementy_lokalizacje (id_elementu, id_lokalizacji, ilosc)
        VALUES (p_id_elementu, p_id_lokalizacji, v_ilosc_lok + p_zmiana)
        ON CONFLICT (id_elementu, id_lokalizacji)
        DO UPDATE SET ilosc = EXCLUDED.ilosc;
    END IF;

    INSERT INTO tbl_transakcje
        (typ_transakcji, id_elementu, id_projektu, id_lokalizacji,
         ilosc_zmiana, ilosc_przed, ilosc_po, cena_jednostkowa, uzytkownik, komentarz)
    VALUES
        (upper(p_typ), p_id_elementu, p_id_projektu, p_id_lokalizacji,
         p_zmiana, COALESCE(v_przed, 0), v_po, p_cena, p_uzytkownik, p_komentarz)
    RETURNING id INTO v_id_transakcji;

    RETURN v_id_transakcji;
END;
$$ LANGUAGE plpgsql;

-- ---------------------------------------------------------------------
-- Wyszukiwanie
-- ---------------------------------------------------------------------

-- Usuwa znaki diakrytyczne (polskie i popularne europejskie). Własna funkcja,
-- dzięki której nie jest potrzebne rozszerzenie unaccent (wymaga uprawnień).
CREATE OR REPLACE FUNCTION dm_unaccent(p_tekst text) RETURNS text AS $$
    SELECT translate(p_tekst,
        'ąćęłńóśźżĄĆĘŁŃÓŚŹŻäöüßÄÖÜáàâãéèêëíìîïñôõúùûýÿçÁÀÂÃÉÈÊËÍÌÎÏÑÔÕÚÙÛÝÇ',
        'acelnoszzACELNOSZZaousAOUaaaaeeeeiiiinoouuuyycAAAAEEEEIIIINOOUUUYC');
$$ LANGUAGE sql IMMUTABLE;

-- Wyszukuje elementy po fragmentach tekstu (bez rozróżniania wielkości liter
-- i polskich znaków). Każde słowo musi wystąpić w którymś z pól opisowych.
CREATE OR REPLACE FUNCTION szukaj_elementow(
    p_tekst text,
    p_limit integer DEFAULT 500
) RETURNS SETOF tbl_elementy AS $$
    SELECT e.*
    FROM tbl_elementy e
    WHERE e.czy_aktywny
      AND NOT EXISTS (
          SELECT 1
          FROM unnest(regexp_split_to_array(trim(dm_unaccent(lower(COALESCE(p_tekst, '')))), '\s+')) AS s(slowo)
          WHERE s.slowo <> ''
            AND position(s.slowo IN dm_unaccent(lower(concat_ws(' ',
                    e.symbol, e.symbol_producenta, e.producent, e.opis,
                    e.wartosc, e.obudowa, e.tagi, e.tolerancja, e.parametry, e.kontener, e.szuflada, e.przegroda,
                    e.szuflada || '/' || e.przegroda, e.kontener || ' ' || e.szuflada)))) = 0
      )
    ORDER BY e.symbol
    LIMIT p_limit;
$$ LANGUAGE sql STABLE;

-- ---------------------------------------------------------------------
-- Pomoc kontekstowa: ręczna zmiana opisu chroni go przed nadpisaniem.
-- Import z pomoc_pola.sql ustawia dm.import_pomocy = 'on' i wtedy flaga się nie zmienia.
-- ---------------------------------------------------------------------
CREATE OR REPLACE FUNCTION public.fn_pomoc_pol_zmiana() RETURNS trigger AS $$
BEGIN
    IF COALESCE(current_setting('dm.import_pomocy', true), '') <> 'on' THEN
        NEW.zmienione := true;
    END IF;
    NEW.zmieniono := current_timestamp;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

DROP TRIGGER IF EXISTS trg_pomoc_pol_zmiana ON public.tbl_pomoc_pol;
CREATE TRIGGER trg_pomoc_pol_zmiana BEFORE UPDATE ON public.tbl_pomoc_pol
    FOR EACH ROW EXECUTE PROCEDURE public.fn_pomoc_pol_zmiana();
