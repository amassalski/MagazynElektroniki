#include "DialogDodajSklep.h"

#include "../Narzedzia/Liczby.h"
#include "../Narzedzia/Tlumaczenia.h"
#include "../Pomoc/PomocGUI.h"

#include <wx/statline.h>

DialogDodajSklep::DialogDodajSklep(wxWindow* parent, LogikaSklepow& logika, const Sklep& sklep)
    : wxDialog(parent, wxID_ANY, sklep.id ? TR("Zmień sklep") : TR("Nowy sklep"), wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      logika_(logika), sklep_(sklep) {
    wxBoxSizer* glowny = new wxBoxSizer(wxVERTICAL);
    wxFlexGridSizer* siatka = new wxFlexGridSizer(2, 6, 8);
    siatka->AddGrowableCol(1);
    auto dodaj = [&](const wxString& etykieta, wxWindow* pole) {
        siatka->Add(new wxStaticText(this, wxID_ANY, etykieta), 0, wxALIGN_CENTER_VERTICAL);
        siatka->Add(pole, 1, wxEXPAND);
    };
    auto tekst = [&](const std::string& v, int szer = 360) {
        return new wxTextCtrl(this, wxID_ANY, TekstZUTF8(v), wxDefaultPosition, wxSize(szer, -1));
    };
    tcNazwa = tekst(sklep.nazwa);
    tcAdres = tekst(sklep.urlBazowy);
    tcSzablon = tekst(sklep.szablonWyszukiwania);
    tcKraj = tekst(sklep.kraj);
    tcWaluta = tekst(sklep.waluta);
    tcDostawa = tekst(liczbaNaTekstPolski(sklep.kosztDostawy, 2, 2));
    tcDarmowa = tekst(sklep.darmowaDostawaOd ? liczbaNaTekstPolski(*sklep.darmowaDostawaOd, 2, 2) : "");
    tcWzorzec = tekst(sklep.regexCeny);
    tcOpis = tekst(sklep.opis);
    spPriorytet = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(80, -1), wxSP_ARROW_KEYS, 1, 99,
                                 sklep.priorytet);
    cbAktywny = new wxCheckBox(this, wxID_ANY, TR("Sklep aktywny (uwzględniany w zestawieniach)"));
    cbAktywny->SetValue(sklep.aktywny);

    dodaj(TR("Nazwa *"), tcNazwa);
    dodaj(TR("Adres strony"), tcAdres);
    dodaj(TR("Adres wyszukiwania (z {q})"), tcSzablon);
    dodaj(TR("Kraj"), tcKraj);
    dodaj(TR("Waluta (3 litery)"), tcWaluta);
    dodaj(TR("Koszt dostawy"), tcDostawa);
    dodaj(TR("Darmowa dostawa od"), tcDarmowa);
    dodaj(TR("Wzorzec ceny (zaawansowane)"), tcWzorzec);
    dodaj(TR("Opis"), tcOpis);
    dodaj(TR("Priorytet (1 = najchętniej)"), spPriorytet);
    PomocGUI::Dodaj(tcNazwa, "sklep.nazwa");
    PomocGUI::Dodaj(tcAdres, "sklep.adres");
    PomocGUI::Dodaj(tcSzablon, "sklep.szablon");
    PomocGUI::Dodaj(tcKraj, "sklep.kraj");
    PomocGUI::Dodaj(tcWaluta, "sklep.waluta");
    PomocGUI::Dodaj(tcDostawa, "sklep.dostawa");
    PomocGUI::Dodaj(tcDarmowa, "sklep.darmowa");
    PomocGUI::Dodaj(tcWzorzec, "sklep.wzorzec");
    PomocGUI::Dodaj(tcOpis, "sklep.opis");
    PomocGUI::Dodaj(spPriorytet, "sklep.priorytet");
    PomocGUI::Dodaj(cbAktywny, "sklep.aktywny");
    siatka->AddSpacer(0);
    siatka->Add(cbAktywny);
    glowny->Add(siatka, 1, wxEXPAND | wxALL, 12);

    wxStaticText* pomoc = new wxStaticText(
        this, wxID_ANY,
        TR("Adres wyszukiwania: adres strony wyników sklepu, w którym szukana fraza jest zastąpiona przez {q},\n"
           "np. https://sklep.pl/szukaj?q={q}. Wzorzec ceny jest potrzebny tylko wtedy, gdy sklep nie podaje ceny\n"
           "w danych strukturalnych: wyrażenie regularne z jedną grupą w nawiasie, np. cena\">([0-9 ,.]+)"));
    pomoc->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
    glowny->Add(pomoc, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);
    glowny->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 10);
    SetSizerAndFit(glowny);
    tcNazwa->SetFocus();

    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { if (Zapisz()) EndModal(wxID_OK); }, wxID_OK);
}

Sklep DialogDodajSklep::Pobierz() const {
    Sklep s = sklep_;
    s.nazwa = tcNazwa->GetValue().Strip(wxString::both).utf8_string();
    s.urlBazowy = tcAdres->GetValue().Strip(wxString::both).utf8_string();
    s.szablonWyszukiwania = tcSzablon->GetValue().Strip(wxString::both).utf8_string();
    s.kraj = tcKraj->GetValue().Strip(wxString::both).utf8_string();
    s.waluta = tcWaluta->GetValue().Strip(wxString::both).Upper().utf8_string();
    s.opis = tcOpis->GetValue().Strip(wxString::both).utf8_string();
    s.regexCeny = tcWzorzec->GetValue().Strip(wxString::both).utf8_string();
    s.priorytet = spPriorytet->GetValue();
    s.aktywny = cbAktywny->GetValue();
    double v = 0;
    s.kosztDostawy = tekstNaLiczbe(tcDostawa->GetValue().utf8_string(), v) ? v : 0.0;
    s.darmowaDostawaOd.reset();
    if (tekstNaLiczbe(tcDarmowa->GetValue().utf8_string(), v)) s.darmowaDostawaOd = v;
    return s;
}

bool DialogDodajSklep::Zapisz() {
    const wxString dostawa = tcDostawa->GetValue().Strip(wxString::both);
    double v = 0;
    if (!dostawa.empty() && !tekstNaLiczbe(dostawa.utf8_string(), v)) {
        wxMessageBox(TR("Koszt dostawy musi być liczbą."), TR("Sklep"), wxOK | wxICON_WARNING, this);
        return false;
    }
    const wxString darmowa = tcDarmowa->GetValue().Strip(wxString::both);
    if (!darmowa.empty() && !tekstNaLiczbe(darmowa.utf8_string(), v)) {
        wxMessageBox(TR("Próg darmowej dostawy musi być liczbą albo pusty."), TR("Sklep"), wxOK | wxICON_WARNING, this);
        return false;
    }
    auto id = logika_.zapiszSklep(Pobierz());
    if (!id) {
        wxMessageBox(TekstZUTF8(logika_.pobierzBlad()), TR("Sklep"), wxOK | wxICON_WARNING, this);
        return false;
    }
    idZapisanego_ = *id;
    return true;
}
