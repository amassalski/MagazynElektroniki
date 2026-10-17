#include "ZdjeciaElementow.h"
#include "../Narzedzia/Konfiguracja.h"
#include "../Narzedzia/Tlumaczenia.h"

#include <wx/datetime.h>
#include <wx/filename.h>
#include <wx/log.h>

namespace ZdjeciaElementow {

wxImage ZKlatki(const KlatkaRGB& k) {
    if (k.pusta()) return wxImage();
    wxImage im(k.szer, k.wys, false);
    std::copy(k.rgb.begin(), k.rgb.begin() + static_cast<size_t>(k.szer) * k.wys * 3, im.GetData());
    return im;
}

wxString Katalog() {
    wxLogNull cisza;
    const wxString katalog = TekstZUTF8(Konfiguracja::pobierz().pobierzUstawieniaAplikacji().sciezkaObrazki);
    if (katalog.empty()) return wxString();
    if (!wxDirExists(katalog) && !wxFileName::Mkdir(katalog, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL)) return wxString();
    wxFileName f(katalog, wxEmptyString);
    f.MakeAbsolute();
    return f.GetPath();
}

static wxString Bezpieczna(const wxString& nazwa) {
    wxString w;
    for (wxUniChar c : nazwa) {
        const wxUint32 u = c.GetValue();
        w += (u < 128 && (wxIsalnum(c) || c == '-' || c == '_')) ? c : wxUniChar('_');
    }
    return w.empty() ? wxString("element") : w.Left(60);
}

wxString ZapiszKlatke(const KlatkaRGB& klatka, const wxString& symbol) {
    wxImage im = ZKlatki(klatka);
    const wxString katalog = Katalog();
    if (!im.IsOk() || katalog.empty()) return wxString();
    const wxString baza = Bezpieczna(symbol) + "_" + wxDateTime::Now().Format("%Y%m%d_%H%M%S");
    wxFileName cel(katalog, baza, "jpg");
    for (int n = 2; cel.FileExists(); ++n) cel.SetName(baza + wxString::Format("_%d", n));
    im.SetOption(wxIMAGE_OPTION_QUALITY, 90);
    wxLogNull cisza;
    return im.SaveFile(cel.GetFullPath(), wxBITMAP_TYPE_JPEG) ? cel.GetFullPath() : wxString();
}

wxString SkopiujDoKatalogu(const wxString& sciezka) {
    wxFileName zrodlo(sciezka);
    const wxString katalog = Katalog();
    if (katalog.empty()) return zrodlo.GetFullPath();
    wxFileName cel(katalog, zrodlo.GetFullName());
    for (int n = 2; cel.FileExists() && cel.GetFullPath() != zrodlo.GetFullPath(); ++n)
        cel.SetName(zrodlo.GetName() + wxString::Format("_%d", n));
    if (cel.GetFullPath() == zrodlo.GetFullPath()) return cel.GetFullPath();
    wxLogNull cisza;
    return wxCopyFile(zrodlo.GetFullPath(), cel.GetFullPath(), false) ? cel.GetFullPath() : zrodlo.GetFullPath();
}

wxImage Wczytaj(const wxString& sciezka, wxSize pole) {
    if (sciezka.empty() || !wxFileExists(sciezka)) return wxImage();
    wxLogNull cisza;
    wxImage im(sciezka);
    if (!im.IsOk() || pole.x <= 0 || pole.y <= 0) return im;
    const double skala = std::min(1.0, std::min(double(pole.x) / im.GetWidth(), double(pole.y) / im.GetHeight()));
    if (skala < 1.0)
        im = im.Scale(std::max(1, int(im.GetWidth() * skala)), std::max(1, int(im.GetHeight() * skala)),
                      wxIMAGE_QUALITY_BILINEAR);
    return im;
}

} // namespace ZdjeciaElementow
