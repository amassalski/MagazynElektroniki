#ifndef DIALOGDODAJSKLEP_H
#define DIALOGDODAJSKLEP_H

#include "../Logika/LogikaSklepow.h"

#include <wx/wx.h>
#include <wx/spinctrl.h>

/// Dodawanie i zmiana sklepu. Zapisuje do bazy po "OK"; przy błędzie zostaje otwarte.
class DialogDodajSklep : public wxDialog {
public:
    DialogDodajSklep(wxWindow* parent, LogikaSklepow& logika, const Sklep& sklep = Sklep());

    long long IdZapisanego() const { return idZapisanego_; }
    /// Wartości z pól (do testów)
    Sklep Pobierz() const;
    /// Zapisuje jak po "OK"; zwraca powodzenie (do testów)
    bool Zapisz();

    wxTextCtrl* tcNazwa = nullptr;
    wxTextCtrl* tcAdres = nullptr;
    wxTextCtrl* tcSzablon = nullptr;
    wxTextCtrl* tcWaluta = nullptr;
    wxTextCtrl* tcDostawa = nullptr;
    wxTextCtrl* tcDarmowa = nullptr;
    wxTextCtrl* tcWzorzec = nullptr;

private:
    LogikaSklepow& logika_;
    Sklep sklep_;
    long long idZapisanego_ = 0;
    wxTextCtrl* tcKraj = nullptr;
    wxTextCtrl* tcOpis = nullptr;
    wxSpinCtrl* spPriorytet = nullptr;
    wxCheckBox* cbAktywny = nullptr;
};

#endif // DIALOGDODAJSKLEP_H
