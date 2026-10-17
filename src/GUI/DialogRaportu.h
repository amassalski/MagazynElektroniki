#ifndef DIALOGRAPORTU_H
#define DIALOGRAPORTU_H

#include "../Logika/LogikaRaportow.h"

#include <wx/wx.h>
#include <wx/listctrl.h>

/// Okno raportów: wybór raportu, podgląd w tabeli, zapis do CSV i HTML (do druku / PDF z przeglądarki)
class DialogRaportu : public wxDialog {
public:
    DialogRaportu(wxWindow* parent, LogikaRaportow& logika);

    /// Buduje wybrany raport i pokazuje go w tabeli (do testów)
    bool Generuj();
    void WybierzRodzaj(RodzajRaportu rodzaj);
    const Raport& AktualnyRaport() const { return raport_; }
    void UstawDaty(const wxString& od, const wxString& doo);

private:
    LogikaRaportow& logika_;
    Raport raport_;
    bool wygenerowany_ = false;
    wxChoice* chRaport = nullptr;
    wxTextCtrl* tcOd = nullptr;
    wxTextCtrl* tcDo = nullptr;
    wxStaticText* lbDaty = nullptr;
    wxStaticText* lbDo = nullptr;
    wxStaticText* lbOpis = nullptr;
    wxStaticText* lbPodsumowanie = nullptr;
    wxListCtrl* lista = nullptr;

    void Pokaz();
    void ZapiszCSV();
    void ZapiszHTML(bool otworz);
    void AktualizujPola();
};

#endif // DIALOGRAPORTU_H
