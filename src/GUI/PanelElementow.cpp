#include "PanelElementow.h"
#include "Ikony.h"
#include "DialogElementu.h"
#include "../Narzedzia/Konfiguracja.h"
#include "../Narzedzia/KodKreskowy.h"
#include "../Narzedzia/Tlumaczenia.h"
#include "../Pomoc/PomocGUI.h"

#include <wx/wrapsizer.h>

#include <algorithm>
#include <cctype>

// ---------------------------------------------------------------------------
// Lista wirtualna
// ---------------------------------------------------------------------------

class PanelElementow::Lista : public wxListCtrl {
public:
    Lista(wxWindow* rodzic, PanelElementow* panel)
        : wxListCtrl(rodzic, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                     wxLC_REPORT | wxLC_VIRTUAL | wxLC_SINGLE_SEL | wxBORDER_THEME),
          panel_(panel) {}

    wxString OnGetItemText(long element, long kolumna) const override {
        if (element < 0 || static_cast<size_t>(element) >= panel_->elementy_.size()) return wxString();
        return panel_->TekstKomorki(panel_->elementy_[static_cast<size_t>(element)], static_cast<int>(kolumna));
    }

    wxListItemAttr* OnGetItemAttr(long element) const override {
        if (element >= 0 && static_cast<size_t>(element) < panel_->elementy_.size() &&
            panel_->elementy_[static_cast<size_t>(element)].niskiStan()) {
            return &panel_->atrybutNiski_;
        }
        return nullptr;
    }

private:
    PanelElementow* panel_;
};

// ---------------------------------------------------------------------------

namespace {

/// Cena do wyświetlenia: co najmniej 2 miejsca po przecinku, najwyżej 4
wxString formatCeny(const std::optional<double>& cena, const std::string& waluta) {
    if (!cena) return wxString();
    wxString t = wxString::Format("%.4f", *cena);
    // obetnij zbędne zera, zostaw 2 miejsca
    const wxUniChar separator = t.Contains(",") ? ',' : '.';
    const size_t pozycja = t.find(separator);
    if (pozycja != wxString::npos) {
        while (t.length() > pozycja + 3 && t.Last() == '0') t.RemoveLast();
    }
    return t + " " + TekstZUTF8(waluta);
}

std::string maleLitery(const std::string& s) {
    std::string wynik = s;
    for (char& c : wynik) {
        // tylko ASCII; polskie litery porównujemy bajtowo (kolejność zbliżona do alfabetycznej)
        if (static_cast<unsigned char>(c) < 128) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return wynik;
}

} // namespace

PanelElementow::PanelElementow(wxWindow* parent, LogikaMagazynu& logika)
    : wxPanel(parent, wxID_ANY), logika_(logika), timer_(this, ID_TIMER) {
    // Kolor wyróżnienia niskiego stanu: czytelny na jasnym i ciemnym tle
    atrybutNiski_.SetTextColour(wxColour(214, 84, 0));

    wxBoxSizer* glowny = new wxBoxSizer(wxVERTICAL);

    // --- szukanie i filtry
    wxBoxSizer* gora = new wxBoxSizer(wxHORIZONTAL);
    szukaj_ = new wxSearchCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(280, -1),
                               wxTE_PROCESS_ENTER);
    szukaj_->SetDescriptiveText(TR("Szukaj: symbol, opis, producent, obudowa, tagi..."));
    szukaj_->ShowCancelButton(true);
    cbPodkategorie_ = new wxCheckBox(this, ID_PODKATEGORIE, TR("Z podkategoriami"));
    cbPodkategorie_->SetValue(true);
    cbNiskie_ = new wxCheckBox(this, ID_NISKIE, TR("Tylko niski stan"));
    gora->Add(szukaj_, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    gora->Add(cbPodkategorie_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    gora->Add(cbNiskie_, 0, wxALIGN_CENTER_VERTICAL);
    glowny->Add(gora, 0, wxEXPAND | wxALL, 4);

    // --- przyciski
    wxWrapSizer* przyciski = new wxWrapSizer(wxHORIZONTAL);
    auto dodajPrzycisk = [&](int id, const wxString& etykieta) {
        wxButton* b = new wxButton(this, id, etykieta);
        Ikony::DlaPrzycisku(b, id == ID_DODAJ ? "nowy" : id == ID_EDYTUJ ? "edytuj" : id == ID_USUN ? "usun" : id == ID_STAN ? "stan" : "");
        przyciski->Add(b, 0, wxRIGHT | wxBOTTOM, 4);
        Bind(wxEVT_UPDATE_UI, &PanelElementow::OnAktualizujPrzycisk, this, id);
    };
    dodajPrzycisk(ID_DODAJ, TR("Dodaj element"));
    dodajPrzycisk(ID_EDYTUJ, TR("Edytuj"));
    dodajPrzycisk(ID_STAN, TR("Zmień stan"));
    dodajPrzycisk(ID_USUN, TR("Usuń"));
    glowny->Add(przyciski, 0, wxEXPAND | wxLEFT | wxRIGHT, 4);

    // --- lista
    lista_ = new Lista(this, this);
    struct Def { const char* tytul; int szerokosc; int wyrownanie; };
    const Def kolumny[K_LICZBA] = {
        {"Symbol", 130, wxLIST_FORMAT_LEFT},    {"Producent", 95, wxLIST_FORMAT_LEFT},
        {"Opis", 200, wxLIST_FORMAT_LEFT},      {"Wartość", 65, wxLIST_FORMAT_LEFT},      {"Tol.", 45, wxLIST_FORMAT_LEFT},
        {"Obudowa", 65, wxLIST_FORMAT_LEFT},    {"Kategoria", 140, wxLIST_FORMAT_LEFT},
        {"Miejsce", 100, wxLIST_FORMAT_LEFT},
        {"Ilość", 55, wxLIST_FORMAT_RIGHT},     {"Min.", 45, wxLIST_FORMAT_RIGHT},
        {"Cena", 85, wxLIST_FORMAT_RIGHT}};
    for (int i = 0; i < K_LICZBA; ++i) {
        lista_->InsertColumn(i, TR(kolumny[i].tytul), kolumny[i].wyrownanie, kolumny[i].szerokosc);
    }
    glowny->Add(lista_, 1, wxEXPAND | wxALL, 4);
    PomocGUI::Dodaj(lista_, "lista.elementy");
    PomocGUI::Dodaj(szukaj_, "szukaj");
    // Dymek wiersza: pełne dane elementu, także to, co nie mieści się w kolumnach
    PomocGUI::DymekWiersza(lista_, [this](long i) {
        if (i < 0 || static_cast<size_t>(i) >= elementy_.size()) return wxString();
        const Element& e = elementy_[static_cast<size_t>(i)];
        wxString t = TekstZUTF8(e.symbol);
        auto dodaj = [&t](const wxString& nazwa, const wxString& wartosc) {
            if (!wartosc.empty()) t += "\n" + nazwa + ": " + wartosc;
        };
        dodaj(TR("Producent"), TekstZUTF8(e.producent + (e.producent.empty() || e.symbolProducenta.empty() ? "" : ", ") + e.symbolProducenta));
        dodaj(TR("Opis"), TekstZUTF8(e.opis));
        dodaj(TR("Wartość"), TekstKomorki(e, K_WARTOSC));
        dodaj(TR("Tolerancja"), TekstZUTF8(e.tolerancja));
        dodaj(TR("Parametry"), TekstZUTF8(e.parametry));
        dodaj(TR("Obudowa"), TekstZUTF8(e.obudowa));
        dodaj(TR("Kategoria"), TekstZUTF8(e.kategoriaPelna));
        dodaj(TR("Miejsce"), TekstZUTF8(e.miejsceTekst()));
        dodaj(TR("Ilość"), wxString::Format("%d", e.iloscDostepna) +
              (e.iloscMinimalna > 0 ? wxString::Format(TR(" (minimum %d)"), e.iloscMinimalna) : wxString()));
        dodaj(TR("Cena"), TekstKomorki(e, K_CENA) + (e.cena ? " " + TekstZUTF8(e.waluta) : wxString()));
        dodaj(TR("Tagi"), TekstZUTF8(e.tagi));
        return t;
    });
    SetSizer(glowny);

    // --- zdarzenia
    szukaj_->Bind(wxEVT_TEXT, &PanelElementow::OnZmianaTekstu, this);
    szukaj_->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent&) { timer_.Stop(); Odswiez(); });
    szukaj_->Bind(wxEVT_SEARCH_CANCEL, [this](wxCommandEvent&) { szukaj_->Clear(); });
    Bind(wxEVT_TIMER, &PanelElementow::OnTimer, this, ID_TIMER);
    Bind(wxEVT_CHECKBOX, &PanelElementow::OnZmianaFiltra, this, ID_PODKATEGORIE);
    Bind(wxEVT_CHECKBOX, &PanelElementow::OnZmianaFiltra, this, ID_NISKIE);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { NowyElement(); }, ID_DODAJ);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EdytujWybrany(); }, ID_EDYTUJ);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ZmienStanWybranego(); }, ID_STAN);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { UsunWybrany(); }, ID_USUN);
    lista_->Bind(wxEVT_LIST_COL_CLICK, &PanelElementow::OnKlikniecieNaglowka, this);
    lista_->Bind(wxEVT_LIST_ITEM_SELECTED, [this](wxListEvent& e) { ZglosWybor(); e.Skip(); });
    lista_->Bind(wxEVT_LIST_ITEM_DESELECTED, [this](wxListEvent& e) { ZglosWybor(); e.Skip(); });
    lista_->Bind(wxEVT_LIST_ITEM_ACTIVATED, &PanelElementow::OnAktywacja, this);
    lista_->Bind(wxEVT_LIST_KEY_DOWN, &PanelElementow::OnKlawisz, this);
    lista_->Bind(wxEVT_LIST_ITEM_RIGHT_CLICK, &PanelElementow::OnPrawyPrzycisk, this);
}

// ---------------------------------------------------------------------------
// Pomocnicze
// ---------------------------------------------------------------------------

wxString PanelElementow::TekstKomorki(const Element& e, int kolumna) const {
    switch (kolumna) {
        case K_SYMBOL: return TekstZUTF8(e.symbol);
        case K_PRODUCENT: return TekstZUTF8(e.producent);
        case K_OPIS: return TekstZUTF8(e.opis);
        case K_WARTOSC: return TekstZUTF8(e.wartosc + (e.wartosc.empty() || e.jednostka.empty() ? "" : " ") + e.jednostka);
        case K_TOL: return TekstZUTF8(e.tolerancja);
        case K_OBUDOWA: return TekstZUTF8(e.obudowa);
        case K_KATEGORIA: return TekstZUTF8(e.kategoriaPelna);
        case K_MIEJSCE: return TekstZUTF8(e.miejsceTekst());
        case K_ILOSC: return wxString::Format("%d", e.iloscDostepna);
        case K_MIN: return e.iloscMinimalna > 0 ? wxString::Format("%d", e.iloscMinimalna) : wxString();
        case K_CENA: return formatCeny(e.cena, e.waluta);
        default: return wxString();
    }
}

void PanelElementow::Komunikat(const wxString& tekst) {
    if (naKomunikat) naKomunikat(tekst);
}

void PanelElementow::Blad(const wxString& naglowek) {
    const wxString tresc = TekstZUTF8(logika_.pobierzBlad());
    Komunikat(TR("✗ ") + naglowek + ": " + tresc);
    wxMessageBox(tresc, naglowek, wxOK | wxICON_WARNING, this);
}

void PanelElementow::PokazStatus() {
    if (!naStatus) return;
    wxString t = TR("Elementów: ") + wxString::Format("%zu", elementy_.size());
    if (limitOsiagniety_) t += TR(" (osiągnięto limit listy, zawęź wyszukiwanie)");
    naStatus(t);
}

long PanelElementow::WybranyIndeks() const {
    return lista_->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
}

const Element* PanelElementow::WybranyElement() const {
    const long i = WybranyIndeks();
    if (i < 0 || static_cast<size_t>(i) >= elementy_.size()) return nullptr;
    return &elementy_[static_cast<size_t>(i)];
}

bool PanelElementow::MaWybor() const {
    return WybranyElement() != nullptr;
}

void PanelElementow::WybierzElement(long long id) {
    for (size_t i = 0; i < elementy_.size(); ++i) {
        if (elementy_[i].id == id) {
            lista_->SetItemState(static_cast<long>(i), wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
                                 wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
            lista_->EnsureVisible(static_cast<long>(i));
            return;
        }
    }
}

void PanelElementow::FokusSzukania() {
    szukaj_->SetFocus();
    szukaj_->SelectAll();
}

void PanelElementow::WybierzPierwszy() {
    if (elementy_.empty()) return;
    lista_->SetItemState(0, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
                         wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
}

void PanelElementow::Szukaj(const wxString& tekst) {
    timer_.Stop();
    szukaj_->ChangeValue(tekst); // bez zdarzenia i opóźnienia
    Odswiez();
}

// ---------------------------------------------------------------------------
// Dane
// ---------------------------------------------------------------------------

void PanelElementow::UstawKategorie(std::optional<long long> idKategorii) {
    kategoria_ = idKategorii;
    Odswiez();
}

void PanelElementow::Wyczysc() {
    timer_.Stop();
    elementy_.clear();
    lista_->SetItemCount(0);
    lista_->Refresh();
    ZglosWybor();
    if (naStatus) naStatus(wxString());
}

void PanelElementow::Odswiez() {
    long long zaznaczony = 0;
    if (const Element* e = WybranyElement()) zaznaczony = e->id;

    FiltrElementow filtr;
    filtr.idKategorii = kategoria_;
    filtr.zPodkategoriami = cbPodkategorie_->GetValue();
    filtr.tekst = szukaj_->GetValue().utf8_string();
    filtr.tylkoNiskieStany = cbNiskie_->GetValue();

    // Zeskanowany kod z etykiety (13 cyfr z prefiksem z ustawień albo 8 cyfr) wskazuje element wprost
    long long zKodu = 0;
    {
        wxString t = szukaj_->GetValue();
        t.Trim(true).Trim(false);
        if (t.length() == 13 || t.length() == 8) {
            const int prefiks = Konfiguracja::pobierz().pobierzUstawieniaAplikacji().prefixEAN;
            if (auto id = KodKreskowy::idZKodu(t.utf8_string(), prefiks)) zKodu = *id;
        }
    }
    bool ok;
    {
        wxBusyCursor zajety;
        ok = logika_.pobierzElementy(filtr, elementy_);
        if (zKodu != 0) {
            if (auto e = logika_.pobierzElement(zKodu)) {
                elementy_.assign(1, *e);
                zaznaczony = e->id;
                ok = true;
                Komunikat(TR("Znaleziono element po kodzie kreskowym: ") + TekstZUTF8(e->symbol));
            }
        }
    }
    if (!ok) {
        elementy_.clear();
        lista_->SetItemCount(0);
        lista_->Refresh();
        Blad(TR("Nie udało się pobrać elementów"));
        return;
    }
    limitOsiagniety_ = static_cast<int>(elementy_.size()) >= filtr.limit;

    if (sortKolumna_ >= 0) Posortuj();
    lista_->SetItemCount(static_cast<long>(elementy_.size()));
    lista_->Refresh();
    if (zaznaczony != 0) WybierzElement(zaznaczony);
    ZglosWybor();
    PokazStatus();
}

void PanelElementow::Posortuj() {
    const int kol = sortKolumna_;
    const bool rosnaco = sortRosnaco_;
    auto klucz = [&](const Element& e) -> std::string {
        switch (kol) {
            case K_SYMBOL: return maleLitery(e.symbol);
            case K_PRODUCENT: return maleLitery(e.producent);
            case K_OPIS: return maleLitery(e.opis);
            case K_WARTOSC: return maleLitery(e.wartosc);
            case K_TOL: return maleLitery(e.tolerancja);
            case K_OBUDOWA: return maleLitery(e.obudowa);
            case K_KATEGORIA: return maleLitery(e.kategoriaPelna);
            case K_MIEJSCE: return maleLitery(e.kontener) + "\x01" + maleLitery(e.szuflada) + "\x01" + maleLitery(e.przegroda);
            default: return std::string();
        }
    };
    auto porownaj = [&](const Element& a, const Element& b) {
        switch (kol) {
            case K_ILOSC: return a.iloscDostepna < b.iloscDostepna;
            case K_MIN: return a.iloscMinimalna < b.iloscMinimalna;
            case K_CENA: return a.cena.value_or(-1.0) < b.cena.value_or(-1.0);
            default: return klucz(a) < klucz(b);
        }
    };
    std::stable_sort(elementy_.begin(), elementy_.end(), [&](const Element& a, const Element& b) {
        return rosnaco ? porownaj(a, b) : porownaj(b, a);
    });
}

// ---------------------------------------------------------------------------
// Akcje
// ---------------------------------------------------------------------------

void PanelElementow::NowyElement() {
    Element nowy;
    nowy.idKategorii = kategoria_;
    DialogElementu dlg(this, logika_, nowy);
    if (dlg.ShowModal() != wxID_OK) return;

    const Element dane = dlg.PobierzElement();
    auto id = logika_.zapiszElement(dane);
    if (!id) return Blad(TR("Nie udało się dodać elementu"));
    Komunikat(TR("Dodano element: ") + TekstZUTF8(dane.symbol));
    Odswiez();
    WybierzElement(*id);
    if (naZmianaDanych) naZmianaDanych();
}

void PanelElementow::EdytujWybrany() {
    const Element* wybrany = WybranyElement();
    if (!wybrany) return;

    // Świeże dane z bazy (ktoś mógł je zmienić od ostatniego odświeżenia)
    auto aktualny = logika_.pobierzElement(wybrany->id);
    if (!aktualny) {
        Blad(TR("Nie udało się odczytać elementu"));
        Odswiez();
        return;
    }
    DialogElementu dlg(this, logika_, *aktualny);
    if (dlg.ShowModal() != wxID_OK) return;

    const Element dane = dlg.PobierzElement();
    auto id = logika_.zapiszElement(dane);
    if (!id) return Blad(TR("Nie udało się zapisać elementu"));
    Komunikat(TR("Zapisano element: ") + TekstZUTF8(dane.symbol));
    Odswiez();
    WybierzElement(*id);
    if (naZmianaDanych) naZmianaDanych();
}

void PanelElementow::UsunWybrany() {
    const Element* wybrany = WybranyElement();
    if (!wybrany) return;
    const long long id = wybrany->id;
    const wxString symbol = TekstZUTF8(wybrany->symbol);

    if (wxMessageBox(TR("Usunąć element „") + symbol + TR("”?\n\nJeśli element jest używany (np. na liście "
                         "części projektu), zostanie tylko ukryty, a dane pozostaną w bazie."),
                     TR("Usuwanie elementu"), wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION, this) != wxYES) {
        return;
    }
    switch (logika_.usunElement(id)) {
        case WynikUsuniecia::Usuniety:
            Komunikat(TR("Usunięto element: ") + symbol);
            break;
        case WynikUsuniecia::Zarchiwizowany:
            Komunikat(TR("Element „") + symbol + TR("” jest używany, więc został ukryty (zarchiwizowany)"));
            break;
        case WynikUsuniecia::Blad:
            return Blad(TR("Nie udało się usunąć elementu"));
    }
    Odswiez();
    if (naZmianaDanych) naZmianaDanych();
}

void PanelElementow::ZmienStanWybranego() {
    const Element* wybrany = WybranyElement();
    if (!wybrany) return;
    auto aktualny = logika_.pobierzElement(wybrany->id);
    if (!aktualny) {
        Blad(TR("Nie udało się odczytać elementu"));
        Odswiez();
        return;
    }
    DialogZmianyIlosci dlg(this, TekstZUTF8(aktualny->symbol), aktualny->iloscDostepna);
    if (dlg.ShowModal() != wxID_OK) return;

    if (!logika_.zmienIlosc(aktualny->id, dlg.Zmiana(), dlg.Typ(), dlg.Komentarz())) {
        return Blad(TR("Nie udało się zmienić stanu"));
    }
    Komunikat(TR("Zmieniono stan elementu ") + TekstZUTF8(aktualny->symbol) + ": " +
              wxString::Format("%+d", dlg.Zmiana()));
    Odswiez();
    WybierzElement(aktualny->id);
    if (naZmianaDanych) naZmianaDanych();
}

// ---------------------------------------------------------------------------
// Zdarzenia
// ---------------------------------------------------------------------------

void PanelElementow::OnZmianaTekstu(wxCommandEvent& event) {
    event.Skip();
    timer_.StartOnce(300); // poczekaj aż użytkownik skończy pisać
}

void PanelElementow::OnTimer(wxTimerEvent&) {
    Odswiez();
}

void PanelElementow::OnZmianaFiltra(wxCommandEvent&) {
    Odswiez();
}

void PanelElementow::OnKlikniecieNaglowka(wxListEvent& event) {
    const int kolumna = event.GetColumn();
    if (kolumna == sortKolumna_) {
        sortRosnaco_ = !sortRosnaco_;
    } else {
        sortKolumna_ = kolumna;
        sortRosnaco_ = true;
    }
    Odswiez();
}

void PanelElementow::OnAktywacja(wxListEvent&) {
    EdytujWybrany();
}

void PanelElementow::OnKlawisz(wxListEvent& event) {
    switch (event.GetKeyCode()) {
        case WXK_DELETE: UsunWybrany(); break;
        case WXK_INSERT: NowyElement(); break;
        default: event.Skip();
    }
}

void PanelElementow::OnPrawyPrzycisk(wxListEvent& event) {
    lista_->SetItemState(event.GetIndex(), wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
                         wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
    wxMenu menu;
    menu.Append(ID_EDYTUJ, TR("Edytuj..."));
    menu.Append(ID_STAN, TR("Zmień stan..."));
    menu.AppendSeparator();
    menu.Append(ID_USUN, TR("Usuń"));
    switch (GetPopupMenuSelectionFromUser(menu)) {
        case ID_EDYTUJ: EdytujWybrany(); break;
        case ID_STAN: ZmienStanWybranego(); break;
        case ID_USUN: UsunWybrany(); break;
        default: break;
    }
}

void PanelElementow::OnAktualizujPrzycisk(wxUpdateUIEvent& event) {
    if (event.GetId() == ID_DODAJ) event.Enable(true);
    else event.Enable(MaWybor());
}
