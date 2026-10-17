#include "Ikony.h"
#include "IkonyDane.h"

#include <wx/image.h>
#include <wx/mstream.h>

namespace {
wxImage Obraz(const DaneIkony& d) {
    wxMemoryInputStream strumien(d.dane, d.dlugosc);
    return wxImage(strumien, wxBITMAP_TYPE_PNG);
}
} // namespace

namespace Ikony {

wxBitmapBundle Pakiet(const std::string& nazwa) {
    if (!wxImage::FindHandler(wxBITMAP_TYPE_PNG)) wxImage::AddHandler(new wxPNGHandler);
    wxVector<wxBitmap> bitmapy;
    for (const DaneIkony& d : kIkony) {
        if (nazwa != d.nazwa) continue;
        wxImage im = Obraz(d);
        if (im.IsOk()) bitmapy.push_back(wxBitmap(im));
    }
    return bitmapy.empty() ? wxBitmapBundle() : wxBitmapBundle::FromBitmaps(bitmapy);
}

void DlaPrzycisku(wxButton* przycisk, const std::string& nazwa) {
    const wxBitmapBundle b = Pakiet(nazwa);
    if (!b.IsOk()) return;
    przycisk->SetBitmap(b);
    przycisk->SetBitmapPosition(wxLEFT);
    przycisk->SetBitmapMargins(4, 0);
}

wxIconBundle IkonaProgramu() {
    wxIconBundle pakiet;
    for (const DaneIkony& d : kIkony) {
        if (std::string("app") != d.nazwa) continue;
        wxImage im = Obraz(d);
        if (!im.IsOk()) continue;
        wxIcon ikona;
        ikona.CopyFromBitmap(wxBitmap(im));
        pakiet.AddIcon(ikona);
    }
    return pakiet;
}

} // namespace Ikony
