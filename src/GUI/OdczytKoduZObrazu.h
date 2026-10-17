#ifndef ODCZYTKODUZOBRAZU_H
#define ODCZYTKODUZOBRAZU_H

#include <wx/image.h>
#include <wx/string.h>

#include <optional>
#include <string>

/// Szuka kodu kreskowego EAN na obrazie (zdjęcie etykiety z telefonu, zrzut z kamery). Próbuje też obrazu obróconego.
std::optional<std::string> OdczytajKodZObrazu(const wxImage& obraz);
/// Jak wyżej, z pliku; przy błędzie wczytania zwraca nullopt i ustawia `blad`
std::optional<std::string> OdczytajKodZPliku(const wxString& sciezka, wxString& blad);

#endif // ODCZYTKODUZOBRAZU_H
