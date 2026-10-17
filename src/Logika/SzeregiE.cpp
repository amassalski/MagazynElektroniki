#include "SzeregiE.h"
#include "../Narzedzia/Liczby.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace SzeregiE {
namespace {

const int E6[] = {10, 15, 22, 33, 47, 68};
const int E12[] = {10, 12, 15, 18, 22, 27, 33, 39, 47, 56, 68, 82};
// E24 nie wynika ze wzoru (np. 26 -> 27, 29 -> 30), więc jest tabelą
const int E24[] = {10, 11, 12, 13, 15, 16, 18, 20, 22, 24, 27, 30, 33, 36, 39, 43, 47, 51, 56, 62, 68, 75, 82, 91};

/// Liczba o 9 cyfrach znaczących: usuwa szum typu 4.7000000000000002
double czysta(double v) {
    return zaokraglij9(v);
}

std::string polskaLiczba(double v) {
    char bufor[40];
    std::snprintf(bufor, sizeof(bufor), "%.3f", v);
    std::string t = bufor;
    while (!t.empty() && t.back() == '0') t.pop_back();
    if (!t.empty() && t.back() == '.') t.pop_back();
    for (char& c : t) if (c == '.') c = ',';
    return t;
}

struct Podzial {
    double mantysa;   ///< 1 <= mantysa < 1000
    int grupa;        ///< wykładnik trójkowy: -12, -9, -6, -3, 0, 3, 6, 9
};

Podzial podziel(double v, RodzajElementu r) {
    int e = static_cast<int>(std::floor(std::log10(v) + 1e-9));
    int g = static_cast<int>(std::floor(e / 3.0)) * 3;
    g = std::max(-12, std::min(9, g));
    // kondensatory: 1000 µF zapisujemy jako 1000u, a nie 1m
    if (r == RodzajElementu::Kondensator && g == -3) g = -6;
    const double m = czysta(v / std::pow(10.0, g));
    return {m, g};
}

const char* przedrostek(int grupa, RodzajElementu r, bool rkm) {
    switch (grupa) {
        case -12: return "p";
        case -9: return "n";
        case -6: return "u";
        case -3: return "m";
        case 0: return rkm ? (r == RodzajElementu::Rezystor ? "R" : r == RodzajElementu::Kondensator ? "F" : "H") : "";
        case 3: return rkm ? "K" : "k";
        case 6: return "M";
        case 9: return "G";
        default: return "";
    }
}

const char* przedrostekSymbol(int grupa) {
    switch (grupa) {
        case -12: return "p";
        case -9: return "n";
        case -6: return "µ";
        case -3: return "m";
        case 3: return "k";
        case 6: return "M";
        case 9: return "G";
        default: return "";
    }
}

std::string przytnij(const std::string& s) {
    const char* sp = " \t\r\n";
    const size_t a = s.find_first_not_of(sp);
    if (a == std::string::npos) return std::string();
    return s.substr(a, s.find_last_not_of(sp) - a + 1);
}

} // namespace

std::string nazwa(Szereg s) {
    switch (s) {
        case Szereg::E6: return "E6";
        case Szereg::E12: return "E12";
        case Szereg::E24: return "E24";
        case Szereg::E48: return "E48";
        case Szereg::E96: return "E96";
    }
    return "";
}

std::optional<Szereg> zNazwy(const std::string& n) {
    std::string t;
    for (char c : n) if (!std::isspace(static_cast<unsigned char>(c))) t += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    for (Szereg s : {Szereg::E6, Szereg::E12, Szereg::E24, Szereg::E48, Szereg::E96})
        if (nazwa(s) == t) return s;
    return std::nullopt;
}

std::string domyslnaTolerancja(Szereg s) {
    switch (s) {
        case Szereg::E6: return "20%";
        case Szereg::E12: return "10%";
        case Szereg::E24: return "5%";
        case Szereg::E48: return "2%";
        case Szereg::E96: return "1%";
    }
    return "";
}

std::vector<int> mantysy(Szereg s) {
    switch (s) {
        case Szereg::E6: return std::vector<int>(std::begin(E6), std::end(E6));
        case Szereg::E12: return std::vector<int>(std::begin(E12), std::end(E12));
        case Szereg::E24: return std::vector<int>(std::begin(E24), std::end(E24));
        case Szereg::E48:
        case Szereg::E96: {
            // E48 i E96 są dokładnie zaokrągleniem 10^(k/N) do trzech cyfr (sprawdzone z tabelami IEC 60063)
            const int n = s == Szereg::E48 ? 48 : 96;
            std::vector<int> w;
            for (int k = 0; k < n; ++k) w.push_back(static_cast<int>(std::lround(100.0 * std::pow(10.0, static_cast<double>(k) / n))));
            return w;
        }
    }
    return {};
}

std::vector<double> wartosci(Szereg s, double od, double doo) {
    std::vector<double> wynik;
    if (!(od > 0) || !(doo >= od)) return wynik;
    const std::vector<int> m = mantysy(s);
    const double dzielnik = s == Szereg::E48 || s == Szereg::E96 ? 100.0 : 10.0;
    const int dek0 = static_cast<int>(std::floor(std::log10(od))) - 1;
    const int dek1 = static_cast<int>(std::floor(std::log10(doo))) + 1;
    for (int d = dek0; d <= dek1; ++d) {
        for (int mm : m) {
            const double v = czysta(mm / dzielnik * std::pow(10.0, d));
            if (v >= od * (1 - 1e-9) && v <= doo * (1 + 1e-9)) wynik.push_back(v);
        }
    }
    std::sort(wynik.begin(), wynik.end());
    wynik.erase(std::unique(wynik.begin(), wynik.end()), wynik.end());
    return wynik;
}

std::string jednostka(RodzajElementu r) {
    return r == RodzajElementu::Rezystor ? "Ω" : r == RodzajElementu::Kondensator ? "F" : "H";
}

std::string nazwaRodzaju(RodzajElementu r) {
    return r == RodzajElementu::Rezystor ? "Rezystor" : r == RodzajElementu::Kondensator ? "Kondensator" : "Cewka";
}

std::string formatRKM(double wartosc, RodzajElementu r) {
    if (!(wartosc > 0)) return "";
    if (r == RodzajElementu::Rezystor && wartosc < 1.0) {   // 0,1 Ω -> 0R1, 0,47 Ω -> 0R47
        const std::string c = polskaLiczba(wartosc);       // "0,47"
        return "0R" + c.substr(c.find(',') == std::string::npos ? c.size() : c.find(',') + 1);
    }
    const Podzial p = podziel(wartosc, r);
    std::string cyfry = polskaLiczba(p.mantysa);
    const std::string lit = przedrostek(p.grupa, r, true);
    const size_t przecinek = cyfry.find(',');
    if (przecinek == std::string::npos) return cyfry + lit;
    return cyfry.substr(0, przecinek) + lit + cyfry.substr(przecinek + 1);
}

std::string formatWartosci(double wartosc, RodzajElementu r) {
    if (!(wartosc > 0)) return "";
    if (r == RodzajElementu::Rezystor && wartosc < 1.0) return polskaLiczba(wartosc);
    const Podzial p = podziel(wartosc, r);
    return polskaLiczba(p.mantysa) + przedrostek(p.grupa, r, false);
}

std::string formatPolaWartosci(double wartosc, RodzajElementu r) {
    const std::string zwykla = formatWartosci(wartosc, r);        // "47", "100n", "4,7k"
    if (zwykla.find(',') == std::string::npos) return zwykla;     // bez ułamka nic nie zmieniamy (47, nie 47R)
    std::string t = formatRKM(wartosc, r);                        // z ułamkiem: litera zamiast przecinka (4k7, 0R47)
    for (char& c : t) if (c == 'K') c = 'k';                      // jak w całym programie: małe k
    return t;
}

std::string formatZJednostka(double wartosc, RodzajElementu r) {
    if (!(wartosc > 0)) return "";
    if (r == RodzajElementu::Rezystor && wartosc < 1.0) return polskaLiczba(wartosc) + " Ω";
    const Podzial p = podziel(wartosc, r);
    return polskaLiczba(p.mantysa) + " " + przedrostekSymbol(p.grupa) + jednostka(r);
}

std::optional<double> parsujWartosc(const std::string& wejscie, RodzajElementu r) {
    const std::string t = przytnij(wejscie);
    if (t.empty()) return std::nullopt;
    size_t i = 0;
    std::string calkowita, ulamek;
    bool przecinek = false;
    auto cyfra = [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; };
    while (i < t.size() && (cyfra(t[i]) || t[i] == ',' || t[i] == '.' || t[i] == ' ')) {
        if (t[i] == ',' || t[i] == '.') {
            if (przecinek) return std::nullopt;
            przecinek = true;
        } else if (cyfra(t[i])) {
            (przecinek ? ulamek : calkowita) += t[i];
        }
        ++i;
    }
    // przedrostek: pierwsza litera (lub µ) po liczbie
    double mnoznik = 1.0;
    std::string reszta = t.substr(i);
    size_t zuzyte = 0;
    if (!reszta.empty()) {
        const unsigned char c0 = static_cast<unsigned char>(reszta[0]);
        char p = 0;
        if (c0 == 0xC2 && reszta.size() >= 2 && static_cast<unsigned char>(reszta[1]) == 0xB5) { p = 'u'; zuzyte = 2; }  // µ
        else if (c0 == 0xCE && reszta.size() >= 2 && static_cast<unsigned char>(reszta[1]) == 0xBC) { p = 'u'; zuzyte = 2; } // μ (grecka)
        else if (c0 < 0x80 && std::isalpha(c0)) { p = static_cast<char>(c0); zuzyte = 1; }
        switch (p) {
            case 0: break;
            case 'R': case 'r': case 'F': case 'f': case 'H': case 'h': mnoznik = 1.0; break;
            case 'p': case 'P': mnoznik = 1e-12; break;
            case 'n': case 'N': mnoznik = 1e-9; break;
            case 'u': case 'U': mnoznik = 1e-6; break;
            case 'k': case 'K': mnoznik = 1e3; break;
            case 'G': case 'g': mnoznik = 1e9; break;
            case 'm': case 'M':
                // rezystory: M = mega, m = mili; kondensatory i cewki: m = mili (M nie występuje)
                mnoznik = (p == 'M' && r == RodzajElementu::Rezystor) ? 1e6 : 1e-3;
                break;
            default: return std::nullopt;
        }
        if (p == 0) return std::nullopt;
        // cyfry zaraz po przedrostku to część ułamkowa: 4k7, 4R7, 4n7
        size_t j = zuzyte;
        while (j < reszta.size() && cyfra(reszta[j])) {
            if (przecinek) return std::nullopt;
            ulamek += reszta[j];
            ++j;
        }
        reszta = reszta.substr(j);
        // reszta może zawierać tylko jednostkę: Ω, ohm, F, H
        std::string jedn;
        for (char c : reszta) if (c != ' ') jedn += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        const bool ok = jedn.empty() || jedn == "f" || jedn == "h" || jedn == "ohm" || jedn == "ohms" || jedn == "r" ||
                        reszta.find("\xCE\xA9") != std::string::npos || reszta.find("\xE2\x84\xA6") != std::string::npos;
        if (!ok) return std::nullopt;
    }
    if (calkowita.empty() && ulamek.empty()) return std::nullopt;
    const double v = strtodKlasyczny(((calkowita.empty() ? "0" : calkowita) + "." + (ulamek.empty() ? "0" : ulamek)).c_str(), nullptr) * mnoznik;
    if (!(v > 0)) return std::nullopt;
    return czysta(v);
}

std::string kategoriaDomyslna(RodzajElementu r, const std::string& obudowa) {
    std::string o;
    for (char c : obudowa) o += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    const bool smd = o.find("SMD") != std::string::npos ||
                     o == "0201" || o == "0402" || o == "0603" || o == "0805" || o == "1206" || o == "1210" ||
                     o == "1812" || o == "2010" || o == "2512";
    switch (r) {
        case RodzajElementu::Rezystor:
            return smd ? "Elementy bierne / Rezystory / Rezystory SMD" : "Elementy bierne / Rezystory / Rezystory THT";
        case RodzajElementu::Kondensator: return "Elementy bierne / Kondensatory / Ceramiczne";
        case RodzajElementu::Cewka: return "Elementy bierne / Cewki i dławiki";
    }
    return "";
}

std::string kolejnaSzuflada(const std::string& baza, int przesuniecie) {
    size_t k = baza.size();
    while (k > 0 && std::isdigit(static_cast<unsigned char>(baza[k - 1]))) --k;
    if (k == baza.size()) return baza;   // brak numeru na końcu
    const std::string cyfry = baza.substr(k);
    const long n = std::strtol(cyfry.c_str(), nullptr, 10) + przesuniecie;
    std::string nowa = std::to_string(n < 0 ? 0 : n);
    while (nowa.size() < cyfry.size()) nowa = "0" + nowa;   // zachowaj wiodące zera: A01 -> A02
    return baza.substr(0, k) + nowa;
}

std::string domyslnyWzorSymbolu(RodzajElementu r) {
    switch (r) {
        case RodzajElementu::Rezystor: return "R-{rkm}-{obudowa}-{tol}";
        case RodzajElementu::Kondensator: return "C-{rkm}-{obudowa}-{napiecie}-{dielektryk}";
        case RodzajElementu::Cewka: return "L-{rkm}-{obudowa}";
    }
    return "";
}

std::string zbudujSymbol(const std::string& wzor, RodzajElementu r, double wartosc, const std::string& obudowa,
                         const std::string& tol, const std::string& moc, const std::string& napiecie,
                         const std::string& dielektryk) {
    auto bezSpacji = [](std::string s) {
        s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c) != 0; }), s.end());
        return s;
    };
    const std::string typ = r == RodzajElementu::Rezystor ? "R" : r == RodzajElementu::Kondensator ? "C" : "L";
    const std::pair<const char*, std::string> podstawienia[] = {
        {"{typ}", typ}, {"{rkm}", formatRKM(wartosc, r)}, {"{wartosc}", formatWartosci(wartosc, r)},
        {"{obudowa}", bezSpacji(obudowa)}, {"{tol}", bezSpacji(tol)}, {"{moc}", bezSpacji(moc)},
        {"{napiecie}", bezSpacji(napiecie)}, {"{dielektryk}", bezSpacji(dielektryk)}};
    std::string s = wzor.empty() ? domyslnyWzorSymbolu(r) : wzor;
    for (const auto& [klucz, wartoscKlucza] : podstawienia) {
        const std::string k = klucz;
        for (size_t poz = s.find(k); poz != std::string::npos; poz = s.find(k, poz + wartoscKlucza.size()))
            s.replace(poz, k.size(), wartoscKlucza);
    }
    // puste człony: "R-4K7--1%" -> "R-4K7-1%", końcowe i początkowe myślniki znikają
    for (size_t poz = s.find("--"); poz != std::string::npos; poz = s.find("--")) s.erase(poz, 1);
    while (!s.empty() && s.back() == '-') s.pop_back();
    while (!s.empty() && s.front() == '-') s.erase(0, 1);
    return s;
}

std::vector<PozycjaImportu> generujSzereg(const ParametrySzeregu& p, std::string* blad) {
    std::vector<PozycjaImportu> wynik;
    auto zle = [&](const std::string& powod) {
        if (blad) *blad = powod;
        return std::vector<PozycjaImportu>();
    };
    if (!(p.od > 0)) return zle("Wartość początkowa musi być większa od zera.");
    if (p.doo < p.od) return zle("Wartość końcowa nie może być mniejsza od początkowej.");
    if (p.ilosc < 0 || p.minimum < 0) return zle("Ilość i stan minimalny nie mogą być ujemne.");
    const std::vector<double> w = wartosci(p.szereg, p.od, p.doo);
    if (w.empty()) return zle("W podanym zakresie nie ma żadnej wartości szeregu.");
    if (w.size() > 5000) return zle("Zakres jest zbyt duży (ponad 5000 pozycji). Zawęź go.");

    const std::string tol = przytnij(p.tolerancja).empty() ? domyslnaTolerancja(p.szereg) : przytnij(p.tolerancja);
    const std::string kat = przytnij(p.kategoria).empty() ? kategoriaDomyslna(p.rodzaj, p.obudowa) : przytnij(p.kategoria);
    const int dekadaStart = static_cast<int>(std::floor(std::log10(w.front()) + 1e-9));

    std::string parametry;
    for (const std::string& cz : {przytnij(p.moc), przytnij(p.napiecie), przytnij(p.dielektryk)})
        if (!cz.empty()) parametry += (parametry.empty() ? "" : "; ") + cz;
    std::string tagi = nazwa(p.szereg);
    if (!przytnij(p.tagi).empty()) tagi += ", " + przytnij(p.tagi);

    for (double v : w) {
        PozycjaImportu poz;
        Element& e = poz.element;
        e.symbol = zbudujSymbol(p.wzorSymbolu, p.rodzaj, v, p.obudowa, tol, p.moc, p.napiecie, p.dielektryk);
        e.wartosc = formatPolaWartosci(v, p.rodzaj);
        e.jednostka = jednostka(p.rodzaj);
        e.obudowa = przytnij(p.obudowa);
        e.tolerancja = tol;
        e.parametry = parametry;
        e.producent = przytnij(p.producent);
        e.tagi = tagi;
        std::string opis = nazwaRodzaju(p.rodzaj) + " " + formatZJednostka(v, p.rodzaj);
        for (const std::string& cz : {tol, przytnij(p.moc), przytnij(p.napiecie), przytnij(p.dielektryk), e.obudowa})
            if (!cz.empty()) opis += " " + cz;
        e.opis = opis;
        e.iloscDostepna = p.ilosc;
        e.iloscMinimalna = p.minimum;
        e.cena = p.cena;
        e.waluta = p.waluta.empty() ? "PLN" : p.waluta;
        MiejsceMagazynowe m = p.miejsce;
        if (p.szufladaNaDekade && !m.szuflada.empty()) {
            const int dek = static_cast<int>(std::floor(std::log10(v) + 1e-9)) - dekadaStart;
            m.szuflada = kolejnaSzuflada(m.szuflada, dek);
        }
        e.ustawMiejsce(m);
        poz.kategoria = kat;
        if (e.symbol.empty()) poz.blad = "Wzór symbolu daje pusty symbol.";
        wynik.push_back(poz);
    }
    return wynik;
}

} // namespace SzeregiE
