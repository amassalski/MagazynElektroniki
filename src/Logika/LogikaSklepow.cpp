#include "LogikaSklepow.h"

#include "../Narzedzia/Liczby.h"

#include <algorithm>
#include <cctype>
#include <cmath>

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
Parametr id(long long v) { return std::to_string(v); }

Parametr liczbaLubNull(const std::optional<double>& v) {
    if (!v) return std::nullopt;
    return liczbaNaTekstKlasyczny(*v);
}

std::string malymi(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string wielkimi(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

const char* KOLUMNY_SKLEPU =
    "s.id, s.nazwa, s.opis, s.url_bazowy, s.kraj, s.waluta, s.czy_aktywny, s.priorytet, "
    "s.szablon_wyszukiwania, s.koszt_dostawy, s.darmowa_dostawa_od, s.regex_ceny, "
    "(SELECT count(*) FROM tbl_elementy_sklepy l WHERE l.id_sklepu = s.id) AS liczba_linkow";

Sklep sklepZWiersza(const WynikSQL& w, int i) {
    Sklep s;
    s.id = w.liczba(i, w.indeksKolumny("id"));
    s.nazwa = w.tekst(i, "nazwa");
    s.opis = w.tekst(i, "opis");
    s.urlBazowy = w.tekst(i, "url_bazowy");
    s.kraj = w.tekst(i, "kraj");
    s.waluta = w.tekst(i, "waluta");
    s.aktywny = w.logiczna(i, w.indeksKolumny("czy_aktywny"), true);
    s.priorytet = static_cast<int>(w.liczba(i, w.indeksKolumny("priorytet"), 5));
    s.szablonWyszukiwania = w.tekst(i, "szablon_wyszukiwania");
    s.kosztDostawy = w.rzeczywista(i, w.indeksKolumny("koszt_dostawy"), 0.0);
    const int kd = w.indeksKolumny("darmowa_dostawa_od");
    if (!w.czyNull(i, kd)) s.darmowaDostawaOd = w.rzeczywista(i, kd);
    s.regexCeny = w.tekst(i, "regex_ceny");
    s.liczbaLinkow = static_cast<int>(w.liczba(i, w.indeksKolumny("liczba_linkow")));
    return s;
}

const char* KOLUMNY_LINKU =
    "l.id, l.id_elementu, l.id_sklepu, s.nazwa AS nazwa_sklepu, l.url, l.kod_w_sklepie, l.cena_aktualna, "
    "COALESCE(l.waluta, s.waluta) AS waluta, to_char(l.ostatnia_weryfikacja, 'YYYY-MM-DD HH24:MI') AS weryfikacja, "
    "l.status_dostepnosci, l.minimalna_ilosc, l.czy_domyslny, l.uwagi";

LinkSklepu linkZWiersza(const WynikSQL& w, int i) {
    LinkSklepu l;
    l.id = w.liczba(i, w.indeksKolumny("id"));
    l.idElementu = w.liczba(i, w.indeksKolumny("id_elementu"));
    l.idSklepu = w.liczba(i, w.indeksKolumny("id_sklepu"));
    l.nazwaSklepu = w.tekst(i, "nazwa_sklepu");
    l.url = w.tekst(i, "url");
    l.kodWSklepie = w.tekst(i, "kod_w_sklepie");
    const int kc = w.indeksKolumny("cena_aktualna");
    if (!w.czyNull(i, kc)) l.cena = w.rzeczywista(i, kc);
    l.waluta = w.tekst(i, "waluta");
    l.ostatniaWeryfikacja = w.tekst(i, "weryfikacja");
    l.status = w.tekst(i, "status_dostepnosci");
    l.minimalnaIlosc = static_cast<int>(w.liczba(i, w.indeksKolumny("minimalna_ilosc"), 1));
    if (l.minimalnaIlosc < 1) l.minimalnaIlosc = 1;
    l.domyslny = w.logiczna(i, w.indeksKolumny("czy_domyslny"), false);
    l.uwagi = w.tekst(i, "uwagi");
    return l;
}

std::vector<std::string> podziel(const std::string& wiersz, char sep) {
    std::vector<std::string> p;
    std::string biezacy;
    for (char c : wiersz) {
        if (c == sep) { p.push_back(przytnij(biezacy)); biezacy.clear(); }
        else biezacy += c;
    }
    p.push_back(przytnij(biezacy));
    return p;
}

/// "https://host/..." -> "https://host/"
std::string poczatekAdresu(const std::string& url) {
    const size_t p = url.find("://");
    if (p == std::string::npos) return std::string();
    const size_t k = url.find('/', p + 3);
    return k == std::string::npos ? url + "/" : url.substr(0, k + 1);
}

} // namespace

LogikaSklepow::LogikaSklepow(ManagerBazyDanych& manager) : manager_(manager) {}

PolaczenieBazy* LogikaSklepow::polaczenie() {
    PolaczenieBazy* pol = manager_.pobierzPolaczenie();
    if (!pol || !pol->czyPolaczony()) {
        blad_ = "Brak połączenia z bazą danych.";
        return nullptr;
    }
    return pol;
}

bool LogikaSklepow::ustawBladBazy(PolaczenieBazy* pol, const std::string& zaZnaczeniem) {
    const std::string blad = pol->pobierzBlad();
    blad_ = zaZnaczeniem.empty() ? blad : zaZnaczeniem + "\n" + blad;
    return false;
}

// ---------------------------------------------------------------------------
// Sklepy
// ---------------------------------------------------------------------------

bool LogikaSklepow::pobierzSklepy(bool tylkoAktywne, std::vector<Sklep>& wynik) {
    wynik.clear();
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return false;
    std::string sql = std::string("SELECT ") + KOLUMNY_SKLEPU + " FROM tbl_sklepy s ";
    if (tylkoAktywne) sql += "WHERE s.czy_aktywny ";
    sql += "ORDER BY s.priorytet, lower(s.nazwa)";
    auto odp = pol->zapytanie(sql);
    if (!odp) return ustawBladBazy(pol, "Nie udało się pobrać sklepów.");
    for (int i = 0; i < odp->liczbaWierszy(); ++i) wynik.push_back(sklepZWiersza(*odp, i));
    return true;
}

std::optional<Sklep> LogikaSklepow::pobierzSklep(long long idSklepu) {
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return std::nullopt;
    auto odp = pol->zapytanie(std::string("SELECT ") + KOLUMNY_SKLEPU + " FROM tbl_sklepy s WHERE s.id = $1::bigint",
                              {id(idSklepu)});
    if (!odp) { ustawBladBazy(pol, "Nie udało się pobrać sklepu."); return std::nullopt; }
    if (odp->liczbaWierszy() != 1) { blad_ = "Nie ma takiego sklepu."; return std::nullopt; }
    return sklepZWiersza(*odp, 0);
}

std::optional<long long> LogikaSklepow::zapiszSklep(const Sklep& s) {
    if (przytnij(s.nazwa).empty()) { blad_ = "Podaj nazwę sklepu."; return std::nullopt; }
    const std::string waluta = wielkimi(przytnij(s.waluta));
    if (waluta.size() != 3) { blad_ = "Waluta to trzy litery, np. PLN, EUR, USD."; return std::nullopt; }
    if (s.kosztDostawy < 0 || (s.darmowaDostawaOd && *s.darmowaDostawaOd < 0)) {
        blad_ = "Koszty dostawy nie mogą być ujemne.";
        return std::nullopt;
    }
    if (!przytnij(s.szablonWyszukiwania).empty() && s.szablonWyszukiwania.find("{q}") == std::string::npos) {
        blad_ = "Szablon wyszukiwania musi zawierać {q} w miejscu szukanej frazy.";
        return std::nullopt;
    }
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return std::nullopt;

    auto powtorka = pol->zapytanie("SELECT 1 FROM tbl_sklepy WHERE lower(nazwa) = lower($1) AND id <> $2::bigint",
                                   {przytnij(s.nazwa), id(s.id)});
    if (!powtorka) { ustawBladBazy(pol); return std::nullopt; }
    if (powtorka->liczbaWierszy() > 0) { blad_ = "Sklep o takiej nazwie już istnieje."; return std::nullopt; }

    const std::vector<Parametr> wspolne = {
        przytnij(s.nazwa), tekstLubNull(s.opis), przytnij(s.urlBazowy), tekstLubNull(s.kraj), waluta,
        std::string(s.aktywny ? "true" : "false"), std::to_string(s.priorytet), tekstLubNull(s.szablonWyszukiwania),
        liczbaNaTekstKlasyczny(s.kosztDostawy), liczbaLubNull(s.darmowaDostawaOd), tekstLubNull(s.regexCeny)};
    if (s.id == 0) {
        auto odp = pol->zapytanie(
            "INSERT INTO tbl_sklepy (nazwa, opis, url_bazowy, kraj, waluta, czy_aktywny, priorytet, "
            "szablon_wyszukiwania, koszt_dostawy, darmowa_dostawa_od, regex_ceny) "
            "VALUES ($1,$2,$3,$4,$5,$6::boolean,$7::int,$8,$9::numeric,$10::numeric,$11) RETURNING id",
            wspolne);
        if (!odp || odp->liczbaWierszy() != 1) { ustawBladBazy(pol, "Nie udało się dodać sklepu."); return std::nullopt; }
        return odp->liczba(0, 0);
    }
    std::vector<Parametr> p = wspolne;
    p.push_back(id(s.id));
    auto odp = pol->zapytanie(
        "UPDATE tbl_sklepy SET nazwa=$1, opis=$2, url_bazowy=$3, kraj=$4, waluta=$5, czy_aktywny=$6::boolean, "
        "priorytet=$7::int, szablon_wyszukiwania=$8, koszt_dostawy=$9::numeric, darmowa_dostawa_od=$10::numeric, "
        "regex_ceny=$11, modyfikowano=current_timestamp WHERE id=$12::bigint",
        p);
    if (!odp) { ustawBladBazy(pol, "Nie udało się zapisać sklepu."); return std::nullopt; }
    if (odp->wierszeZmienione() != 1) { blad_ = "Nie ma takiego sklepu."; return std::nullopt; }
    return s.id;
}

WynikUsuniecia LogikaSklepow::usunSklep(long long idSklepu) {
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return WynikUsuniecia::Blad;
    auto uzycie = pol->zapytanie("SELECT count(*) FROM tbl_elementy_sklepy WHERE id_sklepu = $1::bigint", {id(idSklepu)});
    if (!uzycie) { ustawBladBazy(pol); return WynikUsuniecia::Blad; }
    if (uzycie->liczba(0, 0) > 0) {
        if (!pol->zapytanie("UPDATE tbl_sklepy SET czy_aktywny = false, modyfikowano = current_timestamp "
                            "WHERE id = $1::bigint", {id(idSklepu)})) {
            ustawBladBazy(pol, "Nie udało się wyłączyć sklepu.");
            return WynikUsuniecia::Blad;
        }
        return WynikUsuniecia::Zarchiwizowany;
    }
    if (!pol->zapytanie("DELETE FROM tbl_sklepy WHERE id = $1::bigint", {id(idSklepu)})) {
        ustawBladBazy(pol, "Nie udało się usunąć sklepu.");
        return WynikUsuniecia::Blad;
    }
    return WynikUsuniecia::Usuniety;
}

bool LogikaSklepow::parsujWierszSklepu(const std::string& wiersz, Sklep& s, std::string& blad) {
    const char sep = wiersz.find('\t') != std::string::npos ? '\t' : ';';
    const std::vector<std::string> p = podziel(wiersz, sep);
    s = Sklep();
    s.nazwa = p.size() > 0 ? p[0] : "";
    if (s.nazwa.empty()) { blad = "brak nazwy sklepu"; return false; }
    if (p.size() > 1) s.urlBazowy = p[1];
    if (p.size() > 2) s.szablonWyszukiwania = p[2];
    if (!s.szablonWyszukiwania.empty() && s.szablonWyszukiwania.find("{q}") == std::string::npos) {
        blad = "szablon wyszukiwania musi zawierać {q}";
        return false;
    }
    if (p.size() > 3 && !p[3].empty()) {
        s.waluta = wielkimi(p[3]);
        if (s.waluta.size() != 3) { blad = "waluta to trzy litery"; return false; }
    }
    if (p.size() > 4 && !p[4].empty()) {
        if (!tekstNaLiczbe(p[4], s.kosztDostawy) || s.kosztDostawy < 0) { blad = "błędny koszt dostawy"; return false; }
    }
    if (p.size() > 5 && !p[5].empty()) {
        double v = 0;
        if (!tekstNaLiczbe(p[5], v) || v < 0) { blad = "błędny próg darmowej dostawy"; return false; }
        s.darmowaDostawaOd = v;
    }
    if (s.urlBazowy.empty()) s.urlBazowy = poczatekAdresu(s.szablonWyszukiwania);
    return true;   // brak adresu jest błędem dopiero przy dodawaniu nowego sklepu
}

bool LogikaSklepow::importujSklepy(const std::string& tekst, WynikImportuSklepow& wynik) {
    wynik = WynikImportuSklepow();
    if (!polaczenie()) return false;
    std::vector<Sklep> istniejace;
    if (!pobierzSklepy(false, istniejace)) return false;

    int nr = 0;
    size_t poz = 0;
    while (poz <= tekst.size()) {
        size_t koniec = tekst.find('\n', poz);
        if (koniec == std::string::npos) koniec = tekst.size();
        std::string linia = przytnij(tekst.substr(poz, koniec - poz));
        poz = koniec + 1;
        ++nr;
        if (linia.empty() || linia[0] == '#') continue;
        if (nr == 1 && malymi(linia).rfind("nazwa", 0) == 0) continue;  // nagłówek z eksportu

        Sklep nowy;
        std::string blad;
        if (!parsujWierszSklepu(linia, nowy, blad)) {
            wynik.bledy.push_back("wiersz " + std::to_string(nr) + ": " + blad);
            continue;
        }
        const std::string klucz = malymi(nowy.nazwa);
        auto it = std::find_if(istniejace.begin(), istniejace.end(),
                               [&](const Sklep& s) { return malymi(s.nazwa) == klucz; });
        if (it != istniejace.end()) {
            Sklep zm = *it;           // zmieniamy tylko podane pola
            if (!nowy.urlBazowy.empty()) zm.urlBazowy = nowy.urlBazowy;
            if (!nowy.szablonWyszukiwania.empty()) zm.szablonWyszukiwania = nowy.szablonWyszukiwania;
            zm.waluta = nowy.waluta;
            zm.kosztDostawy = nowy.kosztDostawy;
            if (nowy.darmowaDostawaOd) zm.darmowaDostawaOd = nowy.darmowaDostawaOd;
            zm.aktywny = true;
            if (!zapiszSklep(zm)) { wynik.bledy.push_back("wiersz " + std::to_string(nr) + ": " + blad_); continue; }
            ++wynik.zaktualizowano;
        } else {
            if (nowy.urlBazowy.empty()) {
                wynik.bledy.push_back("wiersz " + std::to_string(nr) + ": podaj adres strony sklepu albo szablon wyszukiwania");
                continue;
            }
            nowy.priorytet = 5;
            auto nid = zapiszSklep(nowy);
            if (!nid) { wynik.bledy.push_back("wiersz " + std::to_string(nr) + ": " + blad_); continue; }
            nowy.id = *nid;
            istniejace.push_back(nowy);
            ++wynik.dodano;
        }
    }
    return true;
}

std::string LogikaSklepow::eksportujSklepy() {
    std::vector<Sklep> sklepy;
    if (!pobierzSklepy(false, sklepy)) return std::string();
    std::string w = "nazwa;adres strony;szablon wyszukiwania;waluta;koszt dostawy;darmowa dostawa od\n";
    for (const Sklep& s : sklepy) {
        w += s.nazwa + ";" + s.urlBazowy + ";" + s.szablonWyszukiwania + ";" + s.waluta + ";" +
             liczbaNaTekstKlasyczny(s.kosztDostawy) + ";" +
             (s.darmowaDostawaOd ? liczbaNaTekstKlasyczny(*s.darmowaDostawaOd) : "") + "\n";
    }
    return w;
}

// ---------------------------------------------------------------------------
// Linki i ceny
// ---------------------------------------------------------------------------

std::string LogikaSklepow::kodujURL(const std::string& t) {
    static const char* hex = "0123456789ABCDEF";
    std::string w;
    for (unsigned char c : t) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') w += static_cast<char>(c);
        else { w += '%'; w += hex[c >> 4]; w += hex[c & 15]; }
    }
    return w;
}

std::string LogikaSklepow::adresWyszukiwania(const Sklep& sklep, const std::string& fraza) {
    std::string szablon = sklep.szablonWyszukiwania;
    if (przytnij(szablon).empty()) return std::string();
    const std::string kod = kodujURL(przytnij(fraza));
    for (const char* znacznik : {"{q}", "{KOD}"}) {
        const std::string z = znacznik;
        size_t p;
        while ((p = szablon.find(z)) != std::string::npos) szablon.replace(p, z.size(), kod);
    }
    return szablon;
}

bool LogikaSklepow::pobierzLinki(long long idElementu, std::vector<LinkSklepu>& wynik) {
    wynik.clear();
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return false;
    auto odp = pol->zapytanie(std::string("SELECT ") + KOLUMNY_LINKU +
                              " FROM tbl_elementy_sklepy l JOIN tbl_sklepy s ON s.id = l.id_sklepu "
                              "WHERE l.id_elementu = $1::bigint ORDER BY l.czy_domyslny DESC, s.priorytet, lower(s.nazwa)",
                              {id(idElementu)});
    if (!odp) return ustawBladBazy(pol, "Nie udało się pobrać linków do sklepów.");
    for (int i = 0; i < odp->liczbaWierszy(); ++i) wynik.push_back(linkZWiersza(*odp, i));
    return true;
}

std::optional<LinkSklepu> LogikaSklepow::pobierzLink(long long idLinku) {
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return std::nullopt;
    auto odp = pol->zapytanie(std::string("SELECT ") + KOLUMNY_LINKU +
                              " FROM tbl_elementy_sklepy l JOIN tbl_sklepy s ON s.id = l.id_sklepu "
                              "WHERE l.id = $1::bigint", {id(idLinku)});
    if (!odp) { ustawBladBazy(pol); return std::nullopt; }
    if (odp->liczbaWierszy() != 1) { blad_ = "Nie ma takiego linku."; return std::nullopt; }
    return linkZWiersza(*odp, 0);
}

std::optional<long long> LogikaSklepow::zapiszLink(const LinkSklepu& l) {
    if (l.idElementu <= 0 || l.idSklepu <= 0) { blad_ = "Wybierz element i sklep."; return std::nullopt; }
    const std::string url = przytnij(l.url);
    if (url.empty()) { blad_ = "Podaj adres strony elementu w sklepie."; return std::nullopt; }
    if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0) {
        blad_ = "Adres musi zaczynać się od http:// albo https://.";
        return std::nullopt;
    }
    if (l.cena && *l.cena < 0) { blad_ = "Cena nie może być ujemna."; return std::nullopt; }
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return std::nullopt;

    auto sklep = pobierzSklep(l.idSklepu);
    if (!sklep) return std::nullopt;
    const std::string waluta = l.waluta.empty() ? sklep->waluta : wielkimi(przytnij(l.waluta));

    auto powtorka = pol->zapytanie("SELECT 1 FROM tbl_elementy_sklepy WHERE id_elementu = $1::bigint "
                                   "AND id_sklepu = $2::bigint AND id <> $3::bigint",
                                   {id(l.idElementu), id(l.idSklepu), id(l.id)});
    if (!powtorka) { ustawBladBazy(pol); return std::nullopt; }
    if (powtorka->liczbaWierszy() > 0) { blad_ = "Ten element ma już link do tego sklepu."; return std::nullopt; }

    std::optional<double> staraCena;
    long long idLinku = l.id;
    if (l.id == 0) {
        auto odp = pol->zapytanie(
            "INSERT INTO tbl_elementy_sklepy (id_elementu, id_sklepu, url, kod_w_sklepie, waluta, minimalna_ilosc, "
            "czy_domyslny, uwagi, status_dostepnosci) VALUES ($1::bigint,$2::bigint,$3,$4,$5,$6::int,$7::boolean,$8,$9) "
            "RETURNING id",
            {id(l.idElementu), id(l.idSklepu), url, tekstLubNull(l.kodWSklepie), waluta,
             std::to_string(std::max(1, l.minimalnaIlosc)), std::string(l.domyslny ? "true" : "false"),
             tekstLubNull(l.uwagi), tekstLubNull(l.status)});
        if (!odp || odp->liczbaWierszy() != 1) { ustawBladBazy(pol, "Nie udało się dodać linku."); return std::nullopt; }
        idLinku = odp->liczba(0, 0);
    } else {
        auto stary = pobierzLink(l.id);
        if (!stary) return std::nullopt;
        staraCena = stary->cena;
        if (!pol->zapytanie(
                "UPDATE tbl_elementy_sklepy SET id_sklepu=$1::bigint, url=$2, kod_w_sklepie=$3, waluta=$4, "
                "minimalna_ilosc=$5::int, czy_domyslny=$6::boolean, uwagi=$7, modyfikowano=current_timestamp "
                "WHERE id=$8::bigint",
                {id(l.idSklepu), url, tekstLubNull(l.kodWSklepie), waluta, std::to_string(std::max(1, l.minimalnaIlosc)),
                 std::string(l.domyslny ? "true" : "false"), tekstLubNull(l.uwagi), id(l.id)})) {
            ustawBladBazy(pol, "Nie udało się zapisać linku.");
            return std::nullopt;
        }
    }
    // najwyżej jeden domyślny link na element pilnuje wyzwalacz w bazie
    if (l.cena && (!staraCena || std::abs(*staraCena - *l.cena) > 1e-9)) {
        if (!ustawCene(idLinku, *l.cena, waluta, l.status, "reczna")) return std::nullopt;
    }
    return idLinku;
}

bool LogikaSklepow::usunLink(long long idLinku) {
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return false;
    if (!pol->zapytanie("DELETE FROM tbl_elementy_sklepy WHERE id = $1::bigint", {id(idLinku)}))
        return ustawBladBazy(pol, "Nie udało się usunąć linku.");
    return true;
}

bool LogikaSklepow::ustawCene(long long idLinku, double cena, const std::string& waluta, const std::string& status,
                              const std::string& zrodlo) {
    if (cena < 0) return ustawBlad("Cena nie może być ujemna.");
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return false;
    Transakcja t(*pol);
    if (!t.czyAktywna()) return ustawBladBazy(pol);
    const std::string c = liczbaNaTekstKlasyczny(cena);
    const Parametr w = tekstLubNull(waluta);
    // wpis do historii robi wyzwalacz fn_historia_cen (tylko gdy cena się zmieniła); źródło przekazujemy ustawieniem
    if (!pol->zapytanie("SELECT set_config('dm.zrodlo_ceny', $1, true)", {zrodlo}) ||
        !pol->zapytanie(
            "UPDATE tbl_elementy_sklepy SET cena_aktualna = $2::numeric, waluta = COALESCE($3, waluta), "
            "status_dostepnosci = COALESCE($4, status_dostepnosci), ostatnia_weryfikacja = current_timestamp "
            "WHERE id = $1::bigint",
            {id(idLinku), c, w, tekstLubNull(status)}) ||
        !t.zatwierdz())
        return ustawBladBazy(pol, "Nie udało się zapisać ceny.");
    return true;
}

bool LogikaSklepow::pobierzHistorie(long long idLinku, std::vector<WpisHistoriiCeny>& wynik) {
    wynik.clear();
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return false;
    auto odp = pol->zapytanie(
        "SELECT to_char(data_ceny, 'YYYY-MM-DD HH24:MI') AS data, cena, waluta, zrodlo FROM tbl_historia_cen_sklepy "
        "WHERE id_elementu_sklepu = $1::bigint ORDER BY data_ceny DESC, id DESC", {id(idLinku)});
    if (!odp) return ustawBladBazy(pol, "Nie udało się pobrać historii cen.");
    for (int i = 0; i < odp->liczbaWierszy(); ++i) {
        WpisHistoriiCeny h;
        h.data = odp->tekst(i, "data");
        h.cena = odp->rzeczywista(i, odp->indeksKolumny("cena"));
        h.waluta = odp->tekst(i, "waluta");
        h.zrodlo = odp->tekst(i, "zrodlo");
        wynik.push_back(h);
    }
    return true;
}

// ---------------------------------------------------------------------------
// Kursy
// ---------------------------------------------------------------------------

bool LogikaSklepow::pobierzKursy(std::map<std::string, double>& wynik) {
    wynik.clear();
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return false;
    auto odp = pol->zapytanie("SELECT waluta, kurs_pln FROM tbl_kursy_walut ORDER BY waluta");
    if (!odp) return ustawBladBazy(pol, "Nie udało się pobrać kursów walut.");
    for (int i = 0; i < odp->liczbaWierszy(); ++i) wynik[odp->tekst(i, 0)] = odp->rzeczywista(i, 1);
    wynik["PLN"] = 1.0;
    return true;
}

bool LogikaSklepow::zapiszKurs(const std::string& waluta, double kurs) {
    const std::string w = wielkimi(przytnij(waluta));
    if (w.size() != 3) return ustawBlad("Waluta to trzy litery.");
    if (!(kurs > 0)) return ustawBlad("Kurs musi być większy od zera.");
    if (w == "PLN") return ustawBlad("Kurs złotego jest zawsze 1.");
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return false;
    if (!pol->zapytanie("INSERT INTO tbl_kursy_walut (waluta, kurs_pln) VALUES ($1, $2::numeric) "
                        "ON CONFLICT (waluta) DO UPDATE SET kurs_pln = EXCLUDED.kurs_pln, zmieniono = current_timestamp",
                        {w, liczbaNaTekstKlasyczny(kurs)}))
        return ustawBladBazy(pol, "Nie udało się zapisać kursu.");
    return true;
}

bool LogikaSklepow::pobierzLinkiElementow(const std::vector<long long>& idElementow, std::vector<LinkSklepu>& wynik,
                                          bool tylkoZCena) {
    wynik.clear();
    if (idElementow.empty()) return true;
    PolaczenieBazy* pol = polaczenie();
    if (!pol) return false;
    std::string lista;
    for (long long e : idElementow) lista += (lista.empty() ? "" : ",") + std::to_string(e);
    auto odp = pol->zapytanie(std::string("SELECT ") + KOLUMNY_LINKU +
                              " FROM tbl_elementy_sklepy l JOIN tbl_sklepy s ON s.id = l.id_sklepu "
                              "WHERE s.czy_aktywny " + std::string(tylkoZCena ? "AND l.cena_aktualna IS NOT NULL " : "") +
                              "AND l.id_elementu IN (" + lista + ") ORDER BY l.id");
    if (!odp) return ustawBladBazy(pol, "Nie udało się pobrać ofert.");
    for (int i = 0; i < odp->liczbaWierszy(); ++i) wynik.push_back(linkZWiersza(*odp, i));
    return true;
}
