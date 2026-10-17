#include "DialogUstawien.h"
#include "../Narzedzia/Tlumaczenia.h"
#include "../BazaDanych/ManagerBazyDanych.h"
#include "../Narzedzia/Konfiguracja.h"
#include "DialogPolaczenia.h"
#include "../Pomoc/PomocGUI.h"
#include <wx/sizer.h>
#include <wx/intl.h>
#include <wx/dirdlg.h>
#include <wx/font.h>
#include <wx/notebook.h>

wxBEGIN_EVENT_TABLE(DialogUstawien, wxDialog)
wxEND_EVENT_TABLE()

DialogUstawien::DialogUstawien(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, TR("Ustawienia"),
               wxDefaultPosition, wxSize(550, 500),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
    
    UtworzKontrole();
    UlozKontrole();
    WczytajUstawienia();
    Center();
}

void DialogUstawien::UtworzKontrole() {
    // === BAZA DANYCH ===
    stOpisBaza = new wxStaticText(this, wxID_ANY, wxEmptyString);
    btnPolaczenie = new wxButton(this, wxID_ANY, TR("&Ustawienia połączenia..."));

    // === ŚCIEŻKI ===
    tcSciezkaObrazki = new wxTextCtrl(this, wxID_ANY);
    btnBrowseObrazki = new wxButton(this, wxID_ANY, TR("..."), wxDefaultPosition, wxSize(40, -1));
    
    tcSciezkaPDF = new wxTextCtrl(this, wxID_ANY);
    btnBrowsePDF = new wxButton(this, wxID_ANY, TR("..."), wxDefaultPosition, wxSize(40, -1));
    
    // === KOD EAN ===
    choiceTypEAN = new wxChoice(this, wxID_ANY);
    choiceTypEAN->Append(TR("EAN13"));
    choiceTypEAN->Append(TR("EAN8"));
    choiceTypEAN->Append(TR("Code128"));
    choiceTypEAN->SetSelection(0);
    
    scPrefixEAN = new wxSpinCtrl(this, wxID_ANY, "20", wxDefaultPosition, wxSize(100, -1),
                                 wxSP_ARROW_KEYS, 0, 99, 20);
    
    // === CZCIONKA ===
    choiceFonty = new wxChoice(this, wxID_ANY);
    choiceFonty->Append(TR("Arial"));
    choiceFonty->Append(TR("Times New Roman"));
    choiceFonty->Append(TR("Courier New"));
    choiceFonty->Append(TR("Verdana"));
    choiceFonty->SetSelection(0);
    
    scRozmiarFontu = new wxSpinCtrl(this, wxID_ANY, "10", wxDefaultPosition, wxSize(100, -1),
                                    wxSP_ARROW_KEYS, 8, 72, 10);
    
    cbPogrubienie = new wxCheckBox(this, wxID_ANY, TR("Pogrubienie"));
    cbKursywa = new wxCheckBox(this, wxID_ANY, TR("Kursywa"));
    
    stPrzeglad = new wxStaticText(this, wxID_ANY, TR("Przykład tekstu"));
    stPrzeglad->SetMinSize(wxSize(200, 40));
    
    // === PRZYCISKI ===
    btnOK = new wxButton(this, wxID_OK, TR("&OK"));
    btnAnuluj = new wxButton(this, wxID_CANCEL, TR("&Anuluj"));
    
    // === EVENT HANDLERY ===
    Bind(wxEVT_BUTTON, &DialogUstawien::OnUstawieniaPolaczenia, this, btnPolaczenie->GetId());
    Bind(wxEVT_BUTTON, &DialogUstawien::OnBrowseObrazki, this, btnBrowseObrazki->GetId());
    Bind(wxEVT_BUTTON, &DialogUstawien::OnBrowsePDF, this, btnBrowsePDF->GetId());
    Bind(wxEVT_CHOICE, &DialogUstawien::OnFontChanged, this, choiceFonty->GetId());
    Bind(wxEVT_SPINCTRL, &DialogUstawien::OnFontChanged, this, scRozmiarFontu->GetId());
    Bind(wxEVT_CHECKBOX, &DialogUstawien::OnFontChanged, this, cbPogrubienie->GetId());
    Bind(wxEVT_CHECKBOX, &DialogUstawien::OnFontChanged, this, cbKursywa->GetId());
    Bind(wxEVT_BUTTON, &DialogUstawien::OnOK, this, wxID_OK);
    Bind(wxEVT_BUTTON, &DialogUstawien::OnAnuluj, this, wxID_CANCEL);
}

void DialogUstawien::UlozKontrole() {
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // === NOTEBOOK (KARTY) ===
    wxNotebook* notebook = new wxNotebook(this, wxID_ANY);

    // --- TAB 1: BAZA DANYCH ---
    wxPanel* panelBaza = new wxPanel(notebook);
    wxBoxSizer* sizer1 = new wxBoxSizer(wxVERTICAL);

    wxStaticBoxSizer* sboxBaza = new wxStaticBoxSizer(wxVERTICAL, panelBaza, TR("Baza danych"));
    stOpisBaza->Reparent(panelBaza);
    btnPolaczenie->Reparent(panelBaza);
    sboxBaza->Add(stOpisBaza, 0, wxEXPAND | wxALL, 8);
    sboxBaza->Add(btnPolaczenie, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);
    sizer1->Add(sboxBaza, 0, wxEXPAND | wxALL, 5);
    sizer1->AddStretchSpacer();

    panelBaza->SetSizer(sizer1);
    notebook->AddPage(panelBaza, TR("Baza danych"));

    // --- TAB 2: ŚCIEŻKI ---
    wxPanel* panelSciezki = new wxPanel(notebook);
    wxBoxSizer* sizer2 = new wxBoxSizer(wxVERTICAL);

    for (wxWindow* w : {(wxWindow*)tcSciezkaObrazki, (wxWindow*)btnBrowseObrazki,
                        (wxWindow*)tcSciezkaPDF, (wxWindow*)btnBrowsePDF}) w->Reparent(panelSciezki);
    wxStaticBoxSizer* sboxSciezki = new wxStaticBoxSizer(wxVERTICAL, panelSciezki, TR("Ścieżki"));
    wxFlexGridSizer* gridSciezki = new wxFlexGridSizer(2, 3, 5, 5);
    gridSciezki->AddGrowableCol(1);
    
    gridSciezki->Add(new wxStaticText(panelSciezki, wxID_ANY, TR("Folder obrazków:")), 0, wxALIGN_CENTER_VERTICAL);
    gridSciezki->Add(tcSciezkaObrazki, 1, wxEXPAND);
    gridSciezki->Add(btnBrowseObrazki, 0);
    
    gridSciezki->Add(new wxStaticText(panelSciezki, wxID_ANY, TR("Folder PDF:")), 0, wxALIGN_CENTER_VERTICAL);
    gridSciezki->Add(tcSciezkaPDF, 1, wxEXPAND);
    gridSciezki->Add(btnBrowsePDF, 0);
    
    sboxSciezki->Add(gridSciezki, 0, wxEXPAND | wxALL, 5);
    sizer2->Add(sboxSciezki, 0, wxEXPAND | wxALL, 5);
    sizer2->AddStretchSpacer();

    panelSciezki->SetSizer(sizer2);
    notebook->AddPage(panelSciezki, TR("Ścieżki"));

    // --- TAB 3: INNE ---
    wxPanel* panelInne = new wxPanel(notebook);
    wxBoxSizer* sizer3 = new wxBoxSizer(wxVERTICAL);

    for (wxWindow* w : {(wxWindow*)choiceTypEAN, (wxWindow*)scPrefixEAN, (wxWindow*)choiceFonty,
                        (wxWindow*)scRozmiarFontu, (wxWindow*)cbPogrubienie, (wxWindow*)cbKursywa,
                        (wxWindow*)stPrzeglad}) w->Reparent(panelInne);

    // Kod EAN
    wxStaticBoxSizer* sboxEAN = new wxStaticBoxSizer(wxVERTICAL, panelInne, TR("Kod EAN"));
    wxFlexGridSizer* gridEAN = new wxFlexGridSizer(2, 2, 5, 10);
    gridEAN->Add(new wxStaticText(panelInne, wxID_ANY, TR("Typ kodu:")), 0, wxALIGN_CENTER_VERTICAL);
    gridEAN->Add(choiceTypEAN, 0);
    gridEAN->Add(new wxStaticText(panelInne, wxID_ANY, TR("Prefix:")), 0, wxALIGN_CENTER_VERTICAL);
    gridEAN->Add(scPrefixEAN, 0);
    sboxEAN->Add(gridEAN, 0, wxEXPAND | wxALL, 5);
    sizer3->Add(sboxEAN, 0, wxEXPAND | wxALL, 5);

    // Czcionka
    wxStaticBoxSizer* sboxCzcionka = new wxStaticBoxSizer(wxVERTICAL, panelInne, TR("Czcionka"));
    wxFlexGridSizer* gridCzcionka = new wxFlexGridSizer(4, 2, 5, 10);
    
    gridCzcionka->Add(new wxStaticText(panelInne, wxID_ANY, TR("Nazwa:")), 0, wxALIGN_CENTER_VERTICAL);
    gridCzcionka->Add(choiceFonty, 0);
    
    gridCzcionka->Add(new wxStaticText(panelInne, wxID_ANY, TR("Rozmiar:")), 0, wxALIGN_CENTER_VERTICAL);
    gridCzcionka->Add(scRozmiarFontu, 0);
    
    gridCzcionka->Add(new wxStaticText(panelInne, wxID_ANY, TR("Styl:")), 0, wxALIGN_CENTER_VERTICAL);
    wxBoxSizer* hSizerStyl = new wxBoxSizer(wxHORIZONTAL);
    hSizerStyl->Add(cbPogrubienie, 0, wxRIGHT, 10);
    hSizerStyl->Add(cbKursywa, 0);
    gridCzcionka->Add(hSizerStyl, 0);
    
    gridCzcionka->Add(new wxStaticText(panelInne, wxID_ANY, TR("Podgląd:")), 0, wxALIGN_TOP);
    gridCzcionka->Add(stPrzeglad, 0, wxEXPAND);
    
    sboxCzcionka->Add(gridCzcionka, 0, wxEXPAND | wxALL, 5);
    sizer3->Add(sboxCzcionka, 0, wxEXPAND | wxALL, 5);
    sizer3->AddStretchSpacer();

    panelInne->SetSizer(sizer3);
    notebook->AddPage(panelInne, TR("Inne"));
    PomocGUI::Dodaj(tcSciezkaObrazki, "ustawienia.obrazki");
    PomocGUI::Dodaj(tcSciezkaPDF, "ustawienia.pdf");
    PomocGUI::Dodaj(choiceTypEAN, "ustawienia.typ_kodu");
    PomocGUI::Dodaj(scPrefixEAN, "ustawienia.prefiks");
    PomocGUI::Dodaj(choiceFonty, "ustawienia.czcionka");

    mainSizer->Add(notebook, 1, wxEXPAND | wxALL, 5);

    // === PRZYCISKI ===
    wxBoxSizer* hSizerBtn = new wxBoxSizer(wxHORIZONTAL);
    hSizerBtn->AddStretchSpacer();
    hSizerBtn->Add(btnOK, 0, wxRIGHT, 5);
    hSizerBtn->Add(btnAnuluj, 0);
    mainSizer->Add(hSizerBtn, 0, wxALL | wxEXPAND, 5);

    SetSizer(mainSizer);
}

void DialogUstawien::OdswiezOpisBazy() {
    const UstawieniaBazy& us = ManagerBazyDanych::pobierz().pobierzUstawienia();
    stOpisBaza->SetLabel(
        TR("Serwer: ") + TekstZUTF8(us.host) + wxString::Format(":%d", us.port) + "\n" +
        TR("Baza: ") + TekstZUTF8(us.bazaDanych) + "\n" +
        TR("Użytkownik: ") + TekstZUTF8(us.uzytkownik));
    stOpisBaza->GetParent()->Layout();
}

void DialogUstawien::OnUstawieniaPolaczenia(wxCommandEvent&) {
    DialogPolaczenia dlg(this);
    if (dlg.ShowModal() == wxID_OK) {
        zmienionoPolaczenie = true;
        OdswiezOpisBazy();
    }
}

void DialogUstawien::WczytajUstawienia() {
    const UstawieniaBazowe us = Konfiguracja::pobierz().pobierzUstawieniaAplikacji();

    OdswiezOpisBazy();

    tcSciezkaObrazki->SetValue(TekstZUTF8(us.sciezkaObrazki));
    tcSciezkaPDF->SetValue(TekstZUTF8(us.sciezkaPDFDatasheet));

    int typ = choiceTypEAN->FindString(TekstZUTF8(us.typKoduEAN));
    choiceTypEAN->SetSelection(typ != wxNOT_FOUND ? typ : 0);
    scPrefixEAN->SetValue(us.prefixEAN);

    // Czcionka z pliku może nie być na liście - dodaj ją
    const wxString nazwaFontu = TekstZUTF8(us.nazwaFontu);
    int font = choiceFonty->FindString(nazwaFontu);
    if (font == wxNOT_FOUND) font = choiceFonty->Append(nazwaFontu);
    choiceFonty->SetSelection(font);
    scRozmiarFontu->SetValue(us.rozmiarFontu);
    cbPogrubienie->SetValue(us.pogrubienie);
    cbKursywa->SetValue(us.kursywa);

    AktualizujPrzeglad();
}

void DialogUstawien::OnBrowseObrazki(wxCommandEvent& event) {
    wxDirDialog dlg(this, TR("Wybierz folder obrazków"), tcSciezkaObrazki->GetValue());
    if (dlg.ShowModal() == wxID_OK) {
        tcSciezkaObrazki->SetValue(dlg.GetPath());
    }
}

void DialogUstawien::OnBrowsePDF(wxCommandEvent& event) {
    wxDirDialog dlg(this, TR("Wybierz folder PDF"), tcSciezkaPDF->GetValue());
    if (dlg.ShowModal() == wxID_OK) {
        tcSciezkaPDF->SetValue(dlg.GetPath());
    }
}

void DialogUstawien::OnFontChanged(wxCommandEvent& event) {
    AktualizujPrzeglad();
}

void DialogUstawien::AktualizujPrzeglad() {
    wxString nazwaFontu = choiceFonty->GetStringSelection();
    int rozmiar = scRozmiarFontu->GetValue();
    wxFontStyle styl = cbKursywa->GetValue() ? wxFONTSTYLE_ITALIC : wxFONTSTYLE_NORMAL;
    wxFontWeight waga = cbPogrubienie->GetValue() ? wxFONTWEIGHT_BOLD : wxFONTWEIGHT_NORMAL;

    wxFont font(wxFontInfo(rozmiar).Family(wxFONTFAMILY_DEFAULT).Style(styl).Weight(waga).FaceName(nazwaFontu));
    stPrzeglad->SetFont(font);
}

void DialogUstawien::OnOK(wxCommandEvent&) {
    Konfiguracja& kfg = Konfiguracja::pobierz();

    // Zachowaj to, czego to okno nie edytuje (np. progi kolorów)
    UstawieniaBazowe us = kfg.pobierzUstawieniaAplikacji();
    us.sciezkaObrazki = tcSciezkaObrazki->GetValue().utf8_string();
    us.sciezkaPDFDatasheet = tcSciezkaPDF->GetValue().utf8_string();
    us.typKoduEAN = choiceTypEAN->GetStringSelection().utf8_string();
    us.prefixEAN = scPrefixEAN->GetValue();
    us.nazwaFontu = choiceFonty->GetStringSelection().utf8_string();
    us.rozmiarFontu = scRozmiarFontu->GetValue();
    us.pogrubienie = cbPogrubienie->GetValue();
    us.kursywa = cbKursywa->GetValue();

    kfg.ustawUstawieniaAplikacji(us);
    if (!kfg.zapisz()) {
        wxMessageBox(TR("Nie udało się zapisać ustawień:\n\n") + TekstZUTF8(kfg.ostatniBlad()),
                     TR("Błąd zapisu"), wxOK | wxICON_ERROR, this);
        return; // zostaw okno otwarte
    }

    EndModal(wxID_OK);
}

void DialogUstawien::OnAnuluj(wxCommandEvent& event) {
    EndModal(wxID_CANCEL);
}
