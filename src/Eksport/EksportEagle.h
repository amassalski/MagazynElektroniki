#ifndef EKSPORTEAGLE_H
#define EKSPORTEAGLE_H

#include "../Model/BOM.h"

#include <string>
#include <vector>

/**
 * Integracja z Eagle CAD 7.x.
 *
 *  - skrypt(): plik .scr z poleceniami ATTRIBUTE, który ustawia w schemacie atrybuty z magazynu
 *    (DM_SYMBOL, DM_STAN, DM_MIEJSCE, MPN, MANUFACTURER) dla części wskazanych oznaczeniami z listy projektu.
 *    Uruchamia się go w edytorze schematu poleceniem RUN/SCRIPT (menu File / Execute Script).
 *  - ulpBOM(): program ULP, który zapisuje listę części schematu jako CSV czytany przez import listy części.
 */
namespace EksportEagle {

struct WynikSkryptu {
    std::string tekst;                       ///< zawartość pliku .scr
    int czesci = 0;                          ///< liczba części (oznaczeń), którym ustawiono atrybuty
    std::vector<std::string> bezOznaczen;    ///< symbole pozycji bez oznaczeń (pominięte)
};

WynikSkryptu skrypt(const std::vector<PozycjaProjektu>& pozycje, const std::string& nazwaProjektu);

/// Tekst do pliku ASCII dla Eagle 7: polskie znaki bez ogonków, apostrof ze znakiem ucieczki, bez znaków nowej linii
std::string tekstDlaEagle(const std::string& utf8);

/// Treść programu dm_bom.ulp
std::string ulpBOM();

} // namespace EksportEagle

#endif // EKSPORTEAGLE_H
