#include "RysowanieEtykiet.h"
#include "../Narzedzia/Tlumaczenia.h"

#include <algorithm>
#include <cmath>

namespace {

wxFont czcionka(double wysokoscPx, bool pogrubiona) {
    wxFont f(wxFontInfo(wxSize(0, std::max(4, static_cast<int>(std::lround(wysokoscPx))))).Family(wxFONTFAMILY_SWISS));
    f.SetWeight(pogrubiona ? wxFONTWEIGHT_BOLD : wxFONTWEIGHT_NORMAL);
    return f;
}

/// Skraca tekst z wielokropkiem tak, by zmieścił się w danej szerokości
wxString dopasuj(wxDC& dc, const wxString& tekst, double szerokosc) {
    if (dc.GetTextExtent(tekst).x <= szerokosc) return tekst;
    wxString t = tekst;
    while (t.length() > 1 && dc.GetTextExtent(t + wxString::FromUTF8("…")).x > szerokosc) t.RemoveLast();
    return t + wxString::FromUTF8("…");
}

} // namespace

void RysujEtykiete(wxDC& dc, const EtykietaDoDruku& et, KodKreskowy::Typ typ, double x, double y, double szer,
                   double wys, double mm) {
    const Element& e = et.element;
    const double margines = std::min(1.5 * mm, std::min(szer, wys) * 0.06);
    const double lewo = x + margines, gora = y + margines, w = szer - 2 * margines, h = wys - 2 * margines;

    dc.SetTextForeground(*wxBLACK);
    dc.SetBrush(*wxBLACK_BRUSH);
    dc.SetPen(*wxTRANSPARENT_PEN);

    const std::vector<bool> kreski = KodKreskowy::moduly(typ, et.kod);
    const bool jestKod = !kreski.empty();

    // podział wysokości: kod (z cyframi) w dolnej części, tekst u góry
    const double wysCyfr = jestKod ? std::min(3.0 * mm, h * 0.14) : 0.0;
    double wysKodu = jestKod ? std::min(h * 0.42, 14.0 * mm) : 0.0;
    double wysTekstu = h - wysKodu - wysCyfr - (jestKod ? 0.5 * mm : 0.0);
    if (wysTekstu < 3.2 * mm && jestKod) {  // bardzo mała etykieta: kod niższy
        wysKodu = std::max(4.0 * mm, h - wysCyfr - 3.2 * mm - 0.5 * mm);
        wysTekstu = h - wysKodu - wysCyfr - 0.5 * mm;
    }

    // --- tekst
    std::vector<std::pair<wxString, bool>> linie;  // tekst, pogrubiona
    linie.push_back({TekstZUTF8(e.symbol), true});
    std::string druga = e.wartosc;
    if (!e.tolerancja.empty()) druga += (druga.empty() ? "" : " ") + e.tolerancja;
    if (!e.obudowa.empty()) druga += (druga.empty() ? "" : "  ") + e.obudowa;
    if (!druga.empty()) linie.push_back({TekstZUTF8(druga), false});
    if (!e.miejsceTekst().empty()) linie.push_back({TekstZUTF8(e.miejsceTekst()), false});
    if (!e.producent.empty()) linie.push_back({TekstZUTF8(e.producent), false});
    if (!e.opis.empty()) linie.push_back({TekstZUTF8(e.opis).BeforeFirst('\n'), false});

    const double wysLinii = std::min(4.2 * mm, std::max(2.4 * mm, wysTekstu / 2.2));
    const int ileLinii = std::max(1, static_cast<int>(wysTekstu / (wysLinii * 1.12)));
    double yy = gora;
    for (int i = 0; i < ileLinii && i < static_cast<int>(linie.size()); ++i) {
        const bool tytul = i == 0;
        double rozmiar = tytul ? wysLinii * 1.05 : wysLinii * 0.85;
        wxFont f = czcionka(rozmiar, linie[static_cast<size_t>(i)].second);
        dc.SetFont(f);
        // tytuł zmniejszamy, aż się zmieści (do połowy)
        wxString t = linie[static_cast<size_t>(i)].first;
        while (dc.GetTextExtent(t).x > w && rozmiar > wysLinii * 0.55) {
            rozmiar *= 0.93;
            f = czcionka(rozmiar, linie[static_cast<size_t>(i)].second);
            dc.SetFont(f);
        }
        t = dopasuj(dc, t, w);
        dc.DrawText(t, wxRound(lewo), wxRound(yy));
        yy += wysLinii * 1.12;
    }

    // --- kod kreskowy
    if (jestKod) {
        const int strefa = 9;  // moduły ciszy po bokach
        const double modul = std::min(w / (static_cast<double>(kreski.size()) + strefa), 0.5 * mm);
        const double szerKodu = modul * static_cast<double>(kreski.size());
        const double x0 = lewo + (w - szerKodu) / 2;
        const double y0 = gora + h - wysCyfr - wysKodu - (wysCyfr > 0 ? 0.3 * mm : 0);
        // pełne pasy: rysujemy kolejne kreski jako prostokąty o zaokrąglonych granicach (bez luk)
        size_t i = 0;
        while (i < kreski.size()) {
            if (!kreski[i]) { ++i; continue; }
            size_t j = i;
            while (j < kreski.size() && kreski[j]) ++j;
            const int xa = wxRound(x0 + modul * static_cast<double>(i));
            const int xb = wxRound(x0 + modul * static_cast<double>(j));
            dc.DrawRectangle(xa, wxRound(y0), std::max(1, xb - xa), wxRound(wysKodu));
            i = j;
        }
        // cyfry pod kodem
        if (wysCyfr > 0) {
            dc.SetFont(czcionka(wysCyfr * 0.95, false));
            const wxString cyfry = TekstZUTF8(et.kod);
            const wxSize sz = dc.GetTextExtent(cyfry);
            dc.DrawText(cyfry, wxRound(x0 + (szerKodu - sz.x) / 2), wxRound(y0 + wysKodu + 0.3 * mm));
        }
    }
}

void RysujStrone(wxDC& dc, const std::vector<EtykietaDoDruku>& etykiety, const UstawieniaDruku& u, int strona,
                 double mm) {
    const auto wpisy = DrukujEtykiety::naStronie(u.format, u.pominiete, static_cast<int>(etykiety.size()), strona);
    for (const auto& w : wpisy) {
        const double x = (w.polozenie.x + u.przesuniecieX) * mm;
        const double y = (w.polozenie.y + u.przesuniecieY) * mm;
        RysujEtykiete(dc, etykiety[static_cast<size_t>(w.numer)], u.typKodu, x, y, u.format.etykietaSzer * mm,
                      u.format.etykietaWys * mm, mm);
        if (u.ramki) {
            dc.SetBrush(*wxTRANSPARENT_BRUSH);
            dc.SetPen(wxPen(wxColour(150, 150, 150), 1));
            dc.DrawRectangle(wxRound(x), wxRound(y), wxRound(u.format.etykietaSzer * mm), wxRound(u.format.etykietaWys * mm));
        }
    }
    // pominięte miejsca na pierwszym arkuszu: delikatny znacznik tylko przy ramkach
    if (u.ramki && strona == 0) {
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.SetPen(wxPen(wxColour(210, 210, 210), 1, wxPENSTYLE_DOT));
        for (int i = 0; i < u.pominiete % std::max(1, u.format.naStrone()); ++i) {
            const auto p = DrukujEtykiety::polozenie(u.format, i);
            dc.DrawRectangle(wxRound((p.x + u.przesuniecieX) * mm), wxRound((p.y + u.przesuniecieY) * mm),
                             wxRound(u.format.etykietaSzer * mm), wxRound(u.format.etykietaWys * mm));
        }
    }
}

wxBitmap RenderujStrone(const std::vector<EtykietaDoDruku>& etykiety, const UstawieniaDruku& u, int strona,
                        double dpi) {
    const double mm = dpi / 25.4;
    const int szer = std::max(1, wxRound(u.format.stronaSzer * mm));
    const int wys = std::max(1, wxRound(u.format.stronaWys * mm));
    wxBitmap bmp(szer, wys, 24);
    wxMemoryDC dc(bmp);
    dc.SetBackground(*wxWHITE_BRUSH);
    dc.Clear();
    RysujStrone(dc, etykiety, u, strona, mm);
    dc.SelectObject(wxNullBitmap);
    return bmp;
}

// ---------------------------------------------------------------------------

WydrukEtykiet::WydrukEtykiet(const std::vector<EtykietaDoDruku>& etykiety, const UstawieniaDruku& ustawienia)
    : wxPrintout(wxString::FromUTF8("Etykiety Domowy Magazyn")), etykiety_(etykiety), ustawienia_(ustawienia) {
    stron_ = DrukujEtykiety::liczbaStron(ustawienia.format, ustawienia.pominiete, static_cast<int>(etykiety.size()));
}

void WydrukEtykiet::GetPageInfo(int* min, int* max, int* od, int* doo) {
    *min = 1;
    *max = std::max(1, stron_);
    *od = 1;
    *doo = std::max(1, stron_);
}

bool WydrukEtykiet::HasPage(int strona) { return strona >= 1 && strona <= stron_; }

bool WydrukEtykiet::OnPrintPage(int strona) {
    wxDC* dc = GetDC();
    if (!dc || !HasPage(strona)) return false;
    // współrzędne w dziesiątych częściach milimetra, strona dopasowana do obszaru wydruku
    FitThisSizeToPage(wxSize(wxRound(ustawienia_.format.stronaSzer * 10), wxRound(ustawienia_.format.stronaWys * 10)));
    RysujStrone(*dc, etykiety_, ustawienia_, strona - 1, 10.0);
    return true;
}
