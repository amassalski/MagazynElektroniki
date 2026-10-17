#ifndef DIALOGKALKULATOROW_H
#define DIALOGKALKULATOROW_H

#include <wx/wx.h>
#include <wx/notebook.h>

/// Narzędzia: dzielnik napięcia, rezystor do diody LED, filtr RC. Wyniki liczą się na bieżąco, w trakcie wpisywania.
class DialogKalkulatorow : public wxDialog {
public:
    explicit DialogKalkulatorow(wxWindow* parent);

    // dostęp dla testów
    void PokazStrone(int nr) { ksiazka->SetSelection(nr); }
    void UstawDzielnik(const wxString& uwe, const wxString& uwy, const wxString& r1, const wxString& r2);
    void UstawLED(const wxString& uzas, const wxString& uf, const wxString& pradMA);
    void UstawRC(const wxString& r, const wxString& c, const wxString& f);
    wxString OpisDzielnika() const { return lbDzielnik->GetLabel(); }
    wxString SprawdzenieDzielnika() const { return lbDzielnik2->GetLabel(); }
    wxString OpisLED() const { return lbLED->GetLabel(); }
    wxString OpisRC() const { return lbRC->GetLabel(); }
    wxString OpisRCDobor() const { return lbRC2->GetLabel(); }

private:
    wxNotebook* ksiazka = nullptr;
    wxChoice *chSzeregD = nullptr, *chSzeregL = nullptr;
    wxTextCtrl *tcUwe = nullptr, *tcUwy = nullptr, *tcR1 = nullptr, *tcR2 = nullptr;
    wxTextCtrl *tcUzas = nullptr, *tcUf = nullptr, *tcI = nullptr;
    wxTextCtrl *tcRC_R = nullptr, *tcRC_C = nullptr, *tcRC_f = nullptr;
    wxStaticText *lbDzielnik = nullptr, *lbDzielnik2 = nullptr, *lbLED = nullptr, *lbRC = nullptr, *lbRC2 = nullptr;

    wxPanel* StronaDzielnika();
    wxPanel* StronaLED();
    wxPanel* StronaRC();
    void PrzeliczDzielnik();
    void PrzeliczLED();
    void PrzeliczRC();
};

#endif // DIALOGKALKULATOROW_H
