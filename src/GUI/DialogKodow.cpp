#include "DialogKodow.h"
#include "../Logika/SzeregiE.h"
#include "../Narzedzia/Tlumaczenia.h"
#include "../Pomoc/PomocGUI.h"

#include <wx/clipbrd.h>
#include <wx/dcbuffer.h>
#include <wx/dcgraph.h>
#include <wx/wrapsizer.h>

using KodyElementow::Kolor;

// ---------------------------------------------------------------------------
// Rysunek rezystora z paskami
// ---------------------------------------------------------------------------
class DialogKodow::Rysunek : public wxPanel {
public:
    Rysunek(wxWindow* parent) : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(420, 130)) {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        Bind(wxEVT_PAINT, [this](wxPaintEvent&) { Maluj(); });
    }
    void Ustaw(const std::vector<Kolor>& p) { paski_ = p; Refresh(); }
private:
    static wxColour Rgb(Kolor k) {
        const unsigned c = KodyElementow::kolorRGB(k);
        return wxColour((c >> 16) & 255, (c >> 8) & 255, c & 255);
    }

    void Maluj() {
        wxAutoBufferedPaintDC pdc(this);
        pdc.SetBackground(wxBrush(GetBackgroundColour()));
        pdc.Clear();
        wxGCDC dc(pdc);                                          // gładkie krawędzie i półprzezroczystość
        const wxSize s = GetClientSize();
        const int cy = s.y / 2;

        // wyprowadzenia (drut) z delikatnym połyskiem
        dc.SetPen(wxPen(wxColour(140, 140, 146), 5));
        dc.DrawLine(6, cy, s.x - 6, cy);
        dc.SetPen(wxPen(wxColour(215, 215, 220), 1));
        dc.DrawLine(6, cy - 1, s.x - 6, cy - 1);

        // korpus: węższy środek i większe, zaokrąglone walce po bokach
        const int dlugosc = 250, cap = 28, hc = 68, hm = 52;   // proporcje zbliżone do prawdziwego rezystora
        const int l = (s.x - dlugosc) / 2, p = l + dlugosc;
        const wxColour korpus(222, 196, 146), obrys(140, 105, 55);
        dc.SetPen(wxPen(obrys, 1));
        dc.SetBrush(wxBrush(korpus));
        dc.DrawRectangle(l + cap / 2, cy - hm / 2, (p - l) - cap, hm);
        dc.DrawRoundedRectangle(l, cy - hc / 2, cap, hc, 10);
        dc.DrawRoundedRectangle(p - cap, cy - hc / 2, cap, hc, 10);

        const int bl = l + cap, br = p - cap;                    // obszar pasków (środek korpusu)
        const size_t n = paski_.size();
        if (n >= 3) {
            // szerokości: cyfry i mnożnik jednakowe, tolerancja szersza (jak w normie IEC 60062), tempco wąski
            const int wb = 12, wTol = 18, odstep = 10, przerwa = 18;
            const int N = static_cast<int>(n);
            auto rysujPasek = [&](int i, int x, int szer) {
                const Kolor k = paski_[static_cast<size_t>(i)];
                if (k == Kolor::Brak) return;                    // brak paska (tolerancja 20 %)
                dc.SetBrush(wxBrush(Rgb(k)));
                dc.SetPen(wxPen(wxColour(60, 50, 40, 150), 1));
                dc.DrawRectangle(x, cy - hm / 2, szer, hm);
            };
            int x = bl + 12;
            const int lewe = (N >= 5 ? 3 : 2) + 1;              // cyfry + mnożnik
            for (int i = 0; i < lewe && i < N; ++i) { rysujPasek(i, x, wb); x += wb + odstep; }
            int prawy = br - 12;
            if (N == 6) { prawy -= wb; rysujPasek(5, prawy, wb); prawy -= przerwa; }
            if (N >= 4) { prawy -= wTol; rysujPasek(lewe, prawy, wTol); }
        }
        // połysk i cień na walcu
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(wxColour(255, 255, 255, 95)));
        dc.DrawRoundedRectangle(bl - cap / 2 + 4, cy - hm / 2 + 4, (br - bl) + cap - 8, 7, 3);
        dc.DrawRoundedRectangle(l + 5, cy - hc / 2 + 5, cap - 10, 7, 3);
        dc.DrawRoundedRectangle(p - cap + 5, cy - hc / 2 + 5, cap - 10, 7, 3);
        dc.SetBrush(wxBrush(wxColour(0, 0, 0, 40)));
        dc.DrawRoundedRectangle(bl - cap / 2 + 4, cy + hm / 2 - 10, (br - bl) + cap - 8, 6, 3);
    }

    std::vector<Kolor> paski_;
};

namespace {
wxString Liczba(double v) {
    wxString t = wxString::Format("%.6g", v);
    t.Replace(".", ",");
    return t;
}
wxString Skopiuj(const wxString& t) {
    if (wxTheClipboard->Open()) { wxTheClipboard->SetData(new wxTextDataObject(t)); wxTheClipboard->Close(); }
    return t;
}
} // namespace

DialogKodow::DialogKodow(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, TR("Narzędzia: paski kolorów i kody SMD"), wxDefaultPosition, wxSize(700, 620),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
    wxBoxSizer* glowny = new wxBoxSizer(wxVERTICAL);
    ksiazka = new wxNotebook(this, wxID_ANY);
    ksiazka->AddPage(StronaPaskow(), TR("Paski → wartość"));
    ksiazka->AddPage(StronaWartosci(), TR("Wartość → paski"));
    ksiazka->AddPage(StronaSMD(), TR("Kod SMD"));
    glowny->Add(ksiazka, 1, wxEXPAND | wxALL, 10);
    glowny->Add(new wxButton(this, wxID_CLOSE, TR("Zamknij")), 0, wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    SetSizer(glowny);
    SetEscapeId(wxID_CLOSE);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EndModal(wxID_CLOSE); }, wxID_CLOSE);
    PrzeliczPaski();
    PrzeliczWartosc();
    PrzeliczSMD();
}

RodzajElementu DialogKodow::Rodzaj() const {
    return chRodzaj && chRodzaj->GetSelection() == 1 ? RodzajElementu::Cewka : RodzajElementu::Rezystor;
}

namespace {
const char* PodpisRoli(KodyElementow::Rola r, int indeksCyfry) {
    switch (r) {
        case KodyElementow::Rola::Cyfra: return indeksCyfry == 0 ? "1. cyfra" : indeksCyfry == 1 ? "2. cyfra" : "3. cyfra";
        case KodyElementow::Rola::Mnoznik: return "mnożnik";
        case KodyElementow::Rola::Tolerancja: return "tolerancja";
        case KodyElementow::Rola::Tempco: return "wsp. temp.";
    }
    return "";
}

// "żółty (4)", "złoty (±5 %)", "brak paska (±20 %)"
wxString EtykietaKoloru(Kolor k, KodyElementow::Rola r) {
    if (k == Kolor::Brak) return TR("brak paska (±20 %)");
    return TekstZUTF8(KodyElementow::nazwa(k)) + " (" + TekstZUTF8(KodyElementow::znaczenieKoloru(k, r)) + ")";
}

Kolor DomyslnyKolor(KodyElementow::Rola r, int indeksCyfry, int n) {
    switch (r) {
        case KodyElementow::Rola::Cyfra: return indeksCyfry == 0 ? Kolor::Zolty : indeksCyfry == 1 ? Kolor::Fioletowy : Kolor::Czarny;
        case KodyElementow::Rola::Mnoznik: return n >= 5 ? Kolor::Brazowy : Kolor::Czerwony;
        case KodyElementow::Rola::Tolerancja: return Kolor::Zloty;
        case KodyElementow::Rola::Tempco: return Kolor::Brazowy;
    }
    return Kolor::Czarny;
}
} // namespace

int DialogKodow::LiczbaPaskow() const {
    long n = 4;
    chLiczba->GetStringSelection().ToLong(&n);
    return static_cast<int>(n);
}

void DialogKodow::PrzebudujLiczbePaskow() {
    const int stara = chLiczba->GetCount() ? LiczbaPaskow() : 4;
    const bool cewka = Rodzaj() == RodzajElementu::Cewka;     // cewki mają 3 lub 4 paski (µH + tolerancja)
    chLiczba->Clear();
    for (const char* t : {"3", "4", "5", "6"}) {
        if (cewka && (std::string(t) == "5" || std::string(t) == "6")) continue;
        chLiczba->Append(t);
    }
    int wybor = chLiczba->FindString(wxString::Format("%d", stara));
    if (wybor == wxNOT_FOUND) wybor = chLiczba->FindString("4");
    chLiczba->SetSelection(wybor);
}

void DialogKodow::PrzebudujPaski() {
    const int n = LiczbaPaskow();
    for (size_t i = 0; i < chPaski.size(); ++i) {
        const bool aktywny = static_cast<int>(i) < n;
        chPaski[i]->Show(aktywny);
        lbPaski[i]->Show(aktywny);
        if (!aktywny) continue;
        const KodyElementow::Rola r = KodyElementow::rolaPaska(static_cast<int>(i), n);
        const int rola = static_cast<int>(r);
        const int idxCyfry = static_cast<int>(i);
        if (rolaSlotu_[i] != rola || kolorySlotu_[i].empty()) {
            // zachowaj poprzedni kolor tylko, gdy pasek ma nadal tę samą rolę
            Kolor wybrany = DomyslnyKolor(r, idxCyfry, n);
            if (rolaSlotu_[i] == rola && chPaski[i]->GetSelection() != wxNOT_FOUND && !kolorySlotu_[i].empty())
                wybrany = kolorySlotu_[i][static_cast<size_t>(chPaski[i]->GetSelection())];
            kolorySlotu_[i] = KodyElementow::dozwoloneKolory(r);
            chPaski[i]->Clear();
            int sel = 0, k = 0;
            for (Kolor kol : kolorySlotu_[i]) {
                chPaski[i]->Append(EtykietaKoloru(kol, r));
                if (kol == wybrany) sel = k;
                ++k;
            }
            chPaski[i]->SetSelection(sel);
            rolaSlotu_[i] = rola;
        }
        lbPaski[i]->SetLabel(TR(PodpisRoli(r, idxCyfry)));
    }
    chPaski[0]->GetParent()->Layout();
}

wxPanel* DialogKodow::StronaPaskow() {
    wxPanel* p = new wxPanel(ksiazka);
    wxBoxSizer* s = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* gora = new wxBoxSizer(wxHORIZONTAL);
    gora->Add(new wxStaticText(p, wxID_ANY, TR("Element:")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    chRodzaj = new wxChoice(p, wxID_ANY);
    chRodzaj->Append(TR("Rezystor"));
    chRodzaj->Append(TR("Cewka (µH)"));
    chRodzaj->SetSelection(0);
    gora->Add(chRodzaj, 0, wxRIGHT, 16);
    gora->Add(new wxStaticText(p, wxID_ANY, TR("Liczba pasków:")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    chLiczba = new wxChoice(p, wxID_ANY);
    PomocGUI::Dodaj(chLiczba, "narzedzia.paski.liczba");
    gora->Add(chLiczba, 0);
    s->Add(gora, 0, wxALL, 10);
    PrzebudujLiczbePaskow();

    rysPaski = new Rysunek(p);
    s->Add(rysPaski, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);

    wxWrapSizer* wybory = new wxWrapSizer(wxHORIZONTAL);
    for (int i = 0; i < 6; ++i) {
        wxBoxSizer* kol = new wxBoxSizer(wxVERTICAL);
        wxStaticText* lb = new wxStaticText(p, wxID_ANY, wxEmptyString);
        wxChoice* c = new wxChoice(p, wxID_ANY, wxDefaultPosition, wxSize(150, -1));
        c->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { PrzeliczPaski(); });
        kol->Add(lb, 0, wxBOTTOM, 2);
        kol->Add(c, 0);
        wybory->Add(kol, 0, wxALL, 4);
        chPaski.push_back(c);
        lbPaski.push_back(lb);
        kolorySlotu_.emplace_back();
        rolaSlotu_.push_back(-1);
    }
    s->Add(wybory, 0, wxEXPAND | wxALL, 6);
    chLiczba->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { PrzebudujPaski(); PrzeliczPaski(); });
    chRodzaj->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { PrzebudujLiczbePaskow(); PrzebudujPaski(); PrzeliczPaski(); });
    PrzebudujPaski();

    lbWynikPaskow = new wxStaticText(p, wxID_ANY, wxEmptyString);
    lbWynikPaskow->SetFont(lbWynikPaskow->GetFont().Scaled(1.3f).Bold());
    s->Add(lbWynikPaskow, 0, wxALL, 12);
    wxButton* kop = new wxButton(p, wxID_ANY, TR("Kopiuj wartość"));
    kop->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Skopiuj(wartosc_); });
    s->Add(kop, 0, wxLEFT, 12);
    p->SetSizer(s);
    return p;
}

void DialogKodow::UstawPaski(const std::vector<Kolor>& paski) {
    if (paski.size() < 3 || paski.size() > 6) return;
    const int idx = chLiczba->FindString(wxString::Format("%d", static_cast<int>(paski.size())));
    if (idx == wxNOT_FOUND) return;
    chLiczba->SetSelection(idx);
    PrzebudujPaski();
    for (size_t i = 0; i < paski.size(); ++i)
        for (size_t k = 0; k < kolorySlotu_[i].size(); ++k)
            if (kolorySlotu_[i][k] == paski[i]) chPaski[i]->SetSelection(static_cast<int>(k));
    PrzeliczPaski();
}

void DialogKodow::PrzeliczPaski() {
    const int n = LiczbaPaskow();
    std::vector<Kolor> paski;
    for (int i = 0; i < n; ++i) {
        const int sel = chPaski[static_cast<size_t>(i)]->GetSelection();
        if (sel == wxNOT_FOUND || kolorySlotu_[static_cast<size_t>(i)].empty()) return;
        paski.push_back(kolorySlotu_[static_cast<size_t>(i)][static_cast<size_t>(sel)]);
    }
    rysPaski->Ustaw(paski);
    std::string blad;
    auto w = KodyElementow::odczytajPaski(paski, &blad);
    if (!w) {
        wartosc_.clear();
        lbWynikPaskow->SetLabel(TekstZUTF8(blad));
        return;
    }
    const RodzajElementu r = Rodzaj();
    double v = w->wartosc;
    // cewka: paski dają µH
    if (r == RodzajElementu::Cewka) v *= 1e-6;
    wartosc_ = TekstZUTF8(SzeregiE::formatPolaWartosci(v, r));
    wxString t = TekstZUTF8(SzeregiE::formatZJednostka(v, r)) + TekstZUTF8(" ±") + Liczba(w->tolerancja) + " %";
    if (w->tempco >= 0) t += wxString::Format(", %d ppm/K", w->tempco);
    lbWynikPaskow->SetLabel(t);
}

wxPanel* DialogKodow::StronaWartosci() {
    wxPanel* p = new wxPanel(ksiazka);
    wxBoxSizer* s = new wxBoxSizer(wxVERTICAL);
    wxFlexGridSizer* g = new wxFlexGridSizer(2, 8, 8);
    g->Add(new wxStaticText(p, wxID_ANY, TR("Wartość rezystora:")), 0, wxALIGN_CENTER_VERTICAL);
    tcWartosc = new wxTextCtrl(p, wxID_ANY, "4k7", wxDefaultPosition, wxSize(140, -1));
    tcWartosc->SetHint(TR("np. 4k7, 330, 0,22"));
    PomocGUI::Dodaj(tcWartosc, "narzedzia.wartosc");
    g->Add(tcWartosc, 0);
    g->Add(new wxStaticText(p, wxID_ANY, TR("Tolerancja:")), 0, wxALIGN_CENTER_VERTICAL);
    chTol = new wxChoice(p, wxID_ANY);
    for (const char* t : {"20 % (brak paska)", "10 %", "5 %", "2 %", "1 %", "0,5 %", "0,25 %", "0,1 %", "0,05 %"})
        chTol->Append(TR(t));
    chTol->SetSelection(2);
    g->Add(chTol, 0);
    g->Add(new wxStaticText(p, wxID_ANY, TR("Liczba pasków:")), 0, wxALIGN_CENTER_VERTICAL);
    chLiczbaW = new wxChoice(p, wxID_ANY);
    chLiczbaW->Append(TR("3 paski (tolerancja 20 %)"));
    chLiczbaW->Append(TR("4 paski (2 cyfry)"));
    chLiczbaW->Append(TR("5 pasków (3 cyfry)"));
    chLiczbaW->Append(TR("6 pasków (3 cyfry + wsp. temperaturowy)"));
    chLiczbaW->SetSelection(1);
    g->Add(chLiczbaW, 0);
    g->Add(new wxStaticText(p, wxID_ANY, TR("Wsp. temperaturowy:")), 0, wxALIGN_CENTER_VERTICAL);
    chTempco = new wxChoice(p, wxID_ANY);
    for (Kolor k : KodyElementow::dozwoloneKolory(KodyElementow::Rola::Tempco))
        chTempco->Append(EtykietaKoloru(k, KodyElementow::Rola::Tempco));
    chTempco->SetSelection(1);   // brązowy, 100 ppm/K
    g->Add(chTempco, 0);
    s->Add(g, 0, wxALL, 10);
    rysWartosc = new Rysunek(p);
    s->Add(rysWartosc, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);
    lbWynikWartosc = new wxStaticText(p, wxID_ANY, wxEmptyString);
    s->Add(lbWynikWartosc, 0, wxALL, 12);
    tcWartosc->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { PrzeliczWartosc(); });
    chTol->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { PrzeliczWartosc(); });
    chLiczbaW->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { PrzeliczWartosc(); });
    chTempco->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { PrzeliczWartosc(); });
    p->SetSizer(s);
    return p;
}

void DialogKodow::UstawWartosc(const wxString& tekst, double tol, int paski) {
    tcWartosc->ChangeValue(tekst);
    static const double t[] = {20, 10, 5, 2, 1, 0.5, 0.25, 0.1, 0.05};
    for (int i = 0; i < 9; ++i) if (t[i] == tol) chTol->SetSelection(i);
    chLiczbaW->SetSelection(std::max(0, std::min(3, paski - 3)));
    PrzeliczWartosc();
}

void DialogKodow::PrzeliczWartosc() {
    static const double t[] = {20, 10, 5, 2, 1, 0.5, 0.25, 0.1, 0.05};
    paskiZWartosci_.clear();
    const int n = chLiczbaW->GetSelection() + 3;
    // 3 paski = zawsze 20 %; wsp. temperaturowy tylko przy 6 paskach
    if (n == 3) chTol->SetSelection(0);
    chTol->Enable(n != 3);
    chTempco->Enable(n == 6);
    auto v = SzeregiE::parsujWartosc(tcWartosc->GetValue().utf8_string(), RodzajElementu::Rezystor);
    if (!v) {
        rysWartosc->Ustaw({});
        lbWynikWartosc->SetLabel(TR("Nie rozumiem wartości (np. 4k7, 330, 0,22)."));
        return;
    }
    const auto kolTempco = KodyElementow::dozwoloneKolory(KodyElementow::Rola::Tempco);
    const int ppm = KodyElementow::wspTemperaturowy(kolTempco[static_cast<size_t>(std::max(0, chTempco->GetSelection()))]);
    paskiZWartosci_ = KodyElementow::paskiDlaWartosci(*v, t[std::max(0, chTol->GetSelection())], n, ppm);
    rysWartosc->Ustaw(paskiZWartosci_);
    if (paskiZWartosci_.empty()) {
        lbWynikWartosc->SetLabel(TR("Tej wartości nie da się zapisać na tylu paskach. Spróbuj z 5 lub 6 paskami."));
        return;
    }
    wxString t2;
    for (Kolor k : paskiZWartosci_) t2 += (t2.empty() ? "" : ", ") + (k == Kolor::Brak ? TR("brak") : TekstZUTF8(KodyElementow::nazwa(k)));
    lbWynikWartosc->SetLabel(TekstZUTF8(SzeregiE::formatZJednostka(*v, RodzajElementu::Rezystor)) + " (" +
                           TekstZUTF8(SzeregiE::formatRKM(*v, RodzajElementu::Rezystor)) + "):  " + t2);
}

wxPanel* DialogKodow::StronaSMD() {
    wxPanel* p = new wxPanel(ksiazka);
    wxBoxSizer* s = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* w = new wxBoxSizer(wxHORIZONTAL);
    w->Add(new wxStaticText(p, wxID_ANY, TR("Kod na obudowie:")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    tcKod = new wxTextCtrl(p, wxID_ANY, "472", wxDefaultPosition, wxSize(140, -1));
    tcKod->SetHint(TR("np. 472, 1002, 4R7, 01A, 104"));
    PomocGUI::Dodaj(tcKod, "narzedzia.smd.kod");
    w->Add(tcKod, 0);
    s->Add(w, 0, wxALL, 10);
    lbWynikSMD = new wxStaticText(p, wxID_ANY, wxEmptyString);
    s->Add(lbWynikSMD, 0, wxALL, 12);
    s->Add(new wxStaticText(p, wxID_ANY,
        TR("Rozpoznawane zapisy: 3 i 4 cyfry (ostatnia to liczba zer), 4R7 i R22 (litera zastępuje przecinek), "
           "EIA-96 (01A = 100 Ω, 68X = 49,9 Ω) oraz kondensatory w pF (104 = 100 nF, 225 = 2,2 µF).")),
           0, wxLEFT | wxRIGHT, 12);
    tcKod->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { PrzeliczSMD(); });
    p->SetSizer(s);
    return p;
}

void DialogKodow::UstawKodSMD(const wxString& kod) {
    tcKod->ChangeValue(kod);
    PrzeliczSMD();
}

void DialogKodow::PrzeliczSMD() {
    const std::string kod = tcKod->GetValue().utf8_string();
    wxString t;
    if (auto r = KodyElementow::rezystorZKodu(kod)) {
        t += TR("Rezystor: ") + TekstZUTF8(SzeregiE::formatZJednostka(*r, RodzajElementu::Rezystor)) + "\n";
        wartosc_ = TekstZUTF8(SzeregiE::formatPolaWartosci(*r, RodzajElementu::Rezystor));
    }
    if (auto c = KodyElementow::kondensatorZKodu(kod))
        t += TR("Kondensator: ") + TekstZUTF8(SzeregiE::formatZJednostka(*c, RodzajElementu::Kondensator)) + "\n";
    lbWynikSMD->SetLabel(t.empty() ? TR("Nie rozpoznaję tego kodu.") : t);
}
