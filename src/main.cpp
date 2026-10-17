#include <wx/wx.h>
#include <wx/filename.h>
#include <wx/intl.h>
#include <wx/log.h>
#include <wx/stdpaths.h>

#include "BazaDanych/ManagerBazyDanych.h"
#include "GUI/OknoGlowne.h"
#include "Narzedzia/Konfiguracja.h"
#include "Narzedzia/Tlumaczenia.h"
#include "Pomoc/PomocGUI.h"

class DomowyMagazynApp : public wxApp {
public:
    bool OnInit() override;
    int OnExit() override;

private:
    wxLocale* m_locale = nullptr;
    void SkonfigurujSciezki();
};

wxIMPLEMENT_APP(DomowyMagazynApp);

// Ustala położenie pliku konfiguracji i skryptów SQL. Program może być
// uruchamiany z folderu build, z folderu projektu albo po zainstalowaniu, więc
// sprawdzamy kilka miejsc: najpierw obok pliku exe, potem bieżący katalog.
void DomowyMagazynApp::SkonfigurujSciezki() {
    const wxString exe = wxFileName(wxStandardPaths::Get().GetExecutablePath()).GetPath();
    const wxString biezacy = wxGetCwd();

    auto polacz = [](const wxString& katalog, const wxString& nazwa) {
        return wxFileName(katalog, nazwa).GetFullPath().utf8_string();
    };
    auto wyzej = [](const wxString& katalog) {
        wxFileName fn(katalog, wxEmptyString);
        fn.RemoveLastDir();
        return fn.GetPath();
    };

    // Plik konfiguracji: pierwszy istniejący; jeśli nie ma żadnego, zapisujemy obok exe
    std::string config = Konfiguracja::znajdzPierwszyIstniejacy({
        polacz(exe, "config.ini"),
        polacz(biezacy, "config.ini"),
        polacz(wyzej(exe), "config.ini"),
    });
    if (config.empty()) {
        config = polacz(exe, "config.ini");
        // Instalacja systemowa (np. pakiet DEB): katalog programu jest tylko do odczytu, więc konfiguracja i dane
        // użytkownika trafiają do jego własnych katalogów.
        // (w pakiecie macOS zapis do wnętrza pliku .app zepsułby podpis i zginąłby przy aktualizacji)
        if (!wxFileName::IsDirWritable(exe) || exe.Contains(".app/Contents/MacOS")) {
            wxStandardPaths& sp = wxStandardPaths::Get();
            sp.SetFileLayout(wxStandardPaths::FileLayout_XDG);
            config = polacz(sp.GetUserConfigDir() + wxFileName::GetPathSeparator() + "DomowyMagazyn", "config.ini");
            Konfiguracja::ustawKatalogDanych(sp.GetUserDataDir().utf8_string());
        }
    }

    ManagerBazyDanych& manager = ManagerBazyDanych::pobierz();
    manager.ustawSciezkeKonfiguracji(config);

    const wxString zasoby = wxStandardPaths::Get().GetResourcesDir();   // macOS: MojProgram.app/Contents/Resources
    manager.ustawKatalogiSQL({
        polacz(exe, "sql"),
        polacz(zasoby, "sql"),
        polacz(wyzej(exe), "sql"),
        polacz(biezacy, "sql"),
        polacz(wyzej(biezacy), "sql"),
    });
}

bool DomowyMagazynApp::OnInit() {
    wxInitAllImageHandlers();  // PNG, JPEG, GIF: zdjęcia elementów i odczyt kodów z obrazów
    SetAppName("DomowyMagazyn");
    if (!wxApp::OnInit())
        return false;

    SkonfigurujSciezki();
    ManagerBazyDanych::pobierz().wczytajUstawienia();

    // Katalogi z tłumaczeniami: obok exe (pl/LC_MESSAGES/DomowyMagazyn.mo)
    // oraz katalog bieżący.
    const wxString katalogExe =
        wxFileName(wxStandardPaths::Get().GetExecutablePath()).GetPath();
    wxLocale::AddCatalogLookupPathPrefix(katalogExe);
    wxLocale::AddCatalogLookupPathPrefix(wxStandardPaths::Get().GetResourcesDir());
    wxLocale::AddCatalogLookupPathPrefix(wxT("."));

    m_locale = new wxLocale();
    bool jezykOk;
    {
        wxLogNull cisza; // brak polskich ustawień w systemie nie jest powodem do okienka z błędem
        jezykOk = m_locale->Init(wxLANGUAGE_POLISH, wxLOCALE_LOAD_DEFAULT);
    }
    if (!jezykOk) {
        wxLogDebug(wxT("Nie udało się ustawić języka polskiego"));
    }
    m_locale->AddCatalog(wxT("DomowyMagazyn"));

    PomocGUI::WczytajOpisyZPliku();   // opisy pól (dymki, F1) działają także bez bazy
    PomocGUI::WlaczKlawiszF1();

    OknoGlowne* frame = new OknoGlowne(TR("Domowy Magazyn"));
    frame->SetClientSize(1180, 700);
    frame->Center();
    frame->Show(true);

    return true;
}

int DomowyMagazynApp::OnExit() {
    ManagerBazyDanych::pobierz().rozlacz();
    delete m_locale;
    m_locale = nullptr;
    return wxApp::OnExit();
}
