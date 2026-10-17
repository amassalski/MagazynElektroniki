#ifndef DIALOGKATALOGU_H
#define DIALOGKATALOGU_H

#include "../Logika/LogikaKatalogu.h"
#include "../Logika/LogikaMagazynu.h"

#include <wx/wx.h>
#include <wx/grid.h>
#include <wx/listctrl.h>
#include <wx/srchctrl.h>
#include <wx/spinctrl.h>

#include <vector>

/// Edycja pozycji katalogu razem z tabelą parametrów
class DialogKatalogu : public wxDialog {
public:
    DialogKatalogu(wxWindow* parent, const PozycjaKatalogu& pozycja, const std::vector<std::string>& rodziny);
    PozycjaKatalogu Pobierz() const;
    /// Do testów: dopisuje parametr do tabeli
    void DodajParametr(const wxString& nazwa, const wxString& wartosc);
    int LiczbaParametrow() const;

private:
    PozycjaKatalogu pozycja_;
    wxTextCtrl *tcSymbol = nullptr, *tcProducent = nullptr, *tcOpis = nullptr, *tcObudowa = nullptr,
               *tcDatasheet = nullptr, *tcUwagi = nullptr;
    wxComboBox* cbRodzina = nullptr;
    wxGrid* siatka = nullptr;
    void OnOK(wxCommandEvent& event);
};

/// Ilość, miejsce i kategoria dla elementu dodawanego z katalogu do magazynu
class DialogDoMagazynu : public wxDialog {
public:
    DialogDoMagazynu(wxWindow* parent, LogikaMagazynu& magazyn, const wxString& symbol);
    int Ilosc() const { return scIlosc->GetValue(); }
    MiejsceMagazynowe Miejsce() const;
    std::optional<long long> WybranaKategoria() const;

private:
    std::vector<long long> idKategorii_;
    wxSpinCtrl* scIlosc = nullptr;
    wxTextCtrl* tcMiejsce = nullptr;
    wxChoice* chKategoria = nullptr;
};

/// Wyszukiwarka pozycji katalogu (do formularza elementu)
class DialogWyboruZKatalogu : public wxDialog {
public:
    DialogWyboruZKatalogu(wxWindow* parent, LogikaKatalogu& katalog, const wxString& fraza);
    /// Wybrana pozycja (z parametrami) albo nullopt
    const std::optional<PozycjaKatalogu>& Wybrana() const { return wybrana_; }
    size_t LiczbaWynikow() const { return lista_.size(); }
    void Szukaj(const wxString& tekst);
    bool Wybierz(size_t indeks);   ///< do testów

private:
    LogikaKatalogu& katalog_;
    std::vector<PozycjaKatalogu> lista_;
    std::optional<PozycjaKatalogu> wybrana_;
    wxSearchCtrl* szukaj_ = nullptr;
    wxListCtrl* widok_ = nullptr;
};

#endif // DIALOGKATALOGU_H
