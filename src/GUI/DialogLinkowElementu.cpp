#include "DialogLinkowElementu.h"

#include "../Narzedzia/Konfiguracja.h"
#include "../Narzedzia/Liczby.h"
#include "../Narzedzia/Tlumaczenia.h"
#include "../Wyszukiwanie/WyszukiwanieDatasheet.h"
#include "../Pomoc/PomocGUI.h"

#include <wx/choicdlg.h>
#include <wx/spinctrl.h>
#include <wx/textdlg.h>
#include <wx/utils.h>

#include <set>

// ===========================================================================
// DialogLinku
// ===========================================================================

DialogLinku::DialogLinku(wxWindow* parent, LogikaSklepow& logika, const std::vector<Sklep>& sklepy, const LinkSklepu& link,
                         const std::string& fraza)
    : wxDialog(parent, wxID_ANY, link.id ? TR("Zmień link do sklepu") : TR("Nowy link do sklepu"), wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      logika_(logika), sklepy_(sklepy), link_(link), fraza_(fraza) {
    wxBoxSizer* glowny = new wxBoxSizer(wxVERTICAL);
    wxFlexGridSizer* siatka = new wxFlexGridSizer(2, 6, 8);
    siatka->AddGrowableCol(1);
    auto dodaj = [&](const wxString& e, wxWindow* p) {
        siatka->Add(new wxStaticText(this, wxID_ANY, e), 0, wxALIGN_CENTER_VERTICAL);
        siatka->Add(p, 1, wxEXPAND);
    };
    chSklep = new wxChoice(this, wxID_ANY);
    int wybor = 0;
    for (size_t i = 0; i < sklepy_.size(); ++i) {
        chSklep->Append(TekstZUTF8(sklepy_[i].nazwa));
        if (sklepy_[i].id == link.idSklepu) wybor = static_cast<int>(i);
    }
    if (!sklepy_.empty()) chSklep->SetSelection(wybor);
    tcAdres = new wxTextCtrl(this, wxID_ANY, TekstZUTF8(link.url), wxDefaultPosition, wxSize(460, -1));
    tcKod = new wxTextCtrl(this, wxID_ANY, TekstZUTF8(link.kodWSklepie));
    tcCena = new wxTextCtrl(this, wxID_ANY, link.cena ? TekstZUTF8(liczbaNaTekstPolski(*link.cena)) : wxString());
    tcWaluta = new wxTextCtrl(this, wxID_ANY, TekstZUTF8(link.waluta));
    spMin = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(90, -1), wxSP_ARROW_KEYS, 1, 1000000, std::max(1, link.minimalnaIlosc));
    cbDomyslny = new wxCheckBox(this, wxID_ANY, TR("Domyślny sklep tego elementu"));
    cbDomyslny->SetValue(link.domyslny);
    tcUwagi = new wxTextCtrl(this, wxID_ANY, TekstZUTF8(link.uwagi));

    dodaj(TR("Sklep *"), chSklep);
    wxBoxSizer* adres = new wxBoxSizer(wxHORIZONTAL);
    adres->Add(tcAdres, 1, wxEXPAND | wxRIGHT, 6);
    wxButton* bSzukaj = new wxButton(this, wxID_ANY, TR("Szukaj w sklepie"));
    adres->Add(bSzukaj, 0);
    siatka->Add(new wxStaticText(this, wxID_ANY, TR("Adres strony elementu *")), 0, wxALIGN_CENTER_VERTICAL);
    siatka->Add(adres, 1, wxEXPAND);
    dodaj(TR("Kod w sklepie"), tcKod);
    dodaj(TR("Cena za sztukę"), tcCena);
    dodaj(TR("Waluta (puste = sklepu)"), tcWaluta);
    dodaj(TR("Minimalna ilość"), spMin);
    siatka->AddSpacer(0);
    siatka->Add(cbDomyslny);
    dodaj(TR("Uwagi"), tcUwagi);
    PomocGUI::Dodaj(tcAdres, "link.url");
    PomocGUI::Dodaj(tcKod, "link.kod");
    PomocGUI::Dodaj(spMin, "link.minimum");
    glowny->Add(siatka, 1, wxEXPAND | wxALL, 12);
    wxStaticText* pomoc = new wxStaticText(
        this, wxID_ANY, TR("Wskazówka: \"Szukaj w sklepie\" otwiera wyniki wyszukiwania w przeglądarce. Wklej tu adres strony samego produktu -\n"
                           "wtedy program potrafi sprawdzić cenę. Ceny można też wpisać ręcznie."));
    pomoc->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
    glowny->Add(pomoc, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);
    glowny->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 10);
    SetSizerAndFit(glowny);

    bSzukaj->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        const int i = chSklep->GetSelection();
        if (i < 0) return;
        const std::string adresW = LogikaSklepow::adresWyszukiwania(sklepy_[static_cast<size_t>(i)], fraza_);
        if (adresW.empty()) {
            wxMessageBox(TR("Ten sklep nie ma ustawionego adresu wyszukiwania (okno Sklepy)."), TR("Link"), wxOK | wxICON_INFORMATION, this);
            return;
        }
        wxLaunchDefaultBrowser(wxString::FromUTF8(adresW));
    });
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { if (Zapisz()) EndModal(wxID_OK); }, wxID_OK);
}

LinkSklepu DialogLinku::Pobierz() const {
    LinkSklepu l = link_;
    const int i = chSklep->GetSelection();
    if (i >= 0) l.idSklepu = sklepy_[static_cast<size_t>(i)].id;
    l.url = tcAdres->GetValue().Strip(wxString::both).utf8_string();
    l.kodWSklepie = tcKod->GetValue().Strip(wxString::both).utf8_string();
    l.waluta = tcWaluta->GetValue().Strip(wxString::both).Upper().utf8_string();
    l.minimalnaIlosc = spMin->GetValue();
    l.domyslny = cbDomyslny->GetValue();
    l.uwagi = tcUwagi->GetValue().Strip(wxString::both).utf8_string();
    l.cena.reset();
    double v = 0;
    if (tekstNaLiczbe(tcCena->GetValue().utf8_string(), v)) l.cena = v;
    return l;
}

bool DialogLinku::Zapisz() {
    const wxString cena = tcCena->GetValue().Strip(wxString::both);
    double v = 0;
    if (!cena.empty() && !tekstNaLiczbe(cena.utf8_string(), v)) {
        wxMessageBox(TR("Cena musi być liczbą albo pusta."), TR("Link"), wxOK | wxICON_WARNING, this);
        return false;
    }
    auto id = logika_.zapiszLink(Pobierz());
    if (!id) {
        wxMessageBox(TekstZUTF8(logika_.pobierzBlad()), TR("Link"), wxOK | wxICON_WARNING, this);
        return false;
    }
    idZapisanego_ = *id;
    return true;
}

// ===========================================================================
// DialogLinkowElementu
// ===========================================================================

DialogLinkowElementu::DialogLinkowElementu(wxWindow* parent, LogikaSklepow& sklepy, LogikaMagazynu& magazyn, long long idElementu)
    : wxDialog(parent, wxID_ANY, TR("Sklepy i ceny elementu"), wxDefaultPosition, wxSize(1100, 600), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      sklepy_logika_(sklepy), magazyn_(magazyn), idElementu_(idElementu), spr_(sklepy) {
    if (auto e = magazyn_.pobierzElement(idElementu)) element_ = *e;
    SetTitle(TR("Sklepy i ceny: ") + TekstZUTF8(element_.symbol));

    wxBoxSizer* glowny = new wxBoxSizer(wxVERTICAL);
    lista = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL | wxBORDER_THEME);
    lista->AppendColumn(TR("Sklep"), wxLIST_FORMAT_LEFT, 130);
    lista->AppendColumn(TR("Cena"), wxLIST_FORMAT_RIGHT, 80);
    lista->AppendColumn(TR("Waluta"), wxLIST_FORMAT_LEFT, 55);
    lista->AppendColumn(TR("Sprawdzono"), wxLIST_FORMAT_LEFT, 120);
    lista->AppendColumn(TR("Dostępność"), wxLIST_FORMAT_LEFT, 110);
    lista->AppendColumn(TR("Min."), wxLIST_FORMAT_RIGHT, 45);
    lista->AppendColumn(TR("Dom."), wxLIST_FORMAT_LEFT, 40);
    lista->AppendColumn(TR("Adres"), wxLIST_FORMAT_LEFT, 480);
    glowny->Add(lista, 1, wxEXPAND | wxALL, 10);
    PomocGUI::DymekZKolumn(lista);

    wxBoxSizer* rzad1 = new wxBoxSizer(wxHORIZONTAL);
    wxButton* bDodaj = new wxButton(this, wxID_ANY, TR("Dodaj..."));
    wxButton* bEdytuj = new wxButton(this, wxID_ANY, TR("Zmień..."));
    wxButton* bUsun = new wxButton(this, wxID_ANY, TR("Usuń"));
    wxButton* bOtworz = new wxButton(this, wxID_ANY, TR("Otwórz w przeglądarce"));
    wxButton* bSzukaj = new wxButton(this, wxID_ANY, TR("Szukaj w sklepie..."));
    wxButton* bHist = new wxButton(this, wxID_ANY, TR("Historia cen..."));
    for (wxButton* b : {bDodaj, bEdytuj, bUsun, bOtworz, bSzukaj, bHist}) rzad1->Add(b, 0, wxRIGHT, 6);
    rzad1->AddStretchSpacer();
    wxButton* bWybrany = new wxButton(this, wxID_ANY, TR("Sprawdź zaznaczony"));
    bSprawdz = new wxButton(this, wxID_ANY, TR("Sprawdź ceny wszystkich"));
    rzad1->Add(bWybrany, 0, wxRIGHT, 6);
    rzad1->Add(bSprawdz, 0);
    glowny->Add(rzad1, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);
    lbStatus = new wxStaticText(this, wxID_ANY, TR("Ceny są sprawdzane tylko na Twoje polecenie, po jednej stronie naraz."));
    glowny->Add(lbStatus, 0, wxALL, 10);

    wxStaticBoxSizer* ds = new wxStaticBoxSizer(wxHORIZONTAL, this, TR("Karta katalogowa (datasheet)"));
    lbDatasheet = new wxStaticText(ds->GetStaticBox(), wxID_ANY, wxEmptyString);
    ds->Add(lbDatasheet, 1, wxALIGN_CENTER_VERTICAL | wxALL, 6);
    wxButton* bDsOtworz = new wxButton(ds->GetStaticBox(), wxID_ANY, TR("Otwórz"));
    wxButton* bDsPobierz = new wxButton(ds->GetStaticBox(), wxID_ANY, TR("Pobierz z adresu..."));
    wxButton* bDsSklepy = new wxButton(ds->GetStaticBox(), wxID_ANY, TR("Znajdź na stronach sklepów"));
    wxButton* bDsNet = new wxButton(ds->GetStaticBox(), wxID_ANY, TR("Szukaj w internecie..."));
    for (wxButton* b : {bDsOtworz, bDsPobierz, bDsSklepy, bDsNet}) ds->Add(b, 0, wxALL, 3);
    glowny->Add(ds, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);

    wxBoxSizer* dol = new wxBoxSizer(wxHORIZONTAL);
    dol->AddStretchSpacer();
    dol->Add(new wxButton(this, wxID_CANCEL, TR("Zamknij")));
    glowny->Add(dol, 0, wxEXPAND | wxALL, 10);
    SetSizer(glowny);
    SetEscapeId(wxID_CANCEL);

    bDodaj->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Dodaj(); });
    bEdytuj->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Edytuj(); });
    bUsun->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Usun(); });
    bOtworz->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Otworz(); });
    bSzukaj->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { SzukajWSklepie(); });
    bHist->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Historia(); });
    bWybrany->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { SprawdzWybrany(); });
    bSprawdz->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (spr_.wToku()) { spr_.Anuluj(); return; }
        SprawdzCeny();
    });
    bDsOtworz->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OtworzDatasheet(); });
    bDsPobierz->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { PobierzDatasheetZAdresu(); });
    bDsSklepy->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ZnajdzDatasheetNaStronach(); });
    bDsNet->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { SzukajDatasheetuWInternecie(); });
    for (wxButton* b : {bEdytuj, bUsun, bOtworz, bHist, bWybrany})
        b->Bind(wxEVT_UPDATE_UI, [this](wxUpdateUIEvent& e) { e.Enable(Wybrany() != nullptr && !spr_.wToku()); });
    bSzukaj->Bind(wxEVT_UPDATE_UI, [this](wxUpdateUIEvent& e) { e.Enable(!sklepy_.empty()); });
    lista->Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](wxListEvent&) { Edytuj(); });
    Odswiez();
}

DialogLinkowElementu::~DialogLinkowElementu() { *zywy_ = false; }

const LinkSklepu* DialogLinkowElementu::Wybrany() const {
    const long i = lista->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (i < 0 || static_cast<size_t>(i) >= linki_.size()) return nullptr;
    return &linki_[static_cast<size_t>(i)];
}

void DialogLinkowElementu::WybierzLink(long long id) {
    for (size_t i = 0; i < linki_.size(); ++i)
        if (linki_[i].id == id) lista->SetItemState(static_cast<long>(i), wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
}

void DialogLinkowElementu::Status(const wxString& t) { lbStatus->SetLabel(t); }

std::string DialogLinkowElementu::Fraza() const {
    return element_.symbolProducenta.empty() ? element_.symbol : element_.symbolProducenta;
}

wxString DialogLinkowElementu::KatalogDokumentow() const {
    return TekstZUTF8(Konfiguracja::pobierz().pobierzUstawieniaAplikacji().sciezkaPDFDatasheet);
}

void DialogLinkowElementu::Odswiez() {
    const long wybrany = Wybrany() ? Wybrany()->id : 0;
    lista->DeleteAllItems();
    sklepy_logika_.pobierzSklepy(true, sklepy_);
    if (!sklepy_logika_.pobierzLinki(idElementu_, linki_)) {
        wxMessageBox(TekstZUTF8(sklepy_logika_.pobierzBlad()), TR("Sklepy i ceny"), wxOK | wxICON_ERROR, this);
        return;
    }
    for (size_t i = 0; i < linki_.size(); ++i) {
        const LinkSklepu& l = linki_[i];
        const long w = lista->InsertItem(static_cast<long>(i), TekstZUTF8(l.nazwaSklepu));
        lista->SetItem(w, 1, l.cena ? TekstZUTF8(liczbaNaTekstPolski(*l.cena)) : wxString("-"));
        lista->SetItem(w, 2, TekstZUTF8(l.waluta));
        lista->SetItem(w, 3, TekstZUTF8(l.ostatniaWeryfikacja));
        lista->SetItem(w, 4, TekstZUTF8(l.status));
        lista->SetItem(w, 5, wxString::Format("%d", l.minimalnaIlosc));
        lista->SetItem(w, 6, l.domyslny ? wxString::FromUTF8("★") : wxString());
        lista->SetItem(w, 7, TekstZUTF8(l.url));
    }
    if (wybrany) WybierzLink(wybrany);
    OdswiezDatasheet();
}

void DialogLinkowElementu::Dodaj() {
    if (sklepy_.empty()) {
        wxMessageBox(TR("Najpierw dodaj sklepy (menu Sklepy → Lista sklepów)."), TR("Sklepy i ceny"), wxOK | wxICON_INFORMATION, this);
        return;
    }
    LinkSklepu l;
    l.idElementu = idElementu_;
    // pierwszy sklep, którego element jeszcze nie ma
    for (const Sklep& s : sklepy_) {
        bool jest = false;
        for (const LinkSklepu& x : linki_) jest = jest || x.idSklepu == s.id;
        if (!jest) { l.idSklepu = s.id; break; }
    }
    DialogLinku dlg(this, sklepy_logika_, sklepy_, l, Fraza());
    if (dlg.ShowModal() == wxID_OK) { Odswiez(); WybierzLink(dlg.IdZapisanego()); }
}

void DialogLinkowElementu::Edytuj() {
    const LinkSklepu* l = Wybrany();
    if (!l) return;
    DialogLinku dlg(this, sklepy_logika_, sklepy_, *l, Fraza());
    if (dlg.ShowModal() == wxID_OK) { Odswiez(); WybierzLink(dlg.IdZapisanego()); }
}

void DialogLinkowElementu::Usun() {
    const LinkSklepu* l = Wybrany();
    if (!l) return;
    if (wxMessageBox(wxString::Format(TR("Usunąć link do sklepu %s razem z historią cen?"), TekstZUTF8(l->nazwaSklepu)), TR("Sklepy i ceny"),
                     wxYES_NO | wxICON_QUESTION, this) != wxYES) return;
    if (!sklepy_logika_.usunLink(l->id)) {
        wxMessageBox(TekstZUTF8(sklepy_logika_.pobierzBlad()), TR("Sklepy i ceny"), wxOK | wxICON_ERROR, this);
        return;
    }
    Odswiez();
}

void DialogLinkowElementu::Otworz() {
    if (const LinkSklepu* l = Wybrany()) wxLaunchDefaultBrowser(TekstZUTF8(l->url));
}

void DialogLinkowElementu::SzukajWSklepie() {
    wxArrayString nazwy;
    for (const Sklep& s : sklepy_) nazwy.Add(TekstZUTF8(s.nazwa));
    int wybor = 0;
    if (const LinkSklepu* l = Wybrany())
        for (size_t i = 0; i < sklepy_.size(); ++i) if (sklepy_[i].id == l->idSklepu) wybor = static_cast<int>(i);
    wxSingleChoiceDialog dlg(this, TR("W którym sklepie szukać \"") + TekstZUTF8(Fraza()) + "\"?", TR("Szukaj w sklepie"), nazwy);
    dlg.SetSelection(wybor);
    if (dlg.ShowModal() != wxID_OK) return;
    const std::string adres = LogikaSklepow::adresWyszukiwania(sklepy_[static_cast<size_t>(dlg.GetSelection())], Fraza());
    if (adres.empty()) {
        wxMessageBox(TR("Ten sklep nie ma ustawionego adresu wyszukiwania (okno Sklepy)."), TR("Sklepy i ceny"), wxOK | wxICON_INFORMATION, this);
        return;
    }
    wxLaunchDefaultBrowser(TekstZUTF8(adres));
}

void DialogLinkowElementu::Historia() {
    const LinkSklepu* l = Wybrany();
    if (!l) return;
    std::vector<WpisHistoriiCeny> h;
    if (!sklepy_logika_.pobierzHistorie(l->id, h)) {
        wxMessageBox(TekstZUTF8(sklepy_logika_.pobierzBlad()), TR("Historia cen"), wxOK | wxICON_ERROR, this);
        return;
    }
    wxString t;
    for (const WpisHistoriiCeny& w : h)
        t += TekstZUTF8(w.data) + "   " + TekstZUTF8(liczbaNaTekstPolski(w.cena)) + " " + TekstZUTF8(w.waluta) + "   (" + TekstZUTF8(w.zrodlo) + ")\n";
    if (t.empty()) t = TR("Brak zapisanych cen.");
    wxMessageBox(t, TR("Historia cen: ") + TekstZUTF8(l->nazwaSklepu), wxOK | wxICON_INFORMATION, this);
}

bool DialogLinkowElementu::SprawdzCeny(std::function<void(bool)> koniec) {
    std::vector<LinkSklepu> doSprawdzenia;
    for (const LinkSklepu& l : linki_) if (!l.url.empty()) doSprawdzenia.push_back(l);
    const bool start = spr_.Start(SprawdzaczCen::ZadaniaDlaLinkow(doSprawdzenia, sklepy_, element_.symbol),
        [this, zywy = zywy_](int n, int r, const WynikSprawdzenia& w) {
            if (!*zywy) return;
            Status(wxString::Format(TR("Sprawdzono %d z %d: %s - %s"), n, r, TekstZUTF8(w.zadanie.opis), TekstZUTF8(w.komunikat)));
        },
        [this, zywy = zywy_, koniec](bool anul) {
            if (!*zywy) return;
            bSprawdz->SetLabel(TR("Sprawdź ceny wszystkich"));
            Odswiez();
            Status(anul ? TR("Przerwano sprawdzanie cen.") : TR("Sprawdzanie cen zakończone."));
            if (koniec) koniec(anul);
        });
    if (start) bSprawdz->SetLabel(TR("Przerwij sprawdzanie"));
    return start;
}

void DialogLinkowElementu::SprawdzWybrany() {
    const LinkSklepu* l = Wybrany();
    if (!l || spr_.wToku()) return;
    spr_.Start(SprawdzaczCen::ZadaniaDlaLinkow({*l}, sklepy_, element_.symbol),
        [this, zywy = zywy_](int, int, const WynikSprawdzenia& w) { if (*zywy) Status(TekstZUTF8(w.zadanie.opis + ": " + w.komunikat)); },
        [this, zywy = zywy_](bool) { if (*zywy) Odswiez(); }, 0);
}

// ---- datasheet ----

wxString DialogLinkowElementu::StatusDatasheetu() const {
    const bool plik = WyszukiwanieDatasheet::czyPobrany(KatalogDokumentow(), element_.symbol);
    if (plik) return TR("Pobrany: ") + WyszukiwanieDatasheet::sciezkaPliku(KatalogDokumentow(), element_.symbol);
    if (!element_.datasheetUrl.empty()) return TR("Nie pobrany. Adres: ") + TekstZUTF8(element_.datasheetUrl);
    return TR("Brak datasheetu.");
}

void DialogLinkowElementu::OdswiezDatasheet() {
    if (auto e = magazyn_.pobierzElement(idElementu_)) element_ = *e;
    lbDatasheet->SetLabel(StatusDatasheetu());
    Layout();
}

void DialogLinkowElementu::OtworzDatasheet() {
    if (WyszukiwanieDatasheet::czyPobrany(KatalogDokumentow(), element_.symbol)) {
        if (!WyszukiwanieDatasheet::otworz(WyszukiwanieDatasheet::sciezkaPliku(KatalogDokumentow(), element_.symbol)))
            wxMessageBox(TR("Nie znaleziono programu do otwierania plików PDF."), TR("Datasheet"), wxOK | wxICON_WARNING, this);
    } else if (!element_.datasheetUrl.empty()) {
        wxLaunchDefaultBrowser(TekstZUTF8(element_.datasheetUrl));
    } else {
        wxMessageBox(TR("Ten element nie ma jeszcze datasheetu. Użyj \"Znajdź na stronach sklepów\" albo \"Szukaj w internecie\"."),
                     TR("Datasheet"), wxOK | wxICON_INFORMATION, this);
    }
}

void DialogLinkowElementu::PobierzDatasheet(const std::string& url, std::function<void(bool)> po) {
    Status(TR("Pobieranie datasheetu..."));
    WyszukiwanieDatasheet::pobierz(url, KatalogDokumentow(), element_.symbol,
        [this, zywy = zywy_, url, po](bool ok, const std::string& blad, const wxString&) {
            if (!*zywy) return;
            if (ok) {
                if (auto e = magazyn_.pobierzElement(idElementu_)) {
                    e->datasheetUrl = url;
                    magazyn_.zapiszElement(*e);
                }
                Status(TR("✓ Datasheet pobrany."));
            } else {
                Status(TR("✗ Nie udało się pobrać datasheetu: ") + TekstZUTF8(blad));
            }
            OdswiezDatasheet();
            if (po) po(ok);
        });
}

void DialogLinkowElementu::PobierzDatasheetZAdresu() {
    wxTextEntryDialog dlg(this, TR("Adres pliku PDF z datasheetem:"), TR("Pobierz datasheet"), TekstZUTF8(element_.datasheetUrl));
    if (dlg.ShowModal() != wxID_OK) return;
    const std::string url = dlg.GetValue().Strip(wxString::both).utf8_string();
    if (url.empty()) return;
    PobierzDatasheet(url);
}

void DialogLinkowElementu::SzukajDatasheetuWInternecie() {
    const auto serwisy = WyszukiwanieDatasheet::adresySerwisow(Fraza());
    wxArrayString nazwy;
    for (const auto& s : serwisy) nazwy.Add(TekstZUTF8(s.nazwa));
    wxSingleChoiceDialog dlg(this, TR("Gdzie szukać \"") + TekstZUTF8(Fraza()) + "\"?\nZnaleziony adres PDF wklej potem w \"Pobierz z adresu\".",
                             TR("Szukaj datasheetu"), nazwy);
    if (dlg.ShowModal() != wxID_OK) return;
    wxLaunchDefaultBrowser(TekstZUTF8(serwisy[static_cast<size_t>(dlg.GetSelection())].url));
}

void DialogLinkowElementu::ZnajdzDatasheetNaStronach() {
    // strony kolejnych linków pobieramy jedna po drugiej (maks. 6), zbierając odnośniki do PDF
    struct Stan {
        std::vector<std::string> adresy;
        size_t nr = 0;
        std::vector<LinkDatasheet> znalezione;
        std::set<std::string> widziane;
    };
    auto stan = std::make_shared<Stan>();
    for (const LinkSklepu& l : linki_) if (!l.url.empty() && stan->adresy.size() < 6) stan->adresy.push_back(l.url);
    if (stan->adresy.empty()) {
        wxMessageBox(TR("Dodaj najpierw link do strony elementu w którymś sklepie."), TR("Datasheet"), wxOK | wxICON_INFORMATION, this);
        return;
    }
    auto krok = std::make_shared<std::function<void()>>();
    *krok = [this, stan, krok, zywy = zywy_]() {
        if (!*zywy) return;
        if (stan->nr >= stan->adresy.size()) {
            if (stan->znalezione.empty()) {
                Status(TR("Nie znaleziono odnośników do datasheetu na stronach sklepów."));
                return;
            }
            wxArrayString opisy;
            for (const LinkDatasheet& d : stan->znalezione)
                opisy.Add((d.tekst.empty() ? wxString() : TekstZUTF8(d.tekst) + " - ") + TekstZUTF8(d.url));
            wxSingleChoiceDialog dlg(this, TR("Znalezione odnośniki (najpewniejsze na górze):"), TR("Datasheet"), opisy);
            if (dlg.ShowModal() == wxID_OK) PobierzDatasheet(stan->znalezione[static_cast<size_t>(dlg.GetSelection())].url);
            else Status(wxEmptyString);
            return;
        }
        const std::string adres = stan->adresy[stan->nr++];
        Status(wxString::Format(TR("Szukanie datasheetu (%zu z %zu)..."), stan->nr, stan->adresy.size()));
        WyszukiwanieDatasheet::szukajNaStronie(adres, [stan, krok, zywy](bool ok, const std::string&, std::vector<LinkDatasheet> linki) {
            if (!*zywy) return;
            if (ok)
                for (const LinkDatasheet& d : linki)
                    if (stan->widziane.insert(d.url).second) stan->znalezione.push_back(d);
            std::stable_sort(stan->znalezione.begin(), stan->znalezione.end(),
                             [](const LinkDatasheet& a, const LinkDatasheet& b) { return a.ocena > b.ocena; });
            (*krok)();
        });
    };
    (*krok)();
}
