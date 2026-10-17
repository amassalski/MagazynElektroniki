#ifndef DIALOGIMPORTUELEMENTOW_H
#define DIALOGIMPORTUELEMENTOW_H

#include "../Logika/ImportElementow.h"
#include "../Logika/LogikaMagazynu.h"
#include "../Logika/PozycjaImportu.h"

#include <wx/wx.h>
#include <wx/listctrl.h>

#include <set>
#include <vector>

/**
 * Wprowadzanie całych zestawów elementów naraz. Dwa tryby:
 *  - Szereg: generator wartości E6 / E12 / E24 / E48 / E96 dla rezystorów, kondensatorów i cewek,
 *  - Plik: elementy z pliku CSV (rezystory, kondensatory, tranzystory, cokolwiek).
 * Wynik widać na liście podglądu (nowy / już jest / błąd), zanim cokolwiek trafi do bazy.
 */
class DialogImportuElementow : public wxDialog {
public:
    enum class Tryb { Szereg, Plik };

    DialogImportuElementow(wxWindow* parent, LogikaMagazynu& magazyn, Tryb tryb);

    /// Wczytuje plik CSV do podglądu (tryb Plik). false przy błędzie (już pokazanym, chyba że cicho).
    bool WczytajPlik(const wxString& sciezka, bool cicho = false);
    /// Przelicza podgląd generatora z bieżących pól (tryb Szereg)
    void PrzeliczSzereg();

    const std::vector<PozycjaImportu>& Pozycje() const { return pozycje_; }
    const WynikImportuElementow& Wynik() const { return wynik_; }
    size_t LiczbaPoprawnych() const;
    /// Do testów: ustawia pole po kluczu ("rodzaj", "szereg", "od", "do", "obudowa", "ilosc", "miejsce", "kategoria", "dekada")
    void UstawPole(const wxString& klucz, const wxString& wartosc);
    bool Zapisz();   ///< zapis do bazy (jak przycisk "Dodaj do magazynu"); false przy błędzie

private:
    LogikaMagazynu& magazyn_;
    Tryb tryb_;
    std::vector<PozycjaImportu> pozycje_;
    std::set<std::string> istniejace_;    ///< symbole małymi literami
    WynikImportuElementow wynik_;

    wxChoice *chRodzaj = nullptr, *chSzereg = nullptr, *chDuplikaty = nullptr;
    wxTextCtrl *tcOd = nullptr, *tcDo = nullptr, *tcTol = nullptr, *tcObudowa = nullptr, *tcMoc = nullptr,
               *tcNapiecie = nullptr, *tcDielektryk = nullptr, *tcProducent = nullptr, *tcKategoria = nullptr,
               *tcMiejsce = nullptr, *tcIlosc = nullptr, *tcMinimum = nullptr, *tcCena = nullptr, *tcWzor = nullptr,
               *tcPlik = nullptr;
    wxCheckBox* cbDekada = nullptr;
    wxListCtrl* lista = nullptr;
    wxStaticText *lbPodsumowanie = nullptr, *lbBlad = nullptr;

    void BudujSzereg(wxSizer* gora);
    void BudujPlik(wxSizer* gora);
    void Wypelnij();
    wxString StatusPozycji(const PozycjaImportu& p) const;
    TrybDuplikatow WybranyTryb() const;
    void OnZapiszCSV(wxCommandEvent&);
    void OnSzablon(wxCommandEvent&);
};

#endif // DIALOGIMPORTUELEMENTOW_H
