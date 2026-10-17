#include "PanelKatalogu.h"
#include "Ikony.h"
#include "DialogKatalogu.h"
#include "../Logika/KatalogCSV.h"
#include "../Logika/OdczytDatasheetu.h"
#include "../Narzedzia/Konfiguracja.h"
#include "../Narzedzia/Tlumaczenia.h"
#include "../Pomoc/PomocGUI.h"
#include "../Wyszukiwanie/WyszukiwanieDatasheet.h"

#include <wx/datetime.h>
#include <wx/ffile.h>
#include <wx/filedlg.h>
#include <wx/log.h>
#include <wx/wrapsizer.h>

namespace {

bool ZapiszTekst(const wxString& sciezka, const std::string& tekst) {
    wxLogNull cisza;
    wxFFile f(sciezka, "wb");
    return f.IsOpened() && f.Write(tekst.data(), tekst.size()) == tekst.size();
}

bool WczytajTekst(const wxString& sciezka, std::string& tekst) {
    wxLogNull cisza;
    wxFFile f(sciezka, "rb");
    if (!f.IsOpened()) return false;
    wxString s;
    wxFileOffset n = f.Length();
    tekst.assign(static_cast<size_t>(n), '\0');
    return f.Read(&tekst[0], tekst.size()) == tekst.size();
}

/// Okno z propozycjami odczytanymi z datasheetu - każdą można odznaczyć
class DialogPropozycji : public wxDialog {
public:
    DialogPropozycji(wxWindow* parent, const OdczytDatasheetu::Wynik& w, const PozycjaKatalogu& poz)
        : wxDialog(parent, wxID_ANY, TR("Parametry odczytane z datasheetu"), wxDefaultPosition, wxSize(860, 560),
                   wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER), wynik_(w) {
        wxBoxSizer* g = new wxBoxSizer(wxVERTICAL);
        g->Add(new wxStaticText(this, wxID_ANY,
               TR("To tylko propozycje - program odgaduje je z tekstu karty katalogowej i może się mylić.\n"
                  "Zaznacz te, które są poprawne; zapiszą się z oznaczeniem źródła \"datasheet\".\n"
                  "\"Maks.\" = Absolute Maximum Ratings, \"Elektr.\" = Electrical Characteristics. "
                  "Pozycje oznaczone \"zgadywane\" są odznaczone - sprawdź je przed zaznaczeniem.")),
               0, wxALL, 10);
        lista_ = new wxCheckListBox(this, wxID_ANY);
        for (const auto& p : w.parametry) {
            const int i = lista_->Append(TekstZUTF8(p.nazwa) + ": " + TekstZUTF8(p.wartosc) + "    [" + TekstZUTF8(p.fragment) + "]" +
                                         (p.pewny ? wxString() : TR("   (zgadywane)")));
            lista_->Check(static_cast<unsigned>(i), p.pewny);
        }
        g->Add(lista_, 1, wxEXPAND | wxLEFT | wxRIGHT, 10);
        cbOpis_ = new wxCheckBox(this, wxID_ANY, TR("Ustaw opis: ") + TekstZUTF8(w.opis));
        cbOpis_->SetValue(!w.opis.empty() && poz.opis.empty());
        cbOpis_->Enable(!w.opis.empty());
        cbObudowa_ = new wxCheckBox(this, wxID_ANY, TR("Ustaw obudowę: ") + TekstZUTF8(w.obudowa));
        cbObudowa_->SetValue(!w.obudowa.empty() && poz.obudowa.empty());
        cbObudowa_->Enable(!w.obudowa.empty());
        cbUwagi_ = new wxCheckBox(this, wxID_ANY, TR("Dopisz do uwag: ") + TekstZUTF8(w.uwagi.size() > 110 ? w.uwagi.substr(0, 110) + "..." : w.uwagi));
        cbUwagi_->SetValue(!w.uwagi.empty() && poz.uwagi.find(w.uwagi) == std::string::npos);
        cbUwagi_->Enable(!w.uwagi.empty());
        g->Add(cbOpis_, 0, wxALL, 10);
        g->Add(cbObudowa_, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);
        g->Add(cbUwagi_, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);
        g->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 10);
        SetSizer(g);
    }
    std::vector<ParametrKatalogu> Wybrane() const {
        std::vector<ParametrKatalogu> w;
        for (size_t i = 0; i < wynik_.parametry.size(); ++i)
            if (lista_->IsChecked(static_cast<unsigned>(i)))
                w.push_back({wynik_.parametry[i].nazwa, wynik_.parametry[i].wartosc, "datasheet"});
        return w;
    }
    bool UstawOpis() const { return cbOpis_->GetValue(); }
    bool UstawObudowe() const { return cbObudowa_->GetValue(); }
    bool DopiszUwagi() const { return cbUwagi_->GetValue(); }

private:
    OdczytDatasheetu::Wynik wynik_;
    wxCheckListBox* lista_ = nullptr;
    wxCheckBox *cbOpis_ = nullptr, *cbObudowa_ = nullptr, *cbUwagi_ = nullptr;
};

} // namespace

PanelKatalogu::PanelKatalogu(wxWindow* parent, LogikaMagazynu& magazyn, LogikaKatalogu& katalog)
    : wxPanel(parent), magazyn_(magazyn), katalog_(katalog) {
    wxBoxSizer* glowny = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* gora = new wxBoxSizer(wxHORIZONTAL);
    szukaj_ = new wxSearchCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(280, -1));
    szukaj_->SetDescriptiveText(TR("Szukaj w katalogu..."));
    PomocGUI::Dodaj(szukaj_, "katalog.szukaj");
    gora->Add(szukaj_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    gora->Add(new wxStaticText(this, wxID_ANY, TR("Rodzina:")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    chRodzina_ = new wxChoice(this, wxID_ANY);
    PomocGUI::Dodaj(chRodzina_, "katalog.filtr_rodziny");
    gora->Add(chRodzina_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    cbMagazyn_ = new wxCheckBox(this, wxID_ANY, TR("tylko te, które mam w magazynie"));
    PomocGUI::Dodaj(cbMagazyn_, "katalog.tylko_magazyn");
    gora->Add(cbMagazyn_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    licznik_ = new wxStaticText(this, wxID_ANY, wxEmptyString);
    gora->Add(licznik_, 0, wxALIGN_CENTER_VERTICAL);
    glowny->Add(gora, 0, wxEXPAND | wxALL, 6);

    wxSplitterWindow* podzial = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_LIVE_UPDATE | wxSP_3D);
    widok_ = new wxListCtrl(podzial, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
    const struct { const char* t; int s; } kol[] = {{"Symbol", 150}, {"Producent", 120}, {"Opis", 300}, {"Obudowa", 90}, {"Rodzina", 110}, {"W magazynie", 90}};
    for (int i = 0; i < K_LICZBA; ++i)
        widok_->InsertColumn(i, TR(kol[i].t), i == K_MAGAZYN ? wxLIST_FORMAT_RIGHT : wxLIST_FORMAT_LEFT, kol[i].s);
    PomocGUI::Dodaj(widok_, "katalog.lista");

    wxPanel* prawy = new wxPanel(podzial);
    wxBoxSizer* ps = new wxBoxSizer(wxVERTICAL);
    naglowek_ = new wxStaticText(prawy, wxID_ANY, TR("Wybierz pozycję z listy"));
    wxFont f = naglowek_->GetFont(); f.MakeBold(); f.SetPointSize(f.GetPointSize() + 2);
    naglowek_->SetFont(f);
    ps->Add(naglowek_, 0, wxALL, 8);
    szczegoly_ = new wxStaticText(prawy, wxID_ANY, wxEmptyString);
    ps->Add(szczegoly_, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);
    parametry_ = new wxListCtrl(prawy, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL | wxBORDER_THEME);
    parametry_->InsertColumn(0, TR("Parametr"), wxLIST_FORMAT_LEFT, 170);
    parametry_->InsertColumn(1, TR("Wartość"), wxLIST_FORMAT_LEFT, 170);
    parametry_->InsertColumn(2, TR("Źródło"), wxLIST_FORMAT_LEFT, 80);
    PomocGUI::Dodaj(parametry_, "katalog.parametry");
    ps->Add(parametry_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
    prawy->SetSizer(ps);
    podzial->SplitVertically(widok_, prawy, -430);
    podzial->SetMinimumPaneSize(200);
    podzial->SetSashGravity(1.0);
    glowny->Add(podzial, 1, wxEXPAND | wxLEFT | wxRIGHT, 6);

    wxWrapSizer* przyciski = new wxWrapSizer(wxHORIZONTAL);
    auto przycisk = [&](int id, const wxString& t) {
        wxButton* b = new wxButton(this, id, t);
        przyciski->Add(b, 0, wxALL, 3);
        return b;
    };
    Ikony::DlaPrzycisku(przycisk(ID_K_NOWA, TR("Nowa...")), "nowy");
    wxButton* bEdytuj = przycisk(ID_K_EDYTUJ, TR("Edytuj..."));
    Ikony::DlaPrzycisku(bEdytuj, "edytuj");
    wxButton* bUsun = przycisk(ID_K_USUN, TR("Usuń"));
    Ikony::DlaPrzycisku(bUsun, "usun");
    wxButton* bMag = przycisk(ID_K_DO_MAGAZYNU, TR("Dodaj do magazynu..."));
    PomocGUI::Dodaj(bMag, "katalog.do_magazynu");
    przycisk(ID_K_IMPORT, TR("Import CSV..."));
    przycisk(ID_K_EKSPORT, TR("Eksport CSV..."));
    przycisk(ID_K_SZABLON, TR("Szablon CSV..."));
    wxButton* bOtworz = przycisk(ID_K_DS_OTWORZ, TR("Otwórz datasheet"));
    wxButton* bPobierz = przycisk(ID_K_DS_POBIERZ, TR("Pobierz datasheet"));
    wxButton* bOdczyt = przycisk(ID_K_DS_ODCZYT, TR("Odczytaj parametry z PDF..."));
    PomocGUI::Dodaj(bOdczyt, "katalog.odczyt_pdf");
    glowny->Add(przyciski, 0, wxEXPAND | wxALL, 4);
    SetSizer(glowny);

    for (wxButton* b : {bEdytuj, bUsun, bMag, bOtworz, bPobierz, bOdczyt})
        b->Bind(wxEVT_UPDATE_UI, [this](wxUpdateUIEvent& e) { e.Enable(Wybrana() != nullptr); });

    auto odswiez = [this](wxCommandEvent&) { if (!odswiezanie_) Odswiez(); };
    szukaj_->Bind(wxEVT_TEXT, odswiez);
    chRodzina_->Bind(wxEVT_CHOICE, odswiez);
    cbMagazyn_->Bind(wxEVT_CHECKBOX, odswiez);
    widok_->Bind(wxEVT_LIST_ITEM_SELECTED, [this](wxListEvent&) { PokazSzczegoly(); });
    widok_->Bind(wxEVT_LIST_ITEM_DESELECTED, [this](wxListEvent&) { PokazSzczegoly(); });
    widok_->Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](wxListEvent&) { Edytuj(); });
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Nowa(); }, ID_K_NOWA);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Edytuj(); }, ID_K_EDYTUJ);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Usun(); }, ID_K_USUN);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { DodajDoMagazynu(); }, ID_K_DO_MAGAZYNU);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ImportujCSV(); }, ID_K_IMPORT);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EksportujCSV(); }, ID_K_EKSPORT);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ZapiszSzablon(); }, ID_K_SZABLON);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OtworzDatasheet(); }, ID_K_DS_OTWORZ);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { PobierzDatasheet(); }, ID_K_DS_POBIERZ);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OdczytajDatasheet(); }, ID_K_DS_ODCZYT);
}

wxString PanelKatalogu::KatalogDokumentow() const {
    return TekstZUTF8(Konfiguracja::pobierz().pobierzUstawieniaAplikacji().sciezkaPDFDatasheet);
}

void PanelKatalogu::Blad(const wxString& t) {
    Komunikat(TR("✗ ") + t);
    wxMessageBox(t, TR("Katalog"), wxOK | wxICON_WARNING, this);
}

const PozycjaKatalogu* PanelKatalogu::Wybrana() const {
    const long i = widok_->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (i < 0 || static_cast<size_t>(i) >= lista_.size()) return nullptr;
    return &lista_[static_cast<size_t>(i)];
}

void PanelKatalogu::Wybierz(long long id) {
    for (size_t i = 0; i < lista_.size(); ++i)
        if (lista_[i].id == id) {
            widok_->SetItemState(static_cast<long>(i), wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
            widok_->EnsureVisible(static_cast<long>(i));
            return;
        }
}

void PanelKatalogu::Szukaj(const std::string& tekst) { szukaj_->ChangeValue(TekstZUTF8(tekst)); Odswiez(); }

void PanelKatalogu::WypelnijRodziny() {
    const wxString zaznaczona = chRodzina_->GetSelection() > 0 ? chRodzina_->GetStringSelection() : wxString();
    rodziny_ = katalog_.rodziny();
    chRodzina_->Clear();
    chRodzina_->Append(TR("(wszystkie)"));
    for (const std::string& r : rodziny_) chRodzina_->Append(TekstZUTF8(r));
    int wybor = 0;
    if (!zaznaczona.empty()) { const int n = chRodzina_->FindString(zaznaczona); if (n != wxNOT_FOUND) wybor = n; }
    chRodzina_->SetSelection(wybor);
}

void PanelKatalogu::Wyczysc() {
    lista_.clear();
    widok_->DeleteAllItems();
    PokazSzczegoly();
    licznik_->SetLabel(wxEmptyString);
}

void PanelKatalogu::Odswiez() {
    odswiezanie_ = true;
    const long long zachowaj = Wybrana() ? Wybrana()->id : 0;
    WypelnijRodziny();
    FiltrKatalogu f;
    f.tekst = szukaj_->GetValue().Strip(wxString::both).utf8_string();
    if (chRodzina_->GetSelection() > 0) f.rodzina = chRodzina_->GetStringSelection().utf8_string();
    f.tylkoWMagazynie = cbMagazyn_->GetValue();
    widok_->DeleteAllItems();
    lista_.clear();
    if (!katalog_.pobierz(f, lista_)) {
        Status(TR("✗ Katalog: ") + TekstZUTF8(katalog_.pobierzBlad()));
        lista_.clear();
    }
    for (size_t i = 0; i < lista_.size(); ++i) {
        const PozycjaKatalogu& k = lista_[i];
        const long r = widok_->InsertItem(static_cast<long>(i), TekstZUTF8(k.symbol));
        widok_->SetItem(r, K_PRODUCENT, TekstZUTF8(k.producent));
        widok_->SetItem(r, K_OPIS, TekstZUTF8(k.opis));
        widok_->SetItem(r, K_OBUDOWA, TekstZUTF8(k.obudowa));
        widok_->SetItem(r, K_RODZINA, TekstZUTF8(k.rodzina));
        widok_->SetItem(r, K_MAGAZYN, k.wMagazynie > 0 ? wxString::Format("%d", k.wMagazynie) : wxString("-"));
        if (k.liczbaElementow > 0) widok_->SetItemTextColour(r, wxColour(0, 110, 0));
    }
    licznik_->SetLabel(wxString::Format(TR("Pozycji: %d"), static_cast<int>(lista_.size())));
    odswiezanie_ = false;
    if (zachowaj) Wybierz(zachowaj);
    if (!Wybrana() && !lista_.empty()) widok_->SetItemState(0, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
    PokazSzczegoly();
    Layout();
}

void PanelKatalogu::PokazSzczegoly() {
    parametry_->DeleteAllItems();
    const PozycjaKatalogu* w = Wybrana();
    if (!w) {
        naglowek_->SetLabel(TR("Wybierz pozycję z listy"));
        szczegoly_->SetLabel(wxEmptyString);
        return;
    }
    auto pelna = katalog_.pobierzPozycje(w->id);
    const PozycjaKatalogu& k = pelna ? *pelna : *w;
    naglowek_->SetLabel(TekstZUTF8(k.symbol) + (k.producent.empty() ? wxString() : " (" + TekstZUTF8(k.producent) + ")"));
    wxString t;
    if (!k.opis.empty()) t += TekstZUTF8(k.opis) + "\n";
    if (!k.obudowa.empty()) t += TR("Obudowa: ") + TekstZUTF8(k.obudowa) + "\n";
    if (!k.rodzina.empty()) t += TR("Rodzina: ") + TekstZUTF8(k.rodzina) + "\n";
    t += k.wMagazynie > 0 ? wxString::Format(TR("W magazynie: %d szt. (%d poz.)"), k.wMagazynie, k.liczbaElementow)
                          : TR("Nie ma w magazynie.");
    t += "\n";
    if (WyszukiwanieDatasheet::czyPobrany(KatalogDokumentow(), k.symbol)) t += TR("Datasheet: pobrany\n");
    else if (!k.datasheetUrl.empty()) t += TR("Datasheet: ") + TekstZUTF8(k.datasheetUrl) + "\n";
    if (!k.uwagi.empty()) t += TR("Uwagi: ") + TekstZUTF8(k.uwagi);
    szczegoly_->SetLabel(t);
    for (size_t i = 0; i < k.parametry.size(); ++i) {
        const long r = parametry_->InsertItem(static_cast<long>(i), TekstZUTF8(k.parametry[i].nazwa));
        parametry_->SetItem(r, 1, TekstZUTF8(k.parametry[i].wartosc));
        parametry_->SetItem(r, 2, TekstZUTF8(k.parametry[i].zrodlo));
    }
    szczegoly_->GetParent()->Layout();
}

void PanelKatalogu::Nowa() {
    if (rodziny_.empty()) rodziny_ = katalog_.rodziny();
    DialogKatalogu dlg(this, PozycjaKatalogu{}, rodziny_);
    if (dlg.ShowModal() != wxID_OK) return;
    const PozycjaKatalogu k = dlg.Pobierz();
    const auto id = katalog_.zapisz(k);
    if (!id) { Blad(TekstZUTF8(katalog_.pobierzBlad())); return; }
    Komunikat(TR("✓ Dodano do katalogu: ") + TekstZUTF8(k.symbol));
    Odswiez();
    Wybierz(*id);
}

void PanelKatalogu::Edytuj() {
    const PozycjaKatalogu* w = Wybrana();
    if (!w) return;
    auto pelna = katalog_.pobierzPozycje(w->id);
    if (!pelna) return;
    DialogKatalogu dlg(this, *pelna, katalog_.rodziny());
    if (dlg.ShowModal() != wxID_OK) return;
    PozycjaKatalogu k = dlg.Pobierz();
    k.id = pelna->id;
    const auto id = katalog_.zapisz(k);
    if (!id) { Blad(TekstZUTF8(katalog_.pobierzBlad())); return; }
    Komunikat(TR("✓ Zapisano: ") + TekstZUTF8(k.symbol));
    Odswiez();
    Wybierz(*id);
}

void PanelKatalogu::Usun() {
    const PozycjaKatalogu* w = Wybrana();
    if (!w) return;
    const wxString nazwa = TekstZUTF8(w->symbol);
    if (wxMessageBox(TR("Usunąć z katalogu \"") + nazwa + TR("\"? Elementy w magazynie zostaną nietknięte."),
                     TR("Katalog"), wxYES_NO | wxICON_QUESTION, this) != wxYES) return;
    if (!katalog_.usun(w->id)) { Blad(TekstZUTF8(katalog_.pobierzBlad())); return; }
    Komunikat(TR("Usunięto z katalogu: ") + nazwa);
    Odswiez();
}

void PanelKatalogu::DodajDoMagazynu() {
    const PozycjaKatalogu* w = Wybrana();
    if (!w) return;
    const long long id = w->id;
    const wxString symbol = TekstZUTF8(w->symbol);
    DialogDoMagazynu dlg(this, magazyn_, symbol);
    if (dlg.ShowModal() != wxID_OK) return;
    const auto nowy = katalog_.dodajDoMagazynu(magazyn_, id, dlg.Ilosc(), dlg.Miejsce(), dlg.WybranaKategoria());
    if (!nowy) { Blad(TekstZUTF8(katalog_.pobierzBlad())); return; }
    Komunikat(TR("✓ Dodano do magazynu: ") + symbol);
    if (naZmianaMagazynu) naZmianaMagazynu();
    Odswiez();
    Wybierz(id);
}

int PanelKatalogu::ImportujCSV(const wxString& sciezkaWe, TrybKatalogu tryb) {
    wxString sciezka = sciezkaWe;
    if (sciezka.empty()) {
        wxFileDialog dlg(this, TR("Importuj katalog z CSV"), wxEmptyString, wxEmptyString,
                         TR("Pliki CSV (*.csv;*.txt)|*.csv;*.txt|Wszystkie pliki|*.*"), wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (dlg.ShowModal() != wxID_OK) return -1;
        sciezka = dlg.GetPath();
        wxMessageDialog pytanie(this, TR("Co zrobić z pozycjami, które już są w katalogu (ten sam symbol i producent)?"),
                                TR("Import katalogu"), wxYES_NO | wxCANCEL | wxICON_QUESTION);
        pytanie.SetYesNoCancelLabels(TR("Zaktualizuj"), TR("Pomiń"), TR("Anuluj"));
        const int o = pytanie.ShowModal();
        if (o == wxID_CANCEL) return -1;
        tryb = o == wxID_YES ? TrybKatalogu::Aktualizuj : TrybKatalogu::Pomin;
    }
    std::string tekst;
    if (!WczytajTekst(sciezka, tekst)) { Blad(TR("Nie można odczytać pliku.")); return -1; }
    const auto wczytane = KatalogCSV::parsuj(tekst);
    if (!wczytane.blad.empty()) { Blad(TekstZUTF8(wczytane.blad)); return -1; }
    WynikImportuKatalogu w;
    if (!katalog_.importuj(wczytane.pozycje, tryb, w)) { Blad(TekstZUTF8(katalog_.pobierzBlad())); return -1; }
    wxString msg = wxString::Format(TR("Import katalogu: dodano %d, zaktualizowano %d, pominięto %d"), w.dodano, w.zaktualizowano, w.pominieto);
    if (!w.bledy.empty()) msg += wxString::Format(TR(", błędów: %d"), static_cast<int>(w.bledy.size()));
    Komunikat(msg);
    for (size_t i = 0; i < w.bledy.size() && i < 10; ++i) Komunikat("  " + TekstZUTF8(w.bledy[i]));
    Odswiez();
    return w.dodano + w.zaktualizowano;
}

bool PanelKatalogu::EksportujCSV(const wxString& sciezkaWe) {
    wxString sciezka = sciezkaWe;
    if (sciezka.empty()) {
        wxFileDialog dlg(this, TR("Eksport katalogu do CSV"), wxEmptyString, "katalog_" + wxDateTime::Now().Format("%Y%m%d") + ".csv",
                         TR("Pliki CSV (*.csv)|*.csv"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (dlg.ShowModal() != wxID_OK) return false;
        sciezka = dlg.GetPath();
    }
    FiltrKatalogu f;
    f.limit = 1000000;
    std::vector<PozycjaKatalogu> wszystkie;
    if (!katalog_.pobierz(f, wszystkie)) { Blad(TekstZUTF8(katalog_.pobierzBlad())); return false; }
    for (PozycjaKatalogu& p : wszystkie)
        if (auto pelna = katalog_.pobierzPozycje(p.id)) p.parametry = pelna->parametry;
    if (!ZapiszTekst(sciezka, KatalogCSV::naCSV(wszystkie))) { Blad(TR("Nie udało się zapisać pliku.")); return false; }
    Komunikat(wxString::Format(TR("✓ Wyeksportowano %d pozycji katalogu."), static_cast<int>(wszystkie.size())));
    return true;
}

bool PanelKatalogu::ZapiszSzablon(const wxString& sciezkaWe) {
    wxString sciezka = sciezkaWe;
    if (sciezka.empty()) {
        wxFileDialog dlg(this, TR("Zapisz szablon katalogu"), wxEmptyString, "katalog_szablon.csv",
                         TR("Pliki CSV (*.csv)|*.csv"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (dlg.ShowModal() != wxID_OK) return false;
        sciezka = dlg.GetPath();
    }
    if (!ZapiszTekst(sciezka, KatalogCSV::szablon())) { Blad(TR("Nie udało się zapisać pliku.")); return false; }
    Komunikat(TR("✓ Zapisano szablon katalogu."));
    return true;
}

void PanelKatalogu::OtworzDatasheet() {
    const PozycjaKatalogu* w = Wybrana();
    if (!w) return;
    wxString plik = WyszukiwanieDatasheet::sciezkaPliku(KatalogDokumentow(), w->symbol);
    if (WyszukiwanieDatasheet::czyPobrany(KatalogDokumentow(), w->symbol)) {
        if (!WyszukiwanieDatasheet::otworz(plik))
            wxMessageBox(TR("Nie znaleziono programu do otwierania plików PDF."), TR("Datasheet"), wxOK | wxICON_WARNING, this);
    } else if (!w->datasheetUrl.empty()) {
        wxLaunchDefaultBrowser(TekstZUTF8(w->datasheetUrl));
    } else {
        wxMessageBox(TR("Ta pozycja nie ma datasheetu. Wpisz adres w edycji pozycji albo użyj \"Pobierz datasheet\"."),
                     TR("Datasheet"), wxOK | wxICON_INFORMATION, this);
    }
}

void PanelKatalogu::PobierzDatasheet() {
    const PozycjaKatalogu* w = Wybrana();
    if (!w) return;
    wxTextEntryDialog dlg(this, TR("Adres pliku PDF z datasheetem:"), TR("Pobierz datasheet"), TekstZUTF8(w->datasheetUrl));
    if (dlg.ShowModal() != wxID_OK) return;
    const std::string url = dlg.GetValue().Strip(wxString::both).utf8_string();
    if (url.empty()) return;
    const long long id = w->id;
    const std::string symbol = w->symbol;
    Status(TR("Pobieranie datasheetu..."));
    auto zywy = std::make_shared<bool>(true);
    WyszukiwanieDatasheet::pobierz(url, KatalogDokumentow(), symbol,
        [this, id, url, symbol](bool ok, const std::string& blad, const wxString& sciezka) {
            if (ok) {
                if (auto p = katalog_.pobierzPozycje(id)) {
                    p->datasheetUrl = url;
                    katalog_.zapisz(*p);
                }
                katalog_.ustawDatasheetPlik(id, sciezka.utf8_string());
                Komunikat(TR("✓ Pobrano datasheet: ") + TekstZUTF8(symbol));
            } else {
                Komunikat(TR("✗ Nie udało się pobrać datasheetu: ") + TekstZUTF8(blad));
            }
            Status(wxEmptyString);
            Odswiez();
        });
}

int PanelKatalogu::OdczytajDatasheet(const wxString& sciezkaWe, bool bezPytania) {
    const PozycjaKatalogu* w = Wybrana();
    if (!w) return -1;
    const long long id = w->id;
    wxString sciezka = sciezkaWe;
    if (sciezka.empty()) {
        if (WyszukiwanieDatasheet::czyPobrany(KatalogDokumentow(), w->symbol)) {
            sciezka = WyszukiwanieDatasheet::sciezkaPliku(KatalogDokumentow(), w->symbol);
        } else {
            wxFileDialog dlg(this, TR("Wybierz plik PDF z datasheetem"), KatalogDokumentow(), wxEmptyString,
                             TR("Pliki PDF (*.pdf)|*.pdf"), wxFD_OPEN | wxFD_FILE_MUST_EXIST);
            if (dlg.ShowModal() != wxID_OK) return -1;
            sciezka = dlg.GetPath();
        }
    }
    std::string blad;
    const std::string tekst = OdczytDatasheetu::tekstZPDF(sciezka.utf8_string(), blad);
    if (tekst.empty()) {
        if (bezPytania) { Komunikat(TR("✗ Odczyt datasheetu: ") + TekstZUTF8(blad)); return -1; }
        Blad(TR("Nie udało się odczytać tekstu z PDF: ") + TekstZUTF8(blad));
        return -1;
    }
    const OdczytDatasheetu::Wynik wynik = OdczytDatasheetu::wyciagnij(tekst);
    if (wynik.parametry.empty() && wynik.opis.empty() && wynik.obudowa.empty() && wynik.uwagi.empty()) {
        if (!bezPytania)
            wxMessageBox(TR("Nie znalazłem w tym pliku żadnych parametrów, które program umie rozpoznać."),
                         TR("Datasheet"), wxOK | wxICON_INFORMATION, this);
        return 0;
    }
    auto pelna = katalog_.pobierzPozycje(id);
    if (!pelna) return -1;
    std::vector<ParametrKatalogu> wybrane;
    bool opis = false, obudowa = false, uwagi = false;
    if (bezPytania) {
        for (const auto& p : wynik.parametry) if (p.pewny) wybrane.push_back({p.nazwa, p.wartosc, "datasheet"});
        uwagi = pelna->uwagi.empty() && !wynik.uwagi.empty();
        opis = pelna->opis.empty() && !wynik.opis.empty();
        obudowa = pelna->obudowa.empty() && !wynik.obudowa.empty();
    } else {
        DialogPropozycji dlg(this, wynik, *pelna);
        if (dlg.ShowModal() != wxID_OK) return -1;
        wybrane = dlg.Wybrane();
        opis = dlg.UstawOpis();
        obudowa = dlg.UstawObudowe();
        uwagi = dlg.DopiszUwagi();
    }
    if (opis) pelna->opis = wynik.opis;
    if (obudowa) pelna->obudowa = wynik.obudowa;
    if (uwagi) pelna->uwagi = pelna->uwagi.empty() ? wynik.uwagi : pelna->uwagi + "\n" + wynik.uwagi;
    if (opis || obudowa || uwagi) {
        // zapisz tylko pola główne, parametry dopisze dodajParametry (nie kasując ręcznych)
        if (!katalog_.zapisz(*pelna)) { Blad(TekstZUTF8(katalog_.pobierzBlad())); return -1; }
    }
    if (!wybrane.empty() && !katalog_.dodajParametry(id, wybrane, true)) {
        Blad(TekstZUTF8(katalog_.pobierzBlad()));
        return -1;
    }
    Komunikat(wxString::Format(TR("✓ Z datasheetu zapisano parametrów: %d"), static_cast<int>(wybrane.size())));
    Odswiez();
    Wybierz(id);
    return static_cast<int>(wybrane.size());
}
