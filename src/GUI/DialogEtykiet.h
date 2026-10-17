#ifndef DIALOGETYKIET_H
#define DIALOGETYKIET_H

#include "RysowanieEtykiet.h"

#include <wx/wx.h>
#include <wx/spinctrl.h>

/**
 * Okno drukowania etykiet z kodem kreskowym (EAN-13 / EAN-8 / Code128 wg ustawień).
 * Kod zawiera numer elementu, więc zeskanowanie etykiety (skaner USB wpisuje kod w pole
 * szukania) od razu pokazuje element.
 */
class DialogEtykiet : public wxDialog {
public:
    /// `elementy` - elementy z listy; `idWybranego` - zaznaczony (0 = brak)
    DialogEtykiet(wxWindow* parent, const std::vector<Element>& elementy, long long idWybranego);

    std::vector<EtykietaDoDruku> Etykiety() const;
    UstawieniaDruku Ustawienia() const;
    /// Zapisuje wszystkie strony jako PNG (nazwa_1.png, nazwa_2.png...); zwraca liczbę zapisanych
    int ZapiszPNG(const wxString& sciezkaBazowa, double dpi = 200) const;
    void WybierzTylkoZaznaczony(bool tak);

private:
    std::vector<Element> elementy_;
    long long idWybranego_;
    wxRadioButton* rbZaznaczony = nullptr;
    wxRadioButton* rbWszystkie = nullptr;
    wxSpinCtrl* scKopie = nullptr;
    wxChoice* chFormat = nullptr;
    wxSpinCtrl* scStart = nullptr;
    wxSpinCtrlDouble* scX = nullptr;
    wxSpinCtrlDouble* scY = nullptr;
    wxCheckBox* cbRamki = nullptr;
    wxStaticBitmap* podglad = nullptr;
    wxStaticText* lbInfo = nullptr;

    void Odswiez();
    void Podglad();
    void Drukuj();
    void ZapiszPNGOknem();
};

#endif // DIALOGETYKIET_H
