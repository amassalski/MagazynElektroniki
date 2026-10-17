#include "DialogImportBOM.h"
#include "DialogProjektu.h"
#include "../Logika/DopasowanieBOM.h"
#include "../Narzedzia/Tlumaczenia.h"

#include <wx/filedlg.h>
#include <wx/sizer.h>

namespace {
enum Kol { K_OZN, K_WARTOSC, K_OBUDOWA, K_SYMBOL, K_ILOSC, K_AKCJA, K_ELEMENT, K_LICZBA };
}

DialogImportBOM::DialogImportBOM(wxWindow* parent, LogikaMagazynu& magazyn, LogikaProjektow& projekty,
                                 long long idProjektu, const wxString& nazwaProjektu, const wxString& sciezka)
    : wxDialog(parent, wxID_ANY, TR("Import listy części do projektu: ") + nazwaProjektu, wxDefaultPosition,
               wxSize(1020, 600), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      magazyn_(magazyn), projekty_(projekty), idProjektu_(idProjektu) {
    wxBoxSizer* glowny = new wxBoxSizer(wxVERTICAL);

    // --- plik
    wxBoxSizer* wierszPliku = new wxBoxSizer(wxHORIZONTAL);
    wierszPliku->Add(new wxStaticText(this, wxID_ANY, TR("Plik:")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    tcPlik = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    wierszPliku->Add(tcPlik, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    wxButton* bPlik = new wxButton(this, wxID_ANY, TR("Wybierz plik..."));
    wierszPliku->Add(bPlik, 0);
    glowny->Add(wierszPliku, 0, wxEXPAND | wxALL, 10);

    lbFormat = new wxStaticText(this, wxID_ANY,
                                TR("Obsługiwane: KiCad (CSV z raportu BOM, netlista XML), Eagle (schemat .sch, CSV)."));
    glowny->Add(lbFormat, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);

    // --- opcje
    wxBoxSizer* opcje = new wxBoxSizer(wxHORIZONTAL);
    opcje->Add(new wxStaticText(this, wxID_ANY, TR("Liczba płytek:")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    scPlytek = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(80, -1), wxSP_ARROW_KEYS, 1,
                              10000, 1);
    opcje->Add(scPlytek, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 16);
    cbZastap = new wxCheckBox(this, wxID_ANY, TR("Zastąp obecną listę (poza pozycjami, z których już wydano)"));
    opcje->Add(cbZastap, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 16);
    opcje->Add(new wxStaticText(this, wxID_ANY, TR("Kategoria nowych elementów:")), 0,
               wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    chKategoria = new wxChoice(this, wxID_ANY);
    chKategoria->Append(TR("(bez kategorii)"));
    idKategorii_.push_back(0);
    if (magazyn_.kategorie().empty()) magazyn_.odswiezKategorie();
    for (const Kategoria& k : magazyn_.kategorie()) {
        chKategoria->Append(TekstZUTF8(k.pelnaNazwa));
        idKategorii_.push_back(k.id);
    }
    chKategoria->SetSelection(0);
    opcje->Add(chKategoria, 1, wxALIGN_CENTER_VERTICAL);
    glowny->Add(opcje, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    // --- lista
    lista = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                           wxLC_REPORT | wxLC_SINGLE_SEL | wxBORDER_THEME);
    const struct { const char* t; int s; int f; } kol[K_LICZBA] = {
        {"Oznaczenia", 130, wxLIST_FORMAT_LEFT}, {"Wartość", 80, wxLIST_FORMAT_LEFT},
        {"Obudowa", 130, wxLIST_FORMAT_LEFT},    {"Symbol (MPN)", 110, wxLIST_FORMAT_LEFT},
        {"Ilość", 50, wxLIST_FORMAT_RIGHT},      {"Co zrobić", 150, wxLIST_FORMAT_LEFT},
        {"Element z magazynu", 270, wxLIST_FORMAT_LEFT}};
    for (int i = 0; i < K_LICZBA; ++i) lista->InsertColumn(i, TR(kol[i].t), kol[i].f, kol[i].s);
    glowny->Add(lista, 1, wxEXPAND | wxLEFT | wxRIGHT, 10);

    wxBoxSizer* akcje = new wxBoxSizer(wxHORIZONTAL);
    auto dodajPrzycisk = [&](const wxString& t, std::function<void()> f) {
        wxButton* b = new wxButton(this, wxID_ANY, t);
        b->Bind(wxEVT_BUTTON, [f](wxCommandEvent&) { f(); });
        b->Bind(wxEVT_UPDATE_UI, [this](wxUpdateUIEvent& e) { e.Enable(lista->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED) >= 0); });
        akcje->Add(b, 0, wxRIGHT, 6);
    };
    dodajPrzycisk(TR("Wybierz element z magazynu..."), [this] { WybierzElementDlaZaznaczonej(); });
    dodajPrzycisk(TR("Utwórz nowy element"), [this] { UstawAkcje(WierszImportuBOM::Akcja::UtworzNowy); });
    dodajPrzycisk(TR("Pomiń"), [this] { UstawAkcje(WierszImportuBOM::Akcja::Pomin); });
    glowny->Add(akcje, 0, wxALL, 10);

    lbPodsumowanie = new wxStaticText(this, wxID_ANY, wxEmptyString);
    glowny->Add(lbPodsumowanie, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);

    wxStdDialogButtonSizer* przyciski = new wxStdDialogButtonSizer;
    wxButton* ok = new wxButton(this, wxID_OK, TR("Importuj"));
    przyciski->AddButton(ok);
    przyciski->AddButton(new wxButton(this, wxID_CANCEL, TR("Anuluj")));
    przyciski->Realize();
    glowny->Add(przyciski, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    SetSizer(glowny);

    bPlik->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        wxFileDialog dlg(this, TR("Wybierz plik z listą części"), wxEmptyString, wxEmptyString,
                         TR("Listy części (*.csv;*.xml;*.sch;*.net)|*.csv;*.xml;*.sch;*.net|Wszystkie pliki|*.*"),
                         wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (dlg.ShowModal() != wxID_OK) return;
        tcPlik->SetValue(dlg.GetPath());
        WczytajPlik(dlg.GetPath());
    });
    tcPlik->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent&) { WczytajPlik(tcPlik->GetValue()); });
    lista->Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](wxListEvent&) { WybierzElementDlaZaznaczonej(); });
    Bind(wxEVT_BUTTON, &DialogImportBOM::OnImportuj, this, wxID_OK);
    Bind(wxEVT_UPDATE_UI, [this](wxUpdateUIEvent& e) {
        int ile = 0;
        for (const auto& w : wiersze_) ile += w.akcja != WierszImportuBOM::Akcja::Pomin;
        e.Enable(ile > 0);
    }, wxID_OK);

    // elementy magazynu do dopasowania
    FiltrElementow f;
    magazyn_.pobierzElementy(f, elementy_);

    if (!sciezka.empty()) {
        tcPlik->SetValue(sciezka);
        WczytajPlik(sciezka);
    }
}

wxString DialogImportBOM::OpisElementu(long long id) const {
    for (const Element& e : elementy_) {
        if (e.id != id) continue;
        std::string t = e.symbol;
        if (!e.wartosc.empty()) t += "  " + e.wartosc;
        if (!e.obudowa.empty()) t += "  " + e.obudowa;
        return TekstZUTF8(t) + wxString::Format("  [%d szt.]", e.iloscDostepna);
    }
    return wxString::Format("#%lld", id);
}

bool DialogImportBOM::WczytajPlik(const wxString& sciezka) {
    WynikImportuBOM wynik;
    std::string blad;
    if (!ImportBOM::wczytajPlik(sciezka.utf8_string(), wynik, blad)) {
        wxMessageBox(TekstZUTF8(blad), TR("Import listy części"), wxOK | wxICON_ERROR, this);
        return false;
    }
    wczytane_ = std::move(wynik);
    wiersze_.clear();
    for (const PozycjaBOM& p : wczytane_.pozycje) {
        WierszImportuBOM w;
        w.pozycja = p;
        const auto kandydaci = DopasowanieBOM::znajdz(p, elementy_, 1);
        if (!kandydaci.empty() && kandydaci[0].punkty >= 80) {
            w.akcja = WierszImportuBOM::Akcja::UzyjIstniejacego;
            w.idElementu = kandydaci[0].idElementu;
        } else {
            w.akcja = WierszImportuBOM::Akcja::UtworzNowy;
        }
        wiersze_.push_back(w);
    }
    lbFormat->SetLabel(TR("Format: ") + TekstZUTF8(wczytane_.format) + wxString::Format(TR(", pozycji: %zu, sztuk: %d"),
                                                                                      wczytane_.pozycje.size(),
                                                                                      wczytane_.liczbaSztuk()));
    Wypelnij();
    if (!wczytane_.ostrzezenia.empty()) {
        wxString t;
        for (const std::string& o : wczytane_.ostrzezenia) t += "• " + TekstZUTF8(o) + "\n";
        wxMessageBox(t, TR("Uwagi do wczytanego pliku"), wxOK | wxICON_INFORMATION, this);
    }
    return true;
}

void DialogImportBOM::Wypelnij() {
    lista->DeleteAllItems();
    for (size_t i = 0; i < wiersze_.size(); ++i) {
        lista->InsertItem(static_cast<long>(i), wxEmptyString);
        OdswiezWiersz(i);
    }
    if (!wiersze_.empty()) lista->SetItemState(0, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
    Podsumuj();
}

void DialogImportBOM::OdswiezWiersz(size_t i) {
    const WierszImportuBOM& w = wiersze_[i];
    const long r = static_cast<long>(i);
    lista->SetItem(r, K_OZN, TekstZUTF8(w.pozycja.oznaczenia));
    lista->SetItem(r, K_WARTOSC, TekstZUTF8(w.pozycja.wartosc));
    lista->SetItem(r, K_OBUDOWA, TekstZUTF8(w.pozycja.obudowa));
    lista->SetItem(r, K_SYMBOL, TekstZUTF8(w.pozycja.symbol));
    lista->SetItem(r, K_ILOSC, wxString::Format("%d", w.pozycja.ilosc));
    switch (w.akcja) {
        case WierszImportuBOM::Akcja::Pomin:
            lista->SetItem(r, K_AKCJA, TR("pomiń"));
            lista->SetItem(r, K_ELEMENT, wxEmptyString);
            break;
        case WierszImportuBOM::Akcja::UtworzNowy:
            lista->SetItem(r, K_AKCJA, TR("utwórz nowy element"));
            lista->SetItem(r, K_ELEMENT, wxEmptyString);
            break;
        case WierszImportuBOM::Akcja::UzyjIstniejacego:
            lista->SetItem(r, K_AKCJA, TR("użyj istniejącego"));
            lista->SetItem(r, K_ELEMENT, OpisElementu(w.idElementu));
            break;
    }
    wxColour kolor = w.akcja == WierszImportuBOM::Akcja::Pomin ? wxColour(140, 140, 140)
                     : w.akcja == WierszImportuBOM::Akcja::UtworzNowy ? wxColour(0, 110, 190)
                                                                       : wxColour(30, 130, 40);
    lista->SetItemTextColour(r, kolor);
}

void DialogImportBOM::Podsumuj() {
    int uzyj = 0, nowe = 0, pomin = 0;
    for (const auto& w : wiersze_) {
        if (w.akcja == WierszImportuBOM::Akcja::UzyjIstniejacego) ++uzyj;
        else if (w.akcja == WierszImportuBOM::Akcja::UtworzNowy) ++nowe;
        else ++pomin;
    }
    lbPodsumowanie->SetLabel(wxString::Format(TR("Z magazynu: %d, nowe elementy: %d, pominięte: %d"), uzyj, nowe, pomin));
}

void DialogImportBOM::UstawAkcje(WierszImportuBOM::Akcja akcja) {
    const long r = lista->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (r < 0 || static_cast<size_t>(r) >= wiersze_.size()) return;
    wiersze_[static_cast<size_t>(r)].akcja = akcja;
    OdswiezWiersz(static_cast<size_t>(r));
    Podsumuj();
}

void DialogImportBOM::WybierzElementDlaZaznaczonej() {
    const long r = lista->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (r < 0 || static_cast<size_t>(r) >= wiersze_.size()) return;
    WierszImportuBOM& w = wiersze_[static_cast<size_t>(r)];
    const std::string podpowiedz = !w.pozycja.symbol.empty() ? w.pozycja.symbol : w.pozycja.wartosc;
    DialogWyboruElementu dlg(this, magazyn_, TekstZUTF8(podpowiedz));
    if (dlg.ShowModal() != wxID_OK) return;
    w.akcja = WierszImportuBOM::Akcja::UzyjIstniejacego;
    w.idElementu = dlg.WybraneId();
    if (OpisElementu(w.idElementu).StartsWith("#")) {  // element spoza wczytanej listy
        FiltrElementow f;
        magazyn_.pobierzElementy(f, elementy_);
    }
    OdswiezWiersz(static_cast<size_t>(r));
    Podsumuj();
}

void DialogImportBOM::OnImportuj(wxCommandEvent&) {
    if (wiersze_.empty()) return;
    const int s = chKategoria->GetSelection();
    std::optional<long long> kategoria;
    if (s > 0 && static_cast<size_t>(s) < idKategorii_.size()) kategoria = idKategorii_[static_cast<size_t>(s)];
    for (WierszImportuBOM& w : wiersze_) w.idKategorii = kategoria;

    if (cbZastap->GetValue() &&
        wxMessageBox(TR("Obecna lista części projektu zostanie zastąpiona. Kontynuować?"), TR("Import listy części"),
                     wxYES_NO | wxICON_QUESTION, this) != wxYES) {
        return;
    }
    bool ok;
    {
        wxBusyCursor zajety;
        ok = projekty_.importujBOM(idProjektu_, wiersze_, cbZastap->GetValue(), scPlytek->GetValue(), &wynik_);
    }
    if (!ok) {
        wxMessageBox(TekstZUTF8(projekty_.pobierzBlad()), TR("Import listy części"), wxOK | wxICON_ERROR, this);
        return;
    }
    EndModal(wxID_OK);
}
