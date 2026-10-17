#ifndef TLUMACZENIA_H
#define TLUMACZENIA_H

#include <wx/intl.h>
#include <wx/string.h>

#include <string>

// Teksty w kodzie źródłowym są zapisane w UTF-8. Zwykłe _("...") dekoduje
// literał wg strony kodowej systemu (CP1250), co daje krzaczki. TR() zawsze
// traktuje literał jako UTF-8, a dopiero potem szuka tłumaczenia w katalogu.
#define TR(s) wxGetTranslation(wxString::FromUTF8(s))

// Tekst z warstwy bazy / konfiguracji (std::string w UTF-8) -> wxString.
// Gdy tekst nie jest poprawnym UTF-8 (np. komunikat libpq w stronie kodowej
// systemu), FromUTF8 zwróciłby pusty napis, więc wracamy do kodowania lokalnego.
inline wxString TekstZUTF8(const std::string& tekst) {
    wxString wynik = wxString::FromUTF8(tekst.c_str(), tekst.size());
    if (wynik.empty() && !tekst.empty()) {
        wynik = wxString(tekst.c_str(), wxConvLibc);
    }
    return wynik;
}

#endif // TLUMACZENIA_H
