#include "DialogElementu.h"
#include "DialogKamery.h"
#include "DialogKatalogu.h"
#include "../BazaDanych/ManagerBazyDanych.h"
#include "../Logika/LogikaKatalogu.h"
#include "../Logika/LogikaMagazynu.h"
#include "../Narzedzia/Konfiguracja.h"
#include "../Narzedzia/Liczby.h"
#include "../Narzedzia/Tlumaczenia.h"
#include "../Pomoc/PomocGUI.h"

#include <wx/filedlg.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/sizer.h>
#include <wx/statline.h>

namespace {
const int MAKS_ILOSC = 100000000;

wxString cenaNaTekst(const std::optional<double>& cena) {
    if (!cena) return wxString();
    wxString t = TekstZUTF8(liczbaNaTekstKlasyczny(*cena));
    t.Replace(".", ","); // pole edycji w polskim formacie; tekstNaLiczbe przyjmuje oba
    return t;
}
} // namespace

// ===========================================================================
// DialogElementu
// ===========================================================================

DialogElementu::DialogElementu(wxWindow* parent, LogikaMagazynu& logika, const Element& element)
    : wxDialog(parent, wxID_ANY,
               element.id == 0 ? TR("Nowy element") : TR("Edycja elementu"),
               wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      element_(element) {
    UtworzKontrole(logika);
    WypelnijKontrole();
    tcSymbol->SetFocus();
    tcSymbol->SelectAll();
    Bind(wxEVT_BUTTON, &DialogElementu::OnOK, this, wxID_OK);
}

void DialogElementu::UtworzKontrole(LogikaMagazynu& logika) {
    wxBoxSizer* glowny = new wxBoxSizer(wxVERTICAL);

    wxFlexGridSizer* siatka = new wxFlexGridSizer(2, 8, 10);
    siatka->AddGrowableCol(1);

    auto dodaj = [&](const wxString& etykieta, wxWindow* kontrolka, int proporcja = 0,
                     int flagi = wxEXPAND) {
        siatka->Add(new wxStaticText(this, wxID_ANY, etykieta), 0, wxALIGN_CENTER_VERTICAL);
        siatka->Add(kontrolka, proporcja, flagi);
    };
    auto tekst = [&](long styl = 0) {
        return new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, styl);
    };

    tcSymbol = tekst();
    tcSymbol->SetMinSize(wxSize(380, -1));
    tcSymbolProducenta = tekst();
    tcProducent = tekst();
    chKategoria = new wxChoice(this, wxID_ANY);
    tcOpis = tekst(wxTE_MULTILINE);
    tcOpis->SetMinSize(wxSize(-1, 64));
    tcWartosc = tekst();
    tcJednostka = tekst();
    tcObudowa = tekst();
    scIlosc = new wxSpinCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                             wxSP_ARROW_KEYS, 0, MAKS_ILOSC, 0);
    scMinimum = new wxSpinCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                               wxSP_ARROW_KEYS, 0, MAKS_ILOSC, 0);
    tcCena = tekst();
    tcCena->SetMinSize(wxSize(120, -1));
    cbWaluta = new wxComboBox(this, wxID_ANY, "PLN", wxDefaultPosition, wxSize(90, -1));
    for (const char* w : {"PLN", "EUR", "USD", "GBP", "CNY", "CZK"}) cbWaluta->Append(w);
    tcDatasheet = tekst();
    tcZdjecie = tekst();
    tcTagi = tekst();
    PomocGUI::Dodaj(tcSymbol, "element.symbol");
    PomocGUI::Dodaj(chKategoria, "element.kategoria");
    PomocGUI::Dodaj(tcProducent, "element.producent");
    PomocGUI::Dodaj(tcSymbolProducenta, "element.symbol_producenta");
    PomocGUI::Dodaj(tcOpis, "element.opis");
    PomocGUI::Dodaj(tcWartosc, "element.wartosc");
    PomocGUI::Dodaj(tcJednostka, "element.jednostka");
    PomocGUI::Dodaj(tcObudowa, "element.obudowa");
    PomocGUI::Dodaj(scIlosc, "element.ilosc");
    PomocGUI::Dodaj(scMinimum, "element.minimum");
    PomocGUI::Dodaj(tcCena, "element.cena");
    PomocGUI::Dodaj(cbWaluta, "element.waluta");
    PomocGUI::Dodaj(tcDatasheet, "element.datasheet");
    PomocGUI::Dodaj(tcZdjecie, "element.zdjecie");
    PomocGUI::Dodaj(tcTagi, "element.tagi");

    {
        wxBoxSizer* wiersz = new wxBoxSizer(wxHORIZONTAL);
        wiersz->Add(tcSymbol, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
        wxButton* bKatalog = new wxButton(this, wxID_ANY, TR("Z katalogu..."));
        PomocGUI::Dodaj(bKatalog, "element.z_katalogu");
        bKatalog->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { WybierzZKatalogu(); });
        wiersz->Add(bKatalog, 0, wxALIGN_CENTER_VERTICAL);
        siatka->Add(new wxStaticText(this, wxID_ANY, TR("Symbol *")), 0, wxALIGN_CENTER_VERTICAL);
        siatka->Add(wiersz, 1, wxEXPAND);
    }
    dodaj(TR("Kategoria"), chKategoria, 1);
    dodaj(TR("Producent"), tcProducent, 1);
    dodaj(TR("Symbol producenta"), tcSymbolProducenta, 1);
    dodaj(TR("Opis"), tcOpis, 1);
    dodaj(TR("Wartość"), tcWartosc, 1);
    dodaj(TR("Jednostka"), tcJednostka, 0, 0);
    dodaj(TR("Obudowa"), tcObudowa, 1);

    tcTolerancja = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(90, -1));
    tcTolerancja->SetHint(TR("np. 1%"));
    tcParametry = new wxTextCtrl(this, wxID_ANY, wxEmptyString);
    tcParametry->SetHint(TR("np. 0,1 W; 50 V; X7R"));
    dodaj(TR("Tolerancja"), tcTolerancja, 0, 0);
    dodaj(TR("Parametry"), tcParametry, 1);
    PomocGUI::Dodaj(tcTolerancja, "element.tolerancja");
    PomocGUI::Dodaj(tcParametry, "element.parametry");

    // Miejsce magazynowania: kontener, szuflada i opcjonalna przegroda ("Kontener 1", "A1", "1")
    logika_ = &logika;
    cbKontener = new wxComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(230, -1));
    wxArrayString kontenery;
    for (const std::string& k : logika.pobierzKontenery()) kontenery.Add(TekstZUTF8(k));
    cbKontener->Append(kontenery);
    if (!kontenery.empty()) cbKontener->AutoComplete(kontenery);
    tcSzuflada = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(70, -1));
    tcSzuflada->SetHint(TR("np. A1"));
    tcPrzegroda = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(60, -1));
    tcPrzegroda->SetHint(TR("np. 1"));
    {
        wxBoxSizer* miejsce = new wxBoxSizer(wxHORIZONTAL);
        miejsce->Add(cbKontener, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
        miejsce->Add(new wxStaticText(this, wxID_ANY, TR("szuflada")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
        miejsce->Add(tcSzuflada, 0, wxALIGN_CENTER_VERTICAL);
        miejsce->Add(new wxStaticText(this, wxID_ANY, "/"), 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 4);
        miejsce->Add(tcPrzegroda, 0, wxALIGN_CENTER_VERTICAL);
        siatka->Add(new wxStaticText(this, wxID_ANY, TR("Miejsce")), 0, wxALIGN_CENTER_VERTICAL);
        siatka->Add(miejsce, 1, wxEXPAND);
    }
    cbKontener->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent&) { OdswiezPodpowiedziSzuflad(); });
    cbKontener->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& e) { OdswiezPodpowiedziSzuflad(); e.Skip(); });
    PomocGUI::Dodaj(cbKontener, "element.kontener");
    PomocGUI::Dodaj(tcSzuflada, "element.szuflada");
    PomocGUI::Dodaj(tcPrzegroda, "element.przegroda");

    const wxString etykietaIlosci = element_.id == 0 ? TR("Ilość początkowa") : TR("Ilość dostępna");
    dodaj(etykietaIlosci, scIlosc, 0, 0);
    dodaj(TR("Stan minimalny"), scMinimum, 0, 0);

    wxBoxSizer* cena = new wxBoxSizer(wxHORIZONTAL);
    cena->Add(tcCena, 0, wxRIGHT, 8);
    cena->Add(cbWaluta, 0);
    siatka->Add(new wxStaticText(this, wxID_ANY, TR("Cena jednostkowa")), 0, wxALIGN_CENTER_VERTICAL);
    siatka->Add(cena, 0);

    dodaj(TR("Datasheet (adres)"), tcDatasheet, 1);
    {
        wxBoxSizer* zdjecie = new wxBoxSizer(wxHORIZONTAL);
        zdjecie->Add(tcZdjecie, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
        wxButton* bZdjecie = new wxButton(this, wxID_ANY, TR("Wybierz..."));
        bZdjecie->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { WybierzZdjecie(); });
        zdjecie->Add(bZdjecie, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
        wxButton* bKamera = new wxButton(this, wxID_ANY, TR("Z kamery..."));
        PomocGUI::Dodaj(bKamera, "element.zdjecie_kamera");
        bKamera->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            DialogKamery dlg(this, tcSymbol ? tcSymbol->GetValue() : wxString());
            if (dlg.ShowModal() == wxID_OK && !dlg.Sciezka().empty()) tcZdjecie->SetValue(dlg.Sciezka());
        });
        zdjecie->Add(bKamera, 0, wxALIGN_CENTER_VERTICAL);
        siatka->Add(new wxStaticText(this, wxID_ANY, TR("Zdjęcie")), 0, wxALIGN_CENTER_VERTICAL);
        siatka->Add(zdjecie, 1, wxEXPAND);
    }
    dodaj(TR("Tagi"), tcTagi, 1);
    siatka->AddGrowableRow(4); // opis

    glowny->Add(siatka, 1, wxEXPAND | wxALL, 12);

    if (element_.id != 0) {
        wxStaticText* uwaga = new wxStaticText(this, wxID_ANY,
            TR("Zmiana ilości dostępnej zostanie zapisana w historii magazynu."));
        uwaga->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
        glowny->Add(uwaga, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);
    }

    glowny->Add(new wxStaticLine(this), 0, wxEXPAND);
    glowny->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 10);

    // Lista kategorii: pierwsza pozycja = brak kategorii
    chKategoria->Append(TR("(bez kategorii)"));
    idKategorii_.push_back(0);
    for (const Kategoria& k : logika.kategorie()) {
        chKategoria->Append(TekstZUTF8(k.pelnaNazwa));
        idKategorii_.push_back(k.id);
    }

    SetSizerAndFit(glowny);
    SetMinSize(GetSize());
}

void DialogElementu::WypelnijKontrole() {
    tcSymbol->SetValue(TekstZUTF8(element_.symbol));
    tcSymbolProducenta->SetValue(TekstZUTF8(element_.symbolProducenta));
    tcProducent->SetValue(TekstZUTF8(element_.producent));
    tcOpis->SetValue(TekstZUTF8(element_.opis));
    tcWartosc->SetValue(TekstZUTF8(element_.wartosc));
    tcJednostka->SetValue(TekstZUTF8(element_.jednostka));
    tcObudowa->SetValue(TekstZUTF8(element_.obudowa));
    scIlosc->SetValue(element_.iloscDostepna);
    scMinimum->SetValue(element_.iloscMinimalna);
    tcCena->SetValue(cenaNaTekst(element_.cena));
    cbWaluta->SetValue(TekstZUTF8(element_.waluta.empty() ? "PLN" : element_.waluta));
    tcDatasheet->SetValue(TekstZUTF8(element_.datasheetUrl));
    tcZdjecie->SetValue(TekstZUTF8(element_.zdjecieSciezka));
    tcTagi->SetValue(TekstZUTF8(element_.tagi));
    tcTolerancja->SetValue(TekstZUTF8(element_.tolerancja));
    tcParametry->SetValue(TekstZUTF8(element_.parametry));
    cbKontener->SetValue(TekstZUTF8(element_.kontener));
    tcSzuflada->SetValue(TekstZUTF8(element_.szuflada));
    tcPrzegroda->SetValue(TekstZUTF8(element_.przegroda));
    OdswiezPodpowiedziSzuflad();

    int wybor = 0;
    if (element_.idKategorii) {
        for (size_t i = 1; i < idKategorii_.size(); ++i) {
            if (idKategorii_[i] == *element_.idKategorii) wybor = static_cast<int>(i);
        }
    }
    chKategoria->SetSelection(wybor);
}

void DialogElementu::UzupelnijZKatalogu(const PozycjaKatalogu& poz) {
    const Element k = LogikaKatalogu::elementZKatalogu(poz);
    auto ustaw = [](wxTextCtrl* tc, const std::string& v) { if (!v.empty()) tc->SetValue(TekstZUTF8(v)); };
    ustaw(tcSymbol, k.symbol);
    ustaw(tcProducent, k.producent);
    ustaw(tcOpis, k.opis);
    ustaw(tcWartosc, k.wartosc);
    ustaw(tcObudowa, k.obudowa);
    ustaw(tcTolerancja, k.tolerancja);
    ustaw(tcParametry, k.parametry);
    ustaw(tcDatasheet, k.datasheetUrl);
    ustaw(tcTagi, k.tagi);
}

void DialogElementu::WybierzZKatalogu() {
    LogikaKatalogu katalog(ManagerBazyDanych::pobierz());
    DialogWyboruZKatalogu dlg(this, katalog, tcSymbol->GetValue().Strip(wxString::both));
    if (dlg.ShowModal() != wxID_OK || !dlg.Wybrana()) return;
    UzupelnijZKatalogu(*dlg.Wybrana());
}

void DialogElementu::OdswiezPodpowiedziSzuflad() {
    if (!logika_ || !tcSzuflada) return;
    wxArrayString szuflady;
    for (const std::string& s : logika_->pobierzSzuflady(cbKontener->GetValue().Strip(wxString::both).utf8_string()))
        szuflady.Add(TekstZUTF8(s));
    if (!szuflady.empty()) tcSzuflada->AutoComplete(szuflady);
}

Element DialogElementu::PobierzElement() const {
    Element e = element_;
    e.symbol = tcSymbol->GetValue().Strip(wxString::both).utf8_string();
    e.symbolProducenta = tcSymbolProducenta->GetValue().Strip(wxString::both).utf8_string();
    e.producent = tcProducent->GetValue().Strip(wxString::both).utf8_string();
    e.opis = tcOpis->GetValue().Strip(wxString::both).utf8_string();
    e.wartosc = tcWartosc->GetValue().Strip(wxString::both).utf8_string();
    e.jednostka = tcJednostka->GetValue().Strip(wxString::both).utf8_string();
    e.obudowa = tcObudowa->GetValue().Strip(wxString::both).utf8_string();
    e.iloscDostepna = scIlosc->GetValue();
    e.iloscMinimalna = scMinimum->GetValue();
    e.waluta = cbWaluta->GetValue().Strip(wxString::both).Upper().utf8_string();
    e.datasheetUrl = tcDatasheet->GetValue().Strip(wxString::both).utf8_string();
    e.zdjecieSciezka = tcZdjecie->GetValue().Strip(wxString::both).utf8_string();
    e.tagi = tcTagi->GetValue().Strip(wxString::both).utf8_string();

    e.tolerancja = tcTolerancja->GetValue().Strip(wxString::both).utf8_string();
    e.parametry = tcParametry->GetValue().Strip(wxString::both).utf8_string();

    // Miejsce: pola mogą też zawierać zapis swobodny, np. "Kontener 1 szufladka A1/1" wpisany w pole kontenera
    MiejsceMagazynowe miejsce;
    miejsce.kontener = cbKontener->GetValue().Strip(wxString::both).utf8_string();
    miejsce.szuflada = tcSzuflada->GetValue().Strip(wxString::both).utf8_string();
    miejsce.przegroda = tcPrzegroda->GetValue().Strip(wxString::both).utf8_string();
    if (miejsce.szuflada.empty() && miejsce.przegroda.empty() && !miejsce.kontener.empty())
        miejsce = MiejsceMagazynowe::parsuj(miejsce.kontener);
    e.ustawMiejsce(miejsce);

    double cena = 0;
    if (tekstNaLiczbe(tcCena->GetValue().utf8_string(), cena)) e.cena = cena;
    else e.cena = std::nullopt;

    const int wybor = chKategoria->GetSelection();
    if (wybor > 0 && static_cast<size_t>(wybor) < idKategorii_.size()) {
        e.idKategorii = idKategorii_[static_cast<size_t>(wybor)];
    } else {
        e.idKategorii = std::nullopt;
    }
    return e;
}

void DialogElementu::OnOK(wxCommandEvent& event) {
    if (tcSymbol->GetValue().Strip(wxString::both).empty()) {
        wxMessageBox(TR("Podaj symbol elementu."), TR("Brak danych"), wxOK | wxICON_WARNING, this);
        tcSymbol->SetFocus();
        return;
    }
    double cena = 0;
    const wxString tekstCeny = tcCena->GetValue().Strip(wxString::both);
    if (!tekstCeny.empty() && (!tekstNaLiczbe(tekstCeny.utf8_string(), cena) || cena < 0)) {
        wxMessageBox(TR("Cena musi być liczbą nieujemną, np. 0,35."), TR("Błędna cena"),
                     wxOK | wxICON_WARNING, this);
        tcCena->SetFocus();
        return;
    }
    if (cbWaluta->GetValue().Strip(wxString::both).length() > 3) {
        wxMessageBox(TR("Waluta to kod trzyliterowy, np. PLN."), TR("Błędna waluta"),
                     wxOK | wxICON_WARNING, this);
        cbWaluta->SetFocus();
        return;
    }
    event.Skip(); // zamknij okno z wynikiem wxID_OK
}

// ===========================================================================
// DialogZmianyIlosci
// ===========================================================================

DialogZmianyIlosci::DialogZmianyIlosci(wxWindow* parent, const wxString& symbol, int aktualnaIlosc)
    : wxDialog(parent, wxID_ANY, TR("Zmiana stanu magazynowego"), wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE),
      aktualna_(aktualnaIlosc) {
    wxBoxSizer* glowny = new wxBoxSizer(wxVERTICAL);

    glowny->Add(new wxStaticText(this, wxID_ANY,
                    symbol + TR("\nObecny stan: ") + wxString::Format("%d", aktualnaIlosc)),
                0, wxALL, 12);

    rbPrzyjecie = new wxRadioButton(this, wxID_ANY, TR("Przyjęcie (dodaj do stanu)"),
                                    wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    rbWydanie = new wxRadioButton(this, wxID_ANY, TR("Wydanie (odejmij od stanu)"));
    rbUstaw = new wxRadioButton(this, wxID_ANY, TR("Ustaw stan na (inwentaryzacja)"));
    rbPrzyjecie->SetValue(true);
    glowny->Add(rbPrzyjecie, 0, wxLEFT | wxRIGHT, 12);
    glowny->Add(rbWydanie, 0, wxLEFT | wxRIGHT | wxTOP, 4);
    glowny->Add(rbUstaw, 0, wxLEFT | wxRIGHT | wxTOP, 4);

    wxFlexGridSizer* siatka = new wxFlexGridSizer(2, 8, 10);
    siatka->AddGrowableCol(1);
    scIlosc = new wxSpinCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                             wxSP_ARROW_KEYS, 0, MAKS_ILOSC, 1);
    tcKomentarz = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(320, -1));
    siatka->Add(new wxStaticText(this, wxID_ANY, TR("Ilość")), 0, wxALIGN_CENTER_VERTICAL);
    siatka->Add(scIlosc, 0);
    siatka->Add(new wxStaticText(this, wxID_ANY, TR("Komentarz")), 0, wxALIGN_CENTER_VERTICAL);
    siatka->Add(tcKomentarz, 1, wxEXPAND);
    glowny->Add(siatka, 0, wxEXPAND | wxALL, 12);

    stPodsumowanie = new wxStaticText(this, wxID_ANY, wxEmptyString);
    stPodsumowanie->SetMinSize(wxSize(-1, 24));
    glowny->Add(stPodsumowanie, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);

    glowny->Add(new wxStaticLine(this), 0, wxEXPAND);
    glowny->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 10);
    SetSizerAndFit(glowny);

    for (wxRadioButton* rb : {rbPrzyjecie, rbWydanie, rbUstaw}) {
        rb->Bind(wxEVT_RADIOBUTTON, [this](wxCommandEvent&) { Aktualizuj(); });
    }
    scIlosc->Bind(wxEVT_SPINCTRL, [this](wxSpinEvent&) { Aktualizuj(); });
    scIlosc->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { Aktualizuj(); });
    Bind(wxEVT_BUTTON, &DialogZmianyIlosci::OnOK, this, wxID_OK);

    Aktualizuj();
    scIlosc->SetFocus();
}

int DialogZmianyIlosci::Zmiana() const {
    const int n = scIlosc->GetValue();
    if (rbWydanie->GetValue()) return -n;
    if (rbUstaw->GetValue()) return n - aktualna_;
    return n;
}

std::string DialogZmianyIlosci::Typ() const {
    if (rbWydanie->GetValue()) return "WYDANIE";
    if (rbUstaw->GetValue()) return "KOREKTA";
    return "PRZYJECIE";
}

std::string DialogZmianyIlosci::Komentarz() const {
    return tcKomentarz->GetValue().Strip(wxString::both).utf8_string();
}

void DialogZmianyIlosci::Aktualizuj() {
    const long long po = static_cast<long long>(aktualna_) + Zmiana();
    if (Zmiana() == 0) {
        stPodsumowanie->SetLabel(TR("Stan się nie zmieni."));
    } else if (po < 0) {
        stPodsumowanie->SetLabel(TR("Za mało sztuk: stan nie może być ujemny."));
    } else {
        stPodsumowanie->SetLabel(TR("Stan po zmianie: ") + wxString::Format("%lld", po));
    }
}

void DialogZmianyIlosci::OnOK(wxCommandEvent& event) {
    if (Zmiana() == 0) {
        wxMessageBox(TR("Stan się nie zmieni. Wpisz inną ilość."), TR("Zmiana stanu"),
                     wxOK | wxICON_INFORMATION, this);
        return;
    }
    if (static_cast<long long>(aktualna_) + Zmiana() < 0) {
        wxMessageBox(TR("Nie można wydać więcej sztuk, niż jest w magazynie."), TR("Zmiana stanu"),
                     wxOK | wxICON_WARNING, this);
        return;
    }
    event.Skip();
}

/// Wybiera zdjęcie i kopiuje je do katalogu zdjęć z ustawień (żeby nie zależeć od położenia oryginału)
void DialogElementu::WybierzZdjecie() {
    wxFileDialog dlg(this, TR("Wybierz zdjęcie elementu"), wxEmptyString, wxEmptyString,
                     TR("Obrazy (*.jpg;*.jpeg;*.png;*.bmp;*.gif)|*.jpg;*.jpeg;*.png;*.bmp;*.gif|Wszystkie pliki|*.*"),
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() != wxID_OK) return;
    wxFileName zrodlo(dlg.GetPath());
    const wxString katalog = TekstZUTF8(Konfiguracja::pobierz().pobierzUstawieniaAplikacji().sciezkaObrazki);
    if (katalog.empty() || !wxFileName::Mkdir(katalog, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL)) {
        if (!wxDirExists(katalog)) {
            tcZdjecie->SetValue(zrodlo.GetFullPath());  // bez kopiowania: zostaje ścieżka oryginału
            return;
        }
    }
    wxFileName cel(katalog, zrodlo.GetFullName());
    cel.MakeAbsolute();
    for (int n = 2; cel.FileExists() && cel.GetFullPath() != zrodlo.GetFullPath(); ++n)
        cel.SetName(zrodlo.GetName() + wxString::Format("_%d", n));
    if (cel.GetFullPath() != zrodlo.GetFullPath() && !wxCopyFile(zrodlo.GetFullPath(), cel.GetFullPath(), false)) {
        wxMessageBox(TR("Nie udało się skopiować zdjęcia do katalogu zdjęć."), TR("Zdjęcie"), wxOK | wxICON_WARNING, this);
        tcZdjecie->SetValue(zrodlo.GetFullPath());
        return;
    }
    tcZdjecie->SetValue(cel.GetFullPath());
}
