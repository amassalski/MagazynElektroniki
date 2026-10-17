#include "PanelProjektow.h"
#include "Ikony.h"
#include "../Eksport/EksportEagle.h"
#include <wx/ffile.h>
#include <wx/log.h>
#include "DialogImportBOM.h"
#include "DialogProjektu.h"
#include "../Narzedzia/Tlumaczenia.h"
#include "../Pomoc/PomocGUI.h"

#include <wx/datetime.h>
#include <wx/filedlg.h>
#include <wx/numdlg.h>
#include <wx/wrapsizer.h>

PanelProjektow::PanelProjektow(wxWindow* parent, LogikaMagazynu& magazyn, LogikaProjektow& projekty)
    : wxPanel(parent, wxID_ANY), magazyn_(magazyn), projekty_logika_(projekty) {
    wxBoxSizer* glowny = new wxBoxSizer(wxVERTICAL);
    wxSplitterWindow* podzial = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                                     wxSP_LIVE_UPDATE | wxSP_3D);

    // --- lewa strona: projekty
    wxPanel* lewy = new wxPanel(podzial);
    wxBoxSizer* sl = new wxBoxSizer(wxVERTICAL);
    cbArchiwum_ = new wxCheckBox(lewy, ID_P_ARCHIWUM, TR("Pokaż zarchiwizowane"));
    sl->Add(cbArchiwum_, 0, wxALL, 4);
    wxWrapSizer* przyciskiP = new wxWrapSizer(wxHORIZONTAL);
    for (auto [id, t] : {std::pair<int, const char*>{ID_P_NOWY, "Nowy projekt"}, {ID_P_EDYTUJ, "Edytuj"},
                         {ID_P_USUN, "Usuń"}}) {
        wxButton* bp = new wxButton(lewy, id, TR(t));
        Ikony::DlaPrzycisku(bp, id == ID_P_NOWY ? "nowy" : id == ID_P_EDYTUJ ? "edytuj" : "usun");
        przyciskiP->Add(bp, 0, wxRIGHT | wxBOTTOM, 4);
        Bind(wxEVT_UPDATE_UI, &PanelProjektow::OnAktualizujPrzycisk, this, id);
    }
    sl->Add(przyciskiP, 0, wxEXPAND | wxLEFT | wxRIGHT, 4);
    listaProjektow_ = new wxListCtrl(lewy, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                     wxLC_REPORT | wxLC_SINGLE_SEL | wxBORDER_THEME);
    listaProjektow_->InsertColumn(0, TR("Projekt"), wxLIST_FORMAT_LEFT, 215);
    listaProjektow_->InsertColumn(1, TR("Status"), wxLIST_FORMAT_LEFT, 80);
    listaProjektow_->InsertColumn(2, TR("Poz."), wxLIST_FORMAT_RIGHT, 45);
    listaProjektow_->InsertColumn(3, TR("Braki"), wxLIST_FORMAT_RIGHT, 50);
    sl->Add(listaProjektow_, 1, wxEXPAND | wxALL, 4);
    PomocGUI::Dodaj(listaProjektow_, "lista.projekty");
    PomocGUI::DymekZKolumn(listaProjektow_);
    lewy->SetSizer(sl);

    // --- prawa strona: lista części
    wxPanel* prawy = new wxPanel(podzial);
    wxBoxSizer* sp = new wxBoxSizer(wxVERTICAL);
    naglowek_ = new wxStaticText(prawy, wxID_ANY, TR("Wybierz projekt"));
    wxFont f = naglowek_->GetFont();
    f.SetWeight(wxFONTWEIGHT_BOLD);
    naglowek_->SetFont(f);
    sp->Add(naglowek_, 0, wxALL, 6);
    wxWrapSizer* przyciskiB = new wxWrapSizer(wxHORIZONTAL);
    for (auto [id, t] : {std::pair<int, const char*>{ID_B_IMPORT, "Importuj BOM..."},
                         {ID_B_DODAJ, "Dodaj część..."},
                         {ID_B_EDYTUJ, "Edytuj pozycję..."},
                         {ID_B_USUN, "Usuń pozycję"},
                         {ID_B_WYDAJ, "Wydaj z magazynu..."},
                         {ID_B_WYDAJ_WSZYSTKO, "Wydaj wszystko dostępne"},
                         {ID_B_ZAKUPY, "Lista zakupów (CSV)..."}}) {
        wxButton* bb = new wxButton(prawy, id, TR(t));
        if (id == ID_B_EDYTUJ) Ikony::DlaPrzycisku(bb, "edytuj");
        else if (id == ID_B_USUN) Ikony::DlaPrzycisku(bb, "usun");
        przyciskiB->Add(bb, 0, wxRIGHT | wxBOTTOM, 4);
        Bind(wxEVT_UPDATE_UI, &PanelProjektow::OnAktualizujPrzycisk, this, id);
    }
    sp->Add(przyciskiB, 0, wxEXPAND | wxLEFT | wxRIGHT, 4);
    listaPozycji_ = new wxListCtrl(prawy, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                   wxLC_REPORT | wxLC_SINGLE_SEL | wxBORDER_THEME);
    const struct { const char* t; int s; int f; } kol[K_LICZBA] = {
        {"Oznaczenia", 130, wxLIST_FORMAT_LEFT}, {"Symbol", 150, wxLIST_FORMAT_LEFT},
        {"Producent", 90, wxLIST_FORMAT_LEFT},   {"Wartość", 70, wxLIST_FORMAT_LEFT},
        {"Obudowa", 80, wxLIST_FORMAT_LEFT},     {"Potrzeba", 65, wxLIST_FORMAT_RIGHT},
        {"Wydano", 60, wxLIST_FORMAT_RIGHT},     {"W magazynie", 85, wxLIST_FORMAT_RIGHT},
        {"Brakuje", 65, wxLIST_FORMAT_RIGHT},    {"Zamówiono", 80, wxLIST_FORMAT_RIGHT}};
    for (int i = 0; i < K_LICZBA; ++i) listaPozycji_->InsertColumn(i, TR(kol[i].t), kol[i].f, kol[i].s);
    sp->Add(listaPozycji_, 1, wxEXPAND | wxALL, 4);
    PomocGUI::Dodaj(listaPozycji_, "lista.pozycje");
    PomocGUI::DymekZKolumn(listaPozycji_);
    podsumowanie_ = new wxStaticText(prawy, wxID_ANY, wxEmptyString);
    sp->Add(podsumowanie_, 0, wxALL, 6);
    prawy->SetSizer(sp);

    podzial->SplitVertically(lewy, prawy, 400);
    podzial->SetMinimumPaneSize(220);
    podzial->SetSashGravity(0.0);
    glowny->Add(podzial, 1, wxEXPAND);
    SetSizer(glowny);

    // --- zdarzenia
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { NowyProjekt(); }, ID_P_NOWY);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EdytujProjekt(); }, ID_P_EDYTUJ);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { UsunProjekt(); }, ID_P_USUN);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ImportujBOM(); }, ID_B_IMPORT);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { DodajCzesc(); }, ID_B_DODAJ);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EdytujPozycje(); }, ID_B_EDYTUJ);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { UsunPozycje(); }, ID_B_USUN);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { WydajPozycje(); }, ID_B_WYDAJ);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { WydajWszystko(); }, ID_B_WYDAJ_WSZYSTKO);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ListaZakupow(); }, ID_B_ZAKUPY);
    Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { Odswiez(); }, ID_P_ARCHIWUM);
    listaProjektow_->Bind(wxEVT_LIST_ITEM_SELECTED, [this](wxListEvent&) { OdswiezPozycje(); });
    listaProjektow_->Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](wxListEvent&) { EdytujProjekt(); });
    listaPozycji_->Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](wxListEvent&) { EdytujPozycje(); });
}

// ---------------------------------------------------------------------------

const Projekt* PanelProjektow::WybranyProjekt() const {
    const long i = listaProjektow_->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (i < 0 || static_cast<size_t>(i) >= projekty_.size()) return nullptr;
    return &projekty_[static_cast<size_t>(i)];
}

const PozycjaProjektu* PanelProjektow::WybranaPozycja() const {
    const long i = listaPozycji_->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (i < 0 || static_cast<size_t>(i) >= pozycje_.size()) return nullptr;
    return &pozycje_[static_cast<size_t>(i)];
}

void PanelProjektow::Blad(const wxString& naglowek, const std::string& tresc) {
    Komunikat(TR("✗ ") + naglowek + ": " + TekstZUTF8(tresc));
    wxMessageBox(TekstZUTF8(tresc), naglowek, wxOK | wxICON_ERROR, this);
}

void PanelProjektow::Wyczysc() {
    projekty_.clear();
    pozycje_.clear();
    listaProjektow_->DeleteAllItems();
    listaPozycji_->DeleteAllItems();
    naglowek_->SetLabel(TR("Wybierz projekt"));
    podsumowanie_->SetLabel(wxEmptyString);
}

void PanelProjektow::Odswiez() {
    const Projekt* p = WybranyProjekt();
    WypelnijProjekty(p ? p->id : 0);
}

void PanelProjektow::WypelnijProjekty(long long zachowajId) {
    std::vector<Projekt> nowe;
    if (!projekty_logika_.pobierzProjekty(!cbArchiwum_->GetValue(), nowe)) {
        Wyczysc();
        Komunikat(TR("✗ Nie udało się pobrać projektów: ") + TekstZUTF8(projekty_logika_.pobierzBlad()));
        return;
    }
    projekty_ = std::move(nowe);
    listaProjektow_->Freeze();
    listaProjektow_->DeleteAllItems();
    long wybor = -1;
    for (size_t i = 0; i < projekty_.size(); ++i) {
        const Projekt& p = projekty_[i];
        const long r = static_cast<long>(i);
        wxString nazwa = TekstZUTF8(p.nazwa);
        if (!p.aktywny) nazwa += TR(" (archiwum)");
        listaProjektow_->InsertItem(r, nazwa);
        listaProjektow_->SetItem(r, 1, NazwaStatusuProjektu(p.status));
        listaProjektow_->SetItem(r, 2, wxString::Format("%d", p.liczbaPozycji));
        listaProjektow_->SetItem(r, 3, p.liczbaBrakow > 0 ? wxString::Format("%d", p.liczbaBrakow) : wxString("-"));
        if (!p.aktywny) listaProjektow_->SetItemTextColour(r, wxColour(140, 140, 140));
        else if (p.liczbaBrakow > 0) listaProjektow_->SetItemTextColour(r, wxColour(214, 84, 0));
        if (p.id == zachowajId) wybor = r;
    }
    listaProjektow_->Thaw();
    if (wybor < 0 && !projekty_.empty()) wybor = 0;
    if (wybor >= 0) {
        listaProjektow_->SetItemState(wybor, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
                                      wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
        listaProjektow_->EnsureVisible(wybor);
    }
    OdswiezPozycje();
}

void PanelProjektow::WybierzProjekt(long long id) {
    for (size_t i = 0; i < projekty_.size(); ++i) {
        if (projekty_[i].id == id) {
            listaProjektow_->SetItemState(static_cast<long>(i), wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
                                          wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
            return;
        }
    }
}

void PanelProjektow::OdswiezPozycje() {
    const long zaznaczona = listaPozycji_->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    const long long idZaznaczonej = (zaznaczona >= 0 && static_cast<size_t>(zaznaczona) < pozycje_.size())
                                        ? pozycje_[static_cast<size_t>(zaznaczona)].id : 0;
    pozycje_.clear();
    listaPozycji_->DeleteAllItems();
    const Projekt* p = WybranyProjekt();
    if (!p) {
        naglowek_->SetLabel(TR("Wybierz projekt"));
        podsumowanie_->SetLabel(wxEmptyString);
        return;
    }
    naglowek_->SetLabel(TekstZUTF8(p->nazwa) + "  -  " + NazwaStatusuProjektu(p->status) +
                        (p->opis.empty() ? wxString() : wxString("  -  ") + TekstZUTF8(p->opis).BeforeFirst('\n')));
    if (!projekty_logika_.pobierzPozycje(p->id, pozycje_)) {
        Komunikat(TR("✗ Nie udało się pobrać listy części: ") + TekstZUTF8(projekty_logika_.pobierzBlad()));
        return;
    }
    int sztuk = 0, braki = 0, kompletne = 0;
    long wybor = -1;
    listaPozycji_->Freeze();
    for (size_t i = 0; i < pozycje_.size(); ++i) {
        const PozycjaProjektu& z = pozycje_[i];
        const long r = static_cast<long>(i);
        listaPozycji_->InsertItem(r, TekstZUTF8(z.oznaczenia));
        listaPozycji_->SetItem(r, K_SYMBOL, TekstZUTF8(z.symbol));
        listaPozycji_->SetItem(r, K_PRODUCENT, TekstZUTF8(z.producent));
        listaPozycji_->SetItem(r, K_WARTOSC, TekstZUTF8(z.wartosc));
        listaPozycji_->SetItem(r, K_OBUDOWA, TekstZUTF8(z.obudowa));
        listaPozycji_->SetItem(r, K_WYMAGANE, wxString::Format("%d", z.wymagana));
        listaPozycji_->SetItem(r, K_ZUZYTE, wxString::Format("%d", z.zuzyta));
        listaPozycji_->SetItem(r, K_DOSTEPNE, wxString::Format("%d", z.dostepna));
        listaPozycji_->SetItem(r, K_BRAKUJE, z.brakuje() > 0 ? wxString::Format("%d", z.brakuje()) : wxString("-"));
        listaPozycji_->SetItem(r, K_ZAMOWIONE, z.zamowiona > 0 ? wxString::Format("%d", z.zamowiona) : wxString("-"));
        sztuk += z.wymagana;
        if (z.doWydania() == 0) {
            ++kompletne;
            listaPozycji_->SetItemTextColour(r, wxColour(30, 130, 40));
        } else if (z.brakuje() > 0) {
            ++braki;
            listaPozycji_->SetItemTextColour(r, wxColour(214, 84, 0));
        }
        if (z.id == idZaznaczonej) wybor = r;
    }
    listaPozycji_->Thaw();
    if (wybor >= 0)
        listaPozycji_->SetItemState(wybor, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
                                    wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
    podsumowanie_->SetLabel(wxString::Format(TR("Pozycji: %zu, sztuk: %d, wydano w całości: %d, brakuje w magazynie: %d"),
                                             pozycje_.size(), sztuk, kompletne, braki));
    if (naStatus) naStatus(TR("Projekt: ") + TekstZUTF8(p->nazwa));
}

void PanelProjektow::OnAktualizujPrzycisk(wxUpdateUIEvent& event) {
    const bool baza = IsEnabled();
    switch (event.GetId()) {
        case ID_P_NOWY: event.Enable(baza); break;
        case ID_P_EDYTUJ:
        case ID_P_USUN:
        case ID_B_IMPORT:
        case ID_B_DODAJ:
        case ID_B_WYDAJ_WSZYSTKO: event.Enable(baza && MaProjekt()); break;
        case ID_B_ZAKUPY: event.Enable(baza && MaProjekt()); break;
        default: event.Enable(baza && MaPozycje()); break;
    }
}

// ---------------------------------------------------------------------------
// Projekty
// ---------------------------------------------------------------------------

void PanelProjektow::NowyProjekt() {
    DialogProjektu dlg(this, Projekt{});
    if (dlg.ShowModal() != wxID_OK) return;
    const Projekt p = dlg.PobierzProjekt();
    auto id = projekty_logika_.zapiszProjekt(p);
    if (!id) return Blad(TR("Nowy projekt"), projekty_logika_.pobierzBlad());
    Komunikat(TR("✓ Dodano projekt: ") + TekstZUTF8(p.nazwa));
    WypelnijProjekty(*id);
}

void PanelProjektow::EdytujProjekt() {
    const Projekt* wybrany = WybranyProjekt();
    if (!wybrany) return;
    DialogProjektu dlg(this, *wybrany);
    if (dlg.ShowModal() != wxID_OK) return;
    const Projekt p = dlg.PobierzProjekt();
    if (!projekty_logika_.zapiszProjekt(p)) return Blad(TR("Edycja projektu"), projekty_logika_.pobierzBlad());
    Komunikat(TR("✓ Zapisano projekt: ") + TekstZUTF8(p.nazwa));
    WypelnijProjekty(p.id);
}

void PanelProjektow::UsunProjekt() {
    const Projekt* wybrany = WybranyProjekt();
    if (!wybrany) return;
    const Projekt p = *wybrany;
    if (wxMessageBox(TR("Usunąć projekt „") + TekstZUTF8(p.nazwa) +
                         TR("” razem z listą części?\n\nJeśli wydano już do niego części, projekt zostanie tylko zarchiwizowany."),
                     TR("Usuń projekt"), wxYES_NO | wxICON_QUESTION, this) != wxYES)
        return;
    switch (projekty_logika_.usunProjekt(p.id)) {
        case WynikUsuniecia::Usuniety: Komunikat(TR("✓ Usunięto projekt: ") + TekstZUTF8(p.nazwa)); break;
        case WynikUsuniecia::Zarchiwizowany:
            Komunikat(TR("✓ Projekt zarchiwizowany (były wydania części): ") + TekstZUTF8(p.nazwa));
            break;
        case WynikUsuniecia::Blad: return Blad(TR("Usuń projekt"), projekty_logika_.pobierzBlad());
    }
    WypelnijProjekty(0);
}

// ---------------------------------------------------------------------------
// Lista części
// ---------------------------------------------------------------------------

void PanelProjektow::ImportujBOM(const wxString& sciezka) {
    const Projekt* wybrany = WybranyProjekt();
    if (!wybrany) return;
    const Projekt p = *wybrany;
    DialogImportBOM dlg(this, magazyn_, projekty_logika_, p.id, TekstZUTF8(p.nazwa), sciezka);
    if (dlg.ShowModal() != wxID_OK) return;
    const WynikImportuDoProjektu& w = dlg.Wynik();
    Komunikat(wxString::Format(TR("✓ Import BOM do projektu %s: nowych pozycji %d, zwiększono %d, nowych elementów %d, pominięto %d"),
                               TekstZUTF8(p.nazwa), w.dodanoPozycji, w.zsumowano, w.utworzonoElementow, w.pominieto));
    WypelnijProjekty(p.id);
    if (naZmianaMagazynu && w.utworzonoElementow > 0) naZmianaMagazynu();
}

void PanelProjektow::DodajCzesc() {
    const Projekt* wybrany = WybranyProjekt();
    if (!wybrany) return;
    PozycjaProjektu poz;
    poz.idProjektu = wybrany->id;
    DialogPozycji dlg(this, magazyn_, poz, true);
    if (dlg.ShowModal() != wxID_OK) return;
    if (!projekty_logika_.dodajPozycje(dlg.PobierzPozycje()))
        return Blad(TR("Dodaj część"), projekty_logika_.pobierzBlad());
    WypelnijProjekty(wybrany->id);
}

void PanelProjektow::EdytujPozycje() {
    const PozycjaProjektu* wybrana = WybranaPozycja();
    const Projekt* projekt = WybranyProjekt();
    if (!wybrana || !projekt) return;
    DialogPozycji dlg(this, magazyn_, *wybrana, false);
    if (dlg.ShowModal() != wxID_OK) return;
    if (!projekty_logika_.zapiszPozycje(dlg.PobierzPozycje()))
        return Blad(TR("Edycja pozycji"), projekty_logika_.pobierzBlad());
    WypelnijProjekty(projekt->id);
}

void PanelProjektow::UsunPozycje() {
    const PozycjaProjektu* wybrana = WybranaPozycja();
    const Projekt* projekt = WybranyProjekt();
    if (!wybrana || !projekt) return;
    if (wxMessageBox(TR("Usunąć z projektu pozycję „") + TekstZUTF8(wybrana->symbol) + "”?", TR("Usuń pozycję"),
                     wxYES_NO | wxICON_QUESTION, this) != wxYES)
        return;
    if (!projekty_logika_.usunPozycje(wybrana->id)) return Blad(TR("Usuń pozycję"), projekty_logika_.pobierzBlad());
    WypelnijProjekty(projekt->id);
}

void PanelProjektow::WydajPozycje() {
    const PozycjaProjektu* wybrana = WybranaPozycja();
    const Projekt* projekt = WybranyProjekt();
    if (!wybrana || !projekt) return;
    const PozycjaProjektu poz = *wybrana;
    const int mozna = std::min(poz.doWydania(), poz.dostepna);
    if (mozna <= 0) {
        wxMessageBox(poz.doWydania() == 0 ? TR("Ta pozycja jest już w całości wydana do projektu.")
                                          : TR("Brak tego elementu w magazynie."),
                     TR("Wydaj z magazynu"), wxOK | wxICON_INFORMATION, this);
        return;
    }
    const long ilosc = wxGetNumberFromUser(TR("Ile sztuk wydać do projektu?\n(w magazynie: ") +
                                               wxString::Format("%d, potrzeba jeszcze: %d)", poz.dostepna, poz.doWydania()),
                                           TR("Ilość:"), TR("Wydaj z magazynu: ") + TekstZUTF8(poz.symbol), mozna, 1,
                                           mozna, this);
    if (ilosc <= 0) return;
    if (!projekty_logika_.wydajDoProjektu(poz.id, static_cast<int>(ilosc)))
        return Blad(TR("Wydaj z magazynu"), projekty_logika_.pobierzBlad());
    Komunikat(wxString::Format(TR("✓ Wydano %ld szt. %s do projektu %s"), ilosc, TekstZUTF8(poz.symbol),
                               TekstZUTF8(projekt->nazwa)));
    WypelnijProjekty(projekt->id);
    if (naZmianaMagazynu) naZmianaMagazynu();
}

void PanelProjektow::WydajWszystko() {
    const Projekt* projekt = WybranyProjekt();
    if (!projekt) return;
    const Projekt p = *projekt;
    if (wxMessageBox(TR("Wydać do projektu „") + TekstZUTF8(p.nazwa) +
                         TR("” wszystkie części, które są dostępne w magazynie?"),
                     TR("Wydaj z magazynu"), wxYES_NO | wxICON_QUESTION, this) != wxYES)
        return;
    int sztuk = 0;
    if (!projekty_logika_.wydajWszystkoDostepne(p.id, &sztuk))
        return Blad(TR("Wydaj z magazynu"), projekty_logika_.pobierzBlad());
    Komunikat(wxString::Format(TR("✓ Wydano do projektu %s: %d szt."), TekstZUTF8(p.nazwa), sztuk));
    WypelnijProjekty(p.id);
    if (naZmianaMagazynu) naZmianaMagazynu();
}

static bool ZapiszTekst(const wxString& sciezka, const std::string& tekst) {
    wxLogNull cisza;
    wxFFile f(sciezka, "wb");
    return f.IsOpened() && f.Write(tekst.data(), tekst.size()) == tekst.size();
}

int PanelProjektow::SkryptEagle(const wxString& sciezkaWe) {
    const Projekt* projekt = WybranyProjekt();
    if (!projekt) return 0;
    std::vector<PozycjaProjektu> pozycje;
    if (!projekty_logika_.pobierzPozycje(projekt->id, pozycje)) {
        Blad(TR("Skrypt dla Eagle"), projekty_logika_.pobierzBlad());
        return 0;
    }
    const auto wynik = EksportEagle::skrypt(pozycje, projekt->nazwa);
    if (wynik.czesci == 0) {
        if (sciezkaWe.empty())
            wxMessageBox(TR("Pozycje projektu nie mają oznaczeń części (R1, C2, ...), więc nie ma czego ustawiać w schemacie."),
                         TR("Skrypt dla Eagle"), wxOK | wxICON_INFORMATION, this);
        return 0;
    }
    wxString sciezka = sciezkaWe;
    if (sciezka.empty()) {
        wxFileDialog dlg(this, TR("Zapisz skrypt dla Eagle"), wxEmptyString,
                         TekstZUTF8(projekt->nazwa) + "_atrybuty.scr", TR("Skrypty Eagle (*.scr)|*.scr"),
                         wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (dlg.ShowModal() != wxID_OK) return 0;
        sciezka = dlg.GetPath();
    }
    if (!ZapiszTekst(sciezka, wynik.tekst)) {
        wxMessageBox(TR("Nie udało się zapisać pliku."), TR("Skrypt dla Eagle"), wxOK | wxICON_ERROR, this);
        return 0;
    }
    Komunikat(wxString::Format(TR("✓ Zapisano skrypt dla Eagle (%d części): "), wynik.czesci) + sciezka);
    return wynik.czesci;
}

bool PanelProjektow::ZapiszUlp(const wxString& sciezkaWe) {
    wxString sciezka = sciezkaWe;
    if (sciezka.empty()) {
        wxFileDialog dlg(this, TR("Zapisz program ULP dla Eagle"), wxEmptyString, "dm_bom.ulp",
                         TR("Programy ULP (*.ulp)|*.ulp"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (dlg.ShowModal() != wxID_OK) return false;
        sciezka = dlg.GetPath();
    }
    if (!ZapiszTekst(sciezka, EksportEagle::ulpBOM())) {
        wxMessageBox(TR("Nie udało się zapisać pliku."), TR("Program ULP"), wxOK | wxICON_ERROR, this);
        return false;
    }
    Komunikat(TR("✓ Zapisano program ULP dla Eagle: ") + sciezka);
    return true;
}

void PanelProjektow::ListaZakupow() {
    const Projekt* projekt = WybranyProjekt();
    if (!projekt) return;
    std::vector<PozycjaProjektu> braki;
    if (!projekty_logika_.pobierzBraki(projekt->id, braki)) return Blad(TR("Lista zakupów"), projekty_logika_.pobierzBlad());
    if (braki.empty()) {
        wxMessageBox(TR("Niczego nie brakuje - wszystkie potrzebne części są w magazynie."), TR("Lista zakupów"),
                     wxOK | wxICON_INFORMATION, this);
        return;
    }
    wxFileDialog dlg(this, TR("Lista zakupów do CSV"), wxEmptyString,
                     "zakupy_" + wxDateTime::Now().Format("%Y%m%d") + ".csv", TR("Pliki CSV (*.csv)|*.csv"),
                     wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() != wxID_OK) return;
    if (!projekty_logika_.eksportujListeZakupow(dlg.GetPath().utf8_string(), projekt->id))
        return Blad(TR("Lista zakupów"), projekty_logika_.pobierzBlad());
    Komunikat(wxString::Format(TR("✓ Zapisano listę zakupów (%zu pozycji): "), braki.size()) + dlg.GetPath());
}
