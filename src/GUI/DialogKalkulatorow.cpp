#include "DialogKalkulatorow.h"
#include <wx/statline.h>
#include "../Logika/Kalkulatory.h"
#include "../Narzedzia/Liczby.h"
#include "../Narzedzia/Tlumaczenia.h"
#include "../Pomoc/PomocGUI.h"

#include <cmath>
#include <optional>

namespace {

std::optional<double> Napiecie(const wxTextCtrl* tc) {
    double v = 0;
    if (!tekstNaLiczbe(tc->GetValue().utf8_string(), v)) return std::nullopt;
    return v;
}
std::optional<double> Opor(const wxTextCtrl* tc) {
    return SzeregiE::parsujWartosc(tc->GetValue().utf8_string(), RodzajElementu::Rezystor);
}
std::optional<double> Pojemnosc(const wxTextCtrl* tc) {
    return SzeregiE::parsujWartosc(tc->GetValue().utf8_string(), RodzajElementu::Kondensator);
}
/// Częstotliwość: "1k", "159", "2,2M", opcjonalnie z "Hz"
std::optional<double> Czestotliwosc(const wxTextCtrl* tc) {
    std::string t = tc->GetValue().utf8_string();
    for (const char* jedn : {"Hz", "hz", "HZ"}) {
        const size_t p = t.rfind(jedn);
        if (p != std::string::npos && p + 2 == t.size()) t.erase(p);
    }
    auto v = SzeregiE::parsujWartosc(t, RodzajElementu::Rezystor);   // k, M, G jak w rezystorach
    return v;
}

wxString Op(double v) { return TekstZUTF8(SzeregiE::formatZJednostka(v, RodzajElementu::Rezystor)); }
wxString Pf(double v) { return TekstZUTF8(SzeregiE::formatZJednostka(v, RodzajElementu::Kondensator)); }
wxString Liczba(double v, int miejsc = 2) { return TekstZUTF8(liczbaNaTekstPolski(v, miejsc, 0)); }

wxChoice* WyborSzeregu(wxWindow* p, Szereg domyslny) {
    wxChoice* c = new wxChoice(p, wxID_ANY);
    for (Szereg s : {Szereg::E6, Szereg::E12, Szereg::E24, Szereg::E48, Szereg::E96}) c->Append(TekstZUTF8(SzeregiE::nazwa(s)));
    c->SetSelection(static_cast<int>(domyslny));
    return c;
}
Szereg Wybrany(const wxChoice* c) { return static_cast<Szereg>(std::max(0, c->GetSelection())); }

wxTextCtrl* Pole(wxWindow* p, wxFlexGridSizer* g, const wxString& etykieta, const wxString& wartosc, const char* klucz) {
    g->Add(new wxStaticText(p, wxID_ANY, etykieta), 0, wxALIGN_CENTER_VERTICAL);
    wxTextCtrl* t = new wxTextCtrl(p, wxID_ANY, wartosc, wxDefaultPosition, wxSize(130, -1));
    PomocGUI::Dodaj(t, klucz);
    g->Add(t, 0);
    return t;
}

wxStaticText* Opis(wxWindow* p, wxSizer* s, const wxString& tekst) {
    wxStaticText* l = new wxStaticText(p, wxID_ANY, tekst);
    l->Wrap(520);
    s->Add(l, 0, wxALL, 10);
    return l;
}

wxStaticText* Wynik(wxWindow* p, wxSizer* s, bool pogrubiony) {
    wxStaticText* l = new wxStaticText(p, wxID_ANY, wxEmptyString);
    if (pogrubiony) l->SetFont(l->GetFont().Scaled(1.2f).Bold());
    s->Add(l, 0, wxALL, 10);
    return l;
}

} // namespace

DialogKalkulatorow::DialogKalkulatorow(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, TR("Narzędzia: kalkulatory"), wxDefaultPosition, wxSize(600, 520),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
    wxBoxSizer* glowny = new wxBoxSizer(wxVERTICAL);
    ksiazka = new wxNotebook(this, wxID_ANY);
    ksiazka->AddPage(StronaDzielnika(), TR("Dzielnik napięcia"));
    ksiazka->AddPage(StronaLED(), TR("Rezystor do LED"));
    ksiazka->AddPage(StronaRC(), TR("Filtr RC"));
    glowny->Add(ksiazka, 1, wxEXPAND | wxALL, 10);
    glowny->Add(new wxButton(this, wxID_CLOSE, TR("Zamknij")), 0, wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    SetSizer(glowny);
    SetEscapeId(wxID_CLOSE);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EndModal(wxID_CLOSE); }, wxID_CLOSE);
    PrzeliczDzielnik();
    PrzeliczLED();
    PrzeliczRC();
}

// ---------------------------------------------------------------- dzielnik
wxPanel* DialogKalkulatorow::StronaDzielnika() {
    wxPanel* p = new wxPanel(ksiazka);
    wxBoxSizer* s = new wxBoxSizer(wxVERTICAL);
    Opis(p, s, TR("Uwy = Uwe · R2 / (R1 + R2).  Wpisz napięcia, a program dobierze rezystory z wybranego szeregu."));
    wxFlexGridSizer* g = new wxFlexGridSizer(2, 6, 8);
    tcUwe = Pole(p, g, TR("Napięcie wejściowe Uwe [V]:"), "5", "kalk.uwe");
    tcUwy = Pole(p, g, TR("Napięcie wyjściowe Uwy [V]:"), "3,3", "kalk.uwy");
    g->Add(new wxStaticText(p, wxID_ANY, TR("Szereg:")), 0, wxALIGN_CENTER_VERTICAL);
    chSzeregD = WyborSzeregu(p, Szereg::E24);
    PomocGUI::Dodaj(chSzeregD, "kalk.szereg");
    g->Add(chSzeregD, 0);
    s->Add(g, 0, wxLEFT | wxRIGHT, 10);
    lbDzielnik = Wynik(p, s, true);
    s->Add(new wxStaticLine(p), 0, wxEXPAND | wxLEFT | wxRIGHT, 10);
    Opis(p, s, TR("Sprawdź własne rezystory (Uwe z góry):"));
    wxFlexGridSizer* g2 = new wxFlexGridSizer(2, 6, 8);
    tcR1 = Pole(p, g2, TR("R1 (do plusa):"), "10k", "kalk.r1");
    tcR2 = Pole(p, g2, TR("R2 (do masy):"), "20k", "kalk.r2");
    s->Add(g2, 0, wxLEFT | wxRIGHT, 10);
    lbDzielnik2 = Wynik(p, s, false);
    p->SetSizer(s);
    for (wxTextCtrl* t : {tcUwe, tcUwy, tcR1, tcR2}) t->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { PrzeliczDzielnik(); });
    chSzeregD->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { PrzeliczDzielnik(); });
    return p;
}

void DialogKalkulatorow::UstawDzielnik(const wxString& uwe, const wxString& uwy, const wxString& r1, const wxString& r2) {
    tcUwe->ChangeValue(uwe); tcUwy->ChangeValue(uwy); tcR1->ChangeValue(r1); tcR2->ChangeValue(r2);
    PrzeliczDzielnik();
}

void DialogKalkulatorow::PrzeliczDzielnik() {
    const auto uwe = Napiecie(tcUwe), uwy = Napiecie(tcUwy);
    if (!uwe || !uwy) {
        lbDzielnik->SetLabel(TR("Wpisz napięcia, np. 5 i 3,3."));
    } else if (auto w = Kalkulatory::dobierzDzielnik(*uwe, *uwy, Wybrany(chSzeregD))) {
        lbDzielnik->SetLabel("R1 = " + Op(w->r1) + ",  R2 = " + Op(w->r2) + "\n" + TR("Uwy = ") + Liczba(w->uwy, 3) + " V  (" +
                             TekstZUTF8(std::string(w->bladProc > 0 ? "+" : "") + liczbaNaTekstPolski(w->bladProc, 2, 2)) + " %)\n" + TR("Prąd dzielnika: ") + Liczba(w->pradMA, 3) + " mA");
    } else {
        lbDzielnik->SetLabel(TR("Uwy musi być dodatnie i mniejsze niż Uwe."));
    }
    const auto r1 = Opor(tcR1), r2 = Opor(tcR2);
    if (!uwe || !r1 || !r2) {
        lbDzielnik2->SetLabel(TR("Wpisz Uwe oraz R1 i R2 (np. 10k, 4k7)."));
    } else if (auto u = Kalkulatory::napiecieDzielnika(*uwe, *r1, *r2)) {
        lbDzielnik2->SetLabel(TR("Uwy = ") + Liczba(*u, 3) + " V,  " + TR("prąd ") + Liczba(*uwe / (*r1 + *r2) * 1000.0, 3) + " mA");
    } else {
        lbDzielnik2->SetLabel(TR("R2 musi być większe od zera."));
    }
}

// ---------------------------------------------------------------- LED
wxPanel* DialogKalkulatorow::StronaLED() {
    wxPanel* p = new wxPanel(ksiazka);
    wxBoxSizer* s = new wxBoxSizer(wxVERTICAL);
    Opis(p, s, TR("R = (Uzas - Uf) / I.  Wybierana jest najbliższa NIE MNIEJSZA wartość z szeregu, żeby nie przekroczyć prądu diody."));
    wxFlexGridSizer* g = new wxFlexGridSizer(2, 6, 8);
    tcUzas = Pole(p, g, TR("Napięcie zasilania [V]:"), "5", "kalk.uzas");
    tcUf = Pole(p, g, TR("Napięcie diody Uf [V]:"), "2", "kalk.uf");
    tcI = Pole(p, g, TR("Prąd diody [mA]:"), "20", "kalk.iled");
    g->Add(new wxStaticText(p, wxID_ANY, TR("Szereg:")), 0, wxALIGN_CENTER_VERTICAL);
    chSzeregL = WyborSzeregu(p, Szereg::E24);
    PomocGUI::Dodaj(chSzeregL, "kalk.szereg");
    g->Add(chSzeregL, 0);
    s->Add(g, 0, wxLEFT | wxRIGHT, 10);
    lbLED = Wynik(p, s, true);
    Opis(p, s, TR("Typowe Uf: czerwona 1,8-2,2 V, żółta/zielona 2,0-2,4 V, niebieska/biała 3,0-3,4 V."));
    p->SetSizer(s);
    for (wxTextCtrl* t : {tcUzas, tcUf, tcI}) t->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { PrzeliczLED(); });
    chSzeregL->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { PrzeliczLED(); });
    return p;
}

void DialogKalkulatorow::UstawLED(const wxString& uzas, const wxString& uf, const wxString& pradMA) {
    tcUzas->ChangeValue(uzas); tcUf->ChangeValue(uf); tcI->ChangeValue(pradMA);
    PrzeliczLED();
}

void DialogKalkulatorow::PrzeliczLED() {
    const auto uz = Napiecie(tcUzas), uf = Napiecie(tcUf), i = Napiecie(tcI);
    if (!uz || !uf || !i) { lbLED->SetLabel(TR("Wpisz trzy wartości liczbowe.")); return; }
    if (auto w = Kalkulatory::rezystorLED(*uz, *uf, *i, Wybrany(chSzeregL))) {
        lbLED->SetLabel(TR("Rezystor: ") + Op(w->rZSzeregu) + "  (" + TR("wyliczone ") + Op(w->rIdealny) + ")\n" +
                        TR("Prąd z tym rezystorem: ") + Liczba(w->pradRzeczywistyMA, 2) + " mA\n" +
                        TR("Moc rezystora: ") + Liczba(w->mocRezystoraMW, 1) + " mW,  " + TR("diody: ") + Liczba(w->mocLEDMW, 1) + " mW");
    } else {
        lbLED->SetLabel(TR("Napięcie zasilania musi być większe od Uf, a prąd dodatni."));
    }
}

// ---------------------------------------------------------------- RC
wxPanel* DialogKalkulatorow::StronaRC() {
    wxPanel* p = new wxPanel(ksiazka);
    wxBoxSizer* s = new wxBoxSizer(wxVERTICAL);
    Opis(p, s, TR("Filtr RC pierwszego rzędu: fc = 1 / (2π · R · C)."));
    wxFlexGridSizer* g = new wxFlexGridSizer(2, 6, 8);
    tcRC_R = Pole(p, g, TR("Rezystancja R:"), "10k", "kalk.rc_r");
    tcRC_C = Pole(p, g, TR("Pojemność C:"), "100n", "kalk.rc_c");
    s->Add(g, 0, wxLEFT | wxRIGHT, 10);
    lbRC = Wynik(p, s, true);
    s->Add(new wxStaticLine(p), 0, wxEXPAND | wxLEFT | wxRIGHT, 10);
    Opis(p, s, TR("Dobierz element dla zadanej częstotliwości granicznej:"));
    wxFlexGridSizer* g2 = new wxFlexGridSizer(2, 6, 8);
    tcRC_f = Pole(p, g2, TR("Częstotliwość fc [Hz]:"), "1k", "kalk.rc_f");
    s->Add(g2, 0, wxLEFT | wxRIGHT, 10);
    lbRC2 = Wynik(p, s, false);
    p->SetSizer(s);
    for (wxTextCtrl* t : {tcRC_R, tcRC_C, tcRC_f}) t->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { PrzeliczRC(); });
    return p;
}

void DialogKalkulatorow::UstawRC(const wxString& r, const wxString& c, const wxString& f) {
    tcRC_R->ChangeValue(r); tcRC_C->ChangeValue(c); tcRC_f->ChangeValue(f);
    PrzeliczRC();
}

void DialogKalkulatorow::PrzeliczRC() {
    const auto r = Opor(tcRC_R), c = Pojemnosc(tcRC_C), f = Czestotliwosc(tcRC_f);
    if (r && c) {
        const double fc = Kalkulatory::czestotliwoscGranicznaRC(*r, *c);
        lbRC->SetLabel(TR("fc = ") + TekstZUTF8(Kalkulatory::formatCzestotliwosci(fc)) + TR(",  τ = ") +
                       TekstZUTF8(Kalkulatory::formatCzasu(Kalkulatory::stalaCzasowaRC(*r, *c))));
    } else {
        lbRC->SetLabel(TR("Wpisz R i C (np. 10k, 100n)."));
    }
    if (!f) { lbRC2->SetLabel(TR("Wpisz częstotliwość (np. 1k, 440).")); return; }
    wxString t;
    if (c) {
        const double rr = Kalkulatory::rDlaFiltru(*f, *c);
        t += TR("Dla C = ") + Pf(*c) + ":  R = " + Op(rr) + "  (" + TR("z szeregu E24: ") + Op(Kalkulatory::najblizsza(rr, Szereg::E24)) + ")";
    }
    if (r) {
        const double cc = Kalkulatory::cDlaFiltru(*f, *r);
        t += (t.empty() ? "" : "\n") + TR("Dla R = ") + Op(*r) + ":  C = " + Pf(cc) + "  (" + TR("z szeregu E12: ") + Pf(Kalkulatory::najblizsza(cc, Szereg::E12)) + ")";
    }
    lbRC2->SetLabel(t);
}
