#include "DialogKamery.h"
#include "ZdjeciaElementow.h"
#include "../Narzedzia/Tlumaczenia.h"

#include <wx/dcbuffer.h>

class DialogKamery::Podglad : public wxPanel {
public:
    explicit Podglad(wxWindow* parent) : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(640, 400)) {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetMinSize(wxSize(480, 300));
        Bind(wxEVT_PAINT, [this](wxPaintEvent&) {
            wxAutoBufferedPaintDC dc(this);
            dc.SetBackground(*wxBLACK_BRUSH);
            dc.Clear();
            const wxSize s = GetClientSize();
            if (obraz_.IsOk()) {
                const double sk = std::min(double(s.x) / obraz_.GetWidth(), double(s.y) / obraz_.GetHeight());
                const int w = std::max(1, int(obraz_.GetWidth() * sk)), h = std::max(1, int(obraz_.GetHeight() * sk));
                dc.DrawBitmap(wxBitmap(obraz_.Scale(w, h, wxIMAGE_QUALITY_NORMAL)), (s.x - w) / 2, (s.y - h) / 2);
            } else {
                dc.SetTextForeground(*wxWHITE);
                const wxSize t = dc.GetTextExtent(tekst_);
                dc.DrawText(tekst_, (s.x - t.x) / 2, (s.y - t.y) / 2);
            }
        });
        Bind(wxEVT_SIZE, [this](wxSizeEvent& e) { Refresh(); e.Skip(); });
    }
    void Pokaz(const wxImage& im) { obraz_ = im; Refresh(); Update(); }
    void Komunikat(const wxString& t) { obraz_ = wxImage(); tekst_ = t; Refresh(); }
private:
    wxImage obraz_;
    wxString tekst_;
};

DialogKamery::DialogKamery(wxWindow* parent, const wxString& symbol)
    : wxDialog(parent, wxID_ANY, TR("Zdjęcie z kamery"), wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      symbol_(symbol), timer_(this) {
    wxBoxSizer* glowny = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* gora = new wxBoxSizer(wxHORIZONTAL);
    gora->Add(new wxStaticText(this, wxID_ANY, TR("Kamera:")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    chKamera = new wxChoice(this, wxID_ANY);
    gora->Add(chKamera, 1, wxRIGHT, 8);
    wxButton* bOdswiez = new wxButton(this, wxID_ANY, TR("Wykryj ponownie"));
    gora->Add(bOdswiez, 0);
    glowny->Add(gora, 0, wxEXPAND | wxALL, 10);

    podglad_ = new Podglad(this);
    glowny->Add(podglad_, 1, wxEXPAND | wxLEFT | wxRIGHT, 10);
    lbInfo = new wxStaticText(this, wxID_ANY, wxEmptyString);
    glowny->Add(lbInfo, 0, wxALL, 10);

    wxBoxSizer* dol = new wxBoxSizer(wxHORIZONTAL);
    bZrob = new wxButton(this, wxID_ANY, TR("Zrób zdjęcie"));
    bPowtorz = new wxButton(this, wxID_ANY, TR("Powtórz"));
    bUzyj = new wxButton(this, wxID_ANY, TR("Użyj zdjęcia"));
    dol->Add(bZrob, 0, wxRIGHT, 6);
    dol->Add(bPowtorz, 0, wxRIGHT, 6);
    dol->Add(bUzyj, 0, wxRIGHT, 6);
    dol->AddStretchSpacer();
    dol->Add(new wxButton(this, wxID_CANCEL, TR("Anuluj")), 0);
    glowny->Add(dol, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    SetSizerAndFit(glowny);

    Bind(wxEVT_TIMER, [this](wxTimerEvent&) { OnTimer(); });
    bZrob->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ZrobZdjecie(); });
    bPowtorz->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Powtorz(); });
    bUzyj->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { if (Zatwierdz()) EndModal(wxID_OK); });
    bOdswiez->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { WykryjKamery(); });
    chKamera->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { OtworzWybrana(); });
    Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& e) { timer_.Stop(); kamera_.zamknij(); e.Skip(); });

    Ustaw(false);
    WykryjKamery();
}

DialogKamery::~DialogKamery() {
    timer_.Stop();
    kamera_.zamknij();
}

void DialogKamery::Ustaw(bool zamrozony) {
    zamrozony_ = zamrozony;
    bZrob->Enable(!zamrozony && kamera_.otwarta());
    bPowtorz->Enable(zamrozony);
    bUzyj->Enable(zamrozony);
    chKamera->Enable(!zamrozony);
}

void DialogKamery::WykryjKamery() {
    timer_.Stop();
    kamera_.zamknij();
    chKamera->Clear();
    {
        wxBusyCursor zajety;
        kamery_ = Kamera::wykryj();
    }
    if (kamery_.empty()) {
        podglad_->Komunikat(TR("Nie wykryto kamery"));
        lbInfo->SetLabel(Kamera::obslugiwana()
            ? TR("Podłącz kamerę USB i naciśnij „Wykryj ponownie”. Na macOS i Windows system może pytać o zgodę na dostęp do kamery.")
            : TR("Ta wersja programu została zbudowana bez obsługi kamery (OpenCV). Zdjęcie możesz wskazać z pliku."));
        Ustaw(false);
        return;
    }
    for (const InfoKamery& k : kamery_) chKamera->Append(TekstZUTF8(k.nazwa));
    chKamera->SetSelection(0);
    OtworzWybrana();
}

void DialogKamery::OtworzWybrana() {
    timer_.Stop();
    const int i = chKamera->GetSelection();
    if (i < 0 || i >= static_cast<int>(kamery_.size())) return;
    if (!kamera_.otworz(kamery_[i].indeks)) {
        podglad_->Komunikat(TekstZUTF8(kamera_.blad()));
        lbInfo->SetLabel(TekstZUTF8(kamera_.blad()));
        Ustaw(false);
        return;
    }
    lbInfo->SetLabel(TR("Ustaw element w kadrze i naciśnij „Zrób zdjęcie”."));
    Ustaw(false);
    timer_.Start(40);
    OnTimer();
}

void DialogKamery::OnTimer() {
    if (zamrozony_ || !kamera_.otwarta()) return;
    KlatkaRGB k;
    if (!kamera_.pobierz(k)) return;
    klatka_ = std::move(k);
    ++klatek_;
    podglad_->Pokaz(ZdjeciaElementow::ZKlatki(klatka_));
}

void DialogKamery::ZrobZdjecie() {
    if (!kamera_.otwarta()) return;
    OnTimer();                       // świeża klatka tuż przed zamrożeniem
    if (klatka_.pusta()) return;
    timer_.Stop();
    Ustaw(true);
    lbInfo->SetLabel(TR("Zdjęcie zrobione. „Użyj zdjęcia” zapisze je w katalogu zdjęć, „Powtórz” wraca do podglądu."));
}

void DialogKamery::Powtorz() {
    if (!kamera_.otwarta()) return;
    Ustaw(false);
    lbInfo->SetLabel(TR("Ustaw element w kadrze i naciśnij „Zrób zdjęcie”."));
    timer_.Start(40);
}

bool DialogKamery::Zatwierdz() {
    if (!zamrozony_ || klatka_.pusta()) return false;
    sciezka_ = ZdjeciaElementow::ZapiszKlatke(klatka_, symbol_);
    if (sciezka_.empty()) {
        wxMessageBox(TR("Nie udało się zapisać zdjęcia. Sprawdź katalog zdjęć w ustawieniach aplikacji."),
                     TR("Zdjęcie z kamery"), wxOK | wxICON_ERROR, this);
        return false;
    }
    return true;
}
