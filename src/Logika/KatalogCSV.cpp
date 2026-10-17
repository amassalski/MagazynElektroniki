#include "KatalogCSV.h"
#include "../Import/ParserCSV.h"

#include <cctype>
#include <map>

const ParametrKatalogu* PozycjaKatalogu::parametr(const std::string& n) const {
    auto male = [](std::string s) { for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return s; };
    for (const ParametrKatalogu& p : parametry)
        if (male(p.nazwa) == male(n)) return &p;
    return nullptr;
}

namespace KatalogCSV {
namespace {

std::string przytnij(const std::string& s) {
    const char* sp = " \t\r\n";
    const size_t a = s.find_first_not_of(sp);
    if (a == std::string::npos) return std::string();
    return s.substr(a, s.find_last_not_of(sp) - a + 1);
}

std::string klucz(const std::string& naglowek) {
    static const std::pair<const char*, char> pl[] = {
        {"ą", 'a'}, {"ć", 'c'}, {"ę", 'e'}, {"ł", 'l'}, {"ń", 'n'}, {"ó", 'o'}, {"ś", 's'}, {"ź", 'z'}, {"ż", 'z'},
        {"Ą", 'a'}, {"Ć", 'c'}, {"Ę", 'e'}, {"Ł", 'l'}, {"Ń", 'n'}, {"Ó", 'o'}, {"Ś", 's'}, {"Ź", 'z'}, {"Ż", 'z'}};
    std::string w;
    for (size_t i = 0; i < naglowek.size();) {
        bool zam = false;
        for (const auto& [z, c] : pl) {
            const std::string zz = z;
            if (naglowek.compare(i, zz.size(), zz) == 0) { w += c; i += zz.size(); zam = true; break; }
        }
        if (zam) continue;
        const unsigned char c = static_cast<unsigned char>(naglowek[i++]);
        if (std::isalnum(c) && c < 0x80) w += static_cast<char>(std::tolower(c));
    }
    return w;
}

enum class Pole { Symbol, Producent, Opis, Obudowa, Rodzina, Datasheet, Uwagi, Parametry };

const std::map<std::string, Pole>& aliasy() {
    static const std::map<std::string, Pole> m = {
        {"symbol", Pole::Symbol}, {"nazwa", Pole::Symbol}, {"name", Pole::Symbol}, {"mpn", Pole::Symbol},
        {"partnumber", Pole::Symbol}, {"part", Pole::Symbol},
        {"producent", Pole::Producent}, {"manufacturer", Pole::Producent}, {"mfr", Pole::Producent},
        {"opis", Pole::Opis}, {"description", Pole::Opis}, {"desc", Pole::Opis},
        {"obudowa", Pole::Obudowa}, {"package", Pole::Obudowa}, {"case", Pole::Obudowa}, {"footprint", Pole::Obudowa},
        {"rodzina", Pole::Rodzina}, {"kategoria", Pole::Rodzina}, {"category", Pole::Rodzina}, {"family", Pole::Rodzina},
        {"grupa", Pole::Rodzina},
        {"datasheet", Pole::Datasheet}, {"karta", Pole::Datasheet}, {"kartakatalogowa", Pole::Datasheet},
        {"uwagi", Pole::Uwagi}, {"notes", Pole::Uwagi}, {"komentarz", Pole::Uwagi},
        {"parametry", Pole::Parametry}, {"parameters", Pole::Parametry}, {"params", Pole::Parametry}};
    return m;
}

std::string cudzyslow(const std::string& s) {
    if (s.find_first_of(";\"\n\r") == std::string::npos) return s;
    std::string w = "\"";
    for (char c : s) { if (c == '"') w += '"'; w += c; }
    return w + "\"";
}

} // namespace

std::vector<ParametrKatalogu> parsujParametry(const std::string& tekst, const std::string& zrodlo) {
    std::vector<ParametrKatalogu> wynik;
    size_t start = 0;
    while (start <= tekst.size()) {
        size_t koniec = tekst.find(';', start);
        if (koniec == std::string::npos) koniec = tekst.size();
        const std::string kawalek = tekst.substr(start, koniec - start);
        const size_t r = kawalek.find('=');
        if (r != std::string::npos) {
            const std::string n = przytnij(kawalek.substr(0, r)), w = przytnij(kawalek.substr(r + 1));
            if (!n.empty() && !w.empty()) wynik.push_back({n, w, zrodlo});
        }
        start = koniec + 1;
    }
    return wynik;
}

std::string parametryNaTekst(const std::vector<ParametrKatalogu>& p) {
    std::string w;
    for (const ParametrKatalogu& x : p) w += (w.empty() ? "" : "; ") + x.nazwa + "=" + x.wartosc;
    return w;
}

Wynik parsuj(const std::string& tekst) {
    Wynik wynik;
    const auto wiersze = ParserCSV::parsuj(tekst);
    if (wiersze.empty()) { wynik.blad = "Plik jest pusty."; return wynik; }
    std::map<size_t, Pole> kolumny;
    std::map<size_t, std::string> parametry;   // dowolne inne kolumny
    bool jestSymbol = false;
    for (size_t i = 0; i < wiersze[0].size(); ++i) {
        const std::string n = przytnij(wiersze[0][i]);
        if (n.empty()) continue;
        auto it = aliasy().find(klucz(n));
        if (it == aliasy().end()) parametry[i] = n;
        else { kolumny[i] = it->second; jestSymbol = jestSymbol || it->second == Pole::Symbol; }
    }
    if (!jestSymbol) {
        wynik.blad = "W pierwszym wierszu brakuje nagłówka \"Symbol\". Użyj szablonu CSV jako wzoru.";
        return wynik;
    }
    for (size_t w = 1; w < wiersze.size(); ++w) {
        Pozycja poz;
        poz.nrWiersza = static_cast<int>(w) + 1;
        PozycjaKatalogu& k = poz.katalog;
        for (size_t c = 0; c < wiersze[w].size(); ++c) {
            const std::string v = przytnij(wiersze[w][c]);
            if (v.empty()) continue;
            auto it = kolumny.find(c);
            if (it != kolumny.end()) {
                switch (it->second) {
                    case Pole::Symbol: k.symbol = v; break;
                    case Pole::Producent: k.producent = v; break;
                    case Pole::Opis: k.opis = v; break;
                    case Pole::Obudowa: k.obudowa = v; break;
                    case Pole::Rodzina: k.rodzina = v; break;
                    case Pole::Datasheet: k.datasheetUrl = v; break;
                    case Pole::Uwagi: k.uwagi = v; break;
                    case Pole::Parametry:
                        for (auto& p : parsujParametry(v)) k.parametry.push_back(p);
                        break;
                }
                continue;
            }
            auto pit = parametry.find(c);
            if (pit != parametry.end()) k.parametry.push_back({pit->second, v, "csv"});
        }
        if (k.symbol.empty()) poz.blad = "brak symbolu";
        wynik.pozycje.push_back(std::move(poz));
    }
    return wynik;
}

std::string naCSV(const std::vector<PozycjaKatalogu>& pozycje) {
    std::string w = "\xEF\xBB\xBF" "Symbol;Producent;Opis;Obudowa;Rodzina;Datasheet;Uwagi;Parametry\n";
    for (const PozycjaKatalogu& k : pozycje)
        w += cudzyslow(k.symbol) + ";" + cudzyslow(k.producent) + ";" + cudzyslow(k.opis) + ";" + cudzyslow(k.obudowa) + ";" +
             cudzyslow(k.rodzina) + ";" + cudzyslow(k.datasheetUrl) + ";" + cudzyslow(k.uwagi) + ";" +
             cudzyslow(parametryNaTekst(k.parametry)) + "\n";
    return w;
}

std::string szablon() {
    return "\xEF\xBB\xBF" "Symbol;Producent;Opis;Obudowa;Rodzina;Datasheet;Uwagi;Parametry\n"
           "LM358;Texas Instruments;Podwójny wzmacniacz operacyjny;DIP-8;Wzmacniacze operacyjne;https://www.ti.com/lit/ds/symlink/lm358.pdf;;"
           "\"Napięcie zasilania=3-32 V; Pasmo=1 MHz\"\n"
           "NE555P;Texas Instruments;Układ czasowy;DIP-8;Układy czasowe;;;Napięcie zasilania=4,5-16 V\n"
           "BC547B;onsemi;Tranzystor NPN;TO-92;Tranzystory;;;\"Uce max=45 V; Ic max=100 mA; hFE=200-450\"\n";
}

} // namespace KatalogCSV
