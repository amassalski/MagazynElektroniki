#ifndef RYSOWANIEETYKIET_H
#define RYSOWANIEETYKIET_H

#include "../Eksport/DrukujEtykiety.h"
#include "../Model/Element.h"
#include "../Narzedzia/KodKreskowy.h"

#include <wx/wx.h>
#include <wx/print.h>

#include <string>
#include <vector>

/// Jedna etykieta do wydruku: element i tekst kodu kreskowego
struct EtykietaDoDruku {
    Element element;
    std::string kod;
};

struct UstawieniaDruku {
    FormatEtykiet format;
    KodKreskowy::Typ typKodu = KodKreskowy::Typ::EAN13;
    int pominiete = 0;             ///< ile miejsc na pierwszym arkuszu jest już zużytych
    double przesuniecieX = 0;      ///< korekta położenia wydruku [mm]
    double przesuniecieY = 0;
    bool ramki = false;            ///< obrys etykiet (do próbnego wydruku)
};

/// Rysuje jedną etykietę w prostokącie (x, y, szer, wys) podanym w jednostkach DC; `naMm` = jednostek DC na milimetr
void RysujEtykiete(wxDC& dc, const EtykietaDoDruku& etykieta, KodKreskowy::Typ typ, double x, double y, double szer,
                   double wys, double naMm);

/// Rysuje całą stronę arkusza (numer od 0)
void RysujStrone(wxDC& dc, const std::vector<EtykietaDoDruku>& etykiety, const UstawieniaDruku& ustawienia, int strona,
                 double naMm);

/// Strona arkusza jako bitmapa (do podglądu i zapisu PNG)
wxBitmap RenderujStrone(const std::vector<EtykietaDoDruku>& etykiety, const UstawieniaDruku& ustawienia, int strona,
                        double dpi);

/// Wydruk przez system (okno drukowania i podgląd wxWidgets; na macOS i Windows także "Zapisz jako PDF")
class WydrukEtykiet : public wxPrintout {
public:
    WydrukEtykiet(const std::vector<EtykietaDoDruku>& etykiety, const UstawieniaDruku& ustawienia);
    bool OnPrintPage(int strona) override;
    bool HasPage(int strona) override;
    void GetPageInfo(int* min, int* max, int* od, int* doo) override;

private:
    std::vector<EtykietaDoDruku> etykiety_;
    UstawieniaDruku ustawienia_;
    int stron_ = 0;
};

#endif // RYSOWANIEETYKIET_H
