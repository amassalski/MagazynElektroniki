#ifndef IMPORTCSVELEMENTOW_H
#define IMPORTCSVELEMENTOW_H

#include "PozycjaImportu.h"

#include <string>
#include <vector>

/**
 * Elementy z pliku CSV (np. cały zestaw rezystorów, kondensatorów, tranzystorów).
 *
 * Kolumny rozpoznawane po nagłówku (wielkość liter, spacje i polskie znaki nie mają znaczenia, są też nazwy angielskie):
 *   Symbol, Wartość, Jednostka, Tolerancja, Obudowa, Parametry, Ilość, Minimum, Kategoria, Miejsce,
 *   Kontener, Szuflada, Przegroda, Producent, Symbol producenta, Opis, Cena, Waluta, Tagi, Datasheet,
 *   Typ (R / C / L: gdy brak symbolu, powstaje z typu, wartości i obudowy),
 *   Moc, Napięcie, Dielektryk (dopisywane do Parametrów).
 * Wymagany jest symbol (albo typ z wartością). Separator (średnik, przecinek, tabulator) jest wykrywany.
 */
namespace ImportCSVElementow {

struct Wynik {
    std::vector<PozycjaImportu> pozycje;
    std::vector<std::string> ostrzezenia;   ///< np. nieznane kolumny
    std::string blad;                       ///< niepusty = pliku nie da się wczytać (brak nagłówka, brak kolumny Symbol)
};

Wynik parsuj(const std::string& tekst);

/// Pozycje jako CSV (UTF-8 z BOM, średnik, przecinek dziesiętny): do poprawienia w Excelu i ponownego wczytania
std::string naCSV(const std::vector<PozycjaImportu>& pozycje);

/// Plik wzorcowy z nagłówkami i przykładowymi wierszami
std::string szablon();

} // namespace ImportCSVElementow

#endif // IMPORTCSVELEMENTOW_H
