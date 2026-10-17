#ifndef DIALOGZESTAWIENIA_H
#define DIALOGZESTAWIENIA_H

#include "../Logika/LogikaProjektow.h"
#include "../Logika/LogikaSklepow.h"
#include "../Logika/PlanZakupow.h"
#include "../Wyszukiwanie/SprawdzanieCen.h"

#include <wx/wx.h>
#include <wx/listctrl.h>

#include <memory>
#include <optional>

/**
 * Najtańsze zestawienie zakupów: braki w projekcie (albo we wszystkich aktywnych) rozdzielone między sklepy
 * tak, by suma z dostawą była jak najniższa; porównanie z wariantem "najtańsze ceny" i "jeden sklep".
 */
class DialogZestawienia : public wxDialog {
public:
    DialogZestawienia(wxWindow* parent, LogikaProjektow& projekty, LogikaSklepow& sklepy,
                      std::optional<long long> idProjektu = std::nullopt);
    ~DialogZestawienia() override;

    /// Wybiera zakres: nullopt = wszystkie aktywne projekty
    void WybierzProjekt(std::optional<long long> id);
    void WybierzTryb(TrybPlanu tryb);
    void OdejmujZamowione(bool tak);
    /// Przelicza zestawienie z bieżących danych w bazie
    bool Przelicz();
    const PlanZakupow& Plan() const { return plan_; }
    const PlanZakupow& PlanDlaTrybu(TrybPlanu t) const;
    const std::vector<PotrzebaZakupu>& Potrzeby() const { return potrzeby_; }
    /// Sprawdza ceny linków potrzebnych elementów, potem przelicza
    bool OdswiezCeny(std::function<void(bool anulowano)> koniec = nullptr);
    bool Sprawdzanie() const { return spr_.wToku(); }
    Raport RaportZPlanu() const;
    wxString TekstPorownania() const { return lbPorownanie->GetLabel(); }

private:
    LogikaProjektow& projekty_;
    LogikaSklepow& sklepy_;
    SprawdzaczCen spr_;
    std::shared_ptr<bool> zywy_ = std::make_shared<bool>(true);
    std::vector<Projekt> listaProjektow_;
    std::vector<PotrzebaZakupu> potrzeby_;
    PlanZakupow plan_, planNajtanszy_, planCeny_, planJeden_;
    wxChoice* chProjekt = nullptr;
    wxRadioButton* rbTryb[3] = {nullptr, nullptr, nullptr};
    wxCheckBox* cbZamowione = nullptr;
    wxListCtrl* lista = nullptr;
    wxStaticText* lbPorownanie = nullptr;
    wxStaticText* lbPodsumowanie = nullptr;
    wxStaticText* lbStatus = nullptr;
    wxButton* bOdswiez = nullptr;

    TrybPlanu Tryb() const;
    void Pokaz();
    void ZapiszCSV();
    void ZapiszHTML();
    void KopiujListe();
};

#endif // DIALOGZESTAWIENIA_H
