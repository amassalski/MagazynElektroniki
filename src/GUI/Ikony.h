#ifndef IKONY_H
#define IKONY_H

#include <wx/bitmap.h>
#include <wx/bmpbndl.h>
#include <wx/icon.h>
#include <wx/iconbndl.h>

#include <wx/button.h>

#include <string>

/// Ikony programu wkompilowane w plik wykonywalny (rysuje je narzedzia/zbuduj_ikony.py)
namespace Ikony {

/// Zestaw rozmiarów ikony o danej nazwie ("nowy", "usun", ...) do menu i przycisków; pusty, gdy nie ma takiej nazwy
wxBitmapBundle Pakiet(const std::string& nazwa);
/// Ustawia ikonę ("nowy", "edytuj", "usun") z lewej strony napisu przycisku
void DlaPrzycisku(wxButton* przycisk, const std::string& nazwa);
/// Ikona programu w wielu rozmiarach (pasek tytułu, pasek zadań)
wxIconBundle IkonaProgramu();

} // namespace Ikony

#endif // IKONY_H
