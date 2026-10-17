#ifndef DIALOGELEMENTU_H
#define DIALOGELEMENTU_H

#include "../Model/Element.h"
#include "../Model/Katalog.h"

#include <wx/wx.h>
#include <wx/spinctrl.h>

#include <string>
#include <vector>

class LogikaMagazynu;

/**
 * @class DialogElementu
 * @brief Formularz dodawania i edycji elementu.
 *
 * Dla nowego elementu przekaż Element z id == 0 (można ustawić idKategorii).
 * Po zatwierdzeniu (wxID_OK) wynik zwraca PobierzElement(); zapis do bazy robi
 * wywołujący.
 */
class DialogElementu : public wxDialog {
public:
    DialogElementu(wxWindow* parent, LogikaMagazynu& logika, const Element& element);

    Element PobierzElement() const;
    /// Wypełnia pola danymi pozycji katalogu (puste pola katalogu nie nadpisują formularza)
    void UzupelnijZKatalogu(const PozycjaKatalogu& poz);

private:
    Element element_;
    std::vector<long long> idKategorii_;   ///< id kategorii odpowiadające pozycjom listy (od 1)

    wxTextCtrl* tcSymbol = nullptr;
    wxTextCtrl* tcSymbolProducenta = nullptr;
    wxTextCtrl* tcProducent = nullptr;
    wxChoice* chKategoria = nullptr;
    wxTextCtrl* tcOpis = nullptr;
    wxTextCtrl* tcWartosc = nullptr;
    wxTextCtrl* tcJednostka = nullptr;
    wxTextCtrl* tcObudowa = nullptr;
    wxTextCtrl* tcTolerancja = nullptr;
    wxTextCtrl* tcParametry = nullptr;
    wxComboBox* cbKontener = nullptr;
    wxTextCtrl* tcSzuflada = nullptr;
    wxTextCtrl* tcPrzegroda = nullptr;
    LogikaMagazynu* logika_ = nullptr;
    void OdswiezPodpowiedziSzuflad();
    void WybierzZKatalogu();
    wxSpinCtrl* scIlosc = nullptr;
    wxSpinCtrl* scMinimum = nullptr;
    wxTextCtrl* tcCena = nullptr;
    wxComboBox* cbWaluta = nullptr;
    wxTextCtrl* tcDatasheet = nullptr;
    wxTextCtrl* tcZdjecie = nullptr;
    void WybierzZdjecie();
    wxTextCtrl* tcTagi = nullptr;

    void UtworzKontrole(LogikaMagazynu& logika);
    void WypelnijKontrole();
    void OnOK(wxCommandEvent& event);
};

/**
 * @class DialogZmianyIlosci
 * @brief Przyjęcie, wydanie albo ustawienie stanu elementu (zapisywane w historii).
 */
class DialogZmianyIlosci : public wxDialog {
public:
    DialogZmianyIlosci(wxWindow* parent, const wxString& symbol, int aktualnaIlosc);

    /// Zmiana ilości: dodatnia = przyjęcie, ujemna = wydanie
    int Zmiana() const;
    /// PRZYJECIE, WYDANIE albo KOREKTA
    std::string Typ() const;
    std::string Komentarz() const;

private:
    int aktualna_;
    wxRadioButton* rbPrzyjecie = nullptr;
    wxRadioButton* rbWydanie = nullptr;
    wxRadioButton* rbUstaw = nullptr;
    wxSpinCtrl* scIlosc = nullptr;
    wxTextCtrl* tcKomentarz = nullptr;
    wxStaticText* stPodsumowanie = nullptr;

    void Aktualizuj();
    void OnOK(wxCommandEvent& event);
};

#endif // DIALOGELEMENTU_H
