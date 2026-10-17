#include "DialogZestawienia.h"

#include "../Eksport/EksportRaport.h"
#include "../Logika/LogikaRaportow.h"
#include "../Narzedzia/Tlumaczenia.h"
#include "../Pomoc/PomocGUI.h"

#include <wx/clipbrd.h>
#include <wx/datetime.h>
#include <wx/filename.h>
#include <wx/filedlg.h>
#include <wx/utils.h>

#include <algorithm>
#include <set>

DialogZestawienia::DialogZestawienia(wxWindow* parent, LogikaProjektow& projekty, LogikaSklepow& sklepy,
                                     std::optional<long long> idProjektu)
    : wxDialog(parent, wxID_ANY, TR("Najtańsze zestawienie zakupów"), wxDefaultPosition, wxSize(1060, 700),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      projekty_(projekty), sklepy_(sklepy), spr_(sklepy) {
    wxBoxSizer* glowny = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* gora = new wxBoxSizer(wxHORIZONTAL);
    gora->Add(new wxStaticText(this, wxID_ANY, TR("Zakres:")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    chProjekt = new wxChoice(this, wxID_ANY);
    chProjekt->Append(TR("Wszystkie aktywne projekty"));
    projekty_.pobierzProjekty(true, listaProjektow_);
    for (const Projekt& p : listaProjektow_) chProjekt->Append(TekstZUTF8(p.nazwa));
    chProjekt->SetSelection(0);
    gora->Add(chProjekt, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 16);
    cbZamowione = new wxCheckBox(this, wxID_ANY, TR("Odejmij już zamówione"));
    cbZamowione->SetValue(true);
    gora->Add(cbZamowione, 0, wxALIGN_CENTER_VERTICAL);
    glowny->Add(gora, 0, wxEXPAND | wxALL, 10);

    wxBoxSizer* tryby = new wxBoxSizer(wxHORIZONTAL);
    rbTryb[0] = new wxRadioButton(this, wxID_ANY, TR("Najniższa suma z dostawą"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    rbTryb[1] = new wxRadioButton(this, wxID_ANY, TR("Najniższe ceny części (bez dostawy)"));
    rbTryb[2] = new wxRadioButton(this, wxID_ANY, TR("Wszystko w jednym sklepie"));
    for (wxRadioButton* r : rbTryb) tryby->Add(r, 0, wxRIGHT, 16);
    glowny->Add(tryby, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);

    lbPorownanie = new wxStaticText(this, wxID_ANY, wxEmptyString);
    glowny->Add(lbPorownanie, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);

    lista = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxBORDER_THEME);
    glowny->Add(lista, 1, wxEXPAND | wxLEFT | wxRIGHT, 10);
    PomocGUI::DymekZKolumn(lista);
    lbPodsumowanie = new wxStaticText(this, wxID_ANY, wxEmptyString);
    glowny->Add(lbPodsumowanie, 0, wxALL, 10);
    lbStatus = new wxStaticText(this, wxID_ANY, TR("Ceny pochodzą z linków elementów do sklepów (menu Sklepy → Sklepy i ceny elementu)."));
    glowny->Add(lbStatus, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);

    wxBoxSizer* dol = new wxBoxSizer(wxHORIZONTAL);
    bOdswiez = new wxButton(this, wxID_ANY, TR("Sprawdź aktualne ceny"));
    wxButton* bPrzelicz = new wxButton(this, wxID_ANY, TR("Przelicz"));
    wxButton* bKopiuj = new wxButton(this, wxID_ANY, TR("Kopiuj listę zamówień"));
    wxButton* bCsv = new wxButton(this, wxID_ANY, TR("Zapisz CSV..."));
    wxButton* bHtml = new wxButton(this, wxID_ANY, TR("Zapisz HTML..."));
    for (wxButton* b : {bOdswiez, bPrzelicz, bKopiuj, bCsv, bHtml}) dol->Add(b, 0, wxRIGHT, 6);
    dol->AddStretchSpacer();
    dol->Add(new wxButton(this, wxID_CANCEL, TR("Zamknij")));
    glowny->Add(dol, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    SetSizer(glowny);
    SetEscapeId(wxID_CANCEL);

    chProjekt->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { Przelicz(); });
    cbZamowione->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { Przelicz(); });
    for (wxRadioButton* r : rbTryb) r->Bind(wxEVT_RADIOBUTTON, [this](wxCommandEvent&) { Pokaz(); });
    bPrzelicz->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Przelicz(); });
    bOdswiez->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (spr_.wToku()) { spr_.Anuluj(); return; }
        OdswiezCeny();
    });
    bKopiuj->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { KopiujListe(); });
    bCsv->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ZapiszCSV(); });
    bHtml->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ZapiszHTML(); });
    for (wxButton* b : {bKopiuj, bCsv, bHtml})
        b->Bind(wxEVT_UPDATE_UI, [this](wxUpdateUIEvent& e) { e.Enable(!plan_.pozycje.empty()); });

    WybierzProjekt(idProjektu);
}

DialogZestawienia::~DialogZestawienia() { *zywy_ = false; }

void DialogZestawienia::WybierzProjekt(std::optional<long long> id) {
    int wybor = 0;
    if (id)
        for (size_t i = 0; i < listaProjektow_.size(); ++i) if (listaProjektow_[i].id == *id) wybor = static_cast<int>(i) + 1;
    chProjekt->SetSelection(wybor);
    Przelicz();
}

void DialogZestawienia::WybierzTryb(TrybPlanu t) {
    rbTryb[static_cast<int>(t)]->SetValue(true);
    Pokaz();
}

void DialogZestawienia::OdejmujZamowione(bool tak) {
    cbZamowione->SetValue(tak);
    Przelicz();
}

TrybPlanu DialogZestawienia::Tryb() const {
    if (rbTryb[1]->GetValue()) return TrybPlanu::NajtanszeCeny;
    if (rbTryb[2]->GetValue()) return TrybPlanu::JedenSklep;
    return TrybPlanu::Najtanszy;
}

const PlanZakupow& DialogZestawienia::PlanDlaTrybu(TrybPlanu t) const {
    switch (t) {
        case TrybPlanu::NajtanszeCeny: return planCeny_;
        case TrybPlanu::JedenSklep: return planJeden_;
        default: return planNajtanszy_;
    }
}

bool DialogZestawienia::Przelicz() {
    const int s = chProjekt->GetSelection();
    std::optional<long long> idProj;
    if (s > 0 && static_cast<size_t>(s - 1) < listaProjektow_.size()) idProj = listaProjektow_[static_cast<size_t>(s - 1)].id;

    std::vector<PozycjaProjektu> braki;
    if (!projekty_.pobierzBraki(idProj, braki)) {
        wxMessageBox(TekstZUTF8(projekty_.pobierzBlad()), TR("Zestawienie"), wxOK | wxICON_ERROR, this);
        return false;
    }
    potrzeby_.clear();
    for (const PozycjaProjektu& p : braki) {
        const int ile = cbZamowione->GetValue() ? std::max(0, p.brakuje() - p.zamowiona) : p.brakuje();
        if (ile > 0) potrzeby_.push_back({p.idElementu, p.symbol, ile});
    }
    std::vector<long long> ids;
    for (const PotrzebaZakupu& p : potrzeby_) ids.push_back(p.idElementu);
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());

    std::vector<LinkSklepu> oferty;
    std::vector<Sklep> sklepy;
    std::map<std::string, double> kursy;
    if (!sklepy_.pobierzLinkiElementow(ids, oferty) || !sklepy_.pobierzSklepy(true, sklepy) || !sklepy_.pobierzKursy(kursy)) {
        wxMessageBox(TekstZUTF8(sklepy_.pobierzBlad()), TR("Zestawienie"), wxOK | wxICON_ERROR, this);
        return false;
    }
    planNajtanszy_ = PlanZakupowLogika::zaplanuj(potrzeby_, oferty, sklepy, kursy, TrybPlanu::Najtanszy);
    planCeny_ = PlanZakupowLogika::zaplanuj(potrzeby_, oferty, sklepy, kursy, TrybPlanu::NajtanszeCeny);
    planJeden_ = PlanZakupowLogika::zaplanuj(potrzeby_, oferty, sklepy, kursy, TrybPlanu::JedenSklep);
    Pokaz();
    return true;
}

Raport DialogZestawienia::RaportZPlanu() const {
    const int s = chProjekt->GetSelection();
    const wxString zakres = s > 0 ? chProjekt->GetString(static_cast<unsigned>(s)) : TR("wszystkie aktywne projekty");
    Raport r = PlanZakupowLogika::naRaport(plan_, ("Zestawienie zakupów: " + zakres).utf8_string());
    r.opis = "Stan na " + wxDateTime::Now().Format("%Y-%m-%d %H:%M").utf8_string() + ". Ceny z ostatniego sprawdzenia, kursy walut wpisane ręcznie.";
    return r;
}

void DialogZestawienia::Pokaz() {
    plan_ = PlanDlaTrybu(Tryb());
    lista->ClearAll();
    const Raport r = RaportZPlanu();
    for (size_t k = 0; k < r.naglowki.size(); ++k)
        lista->AppendColumn(TekstZUTF8(r.naglowki[k]), r.wyrownajDoPrawej[k] ? wxLIST_FORMAT_RIGHT : wxLIST_FORMAT_LEFT,
                            k == 0 ? 130 : k == 1 ? 260 : 110);
    for (size_t i = 0; i < r.wiersze.size(); ++i) {
        const long w = lista->InsertItem(static_cast<long>(i), TekstZUTF8(r.wiersze[i][0]));
        for (size_t k = 1; k < r.wiersze[i].size(); ++k) lista->SetItem(w, static_cast<int>(k), TekstZUTF8(r.wiersze[i][k]));
    }
    wxString podsumowanie;
    for (const std::string& l : r.podsumowanie) podsumowanie += TekstZUTF8(l) + "\n";
    if (potrzeby_.empty()) podsumowanie = TR("Niczego nie brakuje (albo wszystko jest już zamówione).");
    lbPodsumowanie->SetLabel(podsumowanie.Trim());

    auto kwota = [](const PlanZakupow& p, bool wymagajKompletu) -> wxString {
        if (p.pozycje.empty()) return TR("brak");
        if (wymagajKompletu && !p.kompletny()) return TR("brak");
        return TekstZUTF8(LogikaRaportow::formatKwoty(p.razemPLN())) + " zł";
    };
    wxString por = TR("Porównanie (razem z dostawą):  najtańszy układ ") + kwota(planNajtanszy_, false) + TR("   |   najniższe ceny części ") +
                   kwota(planCeny_, false) + TR("   |   jeden sklep ") + kwota(planJeden_, true);
    if (!planNajtanszy_.pozycje.empty() && !planCeny_.pozycje.empty() && planCeny_.razemPLN() > planNajtanszy_.razemPLN() + 0.005)
        por += TR("\nŁączenie zakupów oszczędza ") + TekstZUTF8(LogikaRaportow::formatKwoty(planCeny_.razemPLN() - planNajtanszy_.razemPLN())) + TR(" zł na dostawie.");
    lbPorownanie->SetLabel(por);
    Layout();
}

bool DialogZestawienia::OdswiezCeny(std::function<void(bool)> koniec) {
    std::vector<long long> ids;
    for (const PotrzebaZakupu& p : potrzeby_) ids.push_back(p.idElementu);
    std::vector<LinkSklepu> linki;
    std::vector<Sklep> sklepy;
    if (!sklepy_.pobierzLinkiElementow(ids, linki, false) || !sklepy_.pobierzSklepy(true, sklepy)) return false;
    std::vector<ZadanieCeny> zadania;
    for (const LinkSklepu& l : linki) {
        auto sym = std::find_if(potrzeby_.begin(), potrzeby_.end(), [&](const PotrzebaZakupu& p) { return p.idElementu == l.idElementu; });
        auto z = SprawdzaczCen::ZadaniaDlaLinkow({l}, sklepy, sym == potrzeby_.end() ? std::string() : sym->symbol);
        zadania.insert(zadania.end(), z.begin(), z.end());
    }
    if (zadania.empty()) {
        lbStatus->SetLabel(TR("Brak linków do sprawdzenia - dodaj linki do sklepów w oknie \"Sklepy i ceny elementu\"."));
        return false;
    }
    const bool start = spr_.Start(zadania,
        [this, zywy = zywy_](int n, int r, const WynikSprawdzenia& w) {
            if (*zywy) lbStatus->SetLabel(wxString::Format(TR("Sprawdzono %d z %d: %s - %s"), n, r, TekstZUTF8(w.zadanie.opis), TekstZUTF8(w.komunikat)));
        },
        [this, zywy = zywy_, koniec](bool anul) {
            if (!*zywy) return;
            bOdswiez->SetLabel(TR("Sprawdź aktualne ceny"));
            lbStatus->SetLabel(anul ? TR("Przerwano sprawdzanie cen.") : TR("Sprawdzanie cen zakończone."));
            Przelicz();
            if (koniec) koniec(anul);
        });
    if (start) bOdswiez->SetLabel(TR("Przerwij sprawdzanie"));
    return start;
}

void DialogZestawienia::ZapiszCSV() {
    wxFileDialog dlg(this, TR("Zapisz zestawienie"), wxEmptyString, "zestawienie_zakupow_" + wxDateTime::Now().Format("%Y%m%d") + ".csv",
                     TR("Pliki CSV (*.csv)|*.csv"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() != wxID_OK) return;
    std::string blad;
    if (!EksportRaport::zapiszCSV(dlg.GetPath().utf8_string(), RaportZPlanu(), blad))
        wxMessageBox(TekstZUTF8(blad), TR("Zestawienie"), wxOK | wxICON_ERROR, this);
}

void DialogZestawienia::ZapiszHTML() {
    wxFileDialog dlg(this, TR("Zapisz zestawienie"), wxEmptyString, "zestawienie_zakupow_" + wxDateTime::Now().Format("%Y%m%d") + ".html",
                     TR("Strony HTML (*.html)|*.html"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() != wxID_OK) return;
    std::string blad;
    if (!EksportRaport::zapiszHTML(dlg.GetPath().utf8_string(), RaportZPlanu(), wxDateTime::Now().Format("%Y-%m-%d %H:%M").utf8_string(), blad))
        wxMessageBox(TekstZUTF8(blad), TR("Zestawienie"), wxOK | wxICON_ERROR, this);
    else wxLaunchDefaultBrowser(wxFileName::FileNameToURL(dlg.GetPath()));
}

void DialogZestawienia::KopiujListe() {
    if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData(new wxTextDataObject(TekstZUTF8(PlanZakupowLogika::naListeZamowien(plan_))));
        wxTheClipboard->Close();
        lbStatus->SetLabel(TR("Lista zamówień skopiowana do schowka."));
    }
}
