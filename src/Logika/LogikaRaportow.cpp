#include "LogikaRaportow.h"

#include <algorithm>
#include <iomanip>
#include <locale>
#include <map>
#include <sstream>

namespace {

std::string liczba(long long v) { return std::to_string(v); }

/// Data z pola tekstowego: tylko RRRR-MM-DD, inaczej puste (chroni przed błędem składni w SQL)
bool poprawnaData(const std::string& d) {
    if (d.size() != 10 || d[4] != '-' || d[7] != '-') return false;
    for (size_t i = 0; i < d.size(); ++i) {
        if (i == 4 || i == 7) continue;
        if (d[i] < '0' || d[i] > '9') return false;
    }
    return true;
}

std::string sumyWalut(const std::map<std::string, double>& sumy) {
    std::string t;
    for (const auto& [waluta, suma] : sumy) {
        if (!t.empty()) t += ", ";
        t += LogikaRaportow::formatKwoty(suma) + " " + waluta;
    }
    return t.empty() ? "0,00" : t;
}

} // namespace

LogikaRaportow::LogikaRaportow(ManagerBazyDanych& manager, LogikaMagazynu& magazyn, LogikaProjektow& projekty)
    : manager_(manager), magazyn_(magazyn), projekty_(projekty) {}

std::string LogikaRaportow::formatKwoty(double kwota) {
    std::ostringstream s;
    s.imbue(std::locale::classic());
    s << std::fixed << std::setprecision(2) << kwota;
    std::string t = s.str();
    std::replace(t.begin(), t.end(), '.', ',');
    return t;
}

std::string LogikaRaportow::nazwaRaportu(RodzajRaportu r) {
    switch (r) {
        case RodzajRaportu::StanMagazynu: return "Stan magazynu";
        case RodzajRaportu::NiskieStany: return "Niskie stany (do zamówienia)";
        case RodzajRaportu::WartoscWgKategorii: return "Wartość magazynu wg kategorii";
        case RodzajRaportu::BrakiWProjektach: return "Braki w projektach";
        case RodzajRaportu::HistoriaZmian: return "Historia zmian stanów";
    }
    return "";
}

bool LogikaRaportow::zbuduj(RodzajRaportu rodzaj, const ParametryRaportu& p, Raport& r) {
    r = Raport();
    r.tytul = nazwaRaportu(rodzaj);
    blad_.clear();
    if (!manager_.czyPolaczony()) return ustawBlad("Brak połączenia z bazą danych.");
    switch (rodzaj) {
        case RodzajRaportu::StanMagazynu: return stanMagazynu(p, r);
        case RodzajRaportu::NiskieStany: return niskieStany(p, r);
        case RodzajRaportu::WartoscWgKategorii: return wartoscWgKategorii(p, r);
        case RodzajRaportu::BrakiWProjektach: return brakiWProjektach(p, r);
        case RodzajRaportu::HistoriaZmian: return historiaZmian(p, r);
    }
    return false;
}

// ---------------------------------------------------------------------------

bool LogikaRaportow::stanMagazynu(const ParametryRaportu& p, Raport& r) {
    if (!magazyn_.odswiezKategorie()) return ustawBlad(magazyn_.pobierzBlad());
    FiltrElementow f;
    f.limit = p.limitWierszy + 1;
    std::vector<Element> elementy;
    if (!magazyn_.pobierzElementy(f, elementy)) return ustawBlad(magazyn_.pobierzBlad());
    if (static_cast<int>(elementy.size()) > p.limitWierszy) {
        elementy.resize(static_cast<size_t>(p.limitWierszy));
        r.ucieto = true;
    }
    r.naglowki = {"Symbol", "Producent", "Wartość", "Obudowa", "Kategoria", "Miejsce", "Ilość", "Cena", "Wartość razem", "Waluta"};
    r.wyrownajDoPrawej = {false, false, false, false, false, false, true, true, true, false};
    std::map<std::string, double> sumy;
    long long sztuk = 0;
    for (const Element& e : elementy) {
        const double wartosc = e.cena ? *e.cena * e.iloscDostepna : 0.0;
        if (e.cena) sumy[e.waluta] += wartosc;
        sztuk += e.iloscDostepna;
        r.wiersze.push_back({e.symbol, e.producent, e.wartosc, e.obudowa, e.kategoriaPelna, e.miejsceTekst(), liczba(e.iloscDostepna),
                             e.cena ? formatKwoty(*e.cena) : "", e.cena ? formatKwoty(wartosc) : "", e.waluta});
    }
    r.podsumowanie = {"Elementów: " + liczba(static_cast<long long>(elementy.size())) + ", sztuk: " + liczba(sztuk),
                      "Wartość magazynu (tylko elementy z ceną): " + sumyWalut(sumy)};
    return true;
}

bool LogikaRaportow::niskieStany(const ParametryRaportu& p, Raport& r) {
    FiltrElementow f;
    f.tylkoNiskieStany = true;
    f.limit = p.limitWierszy + 1;
    std::vector<Element> elementy;
    if (!magazyn_.pobierzElementy(f, elementy)) return ustawBlad(magazyn_.pobierzBlad());
    if (static_cast<int>(elementy.size()) > p.limitWierszy) {
        elementy.resize(static_cast<size_t>(p.limitWierszy));
        r.ucieto = true;
    }
    r.opis = "Elementy, których ilość jest mniejsza od stanu minimalnego.";
    r.naglowki = {"Symbol", "Producent", "Wartość", "Obudowa", "Kategoria", "Miejsce", "Ilość", "Minimum", "Do zamówienia"};
    r.wyrownajDoPrawej = {false, false, false, false, false, false, true, true, true};
    std::map<std::string, double> sumy;
    for (const Element& e : elementy) {
        const int brak = e.iloscMinimalna - e.iloscDostepna;
        if (e.cena) sumy[e.waluta] += *e.cena * brak;
        r.wiersze.push_back({e.symbol, e.producent, e.wartosc, e.obudowa, e.kategoriaPelna, e.miejsceTekst(), liczba(e.iloscDostepna),
                             liczba(e.iloscMinimalna), liczba(brak)});
    }
    r.podsumowanie = {"Pozycji do uzupełnienia: " + liczba(static_cast<long long>(elementy.size())),
                      "Szacunkowy koszt uzupełnienia do minimum: " + sumyWalut(sumy)};
    return true;
}

bool LogikaRaportow::wartoscWgKategorii(const ParametryRaportu&, Raport& r) {
    if (!magazyn_.odswiezKategorie()) return ustawBlad(magazyn_.pobierzBlad());
    PolaczenieBazy* pol = manager_.pobierzPolaczenie();
    if (!pol) return ustawBlad("Brak połączenia z bazą danych.");
    auto odp = pol->zapytanie(
        "SELECT id_kategorii, waluta, count(*) AS elementow, COALESCE(sum(ilosc_dostepna), 0) AS sztuk, "
        "       COALESCE(sum(ilosc_dostepna * cena_jednostkowa), 0) AS wartosc "
        "FROM tbl_elementy WHERE czy_aktywny GROUP BY id_kategorii, waluta");
    if (!odp) return ustawBlad(pol->pobierzBlad());

    struct Suma { long long elementow = 0, sztuk = 0; std::map<std::string, double> wartosc; };
    std::map<std::string, Suma> wg;  // kategoria główna -> suma
    for (int i = 0; i < odp->liczbaWierszy(); ++i) {
        std::string nazwa = "(bez kategorii)";
        if (!odp->czyNull(i, 0)) {
            const std::string pelna = magazyn_.pelnaNazwaKategorii(odp->liczba(i, 0));
            if (!pelna.empty()) nazwa = pelna.substr(0, pelna.find(" / "));
        }
        Suma& s = wg[nazwa];
        s.elementow += odp->liczba(i, 2);
        s.sztuk += odp->liczba(i, 3);
        const double w = odp->rzeczywista(i, 4);
        if (w != 0.0) s.wartosc[odp->tekst(i, 1).empty() ? "PLN" : odp->tekst(i, 1)] += w;
    }
    r.opis = "Podział według kategorii głównych (z podkategoriami).";
    r.naglowki = {"Kategoria", "Elementów", "Sztuk", "Wartość"};
    r.wyrownajDoPrawej = {false, true, true, true};
    long long e = 0, s = 0;
    std::map<std::string, double> razem;
    for (const auto& [nazwa, suma] : wg) {
        e += suma.elementow;
        s += suma.sztuk;
        for (const auto& [w, v] : suma.wartosc) razem[w] += v;
        r.wiersze.push_back({nazwa, liczba(suma.elementow), liczba(suma.sztuk), sumyWalut(suma.wartosc)});
    }
    r.podsumowanie = {"Razem: " + liczba(e) + " elementów, " + liczba(s) + " sztuk, wartość " + sumyWalut(razem)};
    return true;
}

bool LogikaRaportow::brakiWProjektach(const ParametryRaportu&, Raport& r) {
    std::vector<PozycjaProjektu> braki;
    if (!projekty_.pobierzBraki(std::nullopt, braki)) return ustawBlad(projekty_.pobierzBlad());
    r.opis = "Części potrzebne w aktywnych projektach (poza zakończonymi), których brakuje w magazynie.";
    r.naglowki = {"Projekt", "Symbol", "Producent", "Wartość", "Obudowa", "Oznaczenia", "Brakuje", "Zamówiono"};
    r.wyrownajDoPrawej = {false, false, false, false, false, false, true, true};
    long long sztuk = 0;
    for (const PozycjaProjektu& b : braki) {
        sztuk += b.brakuje();
        r.wiersze.push_back({b.uwagi, b.symbol, b.producent, b.wartosc, b.obudowa, b.oznaczenia, liczba(b.brakuje()),
                             liczba(b.zamowiona)});
    }
    std::stable_sort(r.wiersze.begin(), r.wiersze.end(),
                     [](const auto& a, const auto& b) { return a[0] < b[0]; });
    r.podsumowanie = {"Pozycji: " + liczba(static_cast<long long>(braki.size())) + ", brakuje sztuk: " + liczba(sztuk)};
    return true;
}

bool LogikaRaportow::historiaZmian(const ParametryRaportu& p, Raport& r) {
    PolaczenieBazy* pol = manager_.pobierzPolaczenie();
    if (!pol) return ustawBlad("Brak połączenia z bazą danych.");
    if (!p.dataOd.empty() && !poprawnaData(p.dataOd)) return ustawBlad("Data początkowa ma zły format (RRRR-MM-DD).");
    if (!p.dataDo.empty() && !poprawnaData(p.dataDo)) return ustawBlad("Data końcowa ma zły format (RRRR-MM-DD).");

    std::string sql =
        "SELECT to_char(t.data_transakcji, 'YYYY-MM-DD HH24:MI') AS data, t.typ_transakcji, e.symbol, t.ilosc_zmiana, "
        "       t.ilosc_przed, t.ilosc_po, COALESCE(pr.nazwa, '') AS projekt, COALESCE(t.uzytkownik, '') AS uzytkownik, "
        "       COALESCE(t.komentarz, '') AS komentarz "
        "FROM tbl_transakcje t JOIN tbl_elementy e ON e.id = t.id_elementu "
        "LEFT JOIN tbl_projekty pr ON pr.id = t.id_projektu WHERE true ";
    std::vector<Parametr> param;
    if (!p.dataOd.empty()) {
        param.push_back(p.dataOd);
        sql += "AND t.data_transakcji >= $" + std::to_string(param.size()) + "::date ";
    }
    if (!p.dataDo.empty()) {
        param.push_back(p.dataDo);
        sql += "AND t.data_transakcji < ($" + std::to_string(param.size()) + "::date + 1) ";
    }
    sql += "ORDER BY t.data_transakcji DESC, t.id DESC LIMIT " + std::to_string(p.limitWierszy + 1);
    auto odp = pol->zapytanie(sql, param);
    if (!odp) return ustawBlad(pol->pobierzBlad());

    int n = odp->liczbaWierszy();
    if (n > p.limitWierszy) {
        n = p.limitWierszy;
        r.ucieto = true;
    }
    r.opis = "Zakres: " + (p.dataOd.empty() ? std::string("od początku") : "od " + p.dataOd) + " " +
             (p.dataDo.empty() ? std::string("do dziś") : "do " + p.dataDo);
    r.naglowki = {"Data", "Typ", "Symbol", "Zmiana", "Przed", "Po", "Projekt", "Użytkownik", "Komentarz"};
    r.wyrownajDoPrawej = {false, false, false, true, true, true, false, false, false};
    long long przyjete = 0, wydane = 0;
    for (int i = 0; i < n; ++i) {
        const long long zmiana = odp->liczba(i, 3);
        (zmiana > 0 ? przyjete : wydane) += zmiana;
        r.wiersze.push_back({odp->tekst(i, 0), odp->tekst(i, 1), odp->tekst(i, 2), (zmiana > 0 ? "+" : "") + liczba(zmiana),
                             odp->tekst(i, 4), odp->tekst(i, 5), odp->tekst(i, 6), odp->tekst(i, 7), odp->tekst(i, 8)});
    }
    r.podsumowanie = {"Operacji: " + liczba(n) + ", przybyło sztuk: " + liczba(przyjete) + ", ubyło: " + liczba(-wydane)};
    return true;
}
