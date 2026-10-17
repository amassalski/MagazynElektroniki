#ifndef DIALOGPROJEKTU_H
#define DIALOGPROJEKTU_H

#include "../Logika/LogikaMagazynu.h"
#include "../Model/BOM.h"
#include "../Model/Projekt.h"

#include <wx/wx.h>
#include <wx/listctrl.h>
#include <wx/spinctrl.h>
#include <wx/srchctrl.h>
#include <wx/timer.h>

/// Nazwa statusu projektu do wyświetlenia (PLANOWANY -> "Planowany")
wxString NazwaStatusuProjektu(const std::string& status);

/// Okno nowego / edytowanego projektu (nazwa, status, opis)
class DialogProjektu : public wxDialog {
public:
    DialogProjektu(wxWindow* parent, const Projekt& projekt);
    Projekt PobierzProjekt() const;

private:
    Projekt projekt_;
    wxTextCtrl* tcNazwa = nullptr;
    wxChoice* chStatus = nullptr;
    wxTextCtrl* tcOpis = nullptr;
    void OnOK(wxCommandEvent& event);
};

/// Wybór elementu z magazynu (wyszukiwanie + lista)
class DialogWyboruElementu : public wxDialog {
public:
    DialogWyboruElementu(wxWindow* parent, LogikaMagazynu& logika, const wxString& poczatkoweSzukanie = wxEmptyString);
    long long WybraneId() const { return wybrane_; }

private:
    LogikaMagazynu& logika_;
    std::vector<Element> elementy_;
    long long wybrane_ = 0;
    wxSearchCtrl* szukaj_ = nullptr;
    wxListCtrl* lista_ = nullptr;
    wxTimer timer_;

    void Odswiez();
    void OnOK(wxCommandEvent& event);
};

/// Pozycja listy części: dodawanie (z wyborem elementu) albo edycja (oznaczenia, ilości, uwagi)
class DialogPozycji : public wxDialog {
public:
    DialogPozycji(wxWindow* parent, LogikaMagazynu& logika, const PozycjaProjektu& pozycja, bool nowa);
    PozycjaProjektu PobierzPozycje() const;

private:
    LogikaMagazynu& logika_;
    PozycjaProjektu pozycja_;
    bool nowa_;
    wxStaticText* lbElement = nullptr;
    wxTextCtrl* tcOznaczenia = nullptr;
    wxSpinCtrl* scWymagana = nullptr;
    wxSpinCtrl* scZamowiona = nullptr;
    wxSpinCtrl* scPriorytet = nullptr;
    wxTextCtrl* tcUwagi = nullptr;

    void WybierzElement();
    void OnOK(wxCommandEvent& event);
};

#endif // DIALOGPROJEKTU_H
