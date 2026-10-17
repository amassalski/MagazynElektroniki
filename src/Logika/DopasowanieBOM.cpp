#include "DopasowanieBOM.h"

#include <algorithm>
#include <cctype>

namespace DopasowanieBOM {

namespace {

/// Postać do porównań symboli: bez spacji i myślników, małe litery
std::string kluczSymbolu(const std::string& s) {
    std::string w;
    for (char c : s) {
        if (std::isalnum(static_cast<unsigned char>(c)) || static_cast<unsigned char>(c) >= 0x80)
            w += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return w;
}

} // namespace

std::vector<KandydatElementu> znajdz(const PozycjaBOM& p, const std::vector<Element>& magazyn,
                                     size_t maksymalnie) {
    const std::string mpn = kluczSymbolu(p.symbol);
    const std::string wartosc = ImportBOM::normalizujWartosc(p.wartosc);
    const std::string wartoscSymbol = kluczSymbolu(p.wartosc);
    const std::string obudowa = ImportBOM::normalizujObudowe(p.obudowa);

    struct Wpis {
        KandydatElementu k;
        int stan;
    };
    std::vector<Wpis> wpisy;

    for (const Element& e : magazyn) {
        if (!e.aktywny) continue;
        const std::string sym = kluczSymbolu(e.symbol);
        const std::string symProd = kluczSymbolu(e.symbolProducenta);

        KandydatElementu k;
        k.idElementu = e.id;

        if (!mpn.empty() && (mpn == sym || mpn == symProd)) {
            k.punkty = 100;
            k.powod = "ten sam symbol";
        } else if (wartoscSymbol.size() >= 3 && (wartoscSymbol == sym || wartoscSymbol == symProd)) {
            k.punkty = 90;
            k.powod = "wartość równa symbolowi";
        } else if (!wartosc.empty() && !e.wartosc.empty() &&
                   wartosc == ImportBOM::normalizujWartosc(e.wartosc)) {
            const std::string obudowaElementu = ImportBOM::normalizujObudowe(e.obudowa);
            if (!obudowa.empty() && obudowa == obudowaElementu) {
                k.punkty = 80;
                k.powod = "wartość i obudowa";
            } else if (obudowa.empty() || obudowaElementu.empty()) {
                k.punkty = 50;
                k.powod = "wartość (obudowa nieznana)";
            } else {
                k.punkty = 40;
                k.powod = "wartość (inna obudowa)";
            }
        } else {
            continue;
        }
        wpisy.push_back({k, e.iloscDostepna});
    }

    std::stable_sort(wpisy.begin(), wpisy.end(), [](const Wpis& a, const Wpis& b) {
        if (a.k.punkty != b.k.punkty) return a.k.punkty > b.k.punkty;
        return a.stan > b.stan;
    });

    std::vector<KandydatElementu> wynik;
    for (const Wpis& w : wpisy) {
        if (wynik.size() >= maksymalnie) break;
        wynik.push_back(w.k);
    }
    return wynik;
}

} // namespace DopasowanieBOM
