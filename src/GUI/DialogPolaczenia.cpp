#include "DialogPolaczenia.h"
#include "../BazaDanych/ManagerBazyDanych.h"
#include "../Narzedzia/Tlumaczenia.h"
#include "../Pomoc/PomocGUI.h"

#include <wx/sizer.h>
#include <wx/statline.h>

DialogPolaczenia::DialogPolaczenia(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, TR("Ustawienia połączenia z bazą danych"),
               wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE) {
    UtworzKontrole();
    WczytajUstawienia();
    tcHost->SetFocus();
}

void DialogPolaczenia::UtworzKontrole() {
    wxBoxSizer* glowny = new wxBoxSizer(wxVERTICAL);

    wxFlexGridSizer* siatka = new wxFlexGridSizer(2, 10, 10);
    siatka->AddGrowableCol(1);

    auto dodaj = [&](const wxString& etykieta, wxWindow* kontrolka, int flagi = wxEXPAND) {
        siatka->Add(new wxStaticText(this, wxID_ANY, etykieta), 0, wxALIGN_CENTER_VERTICAL);
        siatka->Add(kontrolka, 1, flagi);
    };

    tcHost = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(280, -1));
    scPort = new wxSpinCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(100, -1),
                            wxSP_ARROW_KEYS, 1, 65535, 5432);
    tcBaza = new wxTextCtrl(this, wxID_ANY);
    tcUzytkownik = new wxTextCtrl(this, wxID_ANY);
    tcHaslo = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD);

    dodaj(TR("Host:"), tcHost);
    dodaj(TR("Port:"), scPort, 0);
    dodaj(TR("Baza danych:"), tcBaza);
    dodaj(TR("Użytkownik:"), tcUzytkownik);
    dodaj(TR("Hasło:"), tcHaslo);
    PomocGUI::Dodaj(tcHost, "polaczenie.host");
    PomocGUI::Dodaj(scPort, "polaczenie.port");
    PomocGUI::Dodaj(tcBaza, "polaczenie.baza");
    PomocGUI::Dodaj(tcUzytkownik, "polaczenie.uzytkownik");
    PomocGUI::Dodaj(tcHaslo, "polaczenie.haslo");

    glowny->Add(siatka, 0, wxEXPAND | wxALL, 12);

    cbZapamietajHaslo = new wxCheckBox(this, wxID_ANY,
        TR("Zapamiętaj hasło w pliku konfiguracji (zapis jako zwykły tekst)"));
    glowny->Add(cbZapamietajHaslo, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);

    wxStaticText* stPlik = new wxStaticText(this, wxID_ANY,
        TR("Plik konfiguracji: ") + TekstZUTF8(ManagerBazyDanych::pobierz().sciezkaKonfiguracji()));
    stPlik->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
    stPlik->Wrap(420);
    glowny->Add(stPlik, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);

    stStatus = new wxStaticText(this, wxID_ANY, wxEmptyString);
    stStatus->SetMinSize(wxSize(420, 40));
    glowny->Add(stStatus, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    glowny->Add(new wxStaticLine(this), 0, wxEXPAND);

    wxBoxSizer* przyciski = new wxBoxSizer(wxHORIZONTAL);
    btnTest = new wxButton(this, wxID_ANY, TR("&Test połączenia"));
    btnZapisz = new wxButton(this, wxID_ANY, TR("&Zapisz ustawienia"));
    przyciski->Add(btnTest, 0, wxRIGHT, 8);
    przyciski->Add(btnZapisz, 0);
    przyciski->AddStretchSpacer();
    wxButton* btnOK = new wxButton(this, wxID_OK, TR("&OK"));
    wxButton* btnAnuluj = new wxButton(this, wxID_CANCEL, TR("&Anuluj"));
    przyciski->Add(btnOK, 0, wxRIGHT, 8);
    przyciski->Add(btnAnuluj, 0);
    glowny->Add(przyciski, 0, wxEXPAND | wxALL, 12);

    btnOK->SetDefault();
    SetSizerAndFit(glowny);
    Center();

    btnTest->Bind(wxEVT_BUTTON, &DialogPolaczenia::OnTestPolaczenie, this);
    btnZapisz->Bind(wxEVT_BUTTON, &DialogPolaczenia::OnZapisz, this);
    Bind(wxEVT_BUTTON, &DialogPolaczenia::OnOK, this, wxID_OK);
}

void DialogPolaczenia::WczytajUstawienia() {
    const UstawieniaBazy& us = ManagerBazyDanych::pobierz().pobierzUstawienia();
    tcHost->SetValue(TekstZUTF8(us.host));
    scPort->SetValue(us.port);
    tcBaza->SetValue(TekstZUTF8(us.bazaDanych));
    tcUzytkownik->SetValue(TekstZUTF8(us.uzytkownik));
    tcHaslo->SetValue(TekstZUTF8(us.haslo));
    cbZapamietajHaslo->SetValue(us.zapamietajHaslo);
}

bool DialogPolaczenia::ZastosujUstawienia() {
    UstawieniaBazy us = ManagerBazyDanych::pobierz().pobierzUstawienia();
    us.host = tcHost->GetValue().Trim().Trim(false).utf8_string();
    us.port = scPort->GetValue();
    us.bazaDanych = tcBaza->GetValue().Trim().Trim(false).utf8_string();
    us.uzytkownik = tcUzytkownik->GetValue().Trim().Trim(false).utf8_string();
    us.haslo = tcHaslo->GetValue().utf8_string();
    us.zapamietajHaslo = cbZapamietajHaslo->GetValue();

    if (us.host.empty() || us.bazaDanych.empty() || us.uzytkownik.empty()) {
        PokazStatus(TR("Podaj host, nazwę bazy i użytkownika."), false);
        return false;
    }

    ManagerBazyDanych::pobierz().ustawUstawienia(us);
    return true;
}

void DialogPolaczenia::OnTestPolaczenie(wxCommandEvent&) {
    if (!ZastosujUstawienia()) return;

    PokazStatus(TR("Łączenie..."), true);
    stStatus->Update();

    std::string blad, wersja;
    bool ok;
    {
        wxBusyCursor zajety;
        ok = ManagerBazyDanych::testujUstawienia(ManagerBazyDanych::pobierz().pobierzUstawienia(),
                                                 blad, &wersja);
    }

    if (ok) {
        PokazStatus(TR("✓ Połączenie udane (PostgreSQL ") + TekstZUTF8(wersja) + ")", true);
    } else {
        PokazStatus(TR("✗ Błąd: ") + TekstZUTF8(blad), false);
    }
}

void DialogPolaczenia::OnZapisz(wxCommandEvent&) {
    if (!ZastosujUstawienia()) return;

    ManagerBazyDanych& manager = ManagerBazyDanych::pobierz();
    if (manager.zapiszUstawienia()) {
        PokazStatus(TR("✓ Ustawienia zapisane"), true);
    } else {
        PokazStatus(TR("✗ Nie udało się zapisać ustawień: ") + TekstZUTF8(manager.pobierzBlad()), false);
    }
}

void DialogPolaczenia::OnOK(wxCommandEvent&) {
    if (!ZastosujUstawienia()) return;

    ManagerBazyDanych& manager = ManagerBazyDanych::pobierz();
    if (!manager.zapiszUstawienia()) {
        wxMessageBox(TR("Nie udało się zapisać ustawień:\n\n") + TekstZUTF8(manager.pobierzBlad()),
                     TR("Błąd zapisu"), wxOK | wxICON_ERROR, this);
        return; // zostaw dialog otwarty
    }
    EndModal(wxID_OK);
}

void DialogPolaczenia::PokazStatus(const wxString& wiadomosc, bool sukces) {
    stStatus->SetLabel(wiadomosc);
    stStatus->SetForegroundColour(sukces ? wxColour(0, 110, 0) : *wxRED);
    stStatus->Wrap(420);
    Layout();
}
