#ifndef IMPORTKATEGORII_H
#define IMPORTKATEGORII_H

#include <string>
#include <vector>

class LogikaMagazynu;

/**
 * Import kategorii z pliku tekstowego (UTF-8). Każda linia to ścieżka kategorii,
 * nazwy rozdzielone znakiem " / " (ze spacjami) albo ">", np.:
 *
 *   # komentarz
 *   Elementy bierne / Rezystory / Rezystory THT
 *   Elementy bierne / Kondensatory
 *
 * Istniejące kategorie są pomijane, więc import można powtarzać.
 */
namespace ImportKategorii {

/// Zamienia tekst na listę ścieżek (każda to lista nazw). Pomija puste linie i komentarze.
std::vector<std::vector<std::string>> parsuj(const std::string& tekst);

struct Wynik {
    int sciezek = 0;  ///< liczba wczytanych ścieżek
    int dodano = 0;   ///< liczba nowo utworzonych kategorii
};

bool importujZTekstu(LogikaMagazynu& logika, const std::string& tekst, Wynik& wynik,
                     std::string& blad);
/// Ścieżka pliku w UTF-8
bool importujZPliku(LogikaMagazynu& logika, const std::string& sciezka, Wynik& wynik,
                    std::string& blad);

} // namespace ImportKategorii

#endif // IMPORTKATEGORII_H
