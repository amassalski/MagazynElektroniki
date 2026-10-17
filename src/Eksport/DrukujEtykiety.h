#ifndef DRUKUJETYKIETY_H
#define DRUKUJETYKIETY_H

#include <string>
#include <vector>

/// Format arkusza z etykietami (wymiary w milimetrach)
struct FormatEtykiet {
    std::string nazwa;
    double stronaSzer = 210, stronaWys = 297;
    int kolumny = 1, wiersze = 1;
    double etykietaSzer = 50, etykietaWys = 25;
    double marginesLewy = 0, marginesGorny = 0;
    double odstepX = 0, odstepY = 0;   ///< odstępy między etykietami

    int naStrone() const { return kolumny * wiersze; }
};

struct PolozenieEtykiety {
    int strona = 0;         ///< od 0
    double x = 0, y = 0;    ///< lewy górny róg etykiety na stronie [mm]
};

/// Układ etykiet na arkuszach (bez wxWidgets): gdzie trafia która etykieta
namespace DrukujEtykiety {

const std::vector<FormatEtykiet>& formatyWbudowane();

/// Pozycja etykiety o numerze `indeks` licząc od pierwszej etykiety pierwszego arkusza (od 0)
PolozenieEtykiety polozenie(const FormatEtykiet& format, int indeks);

/// Ile stron zajmie `liczba` etykiet, gdy na pierwszym arkuszu pomijamy `pominiete` (już zużytych) miejsc
int liczbaStron(const FormatEtykiet& format, int pominiete, int liczba);

/// Etykiety przypadające na daną stronę: pary (numer etykiety w kolejce od 0, położenie)
struct Wpis {
    int numer;
    PolozenieEtykiety polozenie;
};
std::vector<Wpis> naStronie(const FormatEtykiet& format, int pominiete, int liczba, int strona);

/// Czy wymiary formatu mają sens (etykiety mieszczą się na stronie)
bool poprawny(const FormatEtykiet& format, std::string* powod = nullptr);

} // namespace DrukujEtykiety

#endif // DRUKUJETYKIETY_H
