#include "LogikaMagazynu.h"

#include <algorithm>
#include <map>
#include <sstream>

namespace {

std::string przytnij(const std::string& s) {
    const char* spacje = " \t\r\n";
    const size_t a = s.find_first_not_of(spacje);
    if (a == std::string::npos) return std::string();
    const size_t b = s.find_last_not_of(spacje);
    return s.substr(a, b - a + 1);
}

/// Pusty tekst -> NULL
Parametr tekstLubNull(const std::string& s) {
    const std::string t = przytnij(s);
    if (t.empty()) return std::nullopt;
    return t;
}

Parametr liczbaParametr(long long v) { return std::to_string(v); }

Parametr idParametr(const std::optional<long long>& id) {
    if (!id) return std::nullopt;
    return std::to_string(*id);
}

const char* KOLUMNY_ELEMENTU =
    "e.id, e.id_kategorii, e.symbol, e.symbol_producenta, e.producent, e.opis, e.wartosc, "
    "e.jednostka, e.obudowa, e.datasheet_url, e.zdjecie_sciezka, e.ilosc_dostepna, "
    "e.ilosc_minimalna, e.cena_jednostkowa, e.waluta, e.tagi, e.kontener, e.szuflada, e.przegroda, e.tolerancja, e.parametry, e.czy_aktywny";

Element elementZWiersza(const WynikSQL& w, int i) {
    Element e;
    e.id = w.liczba(i, w.indeksKolumny("id"));
    const int kat = w.indeksKolumny("id_kategorii");
    if (!w.czyNull(i, kat)) e.idKategorii = w.liczba(i, kat);
    e.symbol = w.tekst(i, "symbol");
    e.symbolProducenta = w.tekst(i, "symbol_producenta");
    e.producent = w.tekst(i, "producent");
    e.opis = w.tekst(i, "opis");
    e.wartosc = w.tekst(i, "wartosc");
    e.jednostka = w.tekst(i, "jednostka");
    e.obudowa = w.tekst(i, "obudowa");
    e.datasheetUrl = w.tekst(i, "datasheet_url");
    e.zdjecieSciezka = w.tekst(i, "zdjecie_sciezka");
    e.iloscDostepna = static_cast<int>(w.liczba(i, w.indeksKolumny("ilosc_dostepna")));
    e.iloscMinimalna = static_cast<int>(w.liczba(i, w.indeksKolumny("ilosc_minimalna")));
    const int cena = w.indeksKolumny("cena_jednostkowa");
    if (!w.czyNull(i, cena)) e.cena = w.rzeczywista(i, cena);
    e.waluta = w.tekst(i, "waluta");
    e.tagi = w.tekst(i, "tagi");
    e.kontener = w.tekst(i, "kontener");
    e.tolerancja = w.tekst(i, "tolerancja");
    e.parametry = w.tekst(i, "parametry");
    e.szuflada = w.tekst(i, "szuflada");
    e.przegroda = w.tekst(i, "przegroda");
    e.aktywny = w.logiczna(i, w.indeksKolumny("czy_aktywny"), true);
    return e;
}

} // namespace

// ---------------------------------------------------------------------------

LogikaMagazynu::LogikaMagazynu(ManagerBazyDanych& manager) : manager_(manager) {}

PolaczenieBazy* LogikaMagazynu::polaczenie() {
    PolaczenieBazy* pol = manager_.pobierzPolaczenie();
    if (!pol || !pol->czyPolaczony()) {
        blad_ = "Brak połączenia z bazą danych.";
        return nullptr;
    }
    return pol;
}

bool LogikaMagazynu::ustawBlad(const std::string& blad) {
    blad_ = blad;
    return false;
}

bool LogikaMagazynu::ustawBladBazy(PolaczenieBazy* pol, const std::string& zaZnaczeniem) {
    const std::string blad = pol->pobierzBlad();
    if (blad.find("uq_tbl_kategorie_rodzic_nazwa") != std::string::npos) {
        blad_ = "Kategoria o takiej nazwie już istnieje w tym miejscu.";
    } else if (!zaZnaczeniem.empty()) {
        blad_ = zaZnaczeniem + "\n" + blad;
    } else {
        blad_ = blad;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Kategorie
// ---------------------------------------------------------------------------

bool LogikaMagazynu::odswiezKategorie() {
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return false;

    auto wynik = pol->zapytanie(
        "SELECT k.id, k.id_rodzica, k.nazwa, k.opis, k.poziom, COALESCE(c.n, 0) AS n "
        "FROM tbl_kategorie k "
        "LEFT JOIN (SELECT id_kategorii, count(*) AS n FROM tbl_elementy "
        "           WHERE czy_aktywny GROUP BY id_kategorii) c ON c.id_kategorii = k.id "
        "ORDER BY dm_unaccent(lower(k.nazwa)), k.id");
    if (!wynik) return ustawBladBazy(pol, "Nie udało się pobrać kategorii.");

    std::vector<Kategoria> wszystkie;
    std::map<long long, size_t> indeks;
    for (int i = 0; i < wynik->liczbaWierszy(); ++i) {
        Kategoria k;
        k.id = wynik->liczba(i, 0);
        if (!wynik->czyNull(i, 1)) k.idRodzica = wynik->liczba(i, 1);
        k.nazwa = wynik->tekst(i, 2);
        k.opis = wynik->tekst(i, 3);
        k.poziom = static_cast<int>(wynik->liczba(i, 4));
        k.liczbaElementow = static_cast<int>(wynik->liczba(i, 5));
        k.liczbaElementowRazem = k.liczbaElementow;
        indeks[k.id] = wszystkie.size();
        wszystkie.push_back(std::move(k));
    }

    // Dzieci każdego węzła (kolejność z ORDER BY zostaje); osierocone traktujemy jak główne
    std::vector<std::vector<size_t>> dzieci(wszystkie.size());
    std::vector<size_t> korzenie;
    for (size_t i = 0; i < wszystkie.size(); ++i) {
        const auto& rodzic = wszystkie[i].idRodzica;
        auto it = rodzic ? indeks.find(*rodzic) : indeks.end();
        if (it == indeks.end()) korzenie.push_back(i);
        else dzieci[it->second].push_back(i);
    }

    // Układ drzewa (głębia), pełne nazwy i sumy elementów
    std::vector<Kategoria> uporzadkowane;
    uporzadkowane.reserve(wszystkie.size());
    // Rekurencja ręczna na stosie, żeby uniknąć głębokiej rekurencji
    struct Ramka { size_t idx; size_t nastepneDziecko; size_t pozycja; };
    for (size_t korzen : korzenie) {
        std::vector<Ramka> stos;
        auto wejdz = [&](size_t idx, const std::string& prefiks, int poziom) {
            Kategoria k = wszystkie[idx];
            k.poziom = poziom;
            k.pelnaNazwa = prefiks.empty() ? k.nazwa : prefiks + " / " + k.nazwa;
            stos.push_back({idx, 0, uporzadkowane.size()});
            uporzadkowane.push_back(std::move(k));
        };
        wejdz(korzen, "", 0);
        while (!stos.empty()) {
            Ramka& r = stos.back();
            if (r.nastepneDziecko < dzieci[r.idx].size()) {
                const size_t dziecko = dzieci[r.idx][r.nastepneDziecko++];
                const std::string prefiks = uporzadkowane[r.pozycja].pelnaNazwa;
                const int poziom = uporzadkowane[r.pozycja].poziom + 1;
                wejdz(dziecko, prefiks, poziom);
            } else {
                const int suma = uporzadkowane[r.pozycja].liczbaElementowRazem;
                stos.pop_back();
                if (!stos.empty()) uporzadkowane[stos.back().pozycja].liczbaElementowRazem += suma;
            }
        }
    }

    kategorie_ = std::move(uporzadkowane);
    return true;
}

const Kategoria* LogikaMagazynu::znajdzKategorie(long long id) const {
    for (const Kategoria& k : kategorie_) {
        if (k.id == id) return &k;
    }
    return nullptr;
}

std::string LogikaMagazynu::pelnaNazwaKategorii(long long id) const {
    const Kategoria* k = znajdzKategorie(id);
    return k ? k->pelnaNazwa : std::string();
}

std::optional<long long> LogikaMagazynu::dodajKategorie(std::optional<long long> idRodzica,
                                                         const std::string& nazwa,
                                                         const std::string& opis) {
    const std::string n = przytnij(nazwa);
    if (n.empty()) {
        ustawBlad("Nazwa kategorii nie może być pusta.");
        return std::nullopt;
    }
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return std::nullopt;

    auto wynik = pol->zapytanie(
        "INSERT INTO tbl_kategorie (id_rodzica, nazwa, opis, uzytkownik_dod, uzytkownik_mod) "
        "VALUES ($1::bigint, $2, $3, $4, $4) RETURNING id",
        {idParametr(idRodzica), n, tekstLubNull(opis), pol->uzytkownik()});
    if (!wynik || wynik->liczbaWierszy() != 1) {
        ustawBladBazy(pol, "Nie udało się dodać kategorii.");
        return std::nullopt;
    }
    return wynik->liczba(0, 0);
}

bool LogikaMagazynu::zmienNazweKategorii(long long id, const std::string& nazwa) {
    const std::string n = przytnij(nazwa);
    if (n.empty()) return ustawBlad("Nazwa kategorii nie może być pusta.");
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return false;
    auto wynik = pol->zapytanie(
        "UPDATE tbl_kategorie SET nazwa = $2, uzytkownik_mod = $3 WHERE id = $1::bigint",
        {liczbaParametr(id), n, pol->uzytkownik()});
    if (!wynik) return ustawBladBazy(pol, "Nie udało się zmienić nazwy kategorii.");
    return true;
}

bool LogikaMagazynu::przeniesKategorie(long long id, std::optional<long long> nowyRodzic) {
    if (nowyRodzic && *nowyRodzic == id) {
        return ustawBlad("Kategorii nie można przenieść do niej samej.");
    }
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return false;
    auto wynik = pol->zapytanie(
        "UPDATE tbl_kategorie SET id_rodzica = $2::bigint, uzytkownik_mod = $3 WHERE id = $1::bigint",
        {liczbaParametr(id), idParametr(nowyRodzic), pol->uzytkownik()});
    if (!wynik) return ustawBladBazy(pol, "Nie udało się przenieść kategorii.");
    return true;
}

bool LogikaMagazynu::usunKategorie(long long id) {
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return false;

    auto uzycie = pol->zapytanie(
        "SELECT (SELECT count(*) FROM tbl_kategorie WHERE id_rodzica = $1::bigint), "
        "       (SELECT count(*) FROM tbl_elementy WHERE id_kategorii = $1::bigint)",
        {liczbaParametr(id)});
    if (!uzycie || uzycie->liczbaWierszy() != 1) return ustawBladBazy(pol);
    if (uzycie->liczba(0, 0) > 0) {
        return ustawBlad("Kategoria ma podkategorie. Najpierw usuń je lub przenieś.");
    }
    if (uzycie->liczba(0, 1) > 0) {
        return ustawBlad("W kategorii są elementy. Najpierw przenieś je do innej kategorii.");
    }
    auto wynik = pol->zapytanie("DELETE FROM tbl_kategorie WHERE id = $1::bigint",
                                {liczbaParametr(id)});
    if (!wynik) return ustawBladBazy(pol, "Nie udało się usunąć kategorii.");
    return true;
}

bool LogikaMagazynu::dodajSciezkiKategorii(const std::vector<std::vector<std::string>>& sciezki,
                                           int* dodano) {
    if (dodano) *dodano = 0;
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return false;

    Transakcja transakcja(*pol);
    if (!transakcja.czyAktywna()) return ustawBladBazy(pol);

    auto przed = pol->zapytanie("SELECT count(*) FROM tbl_kategorie");
    if (!przed) return ustawBladBazy(pol);

    for (const auto& sciezka : sciezki) {
        std::optional<long long> rodzic;
        for (const std::string& surowa : sciezka) {
            const std::string nazwa = przytnij(surowa);
            if (nazwa.empty()) continue;
            auto wynik = pol->zapytanie("SELECT dodaj_kategorie_jesli_brak($1::bigint, $2::varchar)",
                                        {idParametr(rodzic), nazwa});
            if (!wynik || wynik->liczbaWierszy() != 1) {
                return ustawBladBazy(pol, "Nie udało się dodać kategorii „" + nazwa + "”.");
            }
            rodzic = wynik->liczba(0, 0);
        }
    }

    auto po = pol->zapytanie("SELECT count(*) FROM tbl_kategorie");
    if (!po) return ustawBladBazy(pol);
    if (!transakcja.zatwierdz()) return ustawBladBazy(pol);
    if (dodano) *dodano = static_cast<int>(po->liczba(0, 0) - przed->liczba(0, 0));
    return true;
}

// ---------------------------------------------------------------------------
// Elementy
// ---------------------------------------------------------------------------

void LogikaMagazynu::wypelnijKategorieElementu(std::vector<Element>& elementy) const {
    for (Element& e : elementy) {
        if (e.idKategorii) e.kategoriaPelna = pelnaNazwaKategorii(*e.idKategorii);
    }
}

bool LogikaMagazynu::pobierzElementy(const FiltrElementow& filtr, std::vector<Element>& wynik) {
    wynik.clear();
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return false;
    if (kategorie_.empty() && !odswiezKategorie()) return false;

    std::string sql = std::string("SELECT ") + KOLUMNY_ELEMENTU + " FROM tbl_elementy e WHERE e.czy_aktywny ";
    std::vector<Parametr> parametry;

    if (filtr.idKategorii) {
        parametry.push_back(std::to_string(*filtr.idKategorii));
        const std::string nr = "$" + std::to_string(parametry.size());
        if (filtr.zPodkategoriami) {
            sql += "AND e.id_kategorii IN (SELECT k.id FROM tbl_kategorie k WHERE k.sciezka <@ "
                   "(SELECT sciezka FROM tbl_kategorie WHERE id = " + nr + "::bigint)) ";
        } else {
            sql += "AND e.id_kategorii = " + nr + "::bigint ";
        }
    }
    if (filtr.tylkoNiskieStany) sql += "AND e.ilosc_dostepna < e.ilosc_minimalna ";
    const std::string tekst = przytnij(filtr.tekst);
    if (!tekst.empty()) {
        parametry.push_back(tekst);
        sql += "AND e.id IN (SELECT id FROM szukaj_elementow($" + std::to_string(parametry.size()) +
               "::text, 1000000)) ";
    }
    parametry.push_back(std::to_string(filtr.limit > 0 ? filtr.limit : 20000));
    sql += "ORDER BY lower(e.symbol), e.id LIMIT $" + std::to_string(parametry.size()) + "::integer";

    auto odp = pol->zapytanie(sql, parametry);
    if (!odp) return ustawBladBazy(pol, "Nie udało się pobrać elementów.");

    wynik.reserve(static_cast<size_t>(odp->liczbaWierszy()));
    for (int i = 0; i < odp->liczbaWierszy(); ++i) wynik.push_back(elementZWiersza(*odp, i));
    wypelnijKategorieElementu(wynik);
    return true;
}

std::optional<Element> LogikaMagazynu::pobierzElement(long long id) {
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return std::nullopt;
    auto odp = pol->zapytanie(std::string("SELECT ") + KOLUMNY_ELEMENTU +
                                  " FROM tbl_elementy e WHERE e.id = $1::bigint",
                              {liczbaParametr(id)});
    if (!odp) {
        ustawBladBazy(pol);
        return std::nullopt;
    }
    if (odp->liczbaWierszy() != 1) {
        ustawBlad("Element nie istnieje (mógł zostać usunięty).");
        return std::nullopt;
    }
    std::vector<Element> jeden{elementZWiersza(*odp, 0)};
    wypelnijKategorieElementu(jeden);
    return jeden[0];
}

std::optional<long long> LogikaMagazynu::zapiszElement(const Element& e) {
    const std::string symbol = przytnij(e.symbol);
    if (symbol.empty()) {
        ustawBlad("Symbol elementu jest wymagany.");
        return std::nullopt;
    }
    if (e.iloscDostepna < 0 || e.iloscMinimalna < 0) {
        ustawBlad("Ilości nie mogą być ujemne.");
        return std::nullopt;
    }
    if (e.cena && *e.cena < 0) {
        ustawBlad("Cena nie może być ujemna.");
        return std::nullopt;
    }
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return std::nullopt;

    Parametr cena;
    if (e.cena) cena = liczbaNaTekstKlasyczny(*e.cena);
    const std::string waluta = przytnij(e.waluta).empty() ? "PLN" : przytnij(e.waluta).substr(0, 3);
    const std::string uzytkownik = pol->uzytkownik();

    Transakcja transakcja(*pol);
    if (!transakcja.czyAktywna()) {
        ustawBladBazy(pol);
        return std::nullopt;
    }

    long long id = e.id;
    if (id == 0) {
        auto odp = pol->zapytanie(
            "INSERT INTO tbl_elementy (id_kategorii, symbol, symbol_producenta, producent, opis, "
            " wartosc, jednostka, obudowa, datasheet_url, zdjecie_sciezka, ilosc_dostepna, "
            " ilosc_minimalna, cena_jednostkowa, waluta, tagi, kontener, szuflada, przegroda, tolerancja, parametry, "
            " uzytkownik_dod, uzytkownik_mod) "
            "VALUES ($1::bigint, $2, $3, $4, $5, $6, $7, $8, $9, $10, 0, $11::integer, "
            " $12::numeric, $13, $14, $16, $17, $18, $19, $20, $15, $15) RETURNING id",
            {idParametr(e.idKategorii), symbol, tekstLubNull(e.symbolProducenta),
             tekstLubNull(e.producent), tekstLubNull(e.opis), tekstLubNull(e.wartosc),
             tekstLubNull(e.jednostka), tekstLubNull(e.obudowa), tekstLubNull(e.datasheetUrl),
             tekstLubNull(e.zdjecieSciezka), std::to_string(e.iloscMinimalna), cena, waluta,
             tekstLubNull(e.tagi), uzytkownik, tekstLubNull(e.kontener), tekstLubNull(e.szuflada),
             tekstLubNull(e.przegroda), tekstLubNull(e.tolerancja), tekstLubNull(e.parametry)});
        if (!odp || odp->liczbaWierszy() != 1) {
            ustawBladBazy(pol, "Nie udało się dodać elementu.");
            return std::nullopt;
        }
        id = odp->liczba(0, 0);
        if (e.iloscDostepna > 0 &&
            !pol->zapytanie("SELECT zmien_ilosc_elementu($1::bigint, $2::integer, 'PRZYJECIE'::varchar, "
                            "NULL::bigint, NULL::bigint, 'Stan początkowy'::varchar, $3::varchar)",
                            {liczbaParametr(id), std::to_string(e.iloscDostepna), uzytkownik})) {
            ustawBladBazy(pol, "Nie udało się zapisać stanu początkowego.");
            return std::nullopt;
        }
    } else {
        auto stan = pol->zapytanie(
            "SELECT ilosc_dostepna FROM tbl_elementy WHERE id = $1::bigint FOR UPDATE",
            {liczbaParametr(id)});
        if (!stan) {
            ustawBladBazy(pol);
            return std::nullopt;
        }
        if (stan->liczbaWierszy() != 1) {
            ustawBlad("Element nie istnieje (mógł zostać usunięty).");
            return std::nullopt;
        }
        const long long stara = stan->liczba(0, 0);

        auto odp = pol->zapytanie(
            "UPDATE tbl_elementy SET id_kategorii = $2::bigint, symbol = $3, symbol_producenta = $4, "
            " producent = $5, opis = $6, wartosc = $7, jednostka = $8, obudowa = $9, "
            " datasheet_url = $10, zdjecie_sciezka = $11, ilosc_minimalna = $12::integer, "
            " cena_jednostkowa = $13::numeric, waluta = $14, tagi = $15, uzytkownik_mod = $16, "
            " kontener = $17, szuflada = $18, przegroda = $19, tolerancja = $20, parametry = $21 "
            "WHERE id = $1::bigint",
            {liczbaParametr(id), idParametr(e.idKategorii), symbol, tekstLubNull(e.symbolProducenta),
             tekstLubNull(e.producent), tekstLubNull(e.opis), tekstLubNull(e.wartosc),
             tekstLubNull(e.jednostka), tekstLubNull(e.obudowa), tekstLubNull(e.datasheetUrl),
             tekstLubNull(e.zdjecieSciezka), std::to_string(e.iloscMinimalna), cena, waluta,
             tekstLubNull(e.tagi), uzytkownik, tekstLubNull(e.kontener), tekstLubNull(e.szuflada),
             tekstLubNull(e.przegroda), tekstLubNull(e.tolerancja), tekstLubNull(e.parametry)});
        if (!odp) {
            ustawBladBazy(pol, "Nie udało się zapisać elementu.");
            return std::nullopt;
        }
        const long long roznica = e.iloscDostepna - stara;
        if (roznica != 0 &&
            !pol->zapytanie("SELECT zmien_ilosc_elementu($1::bigint, $2::integer, 'KOREKTA'::varchar, "
                            "NULL::bigint, NULL::bigint, 'Zmiana w formularzu elementu'::varchar, $3::varchar)",
                            {liczbaParametr(id), std::to_string(roznica), uzytkownik})) {
            ustawBladBazy(pol);
            return std::nullopt;
        }
    }

    if (!transakcja.zatwierdz()) {
        ustawBladBazy(pol, "Nie udało się zatwierdzić zmian.");
        return std::nullopt;
    }
    return id;
}

bool LogikaMagazynu::zmienIlosc(long long idElementu, int zmiana, const std::string& typ,
                                const std::string& komentarz) {
    if (zmiana == 0) return ustawBlad("Zmiana ilości wynosi zero.");
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return false;
    auto odp = pol->zapytanie(
        "SELECT zmien_ilosc_elementu($1::bigint, $2::integer, $3::varchar, NULL::bigint, NULL::bigint, "
        "$4::varchar, $5::varchar)",
        {liczbaParametr(idElementu), std::to_string(zmiana), typ, tekstLubNull(komentarz),
         pol->uzytkownik()});
    if (!odp) return ustawBladBazy(pol);
    return true;
}

WynikUsuniecia LogikaMagazynu::usunElement(long long id) {
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return WynikUsuniecia::Blad;

    Transakcja transakcja(*pol);
    if (!transakcja.czyAktywna()) {
        ustawBladBazy(pol);
        return WynikUsuniecia::Blad;
    }

    // Powiązania, które blokują skasowanie (historia ilości nie blokuje: ją kasujemy razem z elementem)
    auto uzycie = pol->zapytanie(
        "SELECT (SELECT count(*) FROM tbl_bom WHERE id_elementu = $1::bigint) + "
        "       (SELECT count(*) FROM tbl_zamienniki WHERE id_elementu_podstawowego = $1::bigint "
        "                                               OR id_elementu_zamiennego = $1::bigint) + "
        "       (SELECT count(*) FROM tbl_elementy_lokalizacje WHERE id_elementu = $1::bigint) + "
        "       (SELECT count(*) FROM tbl_transakcje WHERE id_elementu = $1::bigint "
        "                                               AND (id_projektu IS NOT NULL OR id_lokalizacji IS NOT NULL))",
        {liczbaParametr(id)});
    if (!uzycie || uzycie->liczbaWierszy() != 1) {
        ustawBladBazy(pol);
        return WynikUsuniecia::Blad;
    }

    if (uzycie->liczba(0, 0) > 0) {
        if (!pol->zapytanie("UPDATE tbl_elementy SET czy_aktywny = false, uzytkownik_mod = $2 "
                            "WHERE id = $1::bigint",
                            {liczbaParametr(id), pol->uzytkownik()}) ||
            !transakcja.zatwierdz()) {
            ustawBladBazy(pol, "Nie udało się zarchiwizować elementu.");
            return WynikUsuniecia::Blad;
        }
        return WynikUsuniecia::Zarchiwizowany;
    }

    if (!pol->zapytanie("DELETE FROM tbl_transakcje WHERE id_elementu = $1::bigint", {liczbaParametr(id)}) ||
        !pol->zapytanie("DELETE FROM tbl_elementy WHERE id = $1::bigint", {liczbaParametr(id)}) ||
        !transakcja.zatwierdz()) {
        ustawBladBazy(pol, "Nie udało się usunąć elementu.");
        return WynikUsuniecia::Blad;
    }
    return WynikUsuniecia::Usuniety;
}

// ---------------------------------------------------------------------------
// Miejsca magazynowania
// ---------------------------------------------------------------------------

std::vector<std::string> LogikaMagazynu::pobierzKontenery() {
    std::vector<std::string> wynik;
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return wynik;
    auto odp = pol->zapytanie(
        "SELECT DISTINCT kontener FROM tbl_elementy WHERE czy_aktywny AND kontener IS NOT NULL AND kontener <> '' "
        "ORDER BY kontener");
    if (!odp) {
        ustawBladBazy(pol);
        return wynik;
    }
    for (int i = 0; i < odp->liczbaWierszy(); ++i) wynik.push_back(odp->tekst(i, 0));
    return wynik;
}

std::vector<std::string> LogikaMagazynu::pobierzSzuflady(const std::string& kontener) {
    std::vector<std::string> wynik;
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return wynik;
    auto odp = pol->zapytanie(
        "SELECT DISTINCT szuflada || CASE WHEN COALESCE(przegroda, '') <> '' THEN '/' || przegroda ELSE '' END AS m "
        "FROM tbl_elementy WHERE czy_aktywny AND COALESCE(szuflada, '') <> '' "
        "AND COALESCE(kontener, '') = $1 ORDER BY m",
        {przytnij(kontener)});
    if (!odp) {
        ustawBladBazy(pol);
        return wynik;
    }
    for (int i = 0; i < odp->liczbaWierszy(); ++i) wynik.push_back(odp->tekst(i, 0));
    return wynik;
}

std::optional<int> LogikaMagazynu::zmienNazweKontenera(const std::string& stara, const std::string& nowa) {
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return std::nullopt;
    const std::string s = przytnij(stara), n = przytnij(nowa);
    if (s.empty()) {
        ustawBlad("Podaj nazwę kontenera, którą chcesz zmienić.");
        return std::nullopt;
    }
    auto odp = pol->zapytanie(
        "WITH z AS (UPDATE tbl_elementy SET kontener = NULLIF($2, '') WHERE kontener = $1 RETURNING 1) "
        "SELECT count(*) FROM z",
        {s, n});
    if (!odp || odp->liczbaWierszy() != 1) {
        ustawBladBazy(pol, "Nie udało się zmienić nazwy kontenera.");
        return std::nullopt;
    }
    return static_cast<int>(odp->liczba(0, 0));
}

std::optional<Element> LogikaMagazynu::znajdzPoSymbolu(const std::string& symbol) {
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return std::nullopt;
    auto odp = pol->zapytanie(std::string("SELECT ") + KOLUMNY_ELEMENTU +
                                  " FROM tbl_elementy e WHERE e.czy_aktywny AND lower(e.symbol) = lower($1) ORDER BY e.id LIMIT 1",
                              {przytnij(symbol)});
    if (!odp) {
        ustawBladBazy(pol);
        return std::nullopt;
    }
    if (odp->liczbaWierszy() != 1) return std::nullopt;
    std::vector<Element> jeden{elementZWiersza(*odp, 0)};
    wypelnijKategorieElementu(jeden);
    return jeden[0];
}

std::set<std::string> LogikaMagazynu::pobierzSymbole() {
    std::set<std::string> wynik;
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return wynik;
    auto odp = pol->zapytanie("SELECT lower(symbol) FROM tbl_elementy WHERE czy_aktywny");
    if (!odp) {
        ustawBladBazy(pol);
        return wynik;
    }
    for (int i = 0; i < odp->liczbaWierszy(); ++i) wynik.insert(odp->tekst(i, 0));
    return wynik;
}
