#include "DialogImportuElementow.h"
#include "../Narzedzia/Liczby.h"
#include "../Logika/ImportCSVElementow.h"
#include "../Logika/SzeregiE.h"
#include "../Narzedzia/Miejsce.h"
#include "../Narzedzia/Tlumaczenia.h"
#include "../Pomoc/PomocGUI.h"

#include <wx/filedlg.h>
#include <wx/sizer.h>
#include <wx/ffile.h>
#include <wx/log.h>

#include <algorithm>

namespace {
enum Kol { K_SYMBOL, K_WARTOSC, K_TOL, K_OBUDOWA, K_ILOSC, K_KATEGORIA, K_MIEJSCE, K_STATUS, K_LICZBA };

std::string Male(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return c < 128 ? std::tolower(c) : c; });
    return s;
}

std::string Tekst(const wxTextCtrl* t) { return t->GetValue().Strip(wxString::both).utf8_string(); }

std::optional<double> Liczba(const std::string& t) {
    if (t.empty()) return std::nullopt;
    try {
        std::string s = t;
        std::replace(s.begin(), s.end(), ',', '.');
        size_t n = 0;
        char* koniec = nullptr;
        const double v = strtodKlasyczny(s.c_str(), &koniec);
        n = static_cast<size_t>(koniec - s.c_str());
        if (n == s.size() && n > 0) return v;
    } catch (...) {}
    return std::nullopt;
}
} // namespace

DialogImportuElementow::DialogImportuElementow(wxWindow* parent, LogikaMagazynu& magazyn, Tryb tryb)
    : wxDialog(parent, wxID_ANY,
               tryb == Tryb::Szereg ? TR("Generator szeregów E (rezystory, kondensatory, cewki)")
                                    : TR("Import elementów z pliku CSV"),
               wxDefaultPosition, wxSize(1100, tryb == Tryb::Szereg ? 820 : 640),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      magazyn_(magazyn), tryb_(tryb) {
    for (const std::string& s : magazyn_.pobierzSymbole()) istniejace_.insert(Male(s));

    wxBoxSizer* glowny = new wxBoxSizer(wxVERTICAL);
    if (tryb_ == Tryb::Szereg) BudujSzereg(glowny); else BudujPlik(glowny);

    lbBlad = new wxStaticText(this, wxID_ANY, wxEmptyString);
    lbBlad->SetForegroundColour(wxColour(190, 30, 30));
    glowny->Add(lbBlad, 0, wxLEFT | wxRIGHT | wxTOP, 10);

    lista = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 200),
                           wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_VIRTUAL * 0 | wxBORDER_THEME);
    const struct { const char* t; int s; int f; } kol[K_LICZBA] = {
        {"Symbol", 170, wxLIST_FORMAT_LEFT}, {"Wartość", 80, wxLIST_FORMAT_LEFT}, {"Tol.", 55, wxLIST_FORMAT_LEFT},
        {"Obudowa", 90, wxLIST_FORMAT_LEFT}, {"Ilość", 55, wxLIST_FORMAT_RIGHT},
        {"Kategoria", 230, wxLIST_FORMAT_LEFT}, {"Miejsce", 140, wxLIST_FORMAT_LEFT},
        {"Status", 200, wxLIST_FORMAT_LEFT}};
    for (int i = 0; i < K_LICZBA; ++i) lista->InsertColumn(i, TR(kol[i].t), kol[i].f, kol[i].s);
    glowny->Add(lista, 1, wxEXPAND | wxALL, 10);

    wxBoxSizer* dol = new wxBoxSizer(wxHORIZONTAL);
    dol->Add(new wxStaticText(this, wxID_ANY, TR("Gdy symbol już jest w magazynie:")), 0,
             wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    chDuplikaty = new wxChoice(this, wxID_ANY);
    chDuplikaty->Append(TR("pomiń (nic nie zmieniaj)"));
    chDuplikaty->Append(TR("dodaj ilość do istniejącego"));
    chDuplikaty->Append(TR("uzupełnij dane (stan bez zmian)"));
    chDuplikaty->SetSelection(0);
    PomocGUI::Dodaj(chDuplikaty, "import.duplikaty");
    dol->Add(chDuplikaty, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 16);
    lbPodsumowanie = new wxStaticText(this, wxID_ANY, wxEmptyString);
    dol->Add(lbPodsumowanie, 1, wxALIGN_CENTER_VERTICAL);
    glowny->Add(dol, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);

    wxBoxSizer* przyciski = new wxBoxSizer(wxHORIZONTAL);
    if (tryb_ == Tryb::Szereg) {
        wxButton* b = new wxButton(this, wxID_ANY, TR("Zapisz jako CSV..."));
        b->Bind(wxEVT_BUTTON, &DialogImportuElementow::OnZapiszCSV, this);
        przyciski->Add(b, 0, wxRIGHT, 6);
    } else {
        wxButton* b = new wxButton(this, wxID_ANY, TR("Zapisz szablon CSV..."));
        b->Bind(wxEVT_BUTTON, &DialogImportuElementow::OnSzablon, this);
        przyciski->Add(b, 0, wxRIGHT, 6);
    }
    przyciski->AddStretchSpacer();
    wxButton* ok = new wxButton(this, wxID_OK, TR("Dodaj do magazynu"));
    przyciski->Add(ok, 0, wxRIGHT, 6);
    przyciski->Add(new wxButton(this, wxID_CANCEL, TR("Zamknij")), 0);
    glowny->Add(przyciski, 0, wxEXPAND | wxALL, 10);
    SetSizer(glowny);

    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (Zapisz()) EndModal(wxID_OK);
    }, wxID_OK);
    Bind(wxEVT_UPDATE_UI, [this](wxUpdateUIEvent& e) { e.Enable(LiczbaPoprawnych() > 0); }, wxID_OK);
    chDuplikaty->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { Wypelnij(); });

    if (tryb_ == Tryb::Szereg) PrzeliczSzereg(); else Wypelnij();
}

void DialogImportuElementow::BudujSzereg(wxSizer* gora) {
    auto pole = [&](wxFlexGridSizer* g, const wxString& et, wxTextCtrl*& tc, const wxString& wart, const char* klucz,
                    int szer = 110) {
        g->Add(new wxStaticText(this, wxID_ANY, et), 0, wxALIGN_CENTER_VERTICAL);
        tc = new wxTextCtrl(this, wxID_ANY, wart, wxDefaultPosition, wxSize(szer, -1));
        PomocGUI::Dodaj(tc, klucz);
        tc->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { PrzeliczSzereg(); });
        g->Add(tc, 1, wxEXPAND);
    };
    wxFlexGridSizer* g = new wxFlexGridSizer(8, 6, 10);
    g->AddGrowableCol(1); g->AddGrowableCol(3); g->AddGrowableCol(5); g->AddGrowableCol(7);

    g->Add(new wxStaticText(this, wxID_ANY, TR("Rodzaj:")), 0, wxALIGN_CENTER_VERTICAL);
    chRodzaj = new wxChoice(this, wxID_ANY);
    for (const char* t : {"Rezystory", "Kondensatory", "Cewki"}) chRodzaj->Append(TR(t));
    chRodzaj->SetSelection(0);
    PomocGUI::Dodaj(chRodzaj, "szereg.rodzaj");
    chRodzaj->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
        // zakres i obudowa sensowne dla rodzaju
        switch (chRodzaj->GetSelection()) {
            case 0: tcOd->ChangeValue("10"); tcDo->ChangeValue("1M"); break;
            case 1: tcOd->ChangeValue("10p"); tcDo->ChangeValue("10u"); break;
            default: tcOd->ChangeValue("1u"); tcDo->ChangeValue("1m"); break;
        }
        PrzeliczSzereg();
    });
    g->Add(chRodzaj, 1, wxEXPAND);
    g->Add(new wxStaticText(this, wxID_ANY, TR("Szereg:")), 0, wxALIGN_CENTER_VERTICAL);
    chSzereg = new wxChoice(this, wxID_ANY);
    for (const char* t : {"E6", "E12", "E24", "E48", "E96"}) chSzereg->Append(t);
    chSzereg->SetSelection(2);
    PomocGUI::Dodaj(chSzereg, "szereg.szereg");
    chSzereg->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { PrzeliczSzereg(); });
    g->Add(chSzereg, 1, wxEXPAND);
    pole(g, TR("Od:"), tcOd, "10", "szereg.od");
    pole(g, TR("Do:"), tcDo, "1M", "szereg.do");

    pole(g, TR("Tolerancja:"), tcTol, "", "szereg.tolerancja");
    pole(g, TR("Obudowa:"), tcObudowa, "0603", "szereg.obudowa");
    pole(g, TR("Moc:"), tcMoc, "", "szereg.moc");
    pole(g, TR("Napięcie:"), tcNapiecie, "", "szereg.napiecie");
    pole(g, TR("Dielektryk:"), tcDielektryk, "", "szereg.dielektryk");
    pole(g, TR("Producent:"), tcProducent, "", "szereg.producent");
    pole(g, TR("Ilość każdej:"), tcIlosc, "0", "szereg.ilosc");
    pole(g, TR("Minimum:"), tcMinimum, "0", "szereg.minimum");
    pole(g, TR("Cena szt.:"), tcCena, "", "szereg.cena");
    pole(g, TR("Wzór symbolu:"), tcWzor, "", "szereg.wzor");
    pole(g, TR("Kategoria:"), tcKategoria, "", "szereg.kategoria");
    pole(g, TR("Miejsce:"), tcMiejsce, "", "szereg.miejsce");
    gora->Add(g, 0, wxEXPAND | wxALL, 10);

    cbDekada = new wxCheckBox(this, wxID_ANY,
                              TR("Każda dekada wartości do osobnej szuflady (A1, A2, A3, ... licząc od podanej)"));
    PomocGUI::Dodaj(cbDekada, "szereg.dekada");
    cbDekada->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { PrzeliczSzereg(); });
    gora->Add(cbDekada, 0, wxLEFT | wxRIGHT, 10);
    wxStaticText* podp = new wxStaticText(this, wxID_ANY,
        TR("Wartości możesz wpisywać zapisem 4k7, 100n, 2M2, 10 kΩ. Puste pola (tolerancja, wzór, kategoria) dostają wartości domyślne."));
    gora->Add(podp, 0, wxALL, 10);
}

void DialogImportuElementow::BudujPlik(wxSizer* gora) {
    wxBoxSizer* w = new wxBoxSizer(wxHORIZONTAL);
    w->Add(new wxStaticText(this, wxID_ANY, TR("Plik CSV:")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    tcPlik = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    PomocGUI::Dodaj(tcPlik, "csv.plik");
    w->Add(tcPlik, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    wxButton* b = new wxButton(this, wxID_ANY, TR("Wybierz plik..."));
    w->Add(b, 0);
    gora->Add(w, 0, wxEXPAND | wxALL, 10);
    gora->Add(new wxStaticText(this, wxID_ANY,
        TR("Kolumny rozpoznawane po nagłówku: Symbol, Wartość, Tolerancja, Obudowa, Ilość, Minimum, Kategoria, Miejsce, Producent, Cena, Tagi, ... "
           "Plik wzorcowy zapiszesz przyciskiem poniżej.")), 0, wxLEFT | wxRIGHT, 10);
    b->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        wxFileDialog dlg(this, TR("Wybierz plik CSV z elementami"), wxEmptyString, wxEmptyString,
                         TR("Pliki CSV (*.csv;*.txt)|*.csv;*.txt|Wszystkie pliki|*"), wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (dlg.ShowModal() != wxID_OK) return;
        tcPlik->SetValue(dlg.GetPath());
        WczytajPlik(dlg.GetPath());
    });
    tcPlik->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent&) { WczytajPlik(tcPlik->GetValue()); });
}

bool DialogImportuElementow::WczytajPlik(const wxString& sciezka, bool cicho) {
    wxLogNull cisza;   // brak pliku zgłaszamy własnym komunikatem, nie oknem wxLog
    wxFFile f(sciezka, "rb");
    wxString tekstWx;
    std::string tekst;
    if (!f.IsOpened() || !f.ReadAll(&tekstWx, wxConvISO8859_1)) {
        if (!cicho) wxMessageBox(TR("Nie można odczytać pliku: ") + sciezka, TR("Import CSV"), wxOK | wxICON_ERROR, this);
        return false;
    }
    // odczyt bajtowy: ISO8859-1 odwzorowuje bajty 1:1
    for (wxUniChar c : tekstWx) tekst.push_back(static_cast<char>(c.GetValue() & 0xFF));
    auto w = ImportCSVElementow::parsuj(tekst);
    if (!w.blad.empty()) {
        pozycje_.clear();
        Wypelnij();
        lbBlad->SetLabel(TekstZUTF8(w.blad));
        if (!cicho) wxMessageBox(TekstZUTF8(w.blad), TR("Import CSV"), wxOK | wxICON_ERROR, this);
        return false;
    }
    pozycje_ = std::move(w.pozycje);
    Wypelnij();
    if (!w.ostrzezenia.empty()) {
        wxString t;
        for (const auto& o : w.ostrzezenia) t += "• " + TekstZUTF8(o) + "\n";
        lbBlad->SetLabel(TekstZUTF8(w.ostrzezenia.front()) + (w.ostrzezenia.size() > 1 ? " ..." : ""));
        if (!cicho) wxMessageBox(t, TR("Uwagi do pliku"), wxOK | wxICON_INFORMATION, this);
    }
    return true;
}

void DialogImportuElementow::PrzeliczSzereg() {
    if (tryb_ != Tryb::Szereg) return;
    SzeregiE::ParametrySzeregu p;
    static const RodzajElementu rodzaje[] = {RodzajElementu::Rezystor, RodzajElementu::Kondensator, RodzajElementu::Cewka};
    static const Szereg szeregi[] = {Szereg::E6, Szereg::E12, Szereg::E24, Szereg::E48, Szereg::E96};
    p.rodzaj = rodzaje[std::max(0, chRodzaj->GetSelection())];
    p.szereg = szeregi[std::max(0, chSzereg->GetSelection())];
    std::string blad;
    auto od = SzeregiE::parsujWartosc(Tekst(tcOd), p.rodzaj);
    auto doo = SzeregiE::parsujWartosc(Tekst(tcDo), p.rodzaj);
    if (!od || !doo) blad = "Nie rozumiem zakresu wartości (np. 10, 4k7, 100n, 2M2).";
    int ilosc = 0, minimum = 0;
    if (blad.empty()) {
        auto i = Liczba(Tekst(tcIlosc).empty() ? "0" : Tekst(tcIlosc));
        auto m = Liczba(Tekst(tcMinimum).empty() ? "0" : Tekst(tcMinimum));
        if (!i || !m || *i < 0 || *m < 0) blad = "Ilość i minimum muszą być liczbami nieujemnymi.";
        else { ilosc = static_cast<int>(*i); minimum = static_cast<int>(*m); }
    }
    if (blad.empty() && !Tekst(tcCena).empty()) {
        auto c = Liczba(Tekst(tcCena));
        if (!c || *c < 0) blad = "Cena musi być liczbą.";
        else p.cena = c;
    }
    if (!blad.empty()) {
        pozycje_.clear();
        Wypelnij();
        lbBlad->SetLabel(TekstZUTF8(blad));
        return;
    }
    p.od = *od; p.doo = *doo;
    p.tolerancja = Tekst(tcTol); p.obudowa = Tekst(tcObudowa); p.moc = Tekst(tcMoc);
    p.napiecie = Tekst(tcNapiecie); p.dielektryk = Tekst(tcDielektryk); p.producent = Tekst(tcProducent);
    p.wzorSymbolu = Tekst(tcWzor); p.kategoria = Tekst(tcKategoria);
    p.ilosc = ilosc; p.minimum = minimum;
    p.miejsce = MiejsceMagazynowe::parsuj(Tekst(tcMiejsce));
    p.szufladaNaDekade = cbDekada->GetValue();
    pozycje_ = SzeregiE::generujSzereg(p, &blad);
    lbBlad->SetLabel(TekstZUTF8(blad));
    Wypelnij();
}

wxString DialogImportuElementow::StatusPozycji(const PozycjaImportu& p) const {
    if (!p.blad.empty()) return TR("błąd: ") + TekstZUTF8(p.blad);
    if (istniejace_.count(Male(p.element.symbol))) {
        switch (WybranyTryb()) {
            case TrybDuplikatow::Pomin: return TR("już jest, pominę");
            case TrybDuplikatow::DodajIlosc: return TR("już jest, dodam ilość");
            default: return TR("już jest, uzupełnię dane");
        }
    }
    return TR("nowy");
}

TrybDuplikatow DialogImportuElementow::WybranyTryb() const {
    switch (chDuplikaty ? chDuplikaty->GetSelection() : 0) {
        case 1: return TrybDuplikatow::DodajIlosc;
        case 2: return TrybDuplikatow::Aktualizuj;
        default: return TrybDuplikatow::Pomin;
    }
}

size_t DialogImportuElementow::LiczbaPoprawnych() const {
    size_t n = 0;
    for (const auto& p : pozycje_) n += p.blad.empty();
    return n;
}

void DialogImportuElementow::Wypelnij() {
    lista->Freeze();
    lista->DeleteAllItems();
    int nowe = 0, istn = 0, bledy = 0;
    for (size_t i = 0; i < pozycje_.size(); ++i) {
        const PozycjaImportu& p = pozycje_[i];
        const Element& e = p.element;
        const long r = lista->InsertItem(static_cast<long>(i), TekstZUTF8(e.symbol));
        lista->SetItem(r, K_WARTOSC, TekstZUTF8(e.wartosc));
        lista->SetItem(r, K_TOL, TekstZUTF8(e.tolerancja));
        lista->SetItem(r, K_OBUDOWA, TekstZUTF8(e.obudowa));
        lista->SetItem(r, K_ILOSC, wxString::Format("%d", e.iloscDostepna));
        lista->SetItem(r, K_KATEGORIA, TekstZUTF8(p.kategoria));
        lista->SetItem(r, K_MIEJSCE, TekstZUTF8(MiejsceMagazynowe{e.kontener, e.szuflada, e.przegroda}.naTekst()));
        lista->SetItem(r, K_STATUS, StatusPozycji(p));
        if (!p.blad.empty()) { ++bledy; lista->SetItemTextColour(r, wxColour(190, 30, 30)); }
        else if (istniejace_.count(Male(e.symbol))) { ++istn; lista->SetItemTextColour(r, wxColour(120, 120, 120)); }
        else { ++nowe; lista->SetItemTextColour(r, wxColour(0, 110, 190)); }
    }
    lista->Thaw();
    lbPodsumowanie->SetLabel(wxString::Format(TR("Razem: %zu, nowych: %d, już w magazynie: %d, błędnych: %d"),
                                              pozycje_.size(), nowe, istn, bledy));
    if (tryb_ == Tryb::Plik && bledy == 0) lbBlad->SetLabel(wxEmptyString);
}

bool DialogImportuElementow::Zapisz() {
    if (pozycje_.empty()) return false;
    ImportElementow imp(magazyn_);
    WynikImportuElementow w;
    bool ok;
    {
        wxBusyCursor zajety;
        ok = imp.wykonaj(pozycje_, WybranyTryb(), w);
    }
    wynik_ = w;
    if (!ok) {
        lbBlad->SetLabel(TekstZUTF8(imp.pobierzBlad()));
        if (IsShown()) wxMessageBox(TekstZUTF8(imp.pobierzBlad()), TR("Import elementów"), wxOK | wxICON_ERROR, this);
        return false;
    }
    wxString t = wxString::Format(TR("Dodano nowych: %d\nDodano ilość do istniejących: %d\nUzupełniono dane: %d\nPominięto: %d\nUtworzono kategorii: %d"),
                                  w.dodano, w.zsumowano, w.zaktualizowano, w.pominieto, w.utworzonoKategorii);
    if (!w.bledy.empty()) {
        t += wxString::Format(TR("\n\nBłędy (%zu):\n"), w.bledy.size());
        for (size_t i = 0; i < w.bledy.size() && i < 15; ++i) t += "• " + TekstZUTF8(w.bledy[i]) + "\n";
        if (w.bledy.size() > 15) t += "...";
    }
    if (IsShown()) wxMessageBox(t, TR("Import elementów"), wxOK | (w.bledy.empty() ? wxICON_INFORMATION : wxICON_WARNING), this);
    return true;
}

void DialogImportuElementow::OnZapiszCSV(wxCommandEvent&) {
    if (pozycje_.empty()) return;
    wxFileDialog dlg(this, TR("Zapisz zestaw jako CSV"), wxEmptyString, "zestaw_elementow.csv",
                     TR("Pliki CSV (*.csv)|*.csv"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() != wxID_OK) return;
    const std::string csv = ImportCSVElementow::naCSV(pozycje_);
    wxFFile f(dlg.GetPath(), "wb");
    if (!f.IsOpened() || f.Write(csv.data(), csv.size()) != csv.size())
        wxMessageBox(TR("Nie udało się zapisać pliku."), TR("Zapis CSV"), wxOK | wxICON_ERROR, this);
}

void DialogImportuElementow::OnSzablon(wxCommandEvent&) {
    wxFileDialog dlg(this, TR("Zapisz plik wzorcowy"), wxEmptyString, "szablon_elementow.csv",
                     TR("Pliki CSV (*.csv)|*.csv"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() != wxID_OK) return;
    const std::string csv = ImportCSVElementow::szablon();
    wxFFile f(dlg.GetPath(), "wb");
    if (!f.IsOpened() || f.Write(csv.data(), csv.size()) != csv.size())
        wxMessageBox(TR("Nie udało się zapisać pliku."), TR("Szablon CSV"), wxOK | wxICON_ERROR, this);
}

void DialogImportuElementow::UstawPole(const wxString& k, const wxString& w) {
    auto tekst = [&](wxTextCtrl* t) { if (t) t->ChangeValue(w); };
    if (k == "rodzaj") chRodzaj->SetSelection(wxAtoi(w));
    else if (k == "szereg") chSzereg->SetSelection(wxAtoi(w));
    else if (k == "od") tekst(tcOd);
    else if (k == "do") tekst(tcDo);
    else if (k == "obudowa") tekst(tcObudowa);
    else if (k == "ilosc") tekst(tcIlosc);
    else if (k == "miejsce") tekst(tcMiejsce);
    else if (k == "kategoria") tekst(tcKategoria);
    else if (k == "dekada") cbDekada->SetValue(w == "1");
    else if (k == "duplikaty") chDuplikaty->SetSelection(wxAtoi(w));
    if (tryb_ == Tryb::Szereg) PrzeliczSzereg();
}
