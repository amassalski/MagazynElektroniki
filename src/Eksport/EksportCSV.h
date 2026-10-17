#ifndef EKSPORTCSV_H
#define EKSPORTCSV_H

#include "../Model/Element.h"

#include <string>
#include <vector>

/**
 * Eksport do CSV zgodny z polskim Excelem: UTF-8 z BOM, separator średnik,
 * przecinek dziesiętny, końce linii CRLF. Pola z separatorem, cudzysłowem lub
 * końcem linii są ujmowane w cudzysłowy.
 */
namespace EksportCSV {

/// Jedno pole CSV (z ewentualnym cudzysłowem)
std::string pole(const std::string& tekst, char separator = ';');

/// Cała zawartość pliku jako tekst (bez BOM)
std::string zbuduj(const std::vector<std::string>& naglowki,
                   const std::vector<std::vector<std::string>>& wiersze, char separator = ';');

/// Zapis do pliku (ścieżka w UTF-8). Przy błędzie zwraca false i opis w `blad`.
bool zapisz(const std::string& sciezka, const std::vector<std::string>& naglowki,
            const std::vector<std::vector<std::string>>& wiersze, std::string& blad,
            char separator = ';');

/// Nagłówki i wiersze dla listy elementów
std::vector<std::string> naglowkiElementow();
std::vector<std::vector<std::string>> wierszeElementow(const std::vector<Element>& elementy);

bool zapiszElementy(const std::string& sciezka, const std::vector<Element>& elementy,
                    std::string& blad);

} // namespace EksportCSV

#endif // EKSPORTCSV_H
