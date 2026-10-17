#ifndef AKTUALIZACJE_H
#define AKTUALIZACJE_H

#include <optional>
#include <string>
#include <vector>

/**
 * Sprawdzanie, czy jest nowsza wersja programu. Program pobiera mały plik tekstowy z repozytorium na GitHubie
 * (domyślnie DomowyMagazyn/wersja.txt z gałęzi głównej) i porównuje numer z własną wersją. Nic nie instaluje.
 *
 * Format pliku wersja.txt:
 *   1.0.1                                   <- pierwsza linia: numer wersji (może być z "v": v1.0.1)
 *   https://github.com/.../releases         <- opcjonalnie: linia zaczynająca się od http - strona pobierania
 *   Co nowego: kilka zdań...                <- pozostałe linie: opis zmian
 */
namespace Aktualizacje {

struct Informacja {
    std::string wersja;           ///< np. "1.0.1"
    std::string adresPobierania;  ///< pusty = użyj adresu domyślnego
    std::string opis;             ///< co nowego (może być pusty)
};

/// Adres pliku z numerem wersji (gałąź domyślna repozytorium, więc nie zależy od jej nazwy)
std::string adresWersji();
/// Strona, na którą kierujemy użytkownika, gdy plik wersji nie podaje własnej
std::string adresStronyPobierania();

/// Wersja jako liczby: "1.2.10" -> {1,2,10}; nullopt, gdy tekst nie jest numerem wersji
std::optional<std::vector<int>> parsujNumer(const std::string& tekst);
/// -1, 0, 1 (brakujące człony traktowane jak zera: 1.0 == 1.0.0)
int porownaj(const std::vector<int>& a, const std::vector<int>& b);
/// Czy @p dostepna jest nowsza od @p biezaca (false, gdy któryś numer jest nieczytelny)
bool nowsza(const std::string& dostepna, const std::string& biezaca);
/// Czyta treść pliku wersja.txt (z BOM, CRLF, "v" przed numerem); nullopt, gdy pierwsza linia nie jest numerem
std::optional<Informacja> parsujPlikWersji(const std::string& tresc);

} // namespace Aktualizacje

#endif // AKTUALIZACJE_H
