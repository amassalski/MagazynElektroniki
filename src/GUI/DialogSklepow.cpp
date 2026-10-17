#include "DialogSklepow.h"

#include "../Narzedzia/Liczby.h"
#include "../Narzedzia/Tlumaczenia.h"
#include "DialogDodajSklep.h"
#include "../Pomoc/PomocGUI.h"

#include <wx/filedlg.h>
#include <wx/textfile.h>
#include <wx/ffile.h>

#include <sstream>

DialogSklepow::DialogSklepow(wxWindow* parent, LogikaSklepow& logika)
    : wxDialog(parent, wxID_ANY, TR("Sklepy"), wxDefaultPosition, wxSize(980, 520), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      logika_(logika) {
    wxBoxSizer* glowny = new wxBoxSizer(wxVERTICAL);
    lista = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL | wxBORDER_THEME);
    lista->AppendColumn(TR("Nazwa"), wxLIST_FORMAT_LEFT, 150);
    lista->AppendColumn(TR("Waluta"), wxLIST_FORMAT_LEFT, 60);
    lista->AppendColumn(TR("Dostawa"), wxLIST_FORMAT_RIGHT, 75);
    lista->AppendColumn(TR("Darmowa od"), wxLIST_FORMAT_RIGHT, 90);
    lista->AppendColumn(TR("Priorytet"), wxLIST_FORMAT_RIGHT, 65);
    lista->AppendColumn(TR("Aktywny"), wxLIST_FORMAT_LEFT, 65);
    lista->AppendColumn(TR("Linków"), wxLIST_FORMAT_RIGHT, 55);
    lista->AppendColumn(TR("Adres wyszukiwania"), wxLIST_FORMAT_LEFT, 380);
    glowny->Add(lista, 1, wxEXPAND | wxALL, 10);
    PomocGUI::DymekZKolumn(lista);

    wxStaticText* uwaga = new wxStaticText(
        this, wxID_ANY, TR("Adresy wyszukiwania sklepów startowych nie były sprawdzane na działających stronach - popraw je w razie potrzeby."));
    uwaga->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
    glowny->Add(uwaga, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);

    wxBoxSizer* dol = new wxBoxSizer(wxHORIZONTAL);
    wxButton* bDodaj = new wxButton(this, wxID_ANY, TR("Dodaj..."));
    wxButton* bEdytuj = new wxButton(this, wxID_ANY, TR("Zmień..."));
    wxButton* bUsun = new wxButton(this, wxID_ANY, TR("Usuń"));
    wxButton* bImport = new wxButton(this, wxID_ANY, TR("Wczytaj listę..."));
    wxButton* bKursy = new wxButton(this, wxID_ANY, TR("Kursy walut..."));
    for (wxButton* b : {bDodaj, bEdytuj, bUsun, bImport, bKursy}) dol->Add(b, 0, wxRIGHT, 6);
    dol->AddStretchSpacer();
    dol->Add(new wxButton(this, wxID_CANCEL, TR("Zamknij")));
    glowny->Add(dol, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    SetSizer(glowny);
    SetEscapeId(wxID_CANCEL);

    bDodaj->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Dodaj(); });
    bEdytuj->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Edytuj(); });
    bUsun->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Usun(); });
    bImport->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Importuj(); });
    bKursy->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Kursy(); });
    for (wxButton* b : {bEdytuj, bUsun}) b->Bind(wxEVT_UPDATE_UI, [this](wxUpdateUIEvent& e) { e.Enable(Wybrany() != nullptr); });
    lista->Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](wxListEvent&) { Edytuj(); });
    Odswiez();
}

const Sklep* DialogSklepow::Wybrany() const {
    const long i = lista->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (i < 0 || static_cast<size_t>(i) >= sklepy_.size()) return nullptr;
    return &sklepy_[static_cast<size_t>(i)];
}

void DialogSklepow::Wybierz(long long idSklepu) {
    for (size_t i = 0; i < sklepy_.size(); ++i)
        if (sklepy_[i].id == idSklepu) {
            lista->SetItemState(static_cast<long>(i), wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
                                wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
            lista->EnsureVisible(static_cast<long>(i));
        }
}

void DialogSklepow::Odswiez() {
    const long wybrany = Wybrany() ? Wybrany()->id : 0;
    lista->DeleteAllItems();
    if (!logika_.pobierzSklepy(false, sklepy_)) {
        wxMessageBox(TekstZUTF8(logika_.pobierzBlad()), TR("Sklepy"), wxOK | wxICON_ERROR, this);
        return;
    }
    for (size_t i = 0; i < sklepy_.size(); ++i) {
        const Sklep& s = sklepy_[i];
        const long w = lista->InsertItem(static_cast<long>(i), TekstZUTF8(s.nazwa));
        lista->SetItem(w, 1, TekstZUTF8(s.waluta));
        lista->SetItem(w, 2, TekstZUTF8(liczbaNaTekstPolski(s.kosztDostawy, 2, 2)));
        lista->SetItem(w, 3, s.darmowaDostawaOd ? TekstZUTF8(liczbaNaTekstPolski(*s.darmowaDostawaOd, 2, 2)) : wxString("-"));
        lista->SetItem(w, 4, wxString::Format("%d", s.priorytet));
        lista->SetItem(w, 5, s.aktywny ? TR("tak") : TR("nie"));
        lista->SetItem(w, 6, wxString::Format("%d", s.liczbaLinkow));
        lista->SetItem(w, 7, TekstZUTF8(s.szablonWyszukiwania));
        if (!s.aktywny) lista->SetItemTextColour(w, wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
    }
    if (wybrany) Wybierz(wybrany);
}

bool DialogSklepow::Dodaj() {
    DialogDodajSklep dlg(this, logika_);
    if (dlg.ShowModal() != wxID_OK) return false;
    Odswiez();
    Wybierz(dlg.IdZapisanego());
    return true;
}

bool DialogSklepow::Edytuj() {
    const Sklep* s = Wybrany();
    if (!s) return false;
    DialogDodajSklep dlg(this, logika_, *s);
    if (dlg.ShowModal() != wxID_OK) return false;
    Odswiez();
    Wybierz(dlg.IdZapisanego());
    return true;
}

bool DialogSklepow::Usun() {
    const Sklep* s = Wybrany();
    if (!s) return false;
    const wxString pytanie = s->liczbaLinkow > 0
        ? wxString::Format(TR("Sklep \"%s\" jest używany w linkach elementów (%d), więc zostanie tylko wyłączony. Kontynuować?"),
                           TekstZUTF8(s->nazwa), s->liczbaLinkow)
        : wxString::Format(TR("Usunąć sklep \"%s\"?"), TekstZUTF8(s->nazwa));
    if (wxMessageBox(pytanie, TR("Sklepy"), wxYES_NO | wxICON_QUESTION, this) != wxYES) return false;
    if (logika_.usunSklep(s->id) == WynikUsuniecia::Blad) {
        wxMessageBox(TekstZUTF8(logika_.pobierzBlad()), TR("Sklepy"), wxOK | wxICON_ERROR, this);
        return false;
    }
    Odswiez();
    return true;
}

bool DialogSklepow::Importuj() {
    wxDialog dlg(this, wxID_ANY, TR("Wczytaj listę sklepów"), wxDefaultPosition, wxSize(760, 480), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
    wxBoxSizer* g = new wxBoxSizer(wxVERTICAL);
    g->Add(new wxStaticText(&dlg, wxID_ANY,
        TR("Jeden sklep w wierszu, pola oddzielone średnikiem (albo tabulatorem - można wkleić z Excela):\n"
           "nazwa; adres strony; adres wyszukiwania z {q}; waluta; koszt dostawy; darmowa dostawa od\n"
           "Wymagana jest tylko nazwa. Sklep o tej samej nazwie zostanie zaktualizowany. Wiersz od # to komentarz.")),
           0, wxALL, 10);
    wxTextCtrl* pole = new wxTextCtrl(&dlg, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxHSCROLL);
    pole->SetValue("# nazwa;adres strony;adres wyszukiwania;waluta;dostawa;darmowa od\n");
    g->Add(pole, 1, wxEXPAND | wxLEFT | wxRIGHT, 10);
    wxBoxSizer* p = new wxBoxSizer(wxHORIZONTAL);
    wxButton* bPlik = new wxButton(&dlg, wxID_ANY, TR("Z pliku..."));
    wxButton* bEksport = new wxButton(&dlg, wxID_ANY, TR("Wstaw bieżącą listę"));
    p->Add(bPlik, 0, wxRIGHT, 6);
    p->Add(bEksport, 0);
    p->AddStretchSpacer();
    p->Add(dlg.CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0);
    g->Add(p, 0, wxEXPAND | wxALL, 10);
    dlg.SetSizer(g);
    bPlik->Bind(wxEVT_BUTTON, [&](wxCommandEvent&) {
        wxFileDialog fd(&dlg, TR("Plik z listą sklepów"), wxEmptyString, wxEmptyString, TR("Tekst i CSV (*.txt;*.csv)|*.txt;*.csv|Wszystkie pliki|*.*"),
                        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (fd.ShowModal() != wxID_OK) return;
        wxFFile f(fd.GetPath(), "rb");
        wxString tresc;
        if (f.IsOpened() && f.ReadAll(&tresc, wxConvUTF8)) pole->SetValue(tresc);
        else wxMessageBox(TR("Nie można odczytać pliku (oczekiwane kodowanie UTF-8)."), TR("Sklepy"), wxOK | wxICON_WARNING, &dlg);
    });
    bEksport->Bind(wxEVT_BUTTON, [&](wxCommandEvent&) { pole->SetValue(TekstZUTF8(logika_.eksportujSklepy())); });
    if (dlg.ShowModal() != wxID_OK) return false;

    WynikImportuSklepow w;
    if (!logika_.importujSklepy(pole->GetValue().utf8_string(), w)) {
        wxMessageBox(TekstZUTF8(logika_.pobierzBlad()), TR("Sklepy"), wxOK | wxICON_ERROR, this);
        return false;
    }
    wxString info = wxString::Format(TR("Dodano: %d, zaktualizowano: %d, z błędami: %zu."), w.dodano, w.zaktualizowano, w.bledy.size());
    for (const std::string& b : w.bledy) info += "\n" + TekstZUTF8(b);
    wxMessageBox(info, TR("Wczytano listę sklepów"), wxOK | (w.bledy.empty() ? wxICON_INFORMATION : wxICON_WARNING), this);
    Odswiez();
    return w.dodano + w.zaktualizowano > 0;
}

std::string DialogSklepow::KursyNaTekst(const std::map<std::string, double>& kursy) {
    std::string t;
    for (const auto& kv : kursy) if (kv.first != "PLN") t += kv.first + " = " + liczbaNaTekstPolski(kv.second, 4, 2) + "\n";
    return t;
}

bool DialogSklepow::TekstNaKursy(const std::string& tekst, std::map<std::string, double>& kursy, std::string& blad) {
    kursy.clear();
    std::istringstream in(tekst);
    std::string linia;
    int nr = 0;
    while (std::getline(in, linia)) {
        ++nr;
        const size_t a = linia.find_first_not_of(" \t\r");
        if (a == std::string::npos || linia[a] == '#') continue;
        const size_t r = linia.find('=');
        if (r == std::string::npos) { blad = "wiersz " + std::to_string(nr) + ": oczekiwano WALUTA = kurs"; return false; }
        std::string w = linia.substr(a, r - a);
        while (!w.empty() && (w.back() == ' ' || w.back() == '\t')) w.pop_back();
        for (char& c : w) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        double k = 0;
        if (w.size() != 3 || !tekstNaLiczbe(linia.substr(r + 1), k) || !(k > 0)) {
            blad = "wiersz " + std::to_string(nr) + ": waluta ma mieć 3 litery, a kurs być liczbą większą od zera";
            return false;
        }
        if (w == "PLN") { blad = "wiersz " + std::to_string(nr) + ": kurs złotego jest zawsze 1"; return false; }
        kursy[w] = k;
    }
    return true;
}

bool DialogSklepow::Kursy() {
    std::map<std::string, double> kursy;
    if (!logika_.pobierzKursy(kursy)) {
        wxMessageBox(TekstZUTF8(logika_.pobierzBlad()), TR("Kursy walut"), wxOK | wxICON_ERROR, this);
        return false;
    }
    wxDialog dlg(this, wxID_ANY, TR("Kursy walut"), wxDefaultPosition, wxSize(420, 360), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
    wxBoxSizer* g = new wxBoxSizer(wxVERTICAL);
    g->Add(new wxStaticText(&dlg, wxID_ANY, TR("Ile złotych kosztuje jednostka waluty (używane do porównania cen w zestawieniu zakupów).\n"
                                              "Kursy wpisuje się ręcznie - program nie pobiera ich z internetu.")), 0, wxALL, 10);
    wxTextCtrl* pole = new wxTextCtrl(&dlg, wxID_ANY, TekstZUTF8(KursyNaTekst(kursy)), wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE);
    g->Add(pole, 1, wxEXPAND | wxLEFT | wxRIGHT, 10);
    g->Add(dlg.CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 10);
    dlg.SetSizer(g);
    for (;;) {
        if (dlg.ShowModal() != wxID_OK) return false;
        std::map<std::string, double> nowe;
        std::string blad;
        if (!TekstNaKursy(pole->GetValue().utf8_string(), nowe, blad)) {
            wxMessageBox(TekstZUTF8(blad), TR("Kursy walut"), wxOK | wxICON_WARNING, &dlg);
            continue;
        }
        for (const auto& kv : nowe) {
            if (!logika_.zapiszKurs(kv.first, kv.second)) {
                wxMessageBox(TekstZUTF8(logika_.pobierzBlad()), TR("Kursy walut"), wxOK | wxICON_ERROR, &dlg);
                return false;
            }
        }
        return true;
    }
}
