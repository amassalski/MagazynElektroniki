#ifndef PARSERCSV_H
#define PARSERCSV_H

#include <string>
#include <vector>

/**
 * Prosty parser CSV: cudzysłowy ("" jako cudzysłów), znaki nowej linii w polach,
 * CRLF i LF, BOM UTF-8. Separator może być wykryty automatycznie (przecinek,
 * średnik, tabulator).
 */
namespace ParserCSV {

using Wiersz = std::vector<std::string>;

/// Wykrywa separator po pierwszej niepustej linii (poza cudzysłowami)
char wykryjSeparator(const std::string& tekst);

/// Dzieli tekst na wiersze. separator == 0 oznacza wykrycie automatyczne.
/// Całkowicie puste wiersze są pomijane.
std::vector<Wiersz> parsuj(const std::string& tekst, char separator = 0);

} // namespace ParserCSV

#endif // PARSERCSV_H
