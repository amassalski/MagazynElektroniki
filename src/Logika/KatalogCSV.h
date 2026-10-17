#ifndef KATALOGCSV_H
#define KATALOGCSV_H

#include "../Model/Katalog.h"

#include <string>
#include <vector>

/**
 * Katalog elementów z/do pliku CSV.
 * Kolumny: Symbol (wymagana), Producent, Opis, Obudowa, Rodzina (albo Kategoria), Datasheet, Uwagi, Parametry
 * ("Napięcie=2,7-5,5 V; Prąd=1 mA"). Każda inna kolumna staje się parametrem o nazwie z nagłówka (np. "Napięcie zasilania").
 */
namespace KatalogCSV {

struct Pozycja {
    PozycjaKatalogu katalog;
    int nrWiersza = 0;
    std::string blad;
};

struct Wynik {
    std::vector<Pozycja> pozycje;
    std::string blad;                          ///< niepusty = pliku nie da się wczytać
};

Wynik parsuj(const std::string& tekst);
/// "A=1; B=2" -> parametry (zrodlo "csv"); wpisy bez "=" są pomijane
std::vector<ParametrKatalogu> parsujParametry(const std::string& tekst, const std::string& zrodlo = "csv");
/// Parametry jako "A=1; B=2"
std::string parametryNaTekst(const std::vector<ParametrKatalogu>& p);
/// Katalog jako CSV (UTF-8 z BOM, średnik); parametry w jednej kolumnie
std::string naCSV(const std::vector<PozycjaKatalogu>& pozycje);
std::string szablon();

} // namespace KatalogCSV

#endif // KATALOGCSV_H
