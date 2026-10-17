#ifndef DIALOGKODOW_H
#define DIALOGKODOW_H

#include "../Logika/KodyElementow.h"
#include "../Logika/SzeregiE.h"

#include <wx/wx.h>
#include <wx/notebook.h>

#include <vector>

/**
 * Narzędzia: dekoder pasków kolorów rezystorów i cewek (w obie strony) oraz kodów SMD rezystorów i kondensatorów.
 * Wyniki można skopiować do schowka albo wpisać jako wartość nowego elementu (przycisk "Użyj wartości").
 */
class DialogKodow : public wxDialog {
public:
    explicit DialogKodow(wxWindow* parent);

    // dostęp dla testów
    void UstawPaski(const std::vector<KodyElementow::Kolor>& paski);
    wxString OpisPaskow() const { return lbWynikPaskow->GetLabel(); }
    void UstawKodSMD(const wxString& kod);
    void PokazStrone(int nr) { ksiazka->SetSelection(nr); }
    wxString OpisSMD() const { return lbWynikSMD->GetLabel(); }
    void UstawWartosc(const wxString& tekst, double tol, int paski);
    std::vector<KodyElementow::Kolor> PaskiZWartosci() const { return paskiZWartosci_; }
    /// Wartość gotowa do wpisania w pole "Wartość" ("4,7k"); pusta, gdy nic nie rozpoznano
    const wxString& Wartosc() const { return wartosc_; }

private:
    class Rysunek;                         // rezystor z paskami
    wxNotebook* ksiazka = nullptr;
    wxChoice *chRodzaj = nullptr, *chLiczba = nullptr;
    std::vector<wxChoice*> chPaski;
    std::vector<wxStaticText*> lbPaski;                       // podpis nad każdym paskiem (cyfra, mnożnik...)
    std::vector<std::vector<KodyElementow::Kolor>> kolorySlotu_; // kolory dostępne w danym polu wyboru
    std::vector<int> rolaSlotu_;                              // rola, dla której pole jest wypełnione (-1 = puste)
    wxChoice* chTempco = nullptr;
    Rysunek *rysPaski = nullptr, *rysWartosc = nullptr;
    wxStaticText *lbWynikPaskow = nullptr, *lbWynikSMD = nullptr, *lbWynikWartosc = nullptr;
    wxTextCtrl *tcKod = nullptr, *tcWartosc = nullptr;
    wxChoice *chTol = nullptr, *chLiczbaW = nullptr;
    std::vector<KodyElementow::Kolor> paskiZWartosci_;
    wxString wartosc_;

    wxPanel* StronaPaskow();
    wxPanel* StronaWartosci();
    wxPanel* StronaSMD();
    void PrzebudujLiczbePaskow();   ///< dostępne liczby pasków zależą od rodzaju (cewka: 3-4)
    void PrzebudujPaski();          ///< podpisy i listy kolorów zależne od liczby pasków
    int LiczbaPaskow() const;
    void PrzeliczPaski();
    void PrzeliczWartosc();
    void PrzeliczSMD();
    RodzajElementu Rodzaj() const;
};

#endif // DIALOGKODOW_H
