#ifndef DIALOGPOLACZENIA_H
#define DIALOGPOLACZENIA_H

#include <wx/wx.h>
#include <wx/spinctrl.h>
#include <wx/textctrl.h>

/**
 * @class DialogPolaczenia
 * @brief Dialog konfiguracji połączenia z PostgreSQL
 *
 * Pozwala wpisać dane połączenia, sprawdzić je (bez zmiany aktywnego
 * połączenia) i zapisać do pliku konfiguracji. OK zapisuje ustawienia i je
 * stosuje; ponowne połączenie wykonuje okno główne.
 */
class DialogPolaczenia : public wxDialog {
public:
    explicit DialogPolaczenia(wxWindow* parent);

private:
    wxTextCtrl* tcHost = nullptr;
    wxSpinCtrl* scPort = nullptr;
    wxTextCtrl* tcBaza = nullptr;
    wxTextCtrl* tcUzytkownik = nullptr;
    wxTextCtrl* tcHaslo = nullptr;
    wxCheckBox* cbZapamietajHaslo = nullptr;
    wxStaticText* stStatus = nullptr;
    wxButton* btnTest = nullptr;
    wxButton* btnZapisz = nullptr;

    void OnTestPolaczenie(wxCommandEvent& event);
    void OnZapisz(wxCommandEvent& event);
    void OnOK(wxCommandEvent& event);

    void UtworzKontrole();
    void WczytajUstawienia();
    /// Przepisuje wartości z kontrolek do managera; zwraca false (z komunikatem), gdy dane są niepoprawne
    bool ZastosujUstawienia();
    void PokazStatus(const wxString& wiadomosc, bool sukces);
};

#endif // DIALOGPOLACZENIA_H
