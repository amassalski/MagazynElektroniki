#include "PanelPodgladu.h"
#include "DialogKamery.h"
#include "ZdjeciaElementow.h"
#include "../BazaDanych/ManagerBazyDanych.h"
#include "../Logika/LogikaKatalogu.h"
#include "../Narzedzia/Tlumaczenia.h"
#include "../Pomoc/PomocGUI.h"

#include <wx/dcbuffer.h>
#include <wx/filedlg.h>
#include <wx/utils.h>

class PanelPodgladu::Zdjecie : public wxPanel {
public:
    explicit Zdjecie(wxWindow* parent) : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(280, 220)) {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetMinSize(wxSize(200, 160));
        Bind(wxEVT_PAINT, [this](wxPaintEvent&) { Maluj(); });
        Bind(wxEVT_SIZE, [this](wxSizeEvent& e) { przeskalowany_ = false; Refresh(); e.Skip(); });
        Bind(wxEVT_LEFT_DCLICK, [this](wxMouseEvent&) {
            if (!sciezka_.empty() && wxFileExists(sciezka_)) wxLaunchDefaultApplication(sciezka_);
        });
        SetToolTip(TR("Dwukrotne kliknięcie otwiera zdjęcie w domyślnym programie"));
    }
    void Ustaw(const wxString& sciezka) {
        sciezka_ = sciezka;
        obraz_ = wxImage();
        zaladowany_ = false;
        przeskalowany_ = false;
        brak_ = sciezka.empty() ? TR("Brak zdjęcia") : (wxFileExists(sciezka) ? wxString() : TR("Nie znaleziono pliku zdjęcia"));
        if (!sciezka.empty() && wxFileExists(sciezka)) {
            obraz_ = ZdjeciaElementow::Wczytaj(sciezka, wxSize(1600, 1600));
            zaladowany_ = obraz_.IsOk();
            if (!zaladowany_) brak_ = TR("Nie można odczytać zdjęcia");
        }
        Refresh();
    }
    bool Zaladowane() const { return zaladowany_; }
private:
    wxString sciezka_, brak_;
    wxImage obraz_;
    wxBitmap bitmapa_;
    bool zaladowany_ = false, przeskalowany_ = false;

    void Maluj() {
        wxAutoBufferedPaintDC dc(this);
        dc.SetBackground(wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW)));
        dc.Clear();
        const wxSize s = GetClientSize();
        if (zaladowany_) {
            const double sk = std::min(double(s.x) / obraz_.GetWidth(), double(s.y) / obraz_.GetHeight());
            const int w = std::max(1, int(obraz_.GetWidth() * sk)), h = std::max(1, int(obraz_.GetHeight() * sk));
            dc.DrawBitmap(wxBitmap(obraz_.Scale(w, h, wxIMAGE_QUALITY_BILINEAR)), (s.x - w) / 2, (s.y - h) / 2);
        } else {
            dc.SetPen(wxPen(wxColour(180, 180, 180), 1, wxPENSTYLE_SHORT_DASH));
            dc.SetBrush(*wxTRANSPARENT_BRUSH);
            dc.DrawRectangle(4, 4, s.x - 8, s.y - 8);
            dc.SetTextForeground(wxColour(130, 130, 130));
            const wxSize t = dc.GetTextExtent(brak_);
            dc.DrawText(brak_, (s.x - t.x) / 2, (s.y - t.y) / 2);
        }
    }
};

PanelPodgladu::PanelPodgladu(wxWindow* parent, LogikaMagazynu& logika) : wxPanel(parent), logika_(logika) {
    wxBoxSizer* s = new wxBoxSizer(wxVERTICAL);
    zdjecie_ = new Zdjecie(this);
    s->Add(zdjecie_, 1, wxEXPAND | wxALL, 8);
    lbSymbol_ = new wxStaticText(this, wxID_ANY, wxEmptyString);
    lbSymbol_->SetFont(lbSymbol_->GetFont().Bold().Scaled(1.2f));
    s->Add(lbSymbol_, 0, wxLEFT | wxRIGHT | wxEXPAND, 10);
    lbDane_ = new wxStaticText(this, wxID_ANY, wxEmptyString);
    s->Add(lbDane_, 0, wxALL | wxEXPAND, 10);

    bKamera_ = new wxButton(this, wxID_ANY, TR("Zdjęcie z kamery..."));
    bPlik_ = new wxButton(this, wxID_ANY, TR("Zdjęcie z pliku..."));
    bUsun_ = new wxButton(this, wxID_ANY, TR("Usuń zdjęcie"));
    PomocGUI::Dodaj(bKamera_, "podglad.kamera");
    s->Add(bKamera_, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 8);
    s->Add(bPlik_, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 8);
    s->Add(bUsun_, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 8);
    SetSizer(s);
    SetMinSize(wxSize(230, 300));

    bKamera_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ZdjecieZKamery(); });
    bPlik_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ZdjecieZPliku(); });
    bUsun_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { UstawZdjecie(wxEmptyString); });
    bUsun_->Bind(wxEVT_UPDATE_UI, [this](wxUpdateUIEvent& e) { e.Enable(element_ && !element_->zdjecieSciezka.empty()); });
    for (wxButton* b : {bKamera_, bPlik_})
        b->Bind(wxEVT_UPDATE_UI, [this](wxUpdateUIEvent& e) { e.Enable(element_.has_value()); });
    Odswiez();
}

void PanelPodgladu::Pokaz(const Element* e) {
    if (e) element_ = *e; else element_.reset();
    Odswiez();
}

bool PanelPodgladu::ZdjecieWidoczne() const { return zdjecie_->Zaladowane(); }

void PanelPodgladu::Odswiez() {
    if (!element_) {
        lbSymbol_->SetLabel(TR("Nie wybrano elementu"));
        lbDane_->SetLabel(TR("Zaznacz element na liście, aby zobaczyć jego zdjęcie i dane."));
        zdjecie_->Ustaw(wxEmptyString);
    } else {
        const Element& e = *element_;
        lbSymbol_->SetLabel(TekstZUTF8(e.symbol));
        wxString t;
        auto dodaj = [&](const wxString& et, const std::string& v) {
            if (!v.empty()) t += et + ": " + TekstZUTF8(v) + "\n";
        };
        std::string wartosc = e.wartosc;
        if (!e.tolerancja.empty()) wartosc += (wartosc.empty() ? "" : " ") + e.tolerancja;
        dodaj(TR("Wartość"), wartosc);
        dodaj(TR("Obudowa"), e.obudowa);
        dodaj(TR("Producent"), e.producent);
        dodaj(TR("Opis"), e.opis);
        dodaj(TR("Kategoria"), e.kategoriaPelna);
        dodaj(TR("Miejsce"), e.miejsceTekst());
        t += TR("Stan: ") + wxString::Format("%d", e.iloscDostepna);
        if (e.iloscMinimalna > 0) t += wxString::Format(TR(" (minimum %d)"), e.iloscMinimalna);
        // dane z katalogu (powiązanie po symbolu): parametry, których element nie ma u siebie
        if (ManagerBazyDanych::pobierz().czyPolaczony()) {
            LogikaKatalogu katalog(ManagerBazyDanych::pobierz());
            if (auto k = katalog.dlaElementu(e)) {
                t += TR("\n\nZ katalogu:");
                if (!k->opis.empty() && k->opis != e.opis) t += "\n" + TekstZUTF8(k->opis);
                int n = 0;
                for (const ParametrKatalogu& p : k->parametry) {
                    if (++n > 8) { t += "\n..."; break; }
                    t += "\n" + TekstZUTF8(p.nazwa) + ": " + TekstZUTF8(p.wartosc);
                }
                if (k->parametry.empty() && (k->opis.empty() || k->opis == e.opis)) t += TR(" jest, bez dodatkowych danych");
            }
        }
        lbDane_->SetLabel(t);
        zdjecie_->Ustaw(TekstZUTF8(e.zdjecieSciezka));
    }
    lbDane_->Wrap(std::max(150, GetClientSize().x - 24));
    Layout();
}

bool PanelPodgladu::UstawZdjecie(const wxString& sciezka) {
    if (!element_) return false;
    // świeży odczyt z bazy: zapis ze starej kopii z listy mógłby nadpisać zmieniony w międzyczasie stan
    auto e = logika_.pobierzElement(element_->id);
    if (!e) {
        Komunikat(TR("✗ Nie udało się wczytać elementu: ") + TekstZUTF8(logika_.pobierzBlad()));
        return false;
    }
    e->zdjecieSciezka = sciezka.utf8_string();
    if (!logika_.zapiszElement(*e)) {
        const wxString blad = TekstZUTF8(logika_.pobierzBlad());
        Komunikat(TR("✗ Nie udało się zapisać zdjęcia: ") + blad);
        wxMessageBox(blad, TR("Zdjęcie elementu"), wxOK | wxICON_WARNING, this);
        return false;
    }
    element_->zdjecieSciezka = e->zdjecieSciezka;
    Komunikat((sciezka.empty() ? TR("Usunięto zdjęcie elementu ") : TR("Zapisano zdjęcie elementu ")) + TekstZUTF8(e->symbol));
    Odswiez();
    if (naZmiana) naZmiana();
    return true;
}

void PanelPodgladu::ZdjecieZKamery() {
    if (!element_) return;
    DialogKamery dlg(this, TekstZUTF8(element_->symbol));
    if (dlg.ShowModal() != wxID_OK || dlg.Sciezka().empty()) return;
    UstawZdjecie(dlg.Sciezka());
}

void PanelPodgladu::ZdjecieZPliku() {
    if (!element_) return;
    wxFileDialog dlg(this, TR("Wybierz zdjęcie elementu"), wxEmptyString, wxEmptyString,
                     TR("Obrazy (*.jpg;*.jpeg;*.png;*.bmp;*.gif)|*.jpg;*.jpeg;*.png;*.bmp;*.gif|Wszystkie pliki|*.*"),
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() != wxID_OK) return;
    UstawZdjecie(ZdjeciaElementow::SkopiujDoKatalogu(dlg.GetPath()));
}
