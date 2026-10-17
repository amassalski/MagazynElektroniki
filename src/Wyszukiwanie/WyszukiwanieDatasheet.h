#ifndef WYSZUKIWANIEDATASHEET_H
#define WYSZUKIWANIEDATASHEET_H

#include "ParserStrony.h"

#include <wx/string.h>

#include <functional>
#include <string>
#include <vector>

/**
 * Datasheety: szukanie odnośników na stronach sklepów, pobieranie PDF do katalogu dokumentów
 * i otwieranie. Plik ma nazwę z symbolu elementu, więc "czy mam datasheet" sprawdza się bez bazy.
 */
namespace WyszukiwanieDatasheet {

/// "BC547 B/2" -> "BC547_B_2.pdf" (bez znaków niedozwolonych w nazwach plików)
std::string nazwaPliku(const std::string& symbol);

/// Pełna ścieżka pliku w katalogu dokumentów
wxString sciezkaPliku(const wxString& katalog, const std::string& symbol);

/// Gdy plik już jest pobrany
bool czyPobrany(const wxString& katalog, const std::string& symbol);

/// Gotowe adresy wyszukiwania w serwisach z kartami katalogowymi (otwierane w przeglądarce)
struct AdresSerwisu { std::string nazwa; std::string url; };
std::vector<AdresSerwisu> adresySerwisow(const std::string& fraza);

/// Pobiera stronę i zwraca znalezione odnośniki do kart katalogowych
void szukajNaStronie(const std::string& adresStrony,
                     std::function<void(bool ok, const std::string& blad, std::vector<LinkDatasheet>)> po);

/// Pobiera PDF do katalogu (tworzy go, jeśli trzeba); sprawdza, że to naprawdę PDF
void pobierz(const std::string& url, const wxString& katalog, const std::string& symbol,
             std::function<void(bool ok, const std::string& blad, const wxString& sciezka)> po);

/// Otwiera plik w domyślnym programie; false, gdy go nie ma albo nic go nie otwiera
bool otworz(const wxString& sciezka);

} // namespace WyszukiwanieDatasheet

#endif // WYSZUKIWANIEDATASHEET_H
