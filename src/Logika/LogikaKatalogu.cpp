#include "LogikaKatalogu.h"

#include <algorithm>
#include <cctype>

namespace {

std::string przytnij(const std::string& s) {
    const char* sp = " \t\r\n";
    const size_t a = s.find_first_not_of(sp);
    if (a == std::string::npos) return std::string();
    return s.substr(a, s.find_last_not_of(sp) - a + 1);
}
Parametr tekstLubNull(const std::string& s) {
    const std::string t = przytnij(s);
    if (t.empty()) return std::nullopt;
    return t;
}
std::string malymi(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

const char* KOLUMNY =
    "k.id, k.symbol, k.producent, k.opis, k.obudowa, k.rodzina, k.datasheet_url, k.datasheet_plik, k.uwagi, "
    "(SELECT count(*) FROM tbl_elementy e WHERE e.czy_aktywny AND (lower(e.symbol) = lower(k.symbol) "
    "   OR lower(e.symbol_producenta) = lower(k.symbol))) AS liczba_elementow, "
    "(SELECT COALESCE(sum(e.ilosc_dostepna), 0) FROM tbl_elementy e WHERE e.czy_aktywny AND (lower(e.symbol) = lower(k.symbol) "
    "   OR lower(e.symbol_producenta) = lower(k.symbol))) AS w_magazynie";

PozycjaKatalogu pozycjaZWiersza(const WynikSQL& w, int i) {
    PozycjaKatalogu k;
    k.id = w.liczba(i, w.indeksKolumny("id"));
    k.symbol = w.tekst(i, "symbol");
    k.producent = w.tekst(i, "producent");
    k.opis = w.tekst(i, "opis");
    k.obudowa = w.tekst(i, "obudowa");
    k.rodzina = w.tekst(i, "rodzina");
    k.datasheetUrl = w.tekst(i, "datasheet_url");
    k.datasheetPlik = w.tekst(i, "datasheet_plik");
    k.uwagi = w.tekst(i, "uwagi");
    k.liczbaElementow = static_cast<int>(w.liczba(i, w.indeksKolumny("liczba_elementow")));
    k.wMagazynie = static_cast<int>(w.liczba(i, w.indeksKolumny("w_magazynie")));
    return k;
}

} // namespace

PolaczenieBazy* LogikaKatalogu::polaczenie() {
    PolaczenieBazy* pol = manager_.pobierzPolaczenie();
    if (!pol || !pol->czyPolaczony()) {
        blad_ = "Brak połączenia z bazą danych.";
        return nullptr;
    }
    return pol;
}

bool LogikaKatalogu::ustawBladBazy(PolaczenieBazy* pol, const std::string& zaZnaczeniem) {
    const std::string blad = pol->pobierzBlad();
    blad_ = zaZnaczeniem.empty() ? blad : zaZnaczeniem + "\n" + blad;
    return false;
}

bool LogikaKatalogu::pobierz(const FiltrKatalogu& f, std::vector<PozycjaKatalogu>& wynik) {
    wynik.clear();
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return false;
    std::string sql = std::string("SELECT ") + KOLUMNY + " FROM tbl_katalog k WHERE TRUE ";
    std::vector<Parametr> p;
    const std::string t = przytnij(f.tekst);
    if (!t.empty()) {
        p.push_back("%" + t + "%");
        sql += "AND (k.symbol ILIKE $1 OR k.producent ILIKE $1 OR k.opis ILIKE $1 OR k.rodzina ILIKE $1 OR k.obudowa ILIKE $1) ";
    }
    if (!przytnij(f.rodzina).empty()) {
        p.push_back(przytnij(f.rodzina));
        sql += "AND lower(k.rodzina) = lower($" + std::to_string(p.size()) + ") ";
    }
    if (f.tylkoWMagazynie)
        sql += "AND EXISTS (SELECT 1 FROM tbl_elementy e WHERE e.czy_aktywny AND (lower(e.symbol) = lower(k.symbol) "
               "OR lower(e.symbol_producenta) = lower(k.symbol))) ";
    sql += "ORDER BY lower(k.symbol), lower(k.producent) LIMIT " + std::to_string(std::max(1, f.limit));
    auto odp = pol->zapytanie(sql, p);
    if (!odp) return ustawBladBazy(pol, "Nie udało się pobrać katalogu.");
    for (int i = 0; i < odp->liczbaWierszy(); ++i) wynik.push_back(pozycjaZWiersza(*odp, i));
    return true;
}

bool LogikaKatalogu::wczytajParametry(PolaczenieBazy* pol, PozycjaKatalogu& k) {
    auto odp = pol->zapytanie("SELECT nazwa, wartosc, zrodlo FROM tbl_katalog_parametry WHERE id_katalogu = $1::bigint "
                              "ORDER BY kolejnosc, id", {std::to_string(k.id)});
    if (!odp) return ustawBladBazy(pol, "Nie udało się pobrać parametrów.");
    k.parametry.clear();
    for (int i = 0; i < odp->liczbaWierszy(); ++i)
        k.parametry.push_back({odp->tekst(i, "nazwa"), odp->tekst(i, "wartosc"), odp->tekst(i, "zrodlo")});
    return true;
}

std::optional<PozycjaKatalogu> LogikaKatalogu::pobierzPozycje(long long id) {
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return std::nullopt;
    auto odp = pol->zapytanie(std::string("SELECT ") + KOLUMNY + " FROM tbl_katalog k WHERE k.id = $1::bigint",
                              {std::to_string(id)});
    if (!odp) { ustawBladBazy(pol); return std::nullopt; }
    if (odp->liczbaWierszy() != 1) { blad_ = "Pozycja katalogu nie istnieje."; return std::nullopt; }
    PozycjaKatalogu k = pozycjaZWiersza(*odp, 0);
    if (!wczytajParametry(pol, k)) return std::nullopt;
    return k;
}

std::optional<PozycjaKatalogu> LogikaKatalogu::znajdz(const std::string& symbol, const std::string& producent) {
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return std::nullopt;
    std::string sql = std::string("SELECT k.id FROM tbl_katalog k WHERE lower(k.symbol) = lower($1) ");
    std::vector<Parametr> p = {przytnij(symbol)};
    if (!przytnij(producent).empty()) { sql += "AND lower(k.producent) = lower($2) "; p.push_back(przytnij(producent)); }
    sql += "ORDER BY k.id LIMIT 1";
    auto odp = pol->zapytanie(sql, p);
    if (!odp) { ustawBladBazy(pol); return std::nullopt; }
    if (odp->liczbaWierszy() == 0) { blad_.clear(); return std::nullopt; }
    return pobierzPozycje(odp->liczba(0, 0));
}

std::optional<PozycjaKatalogu> LogikaKatalogu::dlaElementu(const Element& e) {
    if (auto k = znajdz(e.symbol, e.producent)) return k;
    if (!przytnij(e.symbolProducenta).empty()) return znajdz(e.symbolProducenta, e.producent);
    return std::nullopt;
}

std::vector<std::string> LogikaKatalogu::rodziny() {
    std::vector<std::string> w;
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return w;
    auto odp = pol->zapytanie("SELECT DISTINCT rodzina FROM tbl_katalog WHERE rodzina IS NOT NULL AND rodzina <> '' "
                              "ORDER BY rodzina");
    if (!odp) return w;
    for (int i = 0; i < odp->liczbaWierszy(); ++i) w.push_back(odp->tekst(i, 0));
    return w;
}

std::optional<long long> LogikaKatalogu::zapisz(const PozycjaKatalogu& k) {
    const std::string symbol = przytnij(k.symbol);
    if (symbol.empty()) { blad_ = "Podaj symbol elementu."; return std::nullopt; }
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return std::nullopt;
    const std::string producent = przytnij(k.producent);

    auto powtorka = pol->zapytanie("SELECT 1 FROM tbl_katalog WHERE lower(symbol) = lower($1) AND lower(producent) = lower($2) "
                                   "AND id <> $3::bigint", {symbol, producent, std::to_string(k.id)});
    if (!powtorka) { ustawBladBazy(pol); return std::nullopt; }
    if (powtorka->liczbaWierszy() > 0) {
        blad_ = "W katalogu jest już pozycja o takim symbolu i producencie.";
        return std::nullopt;
    }
    Transakcja transakcja(*pol);
    long long id = k.id;
    if (id == 0) {
        auto odp = pol->zapytanie(
            "INSERT INTO tbl_katalog (symbol, producent, opis, obudowa, rodzina, datasheet_url, datasheet_plik, uwagi) "
            "VALUES ($1,$2,$3,$4,$5,$6,$7,$8) RETURNING id",
            {symbol, producent, tekstLubNull(k.opis), tekstLubNull(k.obudowa), tekstLubNull(k.rodzina),
             tekstLubNull(k.datasheetUrl), tekstLubNull(k.datasheetPlik), tekstLubNull(k.uwagi)});
        if (!odp || odp->liczbaWierszy() != 1) { ustawBladBazy(pol, "Nie udało się dodać pozycji katalogu."); return std::nullopt; }
        id = odp->liczba(0, 0);
    } else {
        if (!pol->zapytanie(
                "UPDATE tbl_katalog SET symbol=$2, producent=$3, opis=$4, obudowa=$5, rodzina=$6, datasheet_url=$7, "
                "datasheet_plik=$8, uwagi=$9, zmieniono=current_timestamp WHERE id=$1::bigint",
                {std::to_string(id), symbol, producent, tekstLubNull(k.opis), tekstLubNull(k.obudowa),
                 tekstLubNull(k.rodzina), tekstLubNull(k.datasheetUrl), tekstLubNull(k.datasheetPlik),
                 tekstLubNull(k.uwagi)})) {
            ustawBladBazy(pol, "Nie udało się zapisać pozycji katalogu.");
            return std::nullopt;
        }
        if (!pol->zapytanie("DELETE FROM tbl_katalog_parametry WHERE id_katalogu = $1::bigint", {std::to_string(id)})) {
            ustawBladBazy(pol);
            return std::nullopt;
        }
    }
    int kolejnosc = 0;
    std::vector<std::string> uzyte;
    for (const ParametrKatalogu& p : k.parametry) {
        const std::string n = przytnij(p.nazwa), w = przytnij(p.wartosc);
        if (n.empty() || w.empty()) continue;
        if (std::find(uzyte.begin(), uzyte.end(), malymi(n)) != uzyte.end()) continue;   // nazwa raz
        uzyte.push_back(malymi(n));
        if (!pol->zapytanie("INSERT INTO tbl_katalog_parametry (id_katalogu, nazwa, wartosc, zrodlo, kolejnosc) "
                            "VALUES ($1::bigint, $2, $3, $4, $5::integer)",
                            {std::to_string(id), n, w, p.zrodlo.empty() ? std::string("reczny") : p.zrodlo,
                             std::to_string(kolejnosc++)})) {
            ustawBladBazy(pol, "Nie udało się zapisać parametrów.");
            return std::nullopt;
        }
    }
    if (!transakcja.zatwierdz()) { ustawBladBazy(pol); return std::nullopt; }
    return id;
}

bool LogikaKatalogu::usun(long long id) {
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return false;
    if (!pol->zapytanie("DELETE FROM tbl_katalog WHERE id = $1::bigint", {std::to_string(id)}))
        return ustawBladBazy(pol, "Nie udało się usunąć pozycji katalogu.");
    return true;
}

bool LogikaKatalogu::dodajParametry(long long id, const std::vector<ParametrKatalogu>& parametry, bool nadpisz) {
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return false;
    Transakcja transakcja(*pol);
    auto kol = pol->zapytanie("SELECT COALESCE(max(kolejnosc), -1) + 1 FROM tbl_katalog_parametry WHERE id_katalogu = $1::bigint",
                              {std::to_string(id)});
    if (!kol) return ustawBladBazy(pol);
    long long kolejnosc = kol->liczba(0, 0);
    for (const ParametrKatalogu& p : parametry) {
        const std::string n = przytnij(p.nazwa), w = przytnij(p.wartosc);
        if (n.empty() || w.empty()) continue;
        const std::string sql = nadpisz
            ? "INSERT INTO tbl_katalog_parametry (id_katalogu, nazwa, wartosc, zrodlo, kolejnosc) VALUES ($1::bigint,$2,$3,$4,$5::integer) "
              "ON CONFLICT (id_katalogu, lower(nazwa)) DO UPDATE SET wartosc = EXCLUDED.wartosc, zrodlo = EXCLUDED.zrodlo"
            : "INSERT INTO tbl_katalog_parametry (id_katalogu, nazwa, wartosc, zrodlo, kolejnosc) VALUES ($1::bigint,$2,$3,$4,$5::integer) "
              "ON CONFLICT (id_katalogu, lower(nazwa)) DO NOTHING";
        if (!pol->zapytanie(sql, {std::to_string(id), n, w, p.zrodlo.empty() ? std::string("reczny") : p.zrodlo,
                                  std::to_string(kolejnosc++)}))
            return ustawBladBazy(pol, "Nie udało się zapisać parametrów.");
    }
    if (!pol->zapytanie("UPDATE tbl_katalog SET zmieniono = current_timestamp WHERE id = $1::bigint", {std::to_string(id)}))
        return ustawBladBazy(pol);
    return transakcja.zatwierdz() ? true : ustawBladBazy(pol);
}

bool LogikaKatalogu::ustawDatasheetPlik(long long id, const std::string& sciezka) {
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return false;
    if (!pol->zapytanie("UPDATE tbl_katalog SET datasheet_plik = $2, zmieniono = current_timestamp WHERE id = $1::bigint",
                        {std::to_string(id), tekstLubNull(sciezka)}))
        return ustawBladBazy(pol);
    return true;
}

bool LogikaKatalogu::importuj(const std::vector<KatalogCSV::Pozycja>& pozycje, TrybKatalogu tryb, WynikImportuKatalogu& wynik) {
    wynik = WynikImportuKatalogu();
    if (!polaczenie()) return false;
    for (const KatalogCSV::Pozycja& poz : pozycje) {
        auto blad = [&](const std::string& powod) {
            wynik.bledy.push_back("wiersz " + std::to_string(poz.nrWiersza) +
                                  (poz.katalog.symbol.empty() ? "" : " (" + poz.katalog.symbol + ")") + ": " + powod);
        };
        if (!poz.blad.empty()) { blad(poz.blad); continue; }
        auto istn = znajdz(poz.katalog.symbol, poz.katalog.producent.empty() ? std::string() : poz.katalog.producent);
        // pusty producent w pliku pasuje do pozycji o dowolnym producencie tylko wtedy, gdy symbol jest jednoznaczny
        if (istn && !poz.katalog.producent.empty() && malymi(istn->producent) != malymi(poz.katalog.producent)) istn.reset();
        if (!istn) {
            if (!zapisz(poz.katalog)) { blad(blad_); continue; }
            ++wynik.dodano;
            continue;
        }
        if (tryb == TrybKatalogu::Pomin) { ++wynik.pominieto; continue; }
        PozycjaKatalogu k = *istn;
        auto uzupelnij = [](std::string& cel, const std::string& nowa) { if (!nowa.empty()) cel = nowa; };
        uzupelnij(k.opis, poz.katalog.opis);
        uzupelnij(k.obudowa, poz.katalog.obudowa);
        uzupelnij(k.rodzina, poz.katalog.rodzina);
        uzupelnij(k.datasheetUrl, poz.katalog.datasheetUrl);
        uzupelnij(k.uwagi, poz.katalog.uwagi);
        for (const ParametrKatalogu& p : poz.katalog.parametry) {
            auto it = std::find_if(k.parametry.begin(), k.parametry.end(),
                                   [&](const ParametrKatalogu& x) { return malymi(x.nazwa) == malymi(p.nazwa); });
            if (it != k.parametry.end()) *it = p; else k.parametry.push_back(p);
        }
        if (!zapisz(k)) { blad(blad_); continue; }
        ++wynik.zaktualizowano;
    }
    blad_.clear();
    return true;
}

Element LogikaKatalogu::elementZKatalogu(const PozycjaKatalogu& k) {
    Element e;
    e.symbol = k.symbol;
    e.symbolProducenta = k.symbol;
    e.producent = k.producent;
    e.opis = k.opis;
    e.obudowa = k.obudowa;
    e.datasheetUrl = k.datasheetUrl;
    e.tagi = k.rodzina;
    std::string reszta;
    for (const ParametrKatalogu& p : k.parametry) {
        const std::string n = malymi(p.nazwa);
        if (n == "wartość" || n == "wartosc" || n == "value") e.wartosc = p.wartosc;
        else if (n == "tolerancja" || n == "tolerance") e.tolerancja = p.wartosc;
        else reszta += (reszta.empty() ? "" : "; ") + p.nazwa + ": " + p.wartosc;
    }
    // pole parametrów elementu ma 300 znaków: przycinamy na granicy znaku UTF-8
    if (reszta.size() > 300) {
        size_t n = 300;
        while (n > 0 && (static_cast<unsigned char>(reszta[n]) & 0xC0) == 0x80) --n;
        reszta.resize(n);
    }
    e.parametry = reszta;
    return e;
}

std::optional<long long> LogikaKatalogu::dodajDoMagazynu(LogikaMagazynu& magazyn, long long idKatalogu, int ilosc,
                                                          const MiejsceMagazynowe& miejsce,
                                                          std::optional<long long> idKategorii) {
    auto k = pobierzPozycje(idKatalogu);
    if (!k) return std::nullopt;
    if (magazyn.znajdzPoSymbolu(k->symbol)) {
        blad_ = "Element o symbolu " + k->symbol + " jest już w magazynie.";
        return std::nullopt;
    }
    Element e = elementZKatalogu(*k);
    e.iloscDostepna = std::max(0, ilosc);
    e.ustawMiejsce(miejsce);
    e.idKategorii = idKategorii;
    auto id = magazyn.zapiszElement(e);
    if (!id) { blad_ = magazyn.pobierzBlad(); return std::nullopt; }
    blad_.clear();
    return id;
}
