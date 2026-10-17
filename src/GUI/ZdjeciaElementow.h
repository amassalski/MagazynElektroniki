#ifndef ZDJECIAELEMENTOW_H
#define ZDJECIAELEMENTOW_H

#include "../Narzedzia/Kamera.h"

#include <wx/image.h>
#include <wx/string.h>

/// Zapis i odczyt zdjęć elementów w katalogu zdjęć z ustawień aplikacji
namespace ZdjeciaElementow {

wxImage ZKlatki(const KlatkaRGB& klatka);
/// Katalog zdjęć z ustawień (utworzony, jeśli go nie ma); pusty tekst, gdy nie da się go utworzyć
wxString Katalog();
/// Zapisuje klatkę jako JPEG "<symbol>_RRRRMMDD_GGMMSS.jpg"; zwraca pełną ścieżkę albo pusty tekst
wxString ZapiszKlatke(const KlatkaRGB& klatka, const wxString& symbol);
/// Kopiuje plik do katalogu zdjęć (pod unikalną nazwą); gdy się nie da, zwraca ścieżkę oryginału
wxString SkopiujDoKatalogu(const wxString& sciezka);
/// Ładuje zdjęcie i skaluje do pola o podanym rozmiarze z zachowaniem proporcji (nie powiększa); pusty obraz przy błędzie
wxImage Wczytaj(const wxString& sciezka, wxSize pole);

} // namespace ZdjeciaElementow

#endif // ZDJECIAELEMENTOW_H
