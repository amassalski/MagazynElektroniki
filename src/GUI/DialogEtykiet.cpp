#include "DialogEtykiet.h"
#include "../Narzedzia/Konfiguracja.h"
#include "../Narzedzia/Tlumaczenia.h"

#include <wx/filedlg.h>
#include <algorithm>
#include <wx/filename.h>
#include <wx/image.h>
#include <wx/sizer.h>

DialogEtykiet::DialogEtykiet(wxWindow* parent, const std::vector<Element>& elementy, long long idWybranego)
    : wxDialog(parent, wxID_ANY, TR("Drukowanie etykiet"), wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      elementy_(elementy), idWybranego_(idWybranego) {
    wxBoxSizer* glowny = new wxBoxSizer(wxHORIZONTAL);

    wxBoxSizer* lewa = new wxBoxSizer(wxVERTICAL);
    wxStaticBoxSizer* skad = new wxStaticBoxSizer(wxVERTICAL, this, TR("Które elementy"));
    rbZaznaczony = new wxRadioButton(this, wxID_ANY, TR("Tylko zaznaczony element"), wxDefaultPosition, wxDefaultSize,
                                     wxRB_GROUP);
    rbWszystkie = new wxRadioButton(this, wxID_ANY,
                                    wxString::Format(TR("Wszystkie elementy z listy (%zu)"), elementy_.size()));
    skad->Add(rbZaznaczony, 0, wxALL, 4);
    skad->Add(rbWszystkie, 0, wxALL, 4);
    lewa->Add(skad, 0, wxEXPAND | wxBOTTOM, 8);

    wxFlexGridSizer* siatka = new wxFlexGridSizer(2, 6, 8);
    auto dodaj = [&](const wxString& t, wxWindow* k) {
        siatka->Add(new wxStaticText(this, wxID_ANY, t), 0, wxALIGN_CENTER_VERTICAL);
        siatka->Add(k, 0, wxEXPAND);
    };
    scKopie = new wxSpinCtrl(this, wxID_ANY, "1", wxDefaultPosition, wxSize(90, -1), wxSP_ARROW_KEYS, 1, 500, 1);
    chFormat = new wxChoice(this, wxID_ANY);
    for (const FormatEtykiet& f : DrukujEtykiety::formatyWbudowane()) chFormat->Append(TekstZUTF8(f.nazwa));
    chFormat->SetSelection(0);
    scStart = new wxSpinCtrl(this, wxID_ANY, "1", wxDefaultPosition, wxSize(90, -1), wxSP_ARROW_KEYS, 1, 100, 1);
    scX = new wxSpinCtrlDouble(this, wxID_ANY, "0", wxDefaultPosition, wxSize(90, -1), wxSP_ARROW_KEYS, -20, 20, 0, 0.5);
    scY = new wxSpinCtrlDouble(this, wxID_ANY, "0", wxDefaultPosition, wxSize(90, -1), wxSP_ARROW_KEYS, -20, 20, 0, 0.5);
    cbRamki = new wxCheckBox(this, wxID_ANY, TR("Ramki etykiet (wydruk próbny)"));
    dodaj(TR("Kopii każdej etykiety"), scKopie);
    dodaj(TR("Format arkusza"), chFormat);
    dodaj(TR("Zacznij od etykiety nr"), scStart);
    dodaj(TR("Przesunięcie poziome [mm]"), scX);
    dodaj(TR("Przesunięcie pionowe [mm]"), scY);
    lewa->Add(siatka, 0, wxEXPAND | wxBOTTOM, 8);
    lewa->Add(cbRamki, 0, wxBOTTOM, 8);
    lbInfo = new wxStaticText(this, wxID_ANY, wxEmptyString);
    lewa->Add(lbInfo, 0, wxBOTTOM, 10);

    wxBoxSizer* przyciski = new wxBoxSizer(wxVERTICAL);
    wxButton* bPodglad = new wxButton(this, wxID_ANY, TR("Podgląd wydruku..."));
    wxButton* bDrukuj = new wxButton(this, wxID_ANY, TR("Drukuj..."));
    wxButton* bPng = new wxButton(this, wxID_ANY, TR("Zapisz jako PNG..."));
    przyciski->Add(bPodglad, 0, wxEXPAND | wxBOTTOM, 4);
    przyciski->Add(bDrukuj, 0, wxEXPAND | wxBOTTOM, 4);
    przyciski->Add(bPng, 0, wxEXPAND | wxBOTTOM, 10);
    przyciski->Add(new wxButton(this, wxID_CANCEL, TR("Zamknij")), 0, wxEXPAND);
    lewa->Add(przyciski, 0, wxEXPAND);
    glowny->Add(lewa, 0, wxEXPAND | wxALL, 12);

    podglad = new wxStaticBitmap(this, wxID_ANY, wxBitmap(300, 420));
    glowny->Add(podglad, 1, wxEXPAND | wxTOP | wxRIGHT | wxBOTTOM, 12);
    SetSizerAndFit(glowny);
    SetEscapeId(wxID_CANCEL);

    if (idWybranego_ == 0 || std::none_of(elementy_.begin(), elementy_.end(),
                                          [&](const Element& e) { return e.id == idWybranego_; })) {
        rbZaznaczony->Enable(false);
        rbWszystkie->SetValue(true);
    }

    auto odswiez = [this](wxCommandEvent&) { Odswiez(); };
    for (wxControl* c : std::initializer_list<wxControl*>{rbZaznaczony, rbWszystkie, chFormat, cbRamki})
        c->Bind(c == chFormat ? wxEVT_CHOICE : (c == cbRamki ? wxEVT_CHECKBOX : wxEVT_RADIOBUTTON), odswiez);
    for (wxSpinCtrl* s : {scKopie, scStart}) s->Bind(wxEVT_SPINCTRL, [this](wxSpinEvent&) { Odswiez(); });
    for (wxSpinCtrlDouble* s : {scX, scY}) s->Bind(wxEVT_SPINCTRLDOUBLE, [this](wxSpinDoubleEvent&) { Odswiez(); });
    bPodglad->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Podglad(); });
    bDrukuj->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Drukuj(); });
    bPng->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ZapiszPNGOknem(); });
    Odswiez();
}

void DialogEtykiet::WybierzTylkoZaznaczony(bool tak) {
    if (tak && rbZaznaczony->IsEnabled()) rbZaznaczony->SetValue(true);
    else rbWszystkie->SetValue(true);
    Odswiez();
}

std::vector<EtykietaDoDruku> DialogEtykiet::Etykiety() const {
    const UstawieniaBazowe us = Konfiguracja::pobierz().pobierzUstawieniaAplikacji();
    const KodKreskowy::Typ typ = KodKreskowy::typZNazwy(us.typKoduEAN);
    std::vector<EtykietaDoDruku> wynik;
    for (const Element& e : elementy_) {
        if (rbZaznaczony->GetValue() && e.id != idWybranego_) continue;
        EtykietaDoDruku et;
        et.element = e;
        et.kod = KodKreskowy::tekstKoduElementu(typ == KodKreskowy::Typ::EAN8 ? typ : KodKreskowy::Typ::EAN13, e.id,
                                                us.prefixEAN);
        for (int i = 0; i < scKopie->GetValue(); ++i) wynik.push_back(et);
    }
    return wynik;
}

UstawieniaDruku DialogEtykiet::Ustawienia() const {
    UstawieniaDruku u;
    const int f = chFormat->GetSelection();
    const auto& formaty = DrukujEtykiety::formatyWbudowane();
    u.format = formaty[static_cast<size_t>(std::max(0, std::min(f, static_cast<int>(formaty.size()) - 1)))];
    u.typKodu = KodKreskowy::typZNazwy(Konfiguracja::pobierz().pobierzUstawieniaAplikacji().typKoduEAN);
    u.pominiete = std::max(0, scStart->GetValue() - 1);
    u.przesuniecieX = scX->GetValue();
    u.przesuniecieY = scY->GetValue();
    u.ramki = cbRamki->GetValue();
    return u;
}

void DialogEtykiet::Odswiez() {
    const UstawieniaDruku u = Ustawienia();
    scStart->SetRange(1, std::max(1, u.format.naStrone()));
    const auto et = Etykiety();
    const int stron = DrukujEtykiety::liczbaStron(u.format, u.pominiete, static_cast<int>(et.size()));
    lbInfo->SetLabel(wxString::Format(TR("Etykiet: %zu, arkuszy: %d"), et.size(), stron));
    wxBitmap bmp = RenderujStrone(et, u, 0, 60);
    // dopasuj do pola podglądu (maks. 300 x 420)
    wxImage img = bmp.ConvertToImage();
    const double s = std::min(300.0 / img.GetWidth(), 420.0 / img.GetHeight());
    if (s < 1.0 || s > 1.0) img = img.Scale(std::max(1, static_cast<int>(img.GetWidth() * s)), std::max(1, static_cast<int>(img.GetHeight() * s)), wxIMAGE_QUALITY_BILINEAR);
    podglad->SetBitmap(wxBitmap(img));
    podglad->SetMinSize(wxSize(300, 420));
    podglad->Refresh();
    Layout();
}

int DialogEtykiet::ZapiszPNG(const wxString& sciezkaBazowa, double dpi) const {
    const UstawieniaDruku u = Ustawienia();
    const auto et = Etykiety();
    const int stron = DrukujEtykiety::liczbaStron(u.format, u.pominiete, static_cast<int>(et.size()));
    wxFileName nazwa(sciezkaBazowa);
    int zapisane = 0;
    for (int s = 0; s < stron; ++s) {
        wxFileName f = nazwa;
        if (stron > 1) f.SetName(nazwa.GetName() + wxString::Format("_%d", s + 1));
        f.SetExt("png");
        const wxImage img = RenderujStrone(et, u, s, dpi).ConvertToImage();
        if (img.SaveFile(f.GetFullPath(), wxBITMAP_TYPE_PNG)) ++zapisane;
    }
    return zapisane;
}

void DialogEtykiet::ZapiszPNGOknem() {
    if (Etykiety().empty()) return;
    wxFileDialog dlg(this, TR("Zapisz etykiety jako PNG"), wxEmptyString, "etykiety.png", TR("Obrazy PNG (*.png)|*.png"),
                     wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() != wxID_OK) return;
    const int n = ZapiszPNG(dlg.GetPath());
    wxMessageBox(wxString::Format(TR("Zapisano obrazów: %d"), n), TR("Etykiety"), wxOK | wxICON_INFORMATION, this);
}

void DialogEtykiet::Podglad() {
    const auto et = Etykiety();
    if (et.empty()) return;
    wxPrintDialogData dane;
    wxPrintPreview* pp = new wxPrintPreview(new WydrukEtykiet(et, Ustawienia()), new WydrukEtykiet(et, Ustawienia()), &dane);
    if (!pp->IsOk()) {
        delete pp;
        wxMessageBox(TR("Nie można utworzyć podglądu wydruku (brak skonfigurowanej drukarki?)."), TR("Etykiety"),
                     wxOK | wxICON_WARNING, this);
        return;
    }
    wxPreviewFrame* ramka = new wxPreviewFrame(pp, this, TR("Podgląd etykiet"), wxDefaultPosition, wxSize(800, 900));
    ramka->Centre(wxBOTH);
    ramka->InitializeWithModality(wxPreviewFrame_AppModal);
    ramka->Show();
}

void DialogEtykiet::Drukuj() {
    const auto et = Etykiety();
    if (et.empty()) return;
    wxPrintDialogData dane;
    wxPrinter drukarka(&dane);
    WydrukEtykiet wydruk(et, Ustawienia());
    if (!drukarka.Print(this, &wydruk, true) && wxPrinter::GetLastError() == wxPRINTER_ERROR) {
        wxMessageBox(TR("Drukowanie nie powiodło się. Sprawdź ustawienia drukarki."), TR("Etykiety"),
                     wxOK | wxICON_ERROR, this);
    }
}
