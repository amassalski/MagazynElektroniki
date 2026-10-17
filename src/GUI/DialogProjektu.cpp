#include "DialogProjektu.h"
#include "../Narzedzia/Tlumaczenia.h"
#include "../Pomoc/PomocGUI.h"

#include <wx/srchctrl.h>
#include <wx/sizer.h>
#include <wx/timer.h>

namespace {
wxString nazwaStatusu(const std::string& status) {
    if (status == "PLANOWANY") return TR("Planowany");
    if (status == "W_TRAKCIE") return TR("W trakcie");
    if (status == "WSTRZYMANY") return TR("Wstrzymany");
    if (status == "ZAKONCZONY") return TR("Zakończony");
    return TekstZUTF8(status);
}
} // namespace

wxString NazwaStatusuProjektu(const std::string& status) { return nazwaStatusu(status); }

// ===========================================================================
// DialogProjektu
// ===========================================================================

DialogProjektu::DialogProjektu(wxWindow* parent, const Projekt& projekt)
    : wxDialog(parent, wxID_ANY, projekt.id == 0 ? TR("Nowy projekt") : TR("Edycja projektu"),
               wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      projekt_(projekt) {
    wxBoxSizer* glowny = new wxBoxSizer(wxVERTICAL);
    wxFlexGridSizer* siatka = new wxFlexGridSizer(2, 8, 10);
    siatka->AddGrowableCol(1);
    siatka->AddGrowableRow(2);

    tcNazwa = new wxTextCtrl(this, wxID_ANY, TekstZUTF8(projekt.nazwa));
    tcNazwa->SetMinSize(wxSize(380, -1));
    chStatus = new wxChoice(this, wxID_ANY);
    int wybor = 0, i = 0;
    for (const char* s : STATUSY_PROJEKTU) {
        chStatus->Append(nazwaStatusu(s));
        if (projekt.status == s) wybor = i;
        ++i;
    }
    chStatus->SetSelection(wybor);
    tcOpis = new wxTextCtrl(this, wxID_ANY, TekstZUTF8(projekt.opis), wxDefaultPosition, wxSize(-1, 90),
                            wxTE_MULTILINE);

    siatka->Add(new wxStaticText(this, wxID_ANY, TR("Nazwa *")), 0, wxALIGN_CENTER_VERTICAL);
    siatka->Add(tcNazwa, 1, wxEXPAND);
    siatka->Add(new wxStaticText(this, wxID_ANY, TR("Status")), 0, wxALIGN_CENTER_VERTICAL);
    siatka->Add(chStatus, 0);
    siatka->Add(new wxStaticText(this, wxID_ANY, TR("Opis")), 0, wxALIGN_TOP);
    siatka->Add(tcOpis, 1, wxEXPAND);
    PomocGUI::Dodaj(tcNazwa, "projekt.nazwa");
    PomocGUI::Dodaj(chStatus, "projekt.status");
    PomocGUI::Dodaj(tcOpis, "projekt.opis");
    glowny->Add(siatka, 1, wxEXPAND | wxALL, 12);
    glowny->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
    SetSizerAndFit(glowny);
    tcNazwa->SetFocus();
    Bind(wxEVT_BUTTON, &DialogProjektu::OnOK, this, wxID_OK);
}

Projekt DialogProjektu::PobierzProjekt() const {
    Projekt p = projekt_;
    p.nazwa = tcNazwa->GetValue().Strip(wxString::both).utf8_string();
    p.opis = tcOpis->GetValue().Strip(wxString::both).utf8_string();
    const int s = chStatus->GetSelection();
    if (s >= 0 && s < 4) p.status = STATUSY_PROJEKTU[s];
    return p;
}

void DialogProjektu::OnOK(wxCommandEvent& event) {
    if (tcNazwa->GetValue().Strip(wxString::both).empty()) {
        wxMessageBox(TR("Podaj nazwę projektu."), TR("Projekt"), wxOK | wxICON_WARNING, this);
        tcNazwa->SetFocus();
        return;
    }
    event.Skip();
}

// ===========================================================================
// DialogWyboruElementu
// ===========================================================================

DialogWyboruElementu::DialogWyboruElementu(wxWindow* parent, LogikaMagazynu& logika,
                                           const wxString& poczatkoweSzukanie)
    : wxDialog(parent, wxID_ANY, TR("Wybierz element z magazynu"), wxDefaultPosition, wxSize(780, 460),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      logika_(logika), timer_(this) {
    wxBoxSizer* glowny = new wxBoxSizer(wxVERTICAL);
    szukaj_ = new wxSearchCtrl(this, wxID_ANY, poczatkoweSzukanie, wxDefaultPosition, wxDefaultSize,
                               wxTE_PROCESS_ENTER);
    szukaj_->SetDescriptiveText(TR("Szukaj: symbol, opis, wartość, obudowa..."));
    glowny->Add(szukaj_, 0, wxEXPAND | wxALL, 8);

    lista_ = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                            wxLC_REPORT | wxLC_SINGLE_SEL | wxBORDER_THEME);
    lista_->InsertColumn(0, TR("Symbol"), wxLIST_FORMAT_LEFT, 170);
    lista_->InsertColumn(1, TR("Producent"), wxLIST_FORMAT_LEFT, 100);
    lista_->InsertColumn(2, TR("Wartość"), wxLIST_FORMAT_LEFT, 80);
    lista_->InsertColumn(3, TR("Obudowa"), wxLIST_FORMAT_LEFT, 90);
    lista_->InsertColumn(4, TR("Opis"), wxLIST_FORMAT_LEFT, 230);
    lista_->InsertColumn(5, TR("Ilość"), wxLIST_FORMAT_RIGHT, 60);
    glowny->Add(lista_, 1, wxEXPAND | wxLEFT | wxRIGHT, 8);
    glowny->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 8);
    SetSizer(glowny);

    szukaj_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { timer_.StartOnce(250); });
    szukaj_->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent&) { timer_.Stop(); Odswiez(); });
    Bind(wxEVT_TIMER, [this](wxTimerEvent&) { Odswiez(); });
    lista_->Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](wxListEvent&) {
        wxCommandEvent e(wxEVT_BUTTON, wxID_OK);
        OnOK(e);
    });
    Bind(wxEVT_BUTTON, &DialogWyboruElementu::OnOK, this, wxID_OK);
    Bind(wxEVT_UPDATE_UI, [this](wxUpdateUIEvent& e) { e.Enable(lista_->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED) >= 0); }, wxID_OK);
    Odswiez();
    szukaj_->SetFocus();
}

void DialogWyboruElementu::Odswiez() {
    FiltrElementow f;
    f.tekst = szukaj_->GetValue().utf8_string();
    f.limit = 300;
    elementy_.clear();
    lista_->DeleteAllItems();
    if (!logika_.pobierzElementy(f, elementy_)) return;
    long i = 0;
    for (const Element& e : elementy_) {
        lista_->InsertItem(i, TekstZUTF8(e.symbol));
        lista_->SetItem(i, 1, TekstZUTF8(e.producent));
        lista_->SetItem(i, 2, TekstZUTF8(e.wartosc));
        lista_->SetItem(i, 3, TekstZUTF8(e.obudowa));
        lista_->SetItem(i, 4, TekstZUTF8(e.opis));
        lista_->SetItem(i, 5, wxString::Format("%d", e.iloscDostepna));
        ++i;
    }
    if (i > 0) lista_->SetItemState(0, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
                                    wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
}

void DialogWyboruElementu::OnOK(wxCommandEvent&) {
    const long w = lista_->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (w < 0 || static_cast<size_t>(w) >= elementy_.size()) return;
    wybrane_ = elementy_[static_cast<size_t>(w)].id;
    EndModal(wxID_OK);
}

// ===========================================================================
// DialogPozycji
// ===========================================================================

DialogPozycji::DialogPozycji(wxWindow* parent, LogikaMagazynu& logika, const PozycjaProjektu& pozycja, bool nowa)
    : wxDialog(parent, wxID_ANY, nowa ? TR("Dodaj część do projektu") : TR("Edycja pozycji"),
               wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      logika_(logika), pozycja_(pozycja), nowa_(nowa) {
    wxBoxSizer* glowny = new wxBoxSizer(wxVERTICAL);
    wxFlexGridSizer* siatka = new wxFlexGridSizer(2, 8, 10);
    siatka->AddGrowableCol(1);

    wxBoxSizer* wierszElementu = new wxBoxSizer(wxHORIZONTAL);
    lbElement = new wxStaticText(this, wxID_ANY, wxEmptyString);
    lbElement->SetMinSize(wxSize(300, -1));
    wierszElementu->Add(lbElement, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    if (nowa) {
        wxButton* b = new wxButton(this, wxID_ANY, TR("Wybierz..."));
        b->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { WybierzElement(); });
        wierszElementu->Add(b, 0);
    }
    siatka->Add(new wxStaticText(this, wxID_ANY, TR("Element")), 0, wxALIGN_CENTER_VERTICAL);
    siatka->Add(wierszElementu, 1, wxEXPAND);

    tcOznaczenia = new wxTextCtrl(this, wxID_ANY, TekstZUTF8(pozycja.oznaczenia));
    tcOznaczenia->SetHint(TR("np. R1, R2, R5-R7"));
    scWymagana = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(100, -1),
                                wxSP_ARROW_KEYS, 1, 1000000, pozycja.wymagana > 0 ? pozycja.wymagana : 1);
    scZamowiona = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(100, -1),
                                 wxSP_ARROW_KEYS, 0, 1000000, pozycja.zamowiona);
    scPriorytet = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(100, -1),
                                 wxSP_ARROW_KEYS, 1, 10, pozycja.priorytet);
    tcUwagi = new wxTextCtrl(this, wxID_ANY, TekstZUTF8(pozycja.uwagi), wxDefaultPosition, wxSize(-1, 60),
                             wxTE_MULTILINE);

    siatka->Add(new wxStaticText(this, wxID_ANY, TR("Oznaczenia")), 0, wxALIGN_CENTER_VERTICAL);
    siatka->Add(tcOznaczenia, 1, wxEXPAND);
    siatka->Add(new wxStaticText(this, wxID_ANY, TR("Potrzebna ilość")), 0, wxALIGN_CENTER_VERTICAL);
    siatka->Add(scWymagana, 0);
    siatka->Add(new wxStaticText(this, wxID_ANY, TR("Już zamówiono")), 0, wxALIGN_CENTER_VERTICAL);
    siatka->Add(scZamowiona, 0);
    siatka->Add(new wxStaticText(this, wxID_ANY, TR("Priorytet (1-10)")), 0, wxALIGN_CENTER_VERTICAL);
    siatka->Add(scPriorytet, 0);
    siatka->Add(new wxStaticText(this, wxID_ANY, TR("Uwagi")), 0, wxALIGN_TOP);
    siatka->Add(tcUwagi, 1, wxEXPAND);
    PomocGUI::Dodaj(lbElement, "pozycja.element");
    PomocGUI::Dodaj(tcOznaczenia, "pozycja.oznaczenia");
    PomocGUI::Dodaj(scWymagana, "pozycja.potrzebna");
    PomocGUI::Dodaj(scZamowiona, "pozycja.zamowiono");
    PomocGUI::Dodaj(scPriorytet, "pozycja.priorytet");
    PomocGUI::Dodaj(tcUwagi, "pozycja.uwagi");
    glowny->Add(siatka, 1, wxEXPAND | wxALL, 12);
    glowny->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
    SetSizerAndFit(glowny);
    SetMinSize(GetSize());

    if (!nowa) {
        lbElement->SetLabel(TekstZUTF8(pozycja.symbol + (pozycja.wartosc.empty() ? "" : "  " + pozycja.wartosc) +
                                       (pozycja.obudowa.empty() ? "" : "  " + pozycja.obudowa)));
    } else {
        lbElement->SetLabel(TR("(nie wybrano)"));
    }
    Bind(wxEVT_BUTTON, &DialogPozycji::OnOK, this, wxID_OK);
}

void DialogPozycji::WybierzElement() {
    DialogWyboruElementu dlg(this, logika_);
    if (dlg.ShowModal() != wxID_OK) return;
    pozycja_.idElementu = dlg.WybraneId();
    if (auto e = logika_.pobierzElement(pozycja_.idElementu)) {
        lbElement->SetLabel(TekstZUTF8(e->symbol + (e->wartosc.empty() ? "" : "  " + e->wartosc) +
                                       (e->obudowa.empty() ? "" : "  " + e->obudowa)));
    }
}

PozycjaProjektu DialogPozycji::PobierzPozycje() const {
    PozycjaProjektu p = pozycja_;
    p.oznaczenia = tcOznaczenia->GetValue().Strip(wxString::both).utf8_string();
    p.wymagana = scWymagana->GetValue();
    p.zamowiona = scZamowiona->GetValue();
    p.priorytet = scPriorytet->GetValue();
    p.uwagi = tcUwagi->GetValue().Strip(wxString::both).utf8_string();
    return p;
}

void DialogPozycji::OnOK(wxCommandEvent& event) {
    if (nowa_ && pozycja_.idElementu == 0) {
        wxMessageBox(TR("Wybierz element z magazynu."), TR("Pozycja"), wxOK | wxICON_WARNING, this);
        return;
    }
    event.Skip();
}
