#ifndef DIALOGIMPORTBOM_H
#define DIALOGIMPORTBOM_H

#include "../Import/ImportBOM.h"
#include "../Logika/LogikaMagazynu.h"
#include "../Logika/LogikaProjektow.h"

#include <wx/wx.h>
#include <wx/listctrl.h>
#include <wx/spinctrl.h>

#include <vector>

/**
 * Kreator importu listy części z KiCada / Eagle'a do projektu.
 * Po wczytaniu pliku każda pozycja dostaje propozycję: element z magazynu (gdy pasuje
 * symbol albo wartość z obudową) albo "utwórz nowy". Wybór można zmienić dla każdej
 * pozycji osobno, zanim cokolwiek zapisze się w bazie.
 */
class DialogImportBOM : public wxDialog {
public:
    DialogImportBOM(wxWindow* parent, LogikaMagazynu& magazyn, LogikaProjektow& projekty, long long idProjektu,
                    const wxString& nazwaProjektu, const wxString& sciezka = wxEmptyString);

    /// Wczytuje plik (ścieżka UTF-8) i buduje propozycje. Zwraca false przy błędzie (już pokazanym).
    bool WczytajPlik(const wxString& sciezka);
    const WynikImportuDoProjektu& Wynik() const { return wynik_; }
    /// Czy po wczytaniu jest co importować (do testów)
    size_t LiczbaWierszy() const { return wiersze_.size(); }

private:
    LogikaMagazynu& magazyn_;
    LogikaProjektow& projekty_;
    long long idProjektu_;
    WynikImportuDoProjektu wynik_;
    WynikImportuBOM wczytane_;
    std::vector<WierszImportuBOM> wiersze_;
    std::vector<Element> elementy_;           ///< aktywne elementy magazynu (do dopasowania)
    std::vector<long long> idKategorii_;      ///< równolegle do chKategoria

    wxTextCtrl* tcPlik = nullptr;
    wxStaticText* lbFormat = nullptr;
    wxSpinCtrl* scPlytek = nullptr;
    wxCheckBox* cbZastap = nullptr;
    wxChoice* chKategoria = nullptr;
    wxListCtrl* lista = nullptr;
    wxStaticText* lbPodsumowanie = nullptr;

    void Wypelnij();
    void OdswiezWiersz(size_t i);
    void Podsumuj();
    void UstawAkcje(WierszImportuBOM::Akcja akcja);
    void WybierzElementDlaZaznaczonej();
    void OnImportuj(wxCommandEvent& event);
    wxString OpisElementu(long long id) const;
};

#endif // DIALOGIMPORTBOM_H
