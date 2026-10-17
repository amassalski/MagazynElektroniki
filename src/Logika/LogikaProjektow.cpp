#include "LogikaProjektow.h"

#include "../Eksport/EksportCSV.h"

#include <algorithm>
#include <set>

namespace {

std::string przytnij(const std::string& s) {
    const char* spacje = " \t\r\n";
    const size_t a = s.find_first_not_of(spacje);
    if (a == std::string::npos) return std::string();
    return s.substr(a, s.find_last_not_of(spacje) - a + 1);
}

Parametr tekstLubNull(const std::string& s) {
    const std::string t = przytnij(s);
    if (t.empty()) return std::nullopt;
    return t;
}

Parametr id(long long v) { return std::to_string(v); }
Parametr liczba(int v) { return std::to_string(v); }

bool statusPoprawny(const std::string& s) {
    for (const char* dozwolony : STATUSY_PROJEKTU) {
        if (s == dozwolony) return true;
    }
    return false;
}

const char* KOLUMNY_POZYCJI =
    "b.id, b.id_projektu, b.id_elementu, b.oznaczenia, b.ilosc_wymagana, b.ilosc_zamowiona, "
    "b.ilosc_zuzyta, b.priorytet, b.uwagi, e.symbol, e.producent, e.wartosc, e.obudowa, e.opis, "
    "e.ilosc_dostepna, e.symbol_producenta, e.kontener, e.szuflada, e.przegroda";

PozycjaProjektu pozycjaZWiersza(const WynikSQL& w, int i) {
    PozycjaProjektu p;
    p.id = w.liczba(i, w.indeksKolumny("id"));
    p.idProjektu = w.liczba(i, w.indeksKolumny("id_projektu"));
    p.idElementu = w.liczba(i, w.indeksKolumny("id_elementu"));
    p.oznaczenia = w.tekst(i, "oznaczenia");
    p.wymagana = static_cast<int>(w.liczba(i, w.indeksKolumny("ilosc_wymagana")));
    p.zamowiona = static_cast<int>(w.liczba(i, w.indeksKolumny("ilosc_zamowiona")));
    p.zuzyta = static_cast<int>(w.liczba(i, w.indeksKolumny("ilosc_zuzyta")));
    p.priorytet = static_cast<int>(w.liczba(i, w.indeksKolumny("priorytet"), 5));
    p.uwagi = w.tekst(i, "uwagi");
    p.symbol = w.tekst(i, "symbol");
    p.producent = w.tekst(i, "producent");
    p.wartosc = w.tekst(i, "wartosc");
    p.obudowa = w.tekst(i, "obudowa");
    p.opis = w.tekst(i, "opis");
    p.dostepna = static_cast<int>(w.liczba(i, w.indeksKolumny("ilosc_dostepna")));
    p.symbolProducenta = w.tekst(i, "symbol_producenta");
    p.miejsce = MiejsceMagazynowe{w.tekst(i, "kontener"), w.tekst(i, "szuflada"), w.tekst(i, "przegroda")}.naTekst();
    return p;
}

/// Łączy listy oznaczeń bez powtórzeń, w kolejności naturalnej
std::string polaczOznaczenia(const std::string& a, const std::string& b) {
    std::vector<std::string> lista = ImportBOM::rozbijOznaczenia(a);
    for (const std::string& o : ImportBOM::rozbijOznaczenia(b)) lista.push_back(o);
    std::sort(lista.begin(), lista.end(), ImportBOM::porownajNaturalnie);
    lista.erase(std::unique(lista.begin(), lista.end()), lista.end());
    std::string w;
    for (size_t i = 0; i < lista.size(); ++i) {
        if (i) w += ", ";
        w += lista[i];
    }
    return w;
}

} // namespace

// ---------------------------------------------------------------------------

LogikaProjektow::LogikaProjektow(ManagerBazyDanych& manager, LogikaMagazynu& magazyn)
    : manager_(manager), magazyn_(magazyn) {}

PolaczenieBazy* LogikaProjektow::polaczenie() {
    PolaczenieBazy* pol = manager_.pobierzPolaczenie();
    if (!pol || !pol->czyPolaczony()) {
        blad_ = "Brak połączenia z bazą danych.";
        return nullptr;
    }
    return pol;
}

bool LogikaProjektow::ustawBlad(const std::string& blad) {
    blad_ = blad;
    return false;
}

bool LogikaProjektow::ustawBladBazy(PolaczenieBazy* pol, const std::string& zaZnaczeniem) {
    const std::string blad = pol->pobierzBlad();
    // komunikaty z funkcji zmien_ilosc_elementu są już po polsku
    const size_t p = blad.find("Niewystarczająca ilość");
    if (p != std::string::npos) {
        const size_t koniec = blad.find('\n', p);
        blad_ = blad.substr(p, koniec == std::string::npos ? std::string::npos : koniec - p);
    } else if (!zaZnaczeniem.empty()) {
        blad_ = zaZnaczeniem + "\n" + blad;
    } else {
        blad_ = blad;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Projekty
// ---------------------------------------------------------------------------

namespace {
Projekt projektZWiersza(const WynikSQL& w, int i) {
    Projekt p;
    p.id = w.liczba(i, w.indeksKolumny("id"));
    p.nazwa = w.tekst(i, "nazwa");
    p.opis = w.tekst(i, "opis");
    p.status = w.tekst(i, "status");
    p.aktywny = w.logiczna(i, w.indeksKolumny("czy_aktywny"), true);
    p.dataUtworzenia = w.tekst(i, "data_utworzenia");
    p.liczbaPozycji = static_cast<int>(w.liczba(i, w.indeksKolumny("liczba_pozycji")));
    p.liczbaBrakow = static_cast<int>(w.liczba(i, w.indeksKolumny("liczba_brakow")));
    return p;
}

const char* SQL_PROJEKTY =
    "SELECT p.id, p.nazwa, p.opis, p.status, p.czy_aktywny, "
    "       to_char(p.data_utworzenia, 'YYYY-MM-DD HH24:MI') AS data_utworzenia, "
    "       (SELECT count(*) FROM tbl_bom b WHERE b.id_projektu = p.id) AS liczba_pozycji, "
    "       (SELECT count(*) FROM v_bom_stan v WHERE v.id_projektu = p.id AND v.brakuje > 0) AS liczba_brakow "
    "FROM tbl_projekty p ";
} // namespace

bool LogikaProjektow::pobierzProjekty(bool tylkoAktywne, std::vector<Projekt>& wynik) {
    wynik.clear();
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return false;
    auto odp = pol->zapytanie(std::string(SQL_PROJEKTY) +
                              (tylkoAktywne ? "WHERE p.czy_aktywny " : "") + "ORDER BY lower(p.nazwa), p.id");
    if (!odp) return ustawBladBazy(pol, "Nie udało się pobrać projektów.");
    for (int i = 0; i < odp->liczbaWierszy(); ++i) wynik.push_back(projektZWiersza(*odp, i));
    return true;
}

std::optional<Projekt> LogikaProjektow::pobierzProjekt(long long idProjektu) {
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return std::nullopt;
    auto odp = pol->zapytanie(std::string(SQL_PROJEKTY) + "WHERE p.id = $1::bigint", {id(idProjektu)});
    if (!odp) {
        ustawBladBazy(pol, "Nie udało się pobrać projektu.");
        return std::nullopt;
    }
    if (odp->liczbaWierszy() != 1) {
        ustawBlad("Nie znaleziono projektu.");
        return std::nullopt;
    }
    return projektZWiersza(*odp, 0);
}

std::optional<long long> LogikaProjektow::zapiszProjekt(const Projekt& p) {
    const std::string nazwa = przytnij(p.nazwa);
    if (nazwa.empty()) {
        ustawBlad("Nazwa projektu jest wymagana.");
        return std::nullopt;
    }
    if (!statusPoprawny(p.status)) {
        ustawBlad("Nieznany status projektu.");
        return std::nullopt;
    }
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return std::nullopt;

    if (p.id == 0) {
        auto odp = pol->zapytanie(
            "INSERT INTO tbl_projekty (nazwa, opis, status, uzytkownik_dod, czy_aktywny) "
            "VALUES ($1, $2, $3, $4, $5::boolean) RETURNING id",
            {nazwa, tekstLubNull(p.opis), p.status, pol->uzytkownik(), p.aktywny ? "true" : "false"});
        if (!odp || odp->liczbaWierszy() != 1) {
            ustawBladBazy(pol, "Nie udało się dodać projektu.");
            return std::nullopt;
        }
        return odp->liczba(0, 0);
    }
    auto odp = pol->zapytanie(
        "UPDATE tbl_projekty SET nazwa = $2, opis = $3, status = $4, czy_aktywny = $5::boolean, "
        "data_modyfikacji = current_timestamp WHERE id = $1::bigint",
        {id(p.id), nazwa, tekstLubNull(p.opis), p.status, p.aktywny ? "true" : "false"});
    if (!odp) {
        ustawBladBazy(pol, "Nie udało się zapisać projektu.");
        return std::nullopt;
    }
    if (odp->wierszeZmienione() != 1) {
        ustawBlad("Nie znaleziono projektu.");
        return std::nullopt;
    }
    return p.id;
}

WynikUsuniecia LogikaProjektow::usunProjekt(long long idProjektu) {
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return WynikUsuniecia::Blad;
    Transakcja transakcja(*pol);
    if (!transakcja.czyAktywna()) {
        ustawBladBazy(pol);
        return WynikUsuniecia::Blad;
    }
    auto uzycie = pol->zapytanie(
        "SELECT (SELECT count(*) FROM tbl_transakcje WHERE id_projektu = $1::bigint) + "
        "       (SELECT count(*) FROM tbl_bom WHERE id_projektu = $1::bigint AND ilosc_zuzyta > 0)",
        {id(idProjektu)});
    if (!uzycie || uzycie->liczbaWierszy() != 1) {
        ustawBladBazy(pol);
        return WynikUsuniecia::Blad;
    }
    if (uzycie->liczba(0, 0) > 0) {
        if (!pol->zapytanie("UPDATE tbl_projekty SET czy_aktywny = false, "
                            "data_modyfikacji = current_timestamp WHERE id = $1::bigint",
                            {id(idProjektu)}) ||
            !transakcja.zatwierdz()) {
            ustawBladBazy(pol, "Nie udało się zarchiwizować projektu.");
            return WynikUsuniecia::Blad;
        }
        return WynikUsuniecia::Zarchiwizowany;
    }
    if (!pol->zapytanie("DELETE FROM tbl_bom WHERE id_projektu = $1::bigint", {id(idProjektu)}) ||
        !pol->zapytanie("DELETE FROM tbl_projekty WHERE id = $1::bigint", {id(idProjektu)}) ||
        !transakcja.zatwierdz()) {
        ustawBladBazy(pol, "Nie udało się usunąć projektu.");
        return WynikUsuniecia::Blad;
    }
    return WynikUsuniecia::Usuniety;
}

// ---------------------------------------------------------------------------
// Lista części
// ---------------------------------------------------------------------------

bool LogikaProjektow::pobierzPozycje(long long idProjektu, std::vector<PozycjaProjektu>& wynik) {
    wynik.clear();
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return false;
    auto odp = pol->zapytanie(std::string("SELECT ") + KOLUMNY_POZYCJI +
                                  " FROM tbl_bom b JOIN tbl_elementy e ON e.id = b.id_elementu "
                                  "WHERE b.id_projektu = $1::bigint",
                              {id(idProjektu)});
    if (!odp) return ustawBladBazy(pol, "Nie udało się pobrać listy części.");
    for (int i = 0; i < odp->liczbaWierszy(); ++i) wynik.push_back(pozycjaZWiersza(*odp, i));
    std::stable_sort(wynik.begin(), wynik.end(), [](const PozycjaProjektu& a, const PozycjaProjektu& b) {
        if (a.oznaczenia.empty() != b.oznaczenia.empty()) return !a.oznaczenia.empty();
        if (a.oznaczenia != b.oznaczenia) return ImportBOM::porownajNaturalnie(a.oznaczenia, b.oznaczenia);
        return a.symbol < b.symbol;
    });
    return true;
}

std::optional<long long> LogikaProjektow::dodajPozycjeWTransakcji(PolaczenieBazy* pol,
                                                                  const PozycjaProjektu& p,
                                                                  bool* zsumowano) {
    if (zsumowano) *zsumowano = false;
    auto istn = pol->zapytanie(
        "SELECT id, oznaczenia FROM tbl_bom WHERE id_projektu = $1::bigint AND id_elementu = $2::bigint "
        "ORDER BY id LIMIT 1 FOR UPDATE",
        {id(p.idProjektu), id(p.idElementu)});
    if (!istn) {
        ustawBladBazy(pol);
        return std::nullopt;
    }
    if (istn->liczbaWierszy() == 1) {
        const long long idPoz = istn->liczba(0, 0);
        const std::string oznaczenia = polaczOznaczenia(istn->tekst(0, 1), p.oznaczenia);
        if (!pol->zapytanie("UPDATE tbl_bom SET ilosc_wymagana = ilosc_wymagana + $2::integer, "
                            "oznaczenia = $3, modyfikowano = current_timestamp WHERE id = $1::bigint",
                            {id(idPoz), liczba(p.wymagana), tekstLubNull(oznaczenia)})) {
            ustawBladBazy(pol, "Nie udało się zwiększyć ilości w liście części.");
            return std::nullopt;
        }
        if (zsumowano) *zsumowano = true;
        return idPoz;
    }
    auto odp = pol->zapytanie(
        "INSERT INTO tbl_bom (id_projektu, id_elementu, oznaczenia, ilosc_wymagana, ilosc_zamowiona, "
        "priorytet, uwagi) VALUES ($1::bigint, $2::bigint, $3, $4::integer, $5::integer, $6::integer, $7) "
        "RETURNING id",
        {id(p.idProjektu), id(p.idElementu), tekstLubNull(polaczOznaczenia(p.oznaczenia, "")),
         liczba(p.wymagana), liczba(p.zamowiona), liczba(p.priorytet), tekstLubNull(p.uwagi)});
    if (!odp || odp->liczbaWierszy() != 1) {
        ustawBladBazy(pol, "Nie udało się dodać pozycji do listy części.");
        return std::nullopt;
    }
    return odp->liczba(0, 0);
}

std::optional<long long> LogikaProjektow::dodajPozycje(const PozycjaProjektu& p) {
    if (p.wymagana <= 0) {
        ustawBlad("Wymagana ilość musi być większa od zera.");
        return std::nullopt;
    }
    if (p.idProjektu == 0 || p.idElementu == 0) {
        ustawBlad("Wybierz projekt i element.");
        return std::nullopt;
    }
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return std::nullopt;
    Transakcja transakcja(*pol);
    if (!transakcja.czyAktywna()) {
        ustawBladBazy(pol);
        return std::nullopt;
    }
    auto wynik = dodajPozycjeWTransakcji(pol, p, nullptr);
    if (!wynik) return std::nullopt;
    if (!transakcja.zatwierdz()) {
        ustawBladBazy(pol);
        return std::nullopt;
    }
    return wynik;
}

bool LogikaProjektow::zapiszPozycje(const PozycjaProjektu& p) {
    if (p.wymagana <= 0) return ustawBlad("Wymagana ilość musi być większa od zera.");
    if (p.zamowiona < 0) return ustawBlad("Ilość zamówiona nie może być ujemna.");
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return false;
    auto odp = pol->zapytanie(
        "UPDATE tbl_bom SET oznaczenia = $2, ilosc_wymagana = $3::integer, ilosc_zamowiona = $4::integer, "
        "priorytet = $5::integer, uwagi = $6, modyfikowano = current_timestamp WHERE id = $1::bigint",
        {id(p.id), tekstLubNull(polaczOznaczenia(p.oznaczenia, "")), liczba(p.wymagana),
         liczba(p.zamowiona), liczba(p.priorytet), tekstLubNull(p.uwagi)});
    if (!odp) return ustawBladBazy(pol, "Nie udało się zapisać pozycji.");
    if (odp->wierszeZmienione() != 1) return ustawBlad("Nie znaleziono pozycji.");
    return true;
}

bool LogikaProjektow::usunPozycje(long long idPozycji) {
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return false;
    auto odp = pol->zapytanie("SELECT ilosc_zuzyta FROM tbl_bom WHERE id = $1::bigint", {id(idPozycji)});
    if (!odp) return ustawBladBazy(pol);
    if (odp->liczbaWierszy() != 1) return ustawBlad("Nie znaleziono pozycji.");
    if (odp->liczba(0, 0) > 0)
        return ustawBlad("Z tej pozycji wydano już części do projektu, więc nie można jej usunąć.");
    if (!pol->zapytanie("DELETE FROM tbl_bom WHERE id = $1::bigint", {id(idPozycji)}))
        return ustawBladBazy(pol, "Nie udało się usunąć pozycji.");
    return true;
}

// ---------------------------------------------------------------------------
// Wydawanie
// ---------------------------------------------------------------------------

bool LogikaProjektow::wydajDoProjektu(long long idPozycji, int ilosc) {
    if (ilosc <= 0) return ustawBlad("Ilość do wydania musi być większa od zera.");
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return false;
    Transakcja transakcja(*pol);
    if (!transakcja.czyAktywna()) return ustawBladBazy(pol);

    auto poz = pol->zapytanie(
        "SELECT id_projektu, id_elementu, ilosc_wymagana, ilosc_zuzyta FROM tbl_bom "
        "WHERE id = $1::bigint FOR UPDATE",
        {id(idPozycji)});
    if (!poz) return ustawBladBazy(pol);
    if (poz->liczbaWierszy() != 1) return ustawBlad("Nie znaleziono pozycji.");
    const long long idProjektu = poz->liczba(0, 0), idElementu = poz->liczba(0, 1);
    const long long zostalo = poz->liczba(0, 2) - poz->liczba(0, 3);
    if (ilosc > zostalo)
        return ustawBlad("Do projektu potrzeba jeszcze tylko " + std::to_string(zostalo > 0 ? zostalo : 0) +
                         " szt. tego elementu.");

    auto wyd = pol->zapytanie(
        "SELECT zmien_ilosc_elementu($1::bigint, $2::integer, 'WYDANIE', $3::bigint, NULL::bigint, "
        "'Wydanie do projektu', $4::varchar)",
        {id(idElementu), liczba(-ilosc), id(idProjektu), pol->uzytkownik()});
    if (!wyd) return ustawBladBazy(pol);
    if (!pol->zapytanie("UPDATE tbl_bom SET ilosc_zuzyta = ilosc_zuzyta + $2::integer, "
                        "modyfikowano = current_timestamp WHERE id = $1::bigint",
                        {id(idPozycji), liczba(ilosc)}))
        return ustawBladBazy(pol);
    if (!transakcja.zatwierdz()) return ustawBladBazy(pol);
    return true;
}

bool LogikaProjektow::wydajWszystkoDostepne(long long idProjektu, int* wydanoSztuk) {
    if (wydanoSztuk) *wydanoSztuk = 0;
    std::vector<PozycjaProjektu> pozycje;
    if (!pobierzPozycje(idProjektu, pozycje)) return false;

    PolaczenieBazy* pol = polaczenie();
    if (!pol) return false;
    Transakcja transakcja(*pol);
    if (!transakcja.czyAktywna()) return ustawBladBazy(pol);

    int razem = 0;
    for (const PozycjaProjektu& p : pozycje) {
        // stan może zmienić się po pobraniu listy; zapytanie z blokadą daje aktualną wartość
        auto stan = pol->zapytanie("SELECT ilosc_dostepna FROM tbl_elementy WHERE id = $1::bigint FOR UPDATE",
                                   {id(p.idElementu)});
        if (!stan || stan->liczbaWierszy() != 1) return ustawBladBazy(pol);
        const int ile = std::min(p.doWydania(), static_cast<int>(stan->liczba(0, 0)));
        if (ile <= 0) continue;
        if (!pol->zapytanie("SELECT zmien_ilosc_elementu($1::bigint, $2::integer, 'WYDANIE', $3::bigint, "
                            "NULL::bigint, 'Wydanie do projektu', $4::varchar)",
                            {id(p.idElementu), liczba(-ile), id(idProjektu), pol->uzytkownik()}) ||
            !pol->zapytanie("UPDATE tbl_bom SET ilosc_zuzyta = ilosc_zuzyta + $2::integer, "
                            "modyfikowano = current_timestamp WHERE id = $1::bigint",
                            {id(p.id), liczba(ile)}))
            return ustawBladBazy(pol);
        razem += ile;
    }
    if (!transakcja.zatwierdz()) return ustawBladBazy(pol);
    if (wydanoSztuk) *wydanoSztuk = razem;
    return true;
}

// ---------------------------------------------------------------------------
// Import i braki
// ---------------------------------------------------------------------------

bool LogikaProjektow::importujBOM(long long idProjektu, const std::vector<WierszImportuBOM>& wiersze,
                                  bool zastap, int mnoznik, WynikImportuDoProjektu* wynik) {
    WynikImportuDoProjektu w;
    if (wynik) *wynik = w;
    if (mnoznik < 1) return ustawBlad("Liczba płytek musi wynosić co najmniej 1.");
    if (!pobierzProjekt(idProjektu)) return false;

    // 1. Nowe elementy (przed główną transakcją; nie zagnieżdżamy transakcji)
    std::vector<long long> idElementow(wiersze.size(), 0);
    for (size_t i = 0; i < wiersze.size(); ++i) {
        const WierszImportuBOM& r = wiersze[i];
        if (r.akcja == WierszImportuBOM::Akcja::UzyjIstniejacego) {
            if (r.idElementu == 0) return ustawBlad("Nie wybrano elementu dla pozycji: " + r.pozycja.oznaczenia);
            idElementow[i] = r.idElementu;
        } else if (r.akcja == WierszImportuBOM::Akcja::UtworzNowy) {
            Element e;
            e.idKategorii = r.idKategorii;
            e.symbol = !przytnij(r.pozycja.symbol).empty() ? przytnij(r.pozycja.symbol) : przytnij(r.pozycja.wartosc);
            if (e.symbol.empty()) e.symbol = przytnij(r.pozycja.obudowa);
            if (e.symbol.empty()) return ustawBlad("Pozycja bez wartości i symbolu: " + r.pozycja.oznaczenia);
            e.symbolProducenta = r.pozycja.symbol;
            e.producent = r.pozycja.producent;
            e.opis = r.pozycja.opis;
            e.wartosc = r.pozycja.wartosc;
            e.obudowa = r.pozycja.obudowa;
            // rezystory/kondensatory: symbol = wartość + obudowa, żeby nie wszystkie nazywały się "10k"
            if (przytnij(r.pozycja.symbol).empty() && !przytnij(r.pozycja.obudowa).empty())
                e.symbol = przytnij(r.pozycja.wartosc) + " " + przytnij(r.pozycja.obudowa);
            // ten sam symbol już jest w magazynie (np. po poprzednim imporcie): nie dublujemy
            PolaczenieBazy* pol0 = polaczenie();
            if (!pol0) return false;
            auto istn = pol0->zapytanie(
                "SELECT id FROM tbl_elementy WHERE lower(symbol) = lower($1) AND czy_aktywny ORDER BY id LIMIT 1",
                {e.symbol});
            if (!istn) return ustawBladBazy(pol0);
            if (istn->liczbaWierszy() == 1) {
                idElementow[i] = istn->liczba(0, 0);
                continue;
            }
            auto nowy = magazyn_.zapiszElement(e);
            if (!nowy) return ustawBlad("Nie udało się dodać elementu \"" + e.symbol + "\": " + magazyn_.pobierzBlad());
            idElementow[i] = *nowy;
            ++w.utworzonoElementow;
        } else {
            ++w.pominieto;
        }
    }

    // 2. Lista części w jednej transakcji
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return false;
    Transakcja transakcja(*pol);
    if (!transakcja.czyAktywna()) return ustawBladBazy(pol);

    if (zastap) {
        auto del = pol->zapytanie("DELETE FROM tbl_bom WHERE id_projektu = $1::bigint AND ilosc_zuzyta = 0",
                                  {id(idProjektu)});
        if (!del) return ustawBladBazy(pol, "Nie udało się wyczyścić listy części.");
        w.usunieto = static_cast<int>(del->wierszeZmienione());
    }
    for (size_t i = 0; i < wiersze.size(); ++i) {
        if (idElementow[i] == 0) continue;
        PozycjaProjektu p;
        p.idProjektu = idProjektu;
        p.idElementu = idElementow[i];
        p.oznaczenia = wiersze[i].pozycja.oznaczenia;
        p.wymagana = std::max(1, wiersze[i].pozycja.ilosc) * mnoznik;
        bool suma = false;
        if (!dodajPozycjeWTransakcji(pol, p, &suma)) return false;
        if (suma) ++w.zsumowano;
        else ++w.dodanoPozycji;
    }
    if (!transakcja.zatwierdz()) return ustawBladBazy(pol);
    if (wynik) *wynik = w;
    return true;
}

bool LogikaProjektow::pobierzBraki(std::optional<long long> idProjektu, std::vector<PozycjaProjektu>& wynik) {
    wynik.clear();
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return false;
    std::string sql = std::string("SELECT ") + KOLUMNY_POZYCJI +
                      ", p.nazwa AS projekt FROM tbl_bom b JOIN tbl_elementy e ON e.id = b.id_elementu "
                      "JOIN tbl_projekty p ON p.id = b.id_projektu "
                      "WHERE b.ilosc_wymagana - b.ilosc_zuzyta - e.ilosc_dostepna > 0 ";
    std::vector<Parametr> param;
    if (idProjektu) {
        sql += "AND b.id_projektu = $1::bigint ";
        param.push_back(id(*idProjektu));
    } else {
        sql += "AND p.czy_aktywny AND p.status <> 'ZAKONCZONY' ";
    }
    sql += "ORDER BY lower(e.symbol), b.id";
    auto odp = pol->zapytanie(sql, param);
    if (!odp) return ustawBladBazy(pol, "Nie udało się pobrać braków.");
    for (int i = 0; i < odp->liczbaWierszy(); ++i) {
        PozycjaProjektu p = pozycjaZWiersza(*odp, i);
        p.uwagi = odp->tekst(i, "projekt");  // w liście braków pole uwag niesie nazwę projektu
        wynik.push_back(p);
    }
    return true;
}

bool LogikaProjektow::eksportujListeZakupow(const std::string& sciezka, std::optional<long long> idProjektu) {
    std::vector<PozycjaProjektu> braki;
    if (!pobierzBraki(idProjektu, braki)) return false;
    // ten sam element potrzebny w kilku projektach: tu osobne wiersze z nazwą projektu
    std::vector<std::vector<std::string>> wiersze;
    for (const PozycjaProjektu& p : braki) {
        const int doKupienia = std::max(0, p.brakuje() - p.zamowiona);
        wiersze.push_back({p.symbol, p.producent, p.wartosc, p.obudowa, p.oznaczenia, std::to_string(p.brakuje()),
                           std::to_string(p.zamowiona), std::to_string(doKupienia), p.uwagi});
    }
    std::string blad;
    if (!EksportCSV::zapisz(sciezka,
                            {"Symbol", "Producent", "Wartość", "Obudowa", "Oznaczenia", "Brakuje",
                             "Już zamówiono", "Do zamówienia", "Projekt"},
                            wiersze, blad))
        return ustawBlad(blad);
    return true;
}
