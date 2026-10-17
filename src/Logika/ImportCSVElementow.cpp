#include "ImportCSVElementow.h"

#include "SzeregiE.h"
#include "../Import/ParserCSV.h"
#include "../Narzedzia/Liczby.h"

#include <cctype>
#include <map>

namespace ImportCSVElementow {
namespace {

enum class Pole {
    Symbol, Wartosc, Jednostka, Tolerancja, Obudowa, Parametry, Ilosc, Minimum, Kategoria, Miejsce, Kontener,
    Szuflada, Przegroda, Producent, SymbolProducenta, Opis, Cena, Waluta, Tagi, Datasheet, Typ, Moc, Napiecie,
    Dielektryk
};

std::string przytnij(const std::string& s) {
    const char* sp = " \t\r\n";
    const size_t a = s.find_first_not_of(sp);
    if (a == std::string::npos) return std::string();
    return s.substr(a, s.find_last_not_of(sp) - a + 1);
}

/// Nagłówek -> klucz: małe litery, bez polskich znaków, spacji i znaków specjalnych
std::string klucz(const std::string& naglowek) {
    static const std::pair<const char*, char> pl[] = {
        {"ą", 'a'}, {"ć", 'c'}, {"ę", 'e'}, {"ł", 'l'}, {"ń", 'n'}, {"ó", 'o'}, {"ś", 's'}, {"ź", 'z'}, {"ż", 'z'},
        {"Ą", 'a'}, {"Ć", 'c'}, {"Ę", 'e'}, {"Ł", 'l'}, {"Ń", 'n'}, {"Ó", 'o'}, {"Ś", 's'}, {"Ź", 'z'}, {"Ż", 'z'}};
    std::string w;
    for (size_t i = 0; i < naglowek.size();) {
        bool zamieniono = false;
        for (const auto& [z, c] : pl) {
            const std::string zz = z;
            if (naglowek.compare(i, zz.size(), zz) == 0) { w += c; i += zz.size(); zamieniono = true; break; }
        }
        if (zamieniono) continue;
        const unsigned char c = static_cast<unsigned char>(naglowek[i++]);
        if (std::isalnum(c) && c < 0x80) w += static_cast<char>(std::tolower(c));
    }
    return w;
}

const std::map<std::string, Pole>& aliasy() {
    static const std::map<std::string, Pole> m = {
        {"symbol", Pole::Symbol}, {"nazwa", Pole::Symbol}, {"name", Pole::Symbol}, {"part", Pole::Symbol},
        {"wartosc", Pole::Wartosc}, {"value", Pole::Wartosc}, {"wart", Pole::Wartosc},
        {"jednostka", Pole::Jednostka}, {"unit", Pole::Jednostka},
        {"tolerancja", Pole::Tolerancja}, {"tolerance", Pole::Tolerancja}, {"tol", Pole::Tolerancja},
        {"obudowa", Pole::Obudowa}, {"package", Pole::Obudowa}, {"footprint", Pole::Obudowa}, {"case", Pole::Obudowa},
        {"parametry", Pole::Parametry}, {"parametr", Pole::Parametry}, {"params", Pole::Parametry}, {"parameters", Pole::Parametry},
        {"ilosc", Pole::Ilosc}, {"quantity", Pole::Ilosc}, {"qty", Pole::Ilosc}, {"stan", Pole::Ilosc}, {"sztuk", Pole::Ilosc},
        {"minimum", Pole::Minimum}, {"min", Pole::Minimum}, {"stanminimalny", Pole::Minimum}, {"minimalny", Pole::Minimum},
        {"kategoria", Pole::Kategoria}, {"category", Pole::Kategoria}, {"grupa", Pole::Kategoria},
        {"miejsce", Pole::Miejsce}, {"lokalizacja", Pole::Miejsce}, {"location", Pole::Miejsce},
        {"miejscemagazynowania", Pole::Miejsce},
        {"kontener", Pole::Kontener}, {"container", Pole::Kontener}, {"szafka", Pole::Kontener},
        {"szuflada", Pole::Szuflada}, {"drawer", Pole::Szuflada}, {"szufladka", Pole::Szuflada},
        {"przegroda", Pole::Przegroda}, {"bin", Pole::Przegroda}, {"compartment", Pole::Przegroda},
        {"producent", Pole::Producent}, {"manufacturer", Pole::Producent}, {"mfr", Pole::Producent},
        {"symbolproducenta", Pole::SymbolProducenta}, {"mpn", Pole::SymbolProducenta},
        {"numerproducenta", Pole::SymbolProducenta}, {"partnumber", Pole::SymbolProducenta},
        {"opis", Pole::Opis}, {"description", Pole::Opis}, {"desc", Pole::Opis},
        {"cena", Pole::Cena}, {"price", Pole::Cena}, {"cenajednostkowa", Pole::Cena},
        {"waluta", Pole::Waluta}, {"currency", Pole::Waluta},
        {"tagi", Pole::Tagi}, {"tags", Pole::Tagi},
        {"datasheet", Pole::Datasheet}, {"karta", Pole::Datasheet},
        {"typ", Pole::Typ}, {"rodzaj", Pole::Typ}, {"type", Pole::Typ},
        {"moc", Pole::Moc}, {"power", Pole::Moc},
        {"napiecie", Pole::Napiecie}, {"voltage", Pole::Napiecie},
        {"dielektryk", Pole::Dielektryk}, {"dielectric", Pole::Dielektryk}};
    return m;
}

bool rodzajZTekstu(const std::string& t, RodzajElementu& r) {
    const std::string k = klucz(t);
    if (k == "r" || k == "rezystor" || k == "rezystory" || k == "resistor") { r = RodzajElementu::Rezystor; return true; }
    if (k == "c" || k == "kondensator" || k == "kondensatory" || k == "capacitor") { r = RodzajElementu::Kondensator; return true; }
    if (k == "l" || k == "cewka" || k == "dlawik" || k == "cewki" || k == "inductor") { r = RodzajElementu::Cewka; return true; }
    return false;
}

std::string przecinek(const std::string& liczba) {
    std::string w = liczba;
    for (char& c : w) if (c == '.') c = ',';
    return w;
}

std::string cudzyslow(const std::string& s) {
    if (s.find_first_of(";\"\n\r") == std::string::npos) return s;
    std::string w = "\"";
    for (char c : s) { if (c == '"') w += '"'; w += c; }
    return w + "\"";
}

const char* const NAGLOWKI[] = {"Symbol", "Wartość", "Jednostka", "Tolerancja", "Obudowa", "Parametry", "Ilość",
                                "Minimum", "Kategoria", "Kontener", "Szuflada", "Przegroda", "Producent",
                                "Symbol producenta", "Opis", "Cena", "Waluta", "Tagi", "Datasheet"};

} // namespace

Wynik parsuj(const std::string& tekst) {
    Wynik wynik;
    const auto wiersze = ParserCSV::parsuj(tekst);
    if (wiersze.empty()) {
        wynik.blad = "Plik jest pusty.";
        return wynik;
    }
    // nagłówki
    std::map<size_t, Pole> kolumny;
    std::vector<std::string> nieznane;
    for (size_t i = 0; i < wiersze[0].size(); ++i) {
        const std::string naglowek = przytnij(wiersze[0][i]);
        if (naglowek.empty()) continue;
        auto it = aliasy().find(klucz(naglowek));
        if (it == aliasy().end()) nieznane.push_back(naglowek); else kolumny[i] = it->second;
    }
    bool jestSymbol = false, jestTyp = false, jestWartosc = false;
    for (const auto& [i, p] : kolumny) {
        jestSymbol = jestSymbol || p == Pole::Symbol;
        jestTyp = jestTyp || p == Pole::Typ;
        jestWartosc = jestWartosc || p == Pole::Wartosc;
    }
    if (!jestSymbol && !(jestTyp && jestWartosc)) {
        wynik.blad = "W pierwszym wierszu brakuje nagłówka \"Symbol\" (albo \"Typ\" razem z \"Wartość\"). "
                     "Użyj szablonu CSV jako wzoru.";
        return wynik;
    }
    if (!nieznane.empty()) {
        std::string l;
        for (const std::string& n : nieznane) l += (l.empty() ? "" : ", ") + n;
        wynik.ostrzezenia.push_back("Pominięto nieznane kolumny: " + l + ".");
    }

    for (size_t w = 1; w < wiersze.size(); ++w) {
        PozycjaImportu poz;
        poz.nrWiersza = static_cast<int>(w) + 1;
        Element& e = poz.element;
        std::string miejsce, moc, napiecie, dielektryk, typ, ilosc, minimum, cena;
        bool cokolwiek = false;
        for (const auto& [i, p] : kolumny) {
            if (i >= wiersze[w].size()) continue;
            const std::string v = przytnij(wiersze[w][i]);
            if (v.empty()) continue;
            cokolwiek = true;
            switch (p) {
                case Pole::Symbol: e.symbol = v; break;
                case Pole::Wartosc: e.wartosc = v; break;
                case Pole::Jednostka: e.jednostka = v; break;
                case Pole::Tolerancja: e.tolerancja = v; break;
                case Pole::Obudowa: e.obudowa = v; break;
                case Pole::Parametry: e.parametry = v; break;
                case Pole::Ilosc: ilosc = v; break;
                case Pole::Minimum: minimum = v; break;
                case Pole::Kategoria: poz.kategoria = v; break;
                case Pole::Miejsce: miejsce = v; break;
                case Pole::Kontener: e.kontener = v; break;
                case Pole::Szuflada: e.szuflada = v; break;
                case Pole::Przegroda: e.przegroda = v; break;
                case Pole::Producent: e.producent = v; break;
                case Pole::SymbolProducenta: e.symbolProducenta = v; break;
                case Pole::Opis: e.opis = v; break;
                case Pole::Cena: cena = v; break;
                case Pole::Waluta: e.waluta = v; break;
                case Pole::Tagi: e.tagi = v; break;
                case Pole::Datasheet: e.datasheetUrl = v; break;
                case Pole::Typ: typ = v; break;
                case Pole::Moc: moc = v; break;
                case Pole::Napiecie: napiecie = v; break;
                case Pole::Dielektryk: dielektryk = v; break;
            }
        }
        // wiersz bez żadnej treści (same puste komórki) pomijamy po cichu
        if (!cokolwiek) continue;

        // pole "Miejsce" ("Kontener 1 szufladka A1/1") uzupełnia kontener, szufladę i przegrodę, o ile nie podano ich osobno
        if (!miejsce.empty()) {
            const MiejsceMagazynowe m = MiejsceMagazynowe::parsuj(miejsce);
            if (e.kontener.empty()) e.kontener = m.kontener;
            if (e.szuflada.empty()) e.szuflada = m.szuflada;
            if (e.przegroda.empty()) e.przegroda = m.przegroda;
        }
        // moc, napięcie i dielektryk trafiają do parametrów
        for (const std::string& cz : {moc, napiecie, dielektryk})
            if (!cz.empty()) e.parametry += (e.parametry.empty() ? "" : "; ") + cz;

        // typ + wartość bez symbolu: symbol i jednostka powstają automatycznie
        RodzajElementu rodzaj = RodzajElementu::Rezystor;
        const bool maRodzaj = !typ.empty() && rodzajZTekstu(typ, rodzaj);
        if (!typ.empty() && !maRodzaj) poz.blad = "nieznany typ \"" + typ + "\" (użyj R, C albo L)";
        if (maRodzaj && !e.wartosc.empty()) {
            if (auto v = SzeregiE::parsujWartosc(e.wartosc, rodzaj)) {
                if (e.symbol.empty())
                    e.symbol = SzeregiE::zbudujSymbol("", rodzaj, *v, e.obudowa, e.tolerancja, moc, napiecie, dielektryk);
                e.wartosc = SzeregiE::formatPolaWartosci(*v, rodzaj);
                if (e.jednostka.empty()) e.jednostka = SzeregiE::jednostka(rodzaj);
            } else if (e.symbol.empty()) {
                poz.blad = "nie rozumiem wartości \"" + e.wartosc + "\"";
            }
        }
        if (poz.blad.empty() && e.symbol.empty()) poz.blad = "brak symbolu";

        double liczba = 0;
        if (!ilosc.empty()) {
            std::string c;
            for (char ch : ilosc) if (ch != ' ') c += ch;
            if (!tekstNaLiczbe(c, liczba) || liczba < 0 || liczba != static_cast<long long>(liczba) || liczba > 1e9)
                { if (poz.blad.empty()) poz.blad = "ilość musi być liczbą całkowitą nieujemną (\"" + ilosc + "\")"; }
            else e.iloscDostepna = static_cast<int>(liczba);
        }
        if (!minimum.empty()) {
            if (!tekstNaLiczbe(minimum, liczba) || liczba < 0 || liczba != static_cast<long long>(liczba) || liczba > 1e9)
                { if (poz.blad.empty()) poz.blad = "minimum musi być liczbą całkowitą nieujemną (\"" + minimum + "\")"; }
            else e.iloscMinimalna = static_cast<int>(liczba);
        }
        if (!cena.empty()) {
            if (!tekstNaLiczbe(cena, liczba) || liczba < 0) { if (poz.blad.empty()) poz.blad = "cena musi być liczbą nieujemną (\"" + cena + "\")"; }
            else e.cena = liczba;
        }
        if (!e.waluta.empty()) for (char& c : e.waluta) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if (e.waluta.empty()) e.waluta = "PLN";
        wynik.pozycje.push_back(poz);
    }
    return wynik;
}

std::string naCSV(const std::vector<PozycjaImportu>& pozycje) {
    std::string s = "\xEF\xBB\xBF";
    for (size_t i = 0; i < sizeof(NAGLOWKI) / sizeof(NAGLOWKI[0]); ++i) s += (i ? ";" : "") + std::string(NAGLOWKI[i]);
    s += "\r\n";
    for (const PozycjaImportu& p : pozycje) {
        const Element& e = p.element;
        const std::string cena = e.cena ? przecinek(liczbaNaTekstKlasyczny(*e.cena)) : std::string();
        const std::string pola[] = {e.symbol, e.wartosc, e.jednostka, e.tolerancja, e.obudowa, e.parametry,
                                    std::to_string(e.iloscDostepna), std::to_string(e.iloscMinimalna), p.kategoria,
                                    e.kontener, e.szuflada, e.przegroda, e.producent, e.symbolProducenta, e.opis, cena,
                                    e.waluta, e.tagi, e.datasheetUrl};
        for (size_t i = 0; i < sizeof(pola) / sizeof(pola[0]); ++i) s += (i ? ";" : "") + cudzyslow(pola[i]);
        s += "\r\n";
    }
    return s;
}

std::string szablon() {
    std::vector<PozycjaImportu> p(3);
    auto ustaw = [](PozycjaImportu& poz, const char* symbol, const char* wartosc, const char* jedn, const char* tol,
                    const char* obudowa, const char* param, int ilosc, int min, const char* kat, const char* kont,
                    const char* szuf, const char* opis) {
        Element& e = poz.element;
        e.symbol = symbol; e.wartosc = wartosc; e.jednostka = jedn; e.tolerancja = tol; e.obudowa = obudowa;
        e.parametry = param; e.iloscDostepna = ilosc; e.iloscMinimalna = min; e.kontener = kont; e.szuflada = szuf;
        e.opis = opis;
        poz.kategoria = kat;
    };
    ustaw(p[0], "R-4K7-0603-1%", "4,7k", "Ω", "1%", "0603", "0,1 W", 100, 20, "Elementy bierne / Rezystory / Rezystory SMD",
          "Kontener 1", "A1", "Rezystor 4,7 kΩ 1% 0603");
    ustaw(p[1], "C-100N-0603-50V-X7R", "100n", "F", "10%", "0603", "50 V; X7R", 200, 50,
          "Elementy bierne / Kondensatory / Ceramiczne", "Kontener 1", "B1", "Kondensator 100 nF 50 V X7R 0603");
    ustaw(p[2], "BC547B", "", "", "", "TO-92", "NPN; 45 V; 100 mA", 25, 5, "Półprzewodniki / Tranzystory / Bipolarne",
          "Kontener 2", "C3", "Tranzystor NPN");
    p[2].element.producent = "ON Semiconductor";
    return naCSV(p);
}

} // namespace ImportCSVElementow
