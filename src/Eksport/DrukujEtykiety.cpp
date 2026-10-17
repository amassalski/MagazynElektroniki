#include "DrukujEtykiety.h"

#include <algorithm>

namespace DrukujEtykiety {

const std::vector<FormatEtykiet>& formatyWbudowane() {
    static const std::vector<FormatEtykiet> formaty = [] {
        std::vector<FormatEtykiet> f;
        auto dodaj = [&](const char* nazwa, double sw, double wh, int k, int w, double ew, double eh, double ml,
                         double mg, double ox, double oy) {
            FormatEtykiet e;
            e.nazwa = nazwa;
            e.stronaSzer = sw;
            e.stronaWys = wh;
            e.kolumny = k;
            e.wiersze = w;
            e.etykietaSzer = ew;
            e.etykietaWys = eh;
            e.marginesLewy = ml;
            e.marginesGorny = mg;
            e.odstepX = ox;
            e.odstepY = oy;
            f.push_back(e);
        };
        dodaj("A4: 3 x 8 (70 x 37 mm)", 210, 297, 3, 8, 70, 37, 0, 0.5, 0, 0);
        dodaj("A4: 3 x 7 (63,5 x 38,1 mm)", 210, 297, 3, 7, 63.5, 38.1, 7.2, 15.15, 2.5, 0);
        dodaj("A4: 5 x 13 (38,1 x 21,2 mm)", 210, 297, 5, 13, 38.1, 21.2, 4.7, 10.7, 2.5, 0);
        dodaj("A4: 2 x 4 (99 x 67 mm)", 210, 297, 2, 4, 99, 67, 4, 14.5, 0, 0);
        dodaj("Rolka / pojedyncza 50 x 25 mm", 50, 25, 1, 1, 50, 25, 0, 0, 0, 0);
        dodaj("Rolka / pojedyncza 62 x 29 mm", 62, 29, 1, 1, 62, 29, 0, 0, 0, 0);
        return f;
    }();
    return formaty;
}

PolozenieEtykiety polozenie(const FormatEtykiet& f, int indeks) {
    PolozenieEtykiety p;
    const int n = std::max(1, f.naStrone());
    p.strona = indeks / n;
    const int w = indeks % n;
    const int kolumna = w % std::max(1, f.kolumny);
    const int wiersz = w / std::max(1, f.kolumny);
    p.x = f.marginesLewy + kolumna * (f.etykietaSzer + f.odstepX);
    p.y = f.marginesGorny + wiersz * (f.etykietaWys + f.odstepY);
    return p;
}

int liczbaStron(const FormatEtykiet& f, int pominiete, int liczba) {
    if (liczba <= 0) return 0;
    const int n = std::max(1, f.naStrone());
    pominiete = std::max(0, pominiete) % n;
    return (pominiete + liczba + n - 1) / n;
}

std::vector<Wpis> naStronie(const FormatEtykiet& f, int pominiete, int liczba, int strona) {
    std::vector<Wpis> wynik;
    const int n = std::max(1, f.naStrone());
    pominiete = std::max(0, pominiete) % n;
    for (int i = 0; i < liczba; ++i) {
        const PolozenieEtykiety p = polozenie(f, pominiete + i);
        if (p.strona == strona) wynik.push_back({i, p});
    }
    return wynik;
}

bool poprawny(const FormatEtykiet& f, std::string* powod) {
    auto blad = [&](const char* t) {
        if (powod) *powod = t;
        return false;
    };
    if (f.kolumny < 1 || f.wiersze < 1) return blad("Liczba kolumn i wierszy musi wynosić co najmniej 1.");
    if (f.etykietaSzer < 15 || f.etykietaWys < 10) return blad("Etykieta jest za mała (minimum 15 x 10 mm).");
    const double szer = f.marginesLewy + f.kolumny * f.etykietaSzer + (f.kolumny - 1) * f.odstepX;
    const double wys = f.marginesGorny + f.wiersze * f.etykietaWys + (f.wiersze - 1) * f.odstepY;
    if (szer > f.stronaSzer + 0.01 || wys > f.stronaWys + 0.01) return blad("Etykiety nie mieszczą się na stronie.");
    return true;
}

} // namespace DrukujEtykiety
