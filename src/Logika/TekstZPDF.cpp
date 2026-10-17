#include "OdczytDatasheetu.h"

#include <wx/arrstr.h>
#include <wx/filename.h>
#include <wx/utils.h>
#include <wx/string.h>

namespace OdczytDatasheetu {

std::string tekstZPDF(const std::string& sciezka, std::string& blad, int maxStron) {
    blad.clear();
    if (!wxFileName::FileExists(wxString::FromUTF8(sciezka))) { blad = "Nie ma takiego pliku."; return {}; }
    wxArrayString wyj, bledy;
    wxArrayString cmd;
    wxString polecenie = wxString::Format("pdftotext -layout -f 1 -l %d \"%s\" -", maxStron, wxString::FromUTF8(sciezka));
    const long kod = wxExecute(polecenie, wyj, bledy, wxEXEC_SYNC | wxEXEC_NODISABLE);
    if (kod == -1) { blad = "Nie można uruchomić programu pdftotext (pakiet poppler-utils / poppler)."; return {}; }
    if (kod != 0) { blad = "pdftotext zgłosił błąd - plik może być uszkodzony lub zaszyfrowany."; return {}; }
    std::string t;
    for (const wxString& l : wyj) { t += l.utf8_string(); t += '\n'; }
    if (t.find_first_not_of(" \n\t\f") == std::string::npos) {
        blad = "W pliku nie ma warstwy tekstowej (to chyba skan).";
        return {};
    }
    return t;
}

} // namespace OdczytDatasheetu
