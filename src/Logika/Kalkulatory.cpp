#include "Kalkulatory.h"
#include "../Narzedzia/Liczby.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace Kalkulatory {
namespace {
const double PI = 3.14159265358979323846;

std::string trzyCyfry(double v) {
    char b[40];
    const int cyfryPrzedPrzecinkiem = v >= 100 ? 0 : v >= 10 ? 1 : 2;
    std::snprintf(b, sizeof b, "%.*f", cyfryPrzedPrzecinkiem, v);
    std::string t = b;
    if (t.find('.') != std::string::npos) {
        while (!t.empty() && t.back() == '0') t.pop_back();
        if (!t.empty() && t.back() == '.') t.pop_back();
    }
    for (char& c : t) if (c == '.') c = ',';
    return t;
}
} // namespace

double najblizsza(double wartosc, Szereg s) {
    if (!(wartosc > 0)) return 0;
    const double dek = std::floor(std::log10(wartosc));
    const std::vector<double> w = SzeregiE::wartosci(s, std::pow(10.0, dek - 1), std::pow(10.0, dek + 2));
    double najlepsza = 0, roznica = 1e300;
    for (double x : w) {
        const double r = std::fabs(std::log(x / wartosc));
        if (r < roznica) { roznica = r; najlepsza = x; }
    }
    return najlepsza;
}

double najblizszaWieksza(double wartosc, Szereg s) {
    if (!(wartosc > 0)) return 0;
    const double dek = std::floor(std::log10(wartosc));
    const std::vector<double> w = SzeregiE::wartosci(s, std::pow(10.0, dek - 1), std::pow(10.0, dek + 2));
    for (double x : w) if (x >= wartosc * (1 - 1e-9)) return x;
    return 0;
}

std::optional<double> napiecieDzielnika(double uwe, double r1, double r2) {
    if (!(r1 >= 0) || !(r2 > 0) || r1 + r2 <= 0) return std::nullopt;
    return uwe * r2 / (r1 + r2);
}

std::optional<WynikDzielnika> dobierzDzielnik(double uwe, double uwy, Szereg s, double rMin, double rMax) {
    if (!(uwe > 0) || !(uwy > 0) || !(uwy < uwe) || !(rMin > 0) || !(rMax >= rMin)) return std::nullopt;
    std::vector<WynikDzielnika> wszystkie;
    const std::vector<double> kandydaci = SzeregiE::wartosci(s, rMin, rMax);
    const std::vector<double> r2Wszystkie = SzeregiE::wartosci(s, rMin / 100, rMax * 100);
    for (double r1 : kandydaci) {
        // idealne R2 = R1 * Uwy / (Uwe - Uwy); sprawdź wartości z szeregu w pobliżu
        const double ideal = r1 * uwy / (uwe - uwy);
        for (double r2 : r2Wszystkie) {
            if (r2 < ideal / 1.3 || r2 > ideal * 1.3) continue;
            WynikDzielnika w;
            w.r1 = r1; w.r2 = r2;
            w.uwy = uwe * r2 / (r1 + r2);
            w.bladProc = (w.uwy - uwy) / uwy * 100.0;
            w.pradMA = uwe / (r1 + r2) * 1000.0;
            wszystkie.push_back(w);
        }
    }
    if (wszystkie.empty()) return std::nullopt;
    double najmniejszyBlad = 1e300;
    for (const auto& w : wszystkie) najmniejszyBlad = std::min(najmniejszyBlad, std::fabs(w.bladProc));
    // spośród praktycznie równie dokładnych (różnica do 0,05 %) bierzemy parę o sumie najbliższej 20 k:
    // za małe rezystory grzeją się i obciążają źródło, za duże są wrażliwe na obciążenie i zakłócenia
    const WynikDzielnika* wybrany = nullptr;
    double odleglosc = 1e300;
    for (const auto& w : wszystkie) {
        if (std::fabs(w.bladProc) > najmniejszyBlad + 0.05) continue;
        const double d = std::fabs(std::log((w.r1 + w.r2) / 20e3));
        if (d < odleglosc) { odleglosc = d; wybrany = &w; }
    }
    return *wybrany;
}

std::optional<WynikLED> rezystorLED(double uzas, double uf, double pradMA, Szereg s) {
    if (!(uzas > uf) || !(uf >= 0) || !(pradMA > 0)) return std::nullopt;
    const double i = pradMA / 1000.0;
    WynikLED w;
    w.rIdealny = (uzas - uf) / i;
    w.rZSzeregu = najblizszaWieksza(w.rIdealny, s);
    if (!(w.rZSzeregu > 0)) return std::nullopt;
    const double irz = (uzas - uf) / w.rZSzeregu;
    w.pradRzeczywistyMA = irz * 1000.0;
    w.mocRezystoraMW = irz * irz * w.rZSzeregu * 1000.0;
    w.mocLEDMW = uf * irz * 1000.0;
    return w;
}

double czestotliwoscGranicznaRC(double r, double c) { return (r > 0 && c > 0) ? 1.0 / (2 * PI * r * c) : 0; }
double stalaCzasowaRC(double r, double c) { return (r > 0 && c > 0) ? r * c : 0; }
double rDlaFiltru(double f, double c) { return (f > 0 && c > 0) ? 1.0 / (2 * PI * f * c) : 0; }
double cDlaFiltru(double f, double r) { return (f > 0 && r > 0) ? 1.0 / (2 * PI * f * r) : 0; }

std::string formatCzestotliwosci(double hz) {
    if (!(hz > 0)) return "";
    if (hz >= 1e9) return trzyCyfry(hz / 1e9) + " GHz";
    if (hz >= 1e6) return trzyCyfry(hz / 1e6) + " MHz";
    if (hz >= 1e3) return trzyCyfry(hz / 1e3) + " kHz";
    return trzyCyfry(hz) + " Hz";
}

std::string formatCzasu(double s) {
    if (!(s > 0)) return "";
    if (s >= 1) return trzyCyfry(s) + " s";
    if (s >= 1e-3) return trzyCyfry(s * 1e3) + " ms";
    if (s >= 1e-6) return trzyCyfry(s * 1e6) + " \xC2\xB5s";
    return trzyCyfry(s * 1e9) + " ns";
}

} // namespace Kalkulatory
