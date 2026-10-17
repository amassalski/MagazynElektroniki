#include "ImportBOM.h"
#include "../Narzedzia/Liczby.h"

#include "ImportEagle.h"
#include "ImportKiCad.h"
#include "ParserCSV.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>

int WynikImportuBOM::liczbaSztuk() const {
    int suma = 0;
    for (const PozycjaBOM& p : pozycje) suma += p.ilosc;
    return suma;
}

namespace ImportBOM {

namespace {

std::string przytnij(const std::string& s) {
    const char* spacje = " \t\r\n";
    const size_t a = s.find_first_not_of(spacje);
    if (a == std::string::npos) return std::string();
    return s.substr(a, s.find_last_not_of(spacje) - a + 1);
}

std::string male(const std::string& s) {
    std::string w = s;
    for (char& c : w) {
        if (static_cast<unsigned char>(c) < 128) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return w;
}

void zamien(std::string& s, const std::string& co, const std::string& naCo) {
    if (co.empty()) return;
    for (size_t p = s.find(co); p != std::string::npos; p = s.find(co, p + naCo.size())) {
        s.replace(p, co.size(), naCo);
    }
}

bool czyCyfra(char c) { return c >= '0' && c <= '9'; }

/// Nagłówek -> postać do porównań: małe litery, tylko litery/cyfry/spacje
std::string kluczNaglowka(const std::string& naglowek) {
    std::string t = male(przytnij(naglowek));
    std::string w;
    for (char c : t) {
        if (std::isalnum(static_cast<unsigned char>(c)) || static_cast<unsigned char>(c) >= 0x80) w += c;
        else if (c == ' ' || c == '_' || c == '-' || c == '.') w += ' ';
        // nawiasy i inne znaki pomijamy: "Reference(s)" -> "references"
    }
    // złóż wielokrotne spacje
    std::string wynik;
    bool spacja = false;
    for (char c : w) {
        if (c == ' ') {
            spacja = true;
            continue;
        }
        if (spacja && !wynik.empty()) wynik += ' ';
        spacja = false;
        wynik += c;
    }
    return wynik;
}

enum Pole { P_OZNACZENIA, P_WARTOSC, P_OBUDOWA, P_ILOSC, P_SYMBOL, P_PRODUCENT, P_OPIS, P_DNP, P_LICZBA };

/// Nazwy kolumn (w kolejności ważności) dla każdego pola
const std::vector<std::string>& synonimy(Pole pole) {
    static const std::vector<std::vector<std::string>> tabela = {
        /* P_OZNACZENIA */ {"reference", "references", "refs", "ref", "reference designator",
                            "reference designators", "designator", "designators", "refdes", "ref des",
                            "oznaczenie", "oznaczenia", "parts", "part"},
        /* P_WARTOSC */ {"value", "values", "val", "wartosc", "wartość", "comment"},
        /* P_OBUDOWA */ {"footprint", "package", "obudowa", "case", "case size", "pcb footprint",
                         "footprint name"},
        /* P_ILOSC */ {"qty", "quantity", "quantity per pcb", "qty per board", "qnty", "ilosc", "ilość",
                       "count", "liczba"},
        /* P_SYMBOL */ {"mpn", "manufacturer part number", "manufacturer part", "mfr part number",
                        "mfr part", "mfg part number", "manufacturer pn", "mfr pn", "part number",
                        "partnumber", "pn", "symbol", "symbol producenta", "manufacturer number"},
        /* P_PRODUCENT */ {"manufacturer", "manufacturer name", "producent", "mfr", "mfg", "manufacturer1"},
        /* P_OPIS */ {"description", "opis", "desc"},
        /* P_DNP */ {"dnp", "do not populate", "not fitted", "nie montowac", "nie montować"},
    };
    return tabela[static_cast<size_t>(pole)];
}

int ileKolumnRozpoznanych(const ParserCSV::Wiersz& wiersz) {
    int ile = 0;
    std::set<int> uzyte;
    for (const std::string& komorka : wiersz) {
        const std::string k = kluczNaglowka(komorka);
        for (int p = 0; p < P_LICZBA; ++p) {
            const auto& syn = synonimy(static_cast<Pole>(p));
            if (std::find(syn.begin(), syn.end(), k) != syn.end() && uzyte.insert(p).second) ++ile;
        }
    }
    return ile;
}

bool tak(const std::string& s) {
    const std::string t = male(przytnij(s));
    return t == "1" || t == "x" || t == "yes" || t == "tak" || t == "true" || t == "dnp";
}

int liczbaZTekstu(const std::string& s, int domyslna) {
    const std::string t = przytnij(s);
    if (t.empty()) return domyslna;
    char* koniec = nullptr;
    const double v = strtodKlasyczny(t.c_str(), &koniec);
    if (koniec == t.c_str()) return domyslna;
    return v < 0 ? domyslna : static_cast<int>(v + 0.5);
}

std::string polaczOznaczenia(const std::vector<std::string>& lista) {
    std::string w;
    for (size_t i = 0; i < lista.size(); ++i) {
        if (i) w += ", ";
        w += lista[i];
    }
    return w;
}

bool zaczynaSieOd(const std::string& tekst, const char* wzorzec) {
    size_t i = 0;
    if (tekst.compare(0, 3, "\xEF\xBB\xBF") == 0) i = 3;
    while (i < tekst.size() && std::isspace(static_cast<unsigned char>(tekst[i]))) ++i;
    return tekst.compare(i, std::string(wzorzec).size(), wzorzec) == 0;
}

} // namespace

// ---------------------------------------------------------------------------
// Oznaczenia, normalizacja
// ---------------------------------------------------------------------------

bool porownajNaturalnie(const std::string& a, const std::string& b) {
    size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        if (czyCyfra(a[i]) && czyCyfra(b[j])) {
            size_t i2 = i, j2 = j;
            while (i2 < a.size() && czyCyfra(a[i2])) ++i2;
            while (j2 < b.size() && czyCyfra(b[j2])) ++j2;
            std::string na = a.substr(i, i2 - i), nb = b.substr(j, j2 - j);
            na.erase(0, std::min(na.find_first_not_of('0'), na.size()));
            nb.erase(0, std::min(nb.find_first_not_of('0'), nb.size()));
            if (na.size() != nb.size()) return na.size() < nb.size();
            if (na != nb) return na < nb;
            i = i2;
            j = j2;
        } else {
            if (a[i] != b[j]) return a[i] < b[j];
            ++i;
            ++j;
        }
    }
    return (a.size() - i) < (b.size() - j);
}

std::vector<std::string> rozbijOznaczenia(const std::string& tekst) {
    std::vector<std::string> tokeny;
    std::string t;
    for (char c : tekst) {
        if (c == ',' || c == ';' || c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (!t.empty()) tokeny.push_back(t);
            t.clear();
        } else {
            t += c;
        }
    }
    if (!t.empty()) tokeny.push_back(t);

    std::vector<std::string> wynik;
    for (const std::string& token : tokeny) {
        // zakres "R1-R5" albo "R1-5" (ten sam prefiks)
        const size_t kreska = token.find('-');
        if (kreska != std::string::npos && kreska > 0) {
            const std::string lewy = token.substr(0, kreska);
            std::string prawy = token.substr(kreska + 1);
            size_t pl = lewy.size();
            while (pl > 0 && czyCyfra(lewy[pl - 1])) --pl;
            const std::string prefiks = lewy.substr(0, pl);
            const std::string numL = lewy.substr(pl);
            if (!numL.empty() && !prawy.empty()) {
                std::string numP = prawy;
                if (prawy.compare(0, prefiks.size(), prefiks) == 0 && !prefiks.empty()) numP = prawy.substr(prefiks.size());
                bool tylkoCyfry = !numP.empty();
                for (char c : numP) tylkoCyfry = tylkoCyfry && czyCyfra(c);
                if (tylkoCyfry && numL.size() < 7 && numP.size() < 7) {
                    const int od = std::atoi(numL.c_str()), doo = std::atoi(numP.c_str());
                    if (doo >= od && doo - od < 1000) {
                        for (int n = od; n <= doo; ++n) wynik.push_back(prefiks + std::to_string(n));
                        continue;
                    }
                }
            }
        }
        wynik.push_back(token);
    }
    return wynik;
}

std::string normalizujWartosc(const std::string& wartosc) {
    std::string t = male(przytnij(wartosc));
    zamien(t, "\xC2\xB5", "u");      // µ (U+00B5)
    zamien(t, "\xCE\xBC", "u");      // μ (U+03BC)
    zamien(t, "\xCE\xA9", "");       // Ω (U+03A9)
    zamien(t, "\xCF\x89", "");       // ω
    zamien(t, "\xE2\x84\xA6", "");   // Ω (U+2126)
    zamien(t, "ohms", "");
    zamien(t, "ohm", "");
    zamien(t, " ", "");
    zamien(t, ",", ".");
    // tylko pierwsza część: "100n/50v" -> "100n", "10k (1%)" -> "10k"
    for (const char sep : {'/', '('}) {
        const size_t p = t.find(sep);
        if (p != std::string::npos) t = t.substr(0, p);
    }
    // jednostka na końcu: 100nf -> 100n, 10uf -> 10u
    if (t.size() > 1 && t.back() == 'f' && std::string("pnumk").find(t[t.size() - 2]) != std::string::npos) t.pop_back();
    // "4k7" -> "4.7k", "4r7" -> "4.7", "2u2" -> "2.2u"
    for (size_t i = 1; i + 1 < t.size(); ++i) {
        if (std::string("rkmunp").find(t[i]) != std::string::npos && czyCyfra(t[i - 1]) && czyCyfra(t[i + 1])) {
            bool reszta = true;
            for (size_t k = i + 1; k < t.size(); ++k) reszta = reszta && czyCyfra(t[k]);
            bool poczatek = true;
            for (size_t k = 0; k < i; ++k) poczatek = poczatek && (czyCyfra(t[k]) || t[k] == '.');
            if (reszta && poczatek) {
                const char litera = t[i];
                t = t.substr(0, i) + "." + t.substr(i + 1) + (litera == 'r' ? "" : std::string(1, litera));
                break;
            }
        }
    }
    // koniec: "10r" -> "10", "4.70k" -> "4.7k"
    if (t.size() > 1 && t.back() == 'r' && czyCyfra(t[t.size() - 2])) t.pop_back();
    const size_t kropka = t.find('.');
    if (kropka != std::string::npos) {
        size_t koniecLiczby = kropka + 1;
        while (koniecLiczby < t.size() && czyCyfra(t[koniecLiczby])) ++koniecLiczby;
        size_t ostatnia = koniecLiczby;
        while (ostatnia > kropka + 1 && t[ostatnia - 1] == '0') --ostatnia;
        if (ostatnia == kropka + 1) --ostatnia; // zostało "10." -> "10"
        t = t.substr(0, ostatnia) + t.substr(koniecLiczby);
        if (!t.empty() && t[0] == '.') t.insert(0, "0");
    }
    return t;
}

std::string normalizujObudowe(const std::string& obudowa) {
    std::string t = male(przytnij(obudowa));
    const size_t dwukropek = t.rfind(':');
    if (dwukropek != std::string::npos) t = t.substr(dwukropek + 1);

    static const char* kody[] = {"01005", "0201", "0402", "0603", "0805", "1008", "1206",
                                 "1210", "1812", "2010", "2220", "2512"};
    // kod calowy jako osobny ciąg cyfr: "r0805", "r_0805_2012metric", "c-0603k"
    for (size_t i = 0; i < t.size(); ++i) {
        if (!czyCyfra(t[i]) || (i > 0 && czyCyfra(t[i - 1]))) continue;
        size_t k = i;
        while (k < t.size() && czyCyfra(t[k])) ++k;
        const std::string ciag = t.substr(i, k - i);
        for (const char* kod : kody) {
            if (ciag == kod) return kod;
        }
    }
    std::string wynik;
    for (char c : t) {
        if (std::isalnum(static_cast<unsigned char>(c))) wynik += c;
    }
    return wynik;
}

// ---------------------------------------------------------------------------
// Grupowanie
// ---------------------------------------------------------------------------

std::vector<PozycjaBOM> zgrupuj(const std::vector<PozycjaBOM>& surowe, std::vector<std::string>* ostrzezenia) {
    std::vector<PozycjaBOM> wynik;
    std::map<std::string, size_t> indeks;
    std::vector<std::vector<std::string>> oznaczenia;

    for (const PozycjaBOM& p : surowe) {
        if (przytnij(p.wartosc).empty() && przytnij(p.symbol).empty()) {
            if (ostrzezenia) {
                ostrzezenia->push_back("Pominięto pozycję bez wartości i symbolu" +
                                       (p.oznaczenia.empty() ? std::string() : " (" + p.oznaczenia + ")"));
            }
            continue;
        }
        const std::string klucz = male(przytnij(p.wartosc)) + "|" + male(przytnij(p.obudowa)) + "|" +
                                  male(przytnij(p.symbol)) + "|" + male(przytnij(p.producent));
        auto it = indeks.find(klucz);
        if (it == indeks.end()) {
            indeks[klucz] = wynik.size();
            wynik.push_back(p);
            wynik.back().wartosc = przytnij(p.wartosc);
            wynik.back().obudowa = przytnij(p.obudowa);
            wynik.back().symbol = przytnij(p.symbol);
            wynik.back().producent = przytnij(p.producent);
            oznaczenia.push_back(rozbijOznaczenia(p.oznaczenia));
        } else {
            PozycjaBOM& cel = wynik[it->second];
            cel.ilosc += p.ilosc;
            if (cel.opis.empty()) cel.opis = p.opis;
            const auto nowe = rozbijOznaczenia(p.oznaczenia);
            oznaczenia[it->second].insert(oznaczenia[it->second].end(), nowe.begin(), nowe.end());
        }
    }
    for (size_t i = 0; i < wynik.size(); ++i) {
        auto& lista = oznaczenia[i];
        std::sort(lista.begin(), lista.end(), porownajNaturalnie);
        lista.erase(std::unique(lista.begin(), lista.end()), lista.end());
        wynik[i].oznaczenia = polaczOznaczenia(lista);
    }
    std::stable_sort(wynik.begin(), wynik.end(), [](const PozycjaBOM& a, const PozycjaBOM& b) {
        // wg pierwszego oznaczenia (R przed C itd. zachowuje kolejność alfabetyczną prefiksu)
        return porownajNaturalnie(a.oznaczenia, b.oznaczenia);
    });
    return wynik;
}

// ---------------------------------------------------------------------------
// CSV
// ---------------------------------------------------------------------------

bool parsujCSV(const std::string& tekst, WynikImportuBOM& wynik, std::string& blad) {
    const auto wiersze = ParserCSV::parsuj(tekst);
    if (wiersze.empty()) {
        blad = "Plik jest pusty.";
        return false;
    }

    // Nagłówek: pierwszy z początkowych wierszy, w którym rozpoznano co najmniej dwie kolumny
    size_t naglowek = wiersze.size();
    for (size_t i = 0; i < wiersze.size() && i < 15; ++i) {
        if (ileKolumnRozpoznanych(wiersze[i]) >= 2) {
            naglowek = i;
            break;
        }
    }
    if (naglowek == wiersze.size()) {
        blad = "Nie rozpoznano nagłówków kolumn. Oczekiwane nazwy to m.in. Reference, Value, Footprint, "
               "Qty (lub ich polskie odpowiedniki).";
        return false;
    }

    int kolumna[P_LICZBA];
    for (int& k : kolumna) k = -1;
    for (int p = 0; p < P_LICZBA; ++p) {
        int najlepszy = -1;
        size_t waga = synonimy(static_cast<Pole>(p)).size();
        for (size_t c = 0; c < wiersze[naglowek].size(); ++c) {
            const std::string k = kluczNaglowka(wiersze[naglowek][c]);
            const auto& syn = synonimy(static_cast<Pole>(p));
            const auto it = std::find(syn.begin(), syn.end(), k);
            if (it != syn.end()) {
                const size_t w = static_cast<size_t>(it - syn.begin());
                if (w < waga) {
                    waga = w;
                    najlepszy = static_cast<int>(c);
                }
            }
        }
        kolumna[p] = najlepszy;
    }
    if (kolumna[P_WARTOSC] < 0 && kolumna[P_SYMBOL] < 0) {
        blad = "W pliku brakuje kolumny z wartością (Value) albo numerem części (MPN).";
        return false;
    }

    auto komorka = [&](const ParserCSV::Wiersz& w, Pole p) -> std::string {
        const int c = kolumna[p];
        if (c < 0 || static_cast<size_t>(c) >= w.size()) return std::string();
        return przytnij(w[static_cast<size_t>(c)]);
    };

    std::vector<PozycjaBOM> surowe;
    int pominiete = 0;
    for (size_t i = naglowek + 1; i < wiersze.size(); ++i) {
        const auto& w = wiersze[i];
        if (kolumna[P_DNP] >= 0 && tak(komorka(w, P_DNP))) {
            ++pominiete;
            continue;
        }
        PozycjaBOM p;
        p.oznaczenia = komorka(w, P_OZNACZENIA);
        p.wartosc = komorka(w, P_WARTOSC);
        p.obudowa = komorka(w, P_OBUDOWA);
        p.symbol = komorka(w, P_SYMBOL);
        p.producent = komorka(w, P_PRODUCENT);
        p.opis = komorka(w, P_OPIS);
        if (p.wartosc.empty() && p.symbol.empty() && p.oznaczenia.empty()) continue; // pusty wiersz

        const int zRef = static_cast<int>(rozbijOznaczenia(p.oznaczenia).size());
        if (kolumna[P_ILOSC] >= 0 && !komorka(w, P_ILOSC).empty()) {
            p.ilosc = std::max(1, liczbaZTekstu(komorka(w, P_ILOSC), 1));
        } else {
            p.ilosc = std::max(1, zRef);
        }
        surowe.push_back(p);
    }
    if (pominiete > 0) {
        wynik.ostrzezenia.push_back("Pominięto " + std::to_string(pominiete) + " wierszy oznaczonych jako niemontowane (DNP).");
    }

    wynik.pozycje = zgrupuj(surowe, &wynik.ostrzezenia);
    wynik.format = "CSV";
    if (wynik.pozycje.empty()) {
        blad = "W pliku nie znaleziono żadnych pozycji.";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Rozpoznanie formatu
// ---------------------------------------------------------------------------

bool parsujTekst(const std::string& tekst, WynikImportuBOM& wynik, std::string& blad) {
    wynik = WynikImportuBOM();
    blad.clear();

    if (zaczynaSieOd(tekst, "(kicad_sch") || zaczynaSieOd(tekst, "(kicad_pcb") ||
        zaczynaSieOd(tekst, "(export")) {
        blad = "Ten plik KiCada nie jest listą części. W KiCadzie użyj: Schemat → Narzędzia → Generuj "
               "zestawienie materiałów (CSV) albo Plik → Eksport → Netlista (XML).";
        return false;
    }
    if (zaczynaSieOd(tekst, "<")) {
        // XML: rodzaj poznajemy po elemencie głównym
        const size_t eagle = tekst.find("<eagle");
        const size_t kicad = tekst.find("<export");
        if (eagle != std::string::npos && (kicad == std::string::npos || eagle < kicad)) {
            wynik.format = "Eagle (schemat)";
            return ImportEagle::parsujSchemat(tekst, wynik, blad);
        }
        if (kicad != std::string::npos) {
            wynik.format = "KiCad (netlista XML)";
            return ImportKiCad::parsujNetliste(tekst, wynik, blad);
        }
        blad = "Nierozpoznany format XML (oczekiwano schematu Eagle albo netlisty KiCada).";
        return false;
    }
    const bool ok = parsujCSV(tekst, wynik, blad);
    if (ok) wynik.format = "CSV";
    return ok;
}

bool wczytajPlik(const std::string& sciezka, WynikImportuBOM& wynik, std::string& blad) {
    std::ifstream plik(std::filesystem::u8path(sciezka), std::ios::binary);
    if (!plik) {
        blad = "Nie można otworzyć pliku: " + sciezka;
        return false;
    }
    const std::string tekst((std::istreambuf_iterator<char>(plik)), std::istreambuf_iterator<char>());
    if (tekst.empty()) {
        blad = "Plik jest pusty.";
        return false;
    }
    // KiCad zapisuje .kicad_sch jako S-wyrażenie: zaczyna się od "(kicad_sch"
    return parsujTekst(tekst, wynik, blad);
}

} // namespace ImportBOM
