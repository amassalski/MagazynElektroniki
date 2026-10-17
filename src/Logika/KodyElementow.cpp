#include "KodyElementow.h"
#include "SzeregiE.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace KodyElementow {

static const char* kNazwy[] = {"czarny", "brązowy", "czerwony", "pomarańczowy", "żółty", "zielony", "niebieski",
                               "fioletowy", "szary", "biały", "złoty", "srebrny", "brak"};
static const char* kAng[] = {"black", "brown", "red", "orange", "yellow", "green", "blue", "violet", "grey", "white",
                             "gold", "silver", "none"};
static const unsigned kRGB[] = {0x000000, 0x8B4513, 0xD62828, 0xF77F00, 0xFCDD1C, 0x2E9E3B, 0x1D4ED8, 0x8E44AD,
                                0x808080, 0xFFFFFF, 0xD4AF37, 0xC0C0C0, 0xDDDDDD};

std::string nazwa(Kolor k) { return kNazwy[static_cast<int>(k)]; }
std::string nazwaAngielska(Kolor k) { return kAng[static_cast<int>(k)]; }
unsigned kolorRGB(Kolor k) { return kRGB[static_cast<int>(k)]; }

static bool cyfra(Kolor k) { return static_cast<int>(k) <= 9; }

static std::optional<double> mnoznik(Kolor k) {
    if (cyfra(k)) return std::pow(10.0, static_cast<int>(k));
    if (k == Kolor::Zloty) return 0.1;
    if (k == Kolor::Srebrny) return 0.01;
    return std::nullopt;
}

static std::optional<double> tolerancja(Kolor k) {
    switch (k) {
        case Kolor::Brazowy: return 1;   case Kolor::Czerwony: return 2;  case Kolor::Zielony: return 0.5;
        case Kolor::Niebieski: return 0.25; case Kolor::Fioletowy: return 0.1; case Kolor::Szary: return 0.05;
        case Kolor::Zloty: return 5;     case Kolor::Srebrny: return 10;  case Kolor::Brak: return 20;
        default: return std::nullopt;
    }
}

static int tempco(Kolor k) {
    switch (k) {
        case Kolor::Czarny: return 250; case Kolor::Brazowy: return 100; case Kolor::Czerwony: return 50;
        case Kolor::Pomaranczowy: return 15; case Kolor::Zolty: return 25; case Kolor::Zielony: return 20;
        case Kolor::Niebieski: return 10; case Kolor::Fioletowy: return 5; case Kolor::Szary: return 1;
        default: return -1;
    }
}

int wspTemperaturowy(Kolor k) { return tempco(k); }

Rola rolaPaska(int i, int n) {
    const int cyfr = n >= 5 ? 3 : 2;
    if (i < cyfr) return Rola::Cyfra;
    if (i == cyfr) return Rola::Mnoznik;
    return i == cyfr + 1 ? Rola::Tolerancja : Rola::Tempco;
}

std::vector<Kolor> dozwoloneKolory(Rola r) {
    switch (r) {
        case Rola::Cyfra:
            return {Kolor::Czarny, Kolor::Brazowy, Kolor::Czerwony, Kolor::Pomaranczowy, Kolor::Zolty, Kolor::Zielony,
                    Kolor::Niebieski, Kolor::Fioletowy, Kolor::Szary, Kolor::Bialy};
        case Rola::Mnoznik:
            return {Kolor::Czarny, Kolor::Brazowy, Kolor::Czerwony, Kolor::Pomaranczowy, Kolor::Zolty, Kolor::Zielony,
                    Kolor::Niebieski, Kolor::Fioletowy, Kolor::Szary, Kolor::Bialy, Kolor::Zloty, Kolor::Srebrny};
        case Rola::Tolerancja:
            return {Kolor::Brazowy, Kolor::Czerwony, Kolor::Zielony, Kolor::Niebieski, Kolor::Fioletowy, Kolor::Szary,
                    Kolor::Zloty, Kolor::Srebrny, Kolor::Brak};
        case Rola::Tempco:
            return {Kolor::Czarny, Kolor::Brazowy, Kolor::Czerwony, Kolor::Pomaranczowy, Kolor::Zolty, Kolor::Zielony,
                    Kolor::Niebieski, Kolor::Fioletowy, Kolor::Szary};
    }
    return {};
}

static std::string liczbaTekst(double v) {
    char b[32];
    std::snprintf(b, sizeof b, "%g", v);
    std::string t = b;
    for (char& c : t) if (c == '.') c = ',';
    return t;
}

std::string znaczenieKoloru(Kolor k, Rola r) {
    switch (r) {
        case Rola::Cyfra: return std::to_string(static_cast<int>(k));
        case Rola::Mnoznik: {
            auto m = mnoznik(k);
            return m ? "\xC3\x97" + liczbaTekst(*m) : "";
        }
        case Rola::Tolerancja: {
            auto t = tolerancja(k);
            return t ? "\xC2\xB1" + liczbaTekst(*t) + " %" : "";
        }
        case Rola::Tempco: return std::to_string(tempco(k)) + " ppm/K";
    }
    return "";
}

std::optional<WynikPaskow> odczytajPaski(const std::vector<Kolor>& p, std::string* blad) {
    auto err = [&](const char* t) { if (blad) *blad = t; return std::optional<WynikPaskow>(); };
    const size_t n = p.size();
    if (n < 3 || n > 6) return err("Rezystor ma 3, 4, 5 lub 6 pasków.");
    const size_t cyfr = n >= 5 ? 3 : 2;
    double mant = 0;
    for (size_t i = 0; i < cyfr; ++i) {
        if (!cyfra(p[i])) return err("Paski z cyframi muszą być od czarnego do białego (nie złoty, srebrny ani brak).");
        mant = mant * 10 + static_cast<int>(p[i]);
    }
    auto m = mnoznik(p[cyfr]);
    if (!m) return err("Ten kolor nie może być mnożnikiem.");
    WynikPaskow w;
    w.wartosc = std::round(mant * *m * 1e6) / 1e6;
    if (n >= 4) {
        auto t = tolerancja(p[cyfr + 1]);
        if (!t) return err("Ten kolor nie oznacza tolerancji.");   // Kolor::Brak = brak paska = 20 %
        w.tolerancja = *t;
    }
    if (n == 6) {
        w.tempco = tempco(p[5]);
        if (w.tempco < 0) return err("Ten kolor nie oznacza współczynnika temperaturowego.");
    }
    return w;
}

std::vector<Kolor> paskiDlaWartosci(double wartosc, double tol, int liczbaPaskow, int tempcoPpm) {
    if (liczbaPaskow < 3 || liczbaPaskow > 6) return {};
    if (liczbaPaskow == 3 && std::fabs(tol - 20) > 1e-9) return {};
    const int cyfr = liczbaPaskow >= 5 ? 3 : 2;
    if (!(wartosc > 0)) {
        if (wartosc == 0) {
            std::vector<Kolor> w(cyfr, Kolor::Czarny);
            w.push_back(Kolor::Czarny);
            if (liczbaPaskow >= 4) w.push_back(Kolor::Zloty);
            return w;
        }
        return {};
    }
    // znajdź wykładnik tak, by mantysa miała dokładnie 'cyfr' cyfr
    int e = static_cast<int>(std::floor(std::log10(wartosc))) - (cyfr - 1);
    double mant = wartosc / std::pow(10.0, e);
    long m = std::lround(mant);
    if (std::fabs(mant - m) > 1e-6 * mant) {
        // wartość z mniejszą liczbą cyfr znaczących: spróbuj z mantysą z zerem na końcu jest już uwzględnione; inaczej brak
        return {};
    }
    if (e < -2 || e > 9) return {};
    std::vector<Kolor> w;
    long dz = static_cast<long>(std::pow(10, cyfr - 1));
    for (int i = 0; i < cyfr; ++i, dz /= 10) w.push_back(static_cast<Kolor>((m / dz) % 10));
    w.push_back(e >= 0 ? static_cast<Kolor>(e) : (e == -1 ? Kolor::Zloty : Kolor::Srebrny));
    if (liczbaPaskow >= 4) {
        bool jest = false;
        for (Kolor k : dozwoloneKolory(Rola::Tolerancja))
            if (std::fabs(*tolerancja(k) - tol) < 1e-9) { w.push_back(k); jest = true; break; }
        if (!jest) return {};
    }
    if (liczbaPaskow == 6) {
        bool jest = false;
        for (Kolor k : dozwoloneKolory(Rola::Tempco))
            if (tempco(k) == tempcoPpm) { w.push_back(k); jest = true; break; }
        if (!jest) return {};
    }
    return w;
}

static std::string duze(std::string s) {
    s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c); }), s.end());
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

static bool same(const std::string& s, int (*f)(int)) {
    return !s.empty() && std::all_of(s.begin(), s.end(), [&](unsigned char c) { return f(c) != 0; });
}

static std::optional<double> eia96(const std::string& k) {
    if (k.size() != 3 || !std::isdigit(static_cast<unsigned char>(k[0])) || !std::isdigit(static_cast<unsigned char>(k[1])))
        return std::nullopt;
    const int nr = (k[0] - '0') * 10 + (k[1] - '0');
    if (nr < 1 || nr > 96) return std::nullopt;
    double mn;
    switch (k[2]) {
        case 'Z': mn = 0.001; break;           case 'Y': case 'R': mn = 0.01; break;
        case 'X': case 'S': mn = 0.1; break;   case 'A': mn = 1; break;
        case 'B': case 'H': mn = 10; break;    case 'C': mn = 100; break;
        case 'D': mn = 1e3; break;             case 'E': mn = 1e4; break;
        case 'F': mn = 1e5; break;             default: return std::nullopt;
    }
    return std::round(SzeregiE::mantysy(Szereg::E96)[nr - 1] * mn * 1e6) / 1e6;
}

std::optional<double> rezystorZKodu(const std::string& kodWe) {
    const std::string k = duze(kodWe);
    if (k.empty()) return std::nullopt;
    if (k == "0" || k == "000" || k == "0000") return 0.0;
    if (same(k, ::isdigit) && (k.size() == 3 || k.size() == 4)) {
        const size_t cyfr = k.size() - 1;
        const double mant = std::stod(k.substr(0, cyfr));
        return mant * std::pow(10.0, k.back() - '0');
    }
    if (auto e = eia96(k)) return e;
    return SzeregiE::parsujWartosc(k, RodzajElementu::Rezystor);
}

std::optional<double> kondensatorZKodu(const std::string& kodWe) {
    const std::string k = duze(kodWe);
    if (k.empty()) return std::nullopt;
    if (same(k, ::isdigit) && k.size() == 3) {
        const double mant = std::stod(k.substr(0, 2));
        return mant * std::pow(10.0, k[2] - '0') * 1e-12;
    }
    // zapis z literą w miejscu przecinka: 4R7 = 4,7 pF, 4P7, N47
    std::string t = k;
    for (char& c : t) if (c == 'R') c = 'P';
    if (t.find_first_of("PNU") != std::string::npos) {
        std::string m = t;
        for (char& c : m) { if (c == 'P') c = 'p'; else if (c == 'N') c = 'n'; else if (c == 'U') c = 'u'; }
        return SzeregiE::parsujWartosc(m, RodzajElementu::Kondensator);
    }
    return std::nullopt;
}

std::string kodEIA96(double omy) {
    if (!(omy > 0)) return {};
    const auto mant = SzeregiE::mantysy(Szereg::E96);
    static const struct { char lit; double mn; } lit[] = {{'Z', 0.001}, {'Y', 0.01}, {'X', 0.1}, {'A', 1}, {'B', 10},
                                                         {'C', 100}, {'D', 1e3}, {'E', 1e4}, {'F', 1e5}};
    for (const auto& l : lit)
        for (size_t i = 0; i < mant.size(); ++i)
            if (std::fabs(mant[i] * l.mn - omy) <= 1e-9 * omy) {
                char b[8];
                std::snprintf(b, sizeof b, "%02zu%c", i + 1, l.lit);
                return b;
            }
    return {};
}

} // namespace KodyElementow
