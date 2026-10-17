#ifndef DOPASOWANIEBOM_H
#define DOPASOWANIEBOM_H

#include "../Import/ImportBOM.h"
#include "../Model/Element.h"

#include <vector>

/// Propozycja elementu z magazynu dla pozycji wczytanej z pliku
struct KandydatElementu {
    long long idElementu = 0;
    int punkty = 0;        ///< 0..100, im więcej tym pewniejsze dopasowanie
    std::string powod;     ///< krótki opis po polsku ("ten sam symbol", "wartość i obudowa")
};

/**
 * Dopasowanie pozycji z BOM do elementów magazynu (bez bazy danych):
 *  - ten sam symbol / symbol producenta: 100,
 *  - wartość równa symbolowi (np. "LM358"): 90,
 *  - ta sama wartość i obudowa: 80,
 *  - ta sama wartość, obudowa nieznana po jednej ze stron: 50,
 *  - ta sama wartość, inna obudowa: 40.
 * Elementy nieaktywne są pomijane. Wynik posortowany malejąco po punktach, potem
 * po ilości w magazynie.
 */
namespace DopasowanieBOM {

std::vector<KandydatElementu> znajdz(const PozycjaBOM& pozycja, const std::vector<Element>& magazyn,
                                     size_t maksymalnie = 5);

} // namespace DopasowanieBOM

#endif // DOPASOWANIEBOM_H
