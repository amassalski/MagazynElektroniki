#include "OdczytKoduZObrazu.h"
#include "../Narzedzia/KodKreskowy.h"

#include <wx/log.h>

#include <algorithm>
#include <vector>

namespace {

std::optional<std::string> probuj(const wxImage& obraz) {
    const int w = obraz.GetWidth(), h = obraz.GetHeight();
    if (w <= 0 || h <= 0) return std::nullopt;
    const unsigned char* rgb = obraz.GetData();
    std::vector<unsigned char> szary(static_cast<size_t>(w) * static_cast<size_t>(h));
    for (size_t i = 0; i < szary.size(); ++i) {
        const unsigned r = rgb[3 * i], g = rgb[3 * i + 1], b = rgb[3 * i + 2];
        szary[i] = static_cast<unsigned char>((r * 30 + g * 59 + b * 11) / 100);
    }
    return KodKreskowy::odczytajObraz(szary, w, h);
}

} // namespace

std::optional<std::string> OdczytajKodZObrazu(const wxImage& obraz) {
    if (!obraz.IsOk()) return std::nullopt;
    wxImage kopia = obraz.Copy();
    if (kopia.HasAlpha()) kopia.ClearAlpha();
    // bardzo duże zdjęcia zmniejszamy: pasek ma wtedy kilka pikseli na moduł, co wystarcza i jest szybsze
    if (kopia.GetWidth() > 2400) {
        const int nowaSzer = 2400;
        kopia = kopia.Scale(nowaSzer, std::max(1, kopia.GetHeight() * nowaSzer / kopia.GetWidth()), wxIMAGE_QUALITY_BILINEAR);
    }
    if (auto k = probuj(kopia)) return k;
    return probuj(kopia.Rotate90(true));  // zdjęcie zrobione "na stojąco"
}

std::optional<std::string> OdczytajKodZPliku(const wxString& sciezka, wxString& blad) {
    wxImage obraz;
    bool wczytano;
    {
        wxLogNull cisza;  // własny komunikat zamiast okna błędu wxWidgets
        wczytano = obraz.LoadFile(sciezka);
    }
    if (!wczytano) {
        blad = wxString::FromUTF8("Nie można wczytać obrazu.");
        return std::nullopt;
    }
    auto k = OdczytajKodZObrazu(obraz);
    if (!k) blad = wxString::FromUTF8("Nie znaleziono kodu kreskowego na obrazie.");
    return k;
}
