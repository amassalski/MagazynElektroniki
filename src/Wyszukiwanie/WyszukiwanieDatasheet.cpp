#include "WyszukiwanieDatasheet.h"

#include "../Logika/LogikaSklepow.h"
#include "KlientHTTP.h"

#include <wx/app.h>
#include <wx/filename.h>
#include <wx/utils.h>

namespace WyszukiwanieDatasheet {

std::string nazwaPliku(const std::string& symbol) {
    std::string w;
    for (unsigned char c : symbol) {
        if (c >= 0x80 || std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '+') w += static_cast<char>(c);
        else if (!w.empty() && w.back() != '_') w += '_';
    }
    while (!w.empty() && (w.back() == '_' || w.back() == '.')) w.pop_back();
    while (!w.empty() && (w.front() == '.' || w.front() == '_')) w.erase(w.begin());
    if (w.empty()) w = "element";
    if (w.size() > 100) w.resize(100);
    return w + ".pdf";
}

wxString sciezkaPliku(const wxString& katalog, const std::string& symbol) {
    wxFileName f(katalog, wxString::FromUTF8(nazwaPliku(symbol)));
    return f.GetFullPath();
}

bool czyPobrany(const wxString& katalog, const std::string& symbol) {
    return wxFileExists(sciezkaPliku(katalog, symbol));
}

std::vector<AdresSerwisu> adresySerwisow(const std::string& fraza) {
    const std::string k = LogikaSklepow::kodujURL(fraza);
    return {{"Google (PDF)", "https://www.google.com/search?q=" + k + "+datasheet+filetype%3Apdf"},
            {"Octopart", "https://octopart.com/search?q=" + k},
            {"AllDatasheet", "https://www.alldatasheet.com/view.jsp?Searchword=" + k}};
}

void szukajNaStronie(const std::string& adresStrony,
                     std::function<void(bool, const std::string&, std::vector<LinkDatasheet>)> po) {
    KlientHTTP::pobierzStrone(adresStrony, [adresStrony, po](const WynikHTTP& odp) {
        if (!odp.ok) { po(false, odp.blad, {}); return; }
        const std::string baza = odp.adres.empty() ? adresStrony : odp.adres;
        po(true, "", ParserStrony::znajdzDatasheety(odp.tresc, baza));
    });
}

void pobierz(const std::string& url, const wxString& katalog, const std::string& symbol,
             std::function<void(bool, const std::string&, const wxString&)> po) {
    if (!wxFileName::DirExists(katalog) && !wxFileName::Mkdir(katalog, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL)) {
        wxTheApp->CallAfter([po, katalog]() { po(false, "Nie można utworzyć katalogu " + katalog.utf8_string(), wxString()); });
        return;
    }
    const wxString sciezka = sciezkaPliku(katalog, symbol);
    KlientHTTP::pobierzPlik(url, sciezka, [po, sciezka](const WynikHTTP& odp) {
        po(odp.ok, odp.blad, odp.ok ? sciezka : wxString());
    }, 120000, "%PDF");
}

bool otworz(const wxString& sciezka) {
    if (!wxFileExists(sciezka)) return false;
    return wxLaunchDefaultApplication(sciezka);
}

} // namespace WyszukiwanieDatasheet
