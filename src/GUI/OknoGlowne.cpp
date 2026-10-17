#include "OknoGlowne.h"
#include "DialogPolaczenia.h"
#include "DialogEtykiet.h"
#include "DialogImportuElementow.h"
#include "DialogKodow.h"
#include "DialogKalkulatorow.h"
#include "Ikony.h"
#include "DialogRaportu.h"
#include "DialogSklepow.h"
#include "DialogLinkowElementu.h"
#include "DialogZestawienia.h"
#include "OdczytKoduZObrazu.h"
#include "DialogUstawien.h"
#include "PanelElementow.h"
#include "PanelPodgladu.h"
#include "PanelKategorii.h"
#include "PanelProjektow.h"
#include "PanelKatalogu.h"
#include "../BazaDanych/ManagerBazyDanych.h"
#include "../Eksport/EksportCSV.h"
#include "../Logika/Aktualizacje.h"
#include "../Narzedzia/Konfiguracja.h"
#include "../Wyszukiwanie/KlientHTTP.h"
#include "../Import/ImportKategorii.h"
#include "../Narzedzia/Tlumaczenia.h"
#include "../Pomoc/PomocGUI.h"

#include <wx/datetime.h>
#include <wx/filedlg.h>
#include <wx/utils.h>
#include <wx/weakref.h>

#ifndef DM_VERSION
#define DM_VERSION "1.0.0"
#endif

OknoGlowne::OknoGlowne(const wxString& title)
    : wxFrame(NULL, wxID_ANY, title, wxDefaultPosition, wxSize(1260, 780)),
      logika(ManagerBazyDanych::pobierz()),
      logikaProjektow(ManagerBazyDanych::pobierz(), logika),
      logikaKatalogu(ManagerBazyDanych::pobierz()),
      logikaRaportow(ManagerBazyDanych::pobierz(), logika, logikaProjektow),
      logikaSklepow(ManagerBazyDanych::pobierz()) {
    SetIcons(Ikony::IkonaProgramu());

    UtworzMenu();

    // Układ: [kategorie | elementy] nad dziennikiem zdarzeń
    uklad = new wxBoxSizer(wxVERTICAL);
    zakladki = new wxNotebook(this, wxID_ANY);
    podzial = new wxSplitterWindow(zakladki, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                   wxSP_LIVE_UPDATE | wxSP_3D);
    panelKategorii = new PanelKategorii(podzial, logika);
    // prawa część: [lista elementów | zdjęcie i dane zaznaczonego elementu]
    podzialListy = new wxSplitterWindow(podzial, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                        wxSP_LIVE_UPDATE | wxSP_3D);
    panelElementow = new PanelElementow(podzialListy, logika);
    panelPodgladu = new PanelPodgladu(podzialListy, logika);
    podzialListy->SplitVertically(panelElementow, panelPodgladu, -300);
    podzialListy->SetMinimumPaneSize(220);
    podzialListy->SetSashGravity(1.0);
    podzial->SplitVertically(panelKategorii, podzialListy, 270);
    podzial->SetMinimumPaneSize(170);
    podzial->SetSashGravity(0.0);
    panelProjektow = new PanelProjektow(zakladki, logika, logikaProjektow);
    zakladki->AddPage(podzial, TR("Magazyn"), true);
    panelKatalogu = new PanelKatalogu(zakladki, logika, logikaKatalogu);
    zakladki->AddPage(panelProjektow, TR("Projekty"));
    zakladki->AddPage(panelKatalogu, TR("Katalog"));
    uklad->Add(zakladki, 1, wxEXPAND);

    dziennik = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(-1, 96),
                              wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
    uklad->Add(dziennik, 0, wxEXPAND);
    SetSizer(uklad);

    panelKategorii->naWybor = [this](std::optional<long long> id) { panelElementow->UstawKategorie(id); };
    panelKategorii->naZmianaStruktury = [this] { panelElementow->Odswiez(); };
    panelKategorii->naKomunikat = [this](const wxString& tekst) { Zapisz(tekst); };
    panelElementow->naWybor = [this](const Element* e) { panelPodgladu->Pokaz(e); };
    panelPodgladu->naKomunikat = [this](const wxString& tekst) { Zapisz(tekst); };
    panelPodgladu->naZmiana = [this] { panelElementow->Odswiez(); };
    panelElementow->naStatus = [this](const wxString& tekst) { SetStatusText(tekst, 0); };
    panelElementow->naKomunikat = [this](const wxString& tekst) { Zapisz(tekst); };
    panelElementow->naZmianaDanych = [this] { panelKategorii->Odswiez(); };
    panelProjektow->naStatus = [this](const wxString& tekst) { SetStatusText(tekst, 0); };
    panelProjektow->naKomunikat = [this](const wxString& tekst) { Zapisz(tekst); };
    panelProjektow->naZmianaMagazynu = [this] {
        panelElementow->Odswiez();
        panelKategorii->Odswiez();
    };
    panelKategorii->Enable(false);
    panelElementow->Enable(false);
    panelPodgladu->Enable(false);
    panelProjektow->Enable(false);
    panelKatalogu->naStatus = [this](const wxString& tekst) { SetStatusText(tekst, 0); };
    panelKatalogu->naKomunikat = [this](const wxString& tekst) { Zapisz(tekst); };
    panelKatalogu->naZmianaMagazynu = [this] {
        panelElementow->Odswiez();
        panelKategorii->Odswiez();
    };
    panelKatalogu->Enable(false);
    zakladki->Bind(wxEVT_NOTEBOOK_PAGE_CHANGED, [this](wxBookCtrlEvent& e) {
        if (bazaGotowa && zakladki->GetPage(static_cast<size_t>(e.GetSelection())) == panelKatalogu) panelKatalogu->Odswiez();
        e.Skip();
    });

    const int szerokosci[] = {-1, 340};
    CreateStatusBar(2);
    SetStatusWidths(2, szerokosci);
    SetStatusText(TR("Gotowy"), 0);
    SetStatusText(TR("Brak połączenia z bazą"), 1);

    Bind(wxEVT_MENU, &OknoGlowne::OnZakoncz, this, wxID_EXIT);
    Bind(wxEVT_MENU, &OknoGlowne::OnPolacz, this, ID_POLACZ);
    Bind(wxEVT_MENU, &OknoGlowne::OnRozlacz, this, ID_ROZLACZ);
    Bind(wxEVT_MENU, &OknoGlowne::OnUstawieniaPolaczenia, this, ID_USTAWIENIA_POLACZENIA);
    Bind(wxEVT_MENU, &OknoGlowne::OnInicjalizujBaze, this, ID_INICJALIZUJ_BAZE);
    Bind(wxEVT_MENU, &OknoGlowne::OnInfoOBazie, this, ID_INFO_O_BAZIE);
    Bind(wxEVT_MENU, &OknoGlowne::OnUstawieniaAplikacji, this, ID_USTAWIENIA_APLIKACJI);
    Bind(wxEVT_MENU, &OknoGlowne::OnOProgramie, this, wxID_ABOUT);
    Bind(wxEVT_MENU, [](wxCommandEvent&) { PomocGUI::PokazPomocDlaFokusu(); }, wxID_HELP);
    Bind(wxEVT_MENU, [](wxCommandEvent&) { PomocGUI::OtworzWPrzegladarce(); }, ID_POMOC_PRZEGLADARKA);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { SprawdzAktualizacje(false); }, ID_POMOC_AKTUALIZACJE);
    Bind(wxEVT_MENU, [this](wxCommandEvent& e) {
        Konfiguracja& k = Konfiguracja::pobierz();
        k.ustawBool("aktualizacje", "sprawdzaj", e.IsChecked());
        k.zapisz();
    }, ID_POMOC_AUTOAKT);
    Bind(wxEVT_MENU, &OknoGlowne::OnElementNowy, this, ID_ELEMENT_NOWY);
    Bind(wxEVT_MENU, &OknoGlowne::OnElementEdytuj, this, ID_ELEMENT_EDYTUJ);
    Bind(wxEVT_MENU, &OknoGlowne::OnElementStan, this, ID_ELEMENT_STAN);
    Bind(wxEVT_MENU, &OknoGlowne::OnElementUsun, this, ID_ELEMENT_USUN);
    Bind(wxEVT_MENU, &OknoGlowne::OnKategoriaNowa, this, ID_KAT_NOWA);
    Bind(wxEVT_MENU, &OknoGlowne::OnKategoriaPodkategoria, this, ID_KAT_PODKATEGORIA);
    Bind(wxEVT_MENU, &OknoGlowne::OnKategoriaNazwa, this, ID_KAT_NAZWA);
    Bind(wxEVT_MENU, &OknoGlowne::OnKategoriaUsun, this, ID_KAT_USUN);
    Bind(wxEVT_MENU, &OknoGlowne::OnImportKategoriiStartowych, this, ID_IMPORT_STARTOWE);
    Bind(wxEVT_MENU, &OknoGlowne::OnImportKategoriiZPliku, this, ID_IMPORT_KATEGORII);
    for (int id : {(int)ID_PROJ_NOWY, (int)ID_PROJ_EDYTUJ, (int)ID_PROJ_USUN, (int)ID_PROJ_IMPORT,
                   (int)ID_PROJ_DODAJ_CZESC, (int)ID_PROJ_WYDAJ_WSZYSTKO, (int)ID_PROJ_ZAKUPY, (int)ID_PROJ_EAGLE_SKRYPT,
                   (int)ID_PROJ_EAGLE_ULP}) {
        Bind(wxEVT_MENU, &OknoGlowne::OnProjekt, this, id);
        Bind(wxEVT_UPDATE_UI, &OknoGlowne::OnAktualizujMenu, this, id);
    }
    for (int id : {(int)ID_KATALOG_NOWA, (int)ID_KATALOG_EDYTUJ, (int)ID_KATALOG_USUN, (int)ID_KATALOG_DO_MAGAZYNU,
                   (int)ID_KATALOG_IMPORT, (int)ID_KATALOG_EKSPORT, (int)ID_KATALOG_SZABLON, (int)ID_KATALOG_DS_OTWORZ,
                   (int)ID_KATALOG_DS_ODCZYT}) {
        Bind(wxEVT_MENU, &OknoGlowne::OnKatalog, this, id);
        Bind(wxEVT_UPDATE_UI, &OknoGlowne::OnAktualizujMenu, this, id);
    }
    Bind(wxEVT_MENU, &OknoGlowne::OnEtykiety, this, ID_ETYKIETY);
    Bind(wxEVT_MENU, &OknoGlowne::OnOdczytKodu, this, ID_ODCZYT_KODU);
    Bind(wxEVT_MENU, &OknoGlowne::OnGeneratorSzeregow, this, ID_GENERATOR_SZEREGOW);
    Bind(wxEVT_MENU, &OknoGlowne::OnNarzedziaKody, this, ID_NARZEDZIA_KODY);
    Bind(wxEVT_MENU, &OknoGlowne::OnNarzedziaKalkulatory, this, ID_NARZEDZIA_KALKULATORY);
    Bind(wxEVT_MENU, &OknoGlowne::OnImportElementow, this, ID_IMPORT_ELEMENTOW);
    Bind(wxEVT_UPDATE_UI, &OknoGlowne::OnAktualizujMenu, this, ID_GENERATOR_SZEREGOW);
    Bind(wxEVT_UPDATE_UI, &OknoGlowne::OnAktualizujMenu, this, ID_IMPORT_ELEMENTOW);
    Bind(wxEVT_UPDATE_UI, &OknoGlowne::OnAktualizujMenu, this, ID_ETYKIETY);
    Bind(wxEVT_UPDATE_UI, &OknoGlowne::OnAktualizujMenu, this, ID_ODCZYT_KODU);
    Bind(wxEVT_MENU, &OknoGlowne::OnRaporty, this, ID_RAPORTY);
    Bind(wxEVT_UPDATE_UI, &OknoGlowne::OnAktualizujMenu, this, ID_RAPORTY);
    Bind(wxEVT_MENU, &OknoGlowne::OnSklepy, this, ID_SKLEPY);
    Bind(wxEVT_MENU, &OknoGlowne::OnSklepyElementu, this, ID_SKLEPY_ELEMENTU);
    Bind(wxEVT_MENU, &OknoGlowne::OnZestawienie, this, ID_ZESTAWIENIE);
    for (int id : {(int)ID_SKLEPY, (int)ID_SKLEPY_ELEMENTU, (int)ID_ZESTAWIENIE})
        Bind(wxEVT_UPDATE_UI, &OknoGlowne::OnAktualizujMenu, this, id);
    Bind(wxEVT_MENU, &OknoGlowne::OnEksportCSV, this, ID_EKSPORT_CSV);
    Bind(wxEVT_MENU, &OknoGlowne::OnOdswiez, this, ID_ODSWIEZ);
    Bind(wxEVT_MENU, &OknoGlowne::OnSzukaj, this, ID_SZUKAJ);
    Bind(wxEVT_MENU, &OknoGlowne::OnPokazDziennik, this, ID_DZIENNIK);
    for (int id : {(int)ID_ELEMENT_NOWY, (int)ID_ELEMENT_EDYTUJ, (int)ID_ELEMENT_STAN, (int)ID_ELEMENT_USUN,
                   (int)ID_KAT_NOWA, (int)ID_KAT_PODKATEGORIA, (int)ID_KAT_NAZWA, (int)ID_KAT_USUN,
                   (int)ID_IMPORT_STARTOWE, (int)ID_IMPORT_KATEGORII, (int)ID_EKSPORT_CSV,
                   (int)ID_ODSWIEZ, (int)ID_SZUKAJ}) {
        Bind(wxEVT_UPDATE_UI, &OknoGlowne::OnAktualizujMenu, this, id);
    }
    for (int id : {(int)ID_POLACZ, (int)ID_ROZLACZ, (int)ID_INICJALIZUJ_BAZE, (int)ID_INFO_O_BAZIE,
                   (int)ID_USTAWIENIA_POLACZENIA}) {
        Bind(wxEVT_UPDATE_UI, &OknoGlowne::OnAktualizujMenu, this, id);
    }

    Zapisz(TR("Plik konfiguracji: ") + TekstZUTF8(ManagerBazyDanych::pobierz().sciezkaKonfiguracji()));

    // Połącz zaraz po pokazaniu okna
    CallAfter([this] { Polacz(); });
    CallAfter([this] { SprawdzAktualizacjeNaStarcie(); });
}

OknoGlowne::~OknoGlowne() {
    // Nie niszcz okna, dopóki wątek łączenia nie skończy (maks. kilka sekund)
    if (watekLaczenia.joinable()) watekLaczenia.join();
}

void OknoGlowne::UtworzMenu() {
    auto DodajPozycje = [](wxMenu* menu, int id, const wxString& tekst, const char* ikona) {
        wxMenuItem* pozycja = new wxMenuItem(menu, id, tekst);
        pozycja->SetBitmap(Ikony::Pakiet(ikona));
        menu->Append(pozycja);
    };
    wxMenu* menuProgram = new wxMenu;
    DodajPozycje(menuProgram, wxID_EXIT, TR("Za&kończ\tCtrl+Q"), "zamknij");

    wxMenu* menuBaza = new wxMenu;
    DodajPozycje(menuBaza, ID_POLACZ, TR("&Połącz\tCtrl+L"), "polacz");
    menuBaza->Append(ID_ROZLACZ, TR("&Rozłącz"));
    menuBaza->AppendSeparator();
    menuBaza->Append(ID_USTAWIENIA_POLACZENIA, TR("&Ustawienia połączenia..."));
    menuBaza->Append(ID_INICJALIZUJ_BAZE, TR("U&twórz / zaktualizuj strukturę bazy..."));
    menuBaza->Append(ID_INFO_O_BAZIE, TR("&Informacje o bazie..."));

    wxMenu* menuMagazyn = new wxMenu;
    DodajPozycje(menuMagazyn, ID_ELEMENT_NOWY, TR("&Nowy element...\tCtrl+N"), "nowy");
    DodajPozycje(menuMagazyn, ID_ELEMENT_EDYTUJ, TR("&Edytuj element...\tCtrl+E"), "edytuj");
    DodajPozycje(menuMagazyn, ID_ELEMENT_STAN, TR("Zmień &stan magazynowy...\tCtrl+T"), "stan");
    DodajPozycje(menuMagazyn, ID_ELEMENT_USUN, TR("&Usuń element"), "usun");
    menuMagazyn->AppendSeparator();
    DodajPozycje(menuMagazyn, ID_KAT_NOWA, TR("Nowa kategoria &główna...\tCtrl+Shift+N"), "kategoria");
    DodajPozycje(menuMagazyn, ID_KAT_PODKATEGORIA, TR("Nowa &podkategoria..."), "nowy");
    DodajPozycje(menuMagazyn, ID_KAT_NAZWA, TR("Zmień nazwę &kategorii..."), "edytuj");
    DodajPozycje(menuMagazyn, ID_KAT_USUN, TR("Usuń k&ategorię"), "usun");
    menuMagazyn->AppendSeparator();
    DodajPozycje(menuMagazyn, ID_IMPORT_STARTOWE, TR("Importuj kategorie s&tartowe"), "import");
    DodajPozycje(menuMagazyn, ID_IMPORT_KATEGORII, TR("&Importuj kategorie z pliku..."), "import");
    menuMagazyn->AppendSeparator();
    DodajPozycje(menuMagazyn, ID_GENERATOR_SZEREGOW, TR("&Generator szeregów E (rezystory, kondensatory, cewki)..."), "generator");
    DodajPozycje(menuMagazyn, ID_IMPORT_ELEMENTOW, TR("Importuj elementy z pliku &CSV..."), "import");
    menuMagazyn->AppendSeparator();
    DodajPozycje(menuMagazyn, ID_ETYKIETY, TR("Drukuj &etykiety z kodem kreskowym...\tCtrl+P"), "etykieta");
    menuMagazyn->Append(ID_ODCZYT_KODU, TR("Znajdź element po &zdjęciu kodu..."));
    DodajPozycje(menuMagazyn, ID_EKSPORT_CSV, TR("E&ksportuj listę do CSV...\tCtrl+Shift+E"), "eksport");

    wxMenu* menuProjekty = new wxMenu;
    DodajPozycje(menuProjekty, ID_PROJ_NOWY, TR("&Nowy projekt..."), "projekt");
    DodajPozycje(menuProjekty, ID_PROJ_EDYTUJ, TR("&Edytuj projekt..."), "edytuj");
    DodajPozycje(menuProjekty, ID_PROJ_USUN, TR("&Usuń projekt"), "usun");
    menuProjekty->AppendSeparator();
    DodajPozycje(menuProjekty, ID_PROJ_IMPORT, TR("&Importuj listę części (KiCad / Eagle)...\tCtrl+I"), "import");
    DodajPozycje(menuProjekty, ID_PROJ_DODAJ_CZESC, TR("&Dodaj część do projektu..."), "nowy");
    menuProjekty->Append(ID_PROJ_WYDAJ_WSZYSTKO, TR("&Wydaj wszystko dostępne"));
    DodajPozycje(menuProjekty, ID_PROJ_ZAKUPY, TR("&Lista zakupów do CSV..."), "eksport");
    menuProjekty->AppendSeparator();
    DodajPozycje(menuProjekty, ID_PROJ_EAGLE_SKRYPT, TR("Skrypt &atrybutów dla Eagle (.scr)..."), "eksport");
    DodajPozycje(menuProjekty, ID_PROJ_EAGLE_ULP, TR("Program ULP dla Eagle (lista części do CSV)..."), "narzedzia");

    wxMenu* menuKatalog = new wxMenu;
    DodajPozycje(menuKatalog, ID_KATALOG_NOWA, TR("&Nowa pozycja katalogu..."), "nowy");
    DodajPozycje(menuKatalog, ID_KATALOG_EDYTUJ, TR("&Edytuj pozycję..."), "edytuj");
    DodajPozycje(menuKatalog, ID_KATALOG_USUN, TR("&Usuń pozycję"), "usun");
    menuKatalog->AppendSeparator();
    menuKatalog->Append(ID_KATALOG_DO_MAGAZYNU, TR("&Dodaj do magazynu..."));
    menuKatalog->AppendSeparator();
    DodajPozycje(menuKatalog, ID_KATALOG_IMPORT, TR("&Importuj katalog z CSV..."), "import");
    DodajPozycje(menuKatalog, ID_KATALOG_EKSPORT, TR("E&ksportuj katalog do CSV..."), "eksport");
    menuKatalog->Append(ID_KATALOG_SZABLON, TR("Zapisz &szablon CSV..."));
    menuKatalog->AppendSeparator();
    menuKatalog->Append(ID_KATALOG_DS_OTWORZ, TR("&Otwórz datasheet"));
    menuKatalog->Append(ID_KATALOG_DS_ODCZYT, TR("Odczytaj &parametry z datasheetu (PDF)..."));

    wxMenu* menuRaporty = new wxMenu;
    DodajPozycje(menuRaporty, ID_RAPORTY, TR("&Raporty...\tCtrl+R"), "raport");

    wxMenu* menuSklepy = new wxMenu;
    DodajPozycje(menuSklepy, ID_SKLEPY, TR("&Lista sklepów...\tCtrl+Shift+S"), "sklep");
    menuSklepy->Append(ID_SKLEPY_ELEMENTU, TR("Sklepy, ceny i &datasheet wybranego elementu...\tCtrl+K"));
    menuSklepy->AppendSeparator();
    DodajPozycje(menuSklepy, ID_ZESTAWIENIE, TR("&Najtańsze zestawienie zakupów...\tCtrl+Shift+Z"), "sklep");

    wxMenu* menuNarzedzia = new wxMenu;
    wxMenu* menuWidok = new wxMenu;
    DodajPozycje(menuNarzedzia, ID_NARZEDZIA_KODY, TR("&Paski kolorów i kody SMD...\tCtrl+D"), "narzedzia");
    menuNarzedzia->Append(ID_NARZEDZIA_KALKULATORY, TR("&Kalkulatory (dzielnik, LED, filtr RC)...\tCtrl+Shift+K"));

    DodajPozycje(menuWidok, ID_ODSWIEZ, TR("&Odśwież\tF5"), "odswiez");
    DodajPozycje(menuWidok, ID_SZUKAJ, TR("&Szukaj\tCtrl+F"), "szukaj");
    menuWidok->AppendCheckItem(ID_DZIENNIK, TR("Pokaż &dziennik zdarzeń"))->Check(true);

    wxMenu* menuUstawienia = new wxMenu;
    menuUstawienia->Append(ID_USTAWIENIA_APLIKACJI, TR("&Ustawienia aplikacji..."));

    wxMenu* menuPomoc = new wxMenu;
    DodajPozycje(menuPomoc, wxID_HELP, TR("&Spis treści\tF1"), "pomoc");
    menuPomoc->Append(ID_POMOC_PRZEGLADARKA, TR("Otwórz pomoc w &przeglądarce"));
    menuPomoc->AppendSeparator();
    menuPomoc->Append(ID_POMOC_AKTUALIZACJE, TR("Sprawdź &aktualizacje..."));
    menuPomoc->AppendCheckItem(ID_POMOC_AUTOAKT, TR("Sprawdzaj aktualizacje przy &uruchomieniu"))
        ->Check(Konfiguracja::pobierz().pobierzBool("aktualizacje", "sprawdzaj", true));
    menuPomoc->AppendSeparator();
    menuPomoc->Append(wxID_ABOUT, TR("&O programie..."));

    wxMenuBar* menuBar = new wxMenuBar;
    menuBar->Append(menuProgram, TR("&Program"));
    menuBar->Append(menuBaza, TR("&Baza danych"));
    menuBar->Append(menuMagazyn, TR("&Magazyn"));
    menuBar->Append(menuProjekty, TR("Pro&jekty"));
    menuBar->Append(menuKatalog, TR("&Katalog"));
    menuBar->Append(menuRaporty, TR("&Raporty"));
    menuBar->Append(menuSklepy, TR("S&klepy"));
    menuBar->Append(menuNarzedzia, TR("&Narzędzia"));
    menuBar->Append(menuWidok, TR("&Widok"));
    menuBar->Append(menuUstawienia, TR("&Ustawienia"));
    menuBar->Append(menuPomoc, TR("Pomo&c"));
    SetMenuBar(menuBar);
}

// ---------------------------------------------------------------------------
// Pomocnicze
// ---------------------------------------------------------------------------

void OknoGlowne::Zapisz(const wxString& tekst) {
    dziennik->AppendText(wxDateTime::Now().FormatISOTime() + "  " + tekst + "\n");
}

void OknoGlowne::OdswiezStatus() {
    ManagerBazyDanych& manager = ManagerBazyDanych::pobierz();
    if (laczenie) {
        SetStatusText(TR("Łączenie z bazą..."), 1);
    } else if (manager.czyPolaczony()) {
        const UstawieniaBazy& us = manager.pobierzUstawienia();
        SetStatusText(TekstZUTF8(us.host + ":" + std::to_string(us.port) + " / " + us.bazaDanych),
                      1);
    } else {
        SetStatusText(TR("Brak połączenia z bazą"), 1);
    }
}

void OknoGlowne::OnAktualizujMenu(wxUpdateUIEvent& event) {
    const bool polaczony = ManagerBazyDanych::pobierz().czyPolaczony();
    switch (event.GetId()) {
        case ID_POLACZ:
            event.Enable(!laczenie && !polaczony);
            break;
        case ID_ROZLACZ:
        case ID_INICJALIZUJ_BAZE:
        case ID_INFO_O_BAZIE:
            event.Enable(!laczenie && polaczony);
            break;
        case ID_USTAWIENIA_POLACZENIA:
            event.Enable(!laczenie);
            break;
        case ID_ELEMENT_EDYTUJ:
        case ID_ELEMENT_STAN:
        case ID_ELEMENT_USUN:
            event.Enable(bazaGotowa && panelElementow->MaWybor());
            break;
        case ID_SKLEPY_ELEMENTU:
            event.Enable(bazaGotowa && panelElementow->MaWybor());
            break;
        case ID_KATALOG_NOWA:
        case ID_KATALOG_IMPORT:
        case ID_KATALOG_EKSPORT:
        case ID_KATALOG_SZABLON:
            event.Enable(bazaGotowa);
            break;
        case ID_KATALOG_EDYTUJ:
        case ID_KATALOG_USUN:
        case ID_KATALOG_DO_MAGAZYNU:
        case ID_KATALOG_DS_OTWORZ:
        case ID_KATALOG_DS_ODCZYT:
            event.Enable(bazaGotowa && panelKatalogu->MaWybor());
            break;
        case ID_PROJ_NOWY:
        case ID_SKLEPY:
        case ID_ZESTAWIENIE:
        case ID_RAPORTY:
        case ID_ETYKIETY:
        case ID_ODCZYT_KODU:
        case ID_GENERATOR_SZEREGOW:
        case ID_IMPORT_ELEMENTOW:
            event.Enable(bazaGotowa);
            break;
        case ID_PROJ_EDYTUJ:
        case ID_PROJ_USUN:
        case ID_PROJ_IMPORT:
        case ID_PROJ_DODAJ_CZESC:
        case ID_PROJ_WYDAJ_WSZYSTKO:
        case ID_PROJ_ZAKUPY:
        case ID_PROJ_EAGLE_SKRYPT:
            event.Enable(bazaGotowa && panelProjektow->MaProjekt());
            break;
        case ID_PROJ_EAGLE_ULP:
            event.Enable(true);
            break;
        case ID_KAT_NAZWA:
        case ID_KAT_USUN:
            event.Enable(bazaGotowa && panelKategorii->CzyWybranaPrawdziwaKategoria());
            break;
        case ID_ELEMENT_NOWY:
        case ID_KAT_NOWA:
        case ID_KAT_PODKATEGORIA:
        case ID_IMPORT_STARTOWE:
        case ID_IMPORT_KATEGORII:
        case ID_EKSPORT_CSV:
        case ID_ODSWIEZ:
        case ID_SZUKAJ:
            event.Enable(bazaGotowa);
            break;
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// Łączenie
// ---------------------------------------------------------------------------

void OknoGlowne::Polacz() {
    if (laczenie) return;
    ManagerBazyDanych& manager = ManagerBazyDanych::pobierz();

    // Gdy hasło nie jest zapamiętywane, a nie wpisano go w tej sesji, zapytaj
    UstawieniaBazy us = manager.pobierzUstawienia();
    if (!us.zapamietajHaslo && us.haslo.empty()) {
        wxPasswordEntryDialog dlg(this,
            TR("Hasło użytkownika ") + TekstZUTF8(us.uzytkownik) + TR(" do bazy ") +
                TekstZUTF8(us.bazaDanych) + ":",
            TR("Hasło do bazy danych"));
        if (dlg.ShowModal() != wxID_OK) {
            Zapisz(TR("Łączenie anulowane"));
            return;
        }
        us.haslo = dlg.GetValue().utf8_string();
        manager.ustawUstawienia(us);
    }

    if (watekLaczenia.joinable()) watekLaczenia.join();

    laczenie = true;
    OdswiezStatus();
    Zapisz(TR("Łączenie z ") + TekstZUTF8(us.host + ":" + std::to_string(us.port) + "/" + us.bazaDanych) + "...");

    watekLaczenia = std::thread([this] {
        ManagerBazyDanych& m = ManagerBazyDanych::pobierz();
        const bool ok = m.polacz();
        const std::string blad = ok ? std::string() : m.pobierzBlad();
        CallAfter([this, ok, blad] { NaZakonczeniuLaczenia(ok, blad); });
    });
}

void OknoGlowne::NaZakonczeniuLaczenia(bool ok, const std::string& blad) {
    if (watekLaczenia.joinable()) watekLaczenia.join();
    laczenie = false;
    OdswiezStatus();

    if (!ok) {
        Zapisz(TR("✗ Nie udało się połączyć: ") + TekstZUTF8(blad));
        SetStatusText(TR("Brak połączenia z bazą"), 0);
        bazaGotowa = false;
        OdswiezDane();
        return;
    }

    Zapisz(TR("✓ Połączono z bazą danych"));
    SetStatusText(TR("Połączono z bazą danych"), 0);
    SprawdzStrukture();
}

void OknoGlowne::SprawdzStrukture() {
    ManagerBazyDanych& manager = ManagerBazyDanych::pobierz();
    const InformacjeOBazie info = manager.pobierzInformacje();
    if (!info.polaczono) return;

    Zapisz(TR("  PostgreSQL ") + TekstZUTF8(info.wersjaSerwera) + TR(", użytkownik ") +
           TekstZUTF8(info.uzytkownik));

    if (!info.zainicjalizowana) {
        Zapisz(TR("  Baza nie zawiera struktury programu."));
        const int odp = wxMessageBox(
            TR("Baza „") + TekstZUTF8(info.baza) +
                TR("” nie zawiera jeszcze tabel programu.\n\nCzy utworzyć strukturę bazy teraz?"),
            TR("Brak struktury bazy"), wxYES_NO | wxICON_QUESTION, this);
        if (odp == wxYES) InicjalizujBaze();
    } else if (info.wersjaSchematu < ManagerBazyDanych::WYMAGANA_WERSJA_SCHEMATU) {
        Zapisz(TR("  Struktura bazy jest przestarzała."));
        const int odp = wxMessageBox(
            TR("Struktura bazy jest w starszej wersji niż wymaga program.\n\n"
               "Zaktualizować ją teraz? (Dane nie zostaną usunięte.)"),
            TR("Aktualizacja bazy"), wxYES_NO | wxICON_QUESTION, this);
        if (odp == wxYES) InicjalizujBaze();
    } else {
        Zapisz(TR("  Struktura bazy: wersja ") + wxString::Format("%d", info.wersjaSchematu));
    }

    bazaGotowa = manager.czyBazaZainicjalizowana();
    if (bazaGotowa) PomocGUI::WczytajOpisyZBazy();   // opisy pól z bazy nadpisują te z pliku
    OdswiezDane();
}

void OknoGlowne::OdswiezDane() {
    const bool gotowa = bazaGotowa && ManagerBazyDanych::pobierz().czyPolaczony();
    panelKategorii->Enable(gotowa);
    panelElementow->Enable(gotowa);
    panelPodgladu->Enable(gotowa);
    panelProjektow->Enable(gotowa);
    panelKatalogu->Enable(gotowa);
    if (!gotowa) {
        panelKatalogu->Wyczysc();
        panelKategorii->Wyczysc();
        panelElementow->Wyczysc();
        panelProjektow->Wyczysc();
        return;
    }
    panelProjektow->Odswiez();
    panelKatalogu->Odswiez();
    panelKategorii->Odswiez();
    panelElementow->UstawKategorie(panelKategorii->WybranaKategoria());
}

void OknoGlowne::InicjalizujBaze() {
    ManagerBazyDanych& manager = ManagerBazyDanych::pobierz();

    std::string raport;
    bool ok;
    {
        wxBusyCursor zajety;
        ok = manager.inicjalizujBaze(&raport);
    }

    if (ok) {
        Zapisz(TR("✓ Struktura bazy utworzona / zaktualizowana"));
        SetStatusText(TR("Struktura bazy gotowa"), 0);
        bazaGotowa = true;
        OdswiezDane();
        wxMessageBox(TR("Struktura bazy została utworzona lub zaktualizowana.\n\n") + TekstZUTF8(raport),
                     TR("Baza danych"), wxOK | wxICON_INFORMATION, this);
    } else {
        Zapisz(TR("✗ Błąd tworzenia struktury bazy: ") + TekstZUTF8(manager.pobierzBlad()));
        wxMessageBox(TR("Nie udało się utworzyć struktury bazy:\n\n") + TekstZUTF8(manager.pobierzBlad()) +
                         (raport.empty() ? wxString() : TR("\n\nPostęp:\n") + TekstZUTF8(raport)),
                     TR("Błąd bazy danych"), wxOK | wxICON_ERROR, this);
    }
}

// ---------------------------------------------------------------------------
// Menu
// ---------------------------------------------------------------------------

void OknoGlowne::OnZakoncz(wxCommandEvent&) {
    Close(true);
}

void OknoGlowne::OnPolacz(wxCommandEvent&) {
    Polacz();
}

void OknoGlowne::OnRozlacz(wxCommandEvent&) {
    ManagerBazyDanych::pobierz().rozlacz();
    bazaGotowa = false;
    OdswiezDane();
    Zapisz(TR("Rozłączono z bazą danych"));
    SetStatusText(TR("Rozłączono"), 0);
    OdswiezStatus();
}

void OknoGlowne::OnUstawieniaPolaczenia(wxCommandEvent&) {
    DialogPolaczenia dialog(this);
    if (dialog.ShowModal() == wxID_OK) {
        ManagerBazyDanych::pobierz().rozlacz();
        bazaGotowa = false;
        OdswiezDane();
        Zapisz(TR("Zapisano ustawienia połączenia"));
        Polacz();
    }
}

void OknoGlowne::OnInicjalizujBaze(wxCommandEvent&) {
    ManagerBazyDanych& manager = ManagerBazyDanych::pobierz();
    if (manager.czyBazaZainicjalizowana()) {
        const int odp = wxMessageBox(
            TR("Struktura bazy już istnieje.\n\nUruchomić aktualizację? Skrypty są bezpieczne: "
               "uzupełniają brakujące elementy i nie usuwają danych."),
            TR("Aktualizacja struktury bazy"), wxYES_NO | wxICON_QUESTION, this);
        if (odp != wxYES) return;
    }
    InicjalizujBaze();
}

void OknoGlowne::OnInfoOBazie(wxCommandEvent&) {
    const InformacjeOBazie info = ManagerBazyDanych::pobierz().pobierzInformacje();
    if (!info.polaczono) {
        wxMessageBox(TR("Brak połączenia z bazą danych."), TR("Informacje o bazie"),
                     wxOK | wxICON_INFORMATION, this);
        return;
    }

    wxString tekst;
    tekst << TR("Serwer: ") << TekstZUTF8(info.host) << ":" << info.port << "\n"
          << TR("Baza: ") << TekstZUTF8(info.baza) << "\n"
          << TR("Użytkownik: ") << TekstZUTF8(info.uzytkownik) << "\n"
          << TR("Wersja PostgreSQL: ") << TekstZUTF8(info.wersjaSerwera) << "\n"
          << TR("Struktura programu: ")
          << (info.zainicjalizowana ? TR("wersja ") + wxString::Format("%d", info.wersjaSchematu)
                                    : TR("brak (użyj Baza danych → Utwórz / zaktualizuj)"));
    wxMessageBox(tekst, TR("Informacje o bazie"), wxOK | wxICON_INFORMATION, this);
}

void OknoGlowne::OnUstawieniaAplikacji(wxCommandEvent&) {
    DialogUstawien dialog(this);
    dialog.ShowModal();
    if (dialog.ZmienionoPolaczenie()) {
        ManagerBazyDanych::pobierz().rozlacz();
        bazaGotowa = false;
        OdswiezDane();
        Polacz();
    }
}

void OknoGlowne::OnOProgramie(wxCommandEvent&) {
    wxMessageBox(TR("Domowy Magazyn ") + wxString::FromUTF8(DM_VERSION) +
                     TR("\n\nProgram do zarządzania częściami elektronicznymi."),
                 TR("O programie"), wxOK | wxICON_INFORMATION, this);
}

// ---------------------------------------------------------------------------
// Magazyn
// ---------------------------------------------------------------------------

void OknoGlowne::OnElementNowy(wxCommandEvent&) { panelElementow->NowyElement(); }
void OknoGlowne::OnElementEdytuj(wxCommandEvent&) { panelElementow->EdytujWybrany(); }
void OknoGlowne::OnElementStan(wxCommandEvent&) { panelElementow->ZmienStanWybranego(); }
void OknoGlowne::OnElementUsun(wxCommandEvent&) { panelElementow->UsunWybrany(); }
void OknoGlowne::OnKategoriaNowa(wxCommandEvent&) { panelKategorii->DodajKategorieGlowna(); }
void OknoGlowne::OnKategoriaPodkategoria(wxCommandEvent&) { panelKategorii->DodajPodkategorie(); }
void OknoGlowne::OnKategoriaNazwa(wxCommandEvent&) { panelKategorii->ZmienNazwe(); }
void OknoGlowne::OnKategoriaUsun(wxCommandEvent&) { panelKategorii->UsunKategorie(); }

void OknoGlowne::OnOdswiez(wxCommandEvent&) {
    OdswiezDane();
}

void OknoGlowne::OnSzukaj(wxCommandEvent&) {
    panelElementow->FokusSzukania();
}

void OknoGlowne::OnPokazDziennik(wxCommandEvent& event) {
    uklad->Show(dziennik, event.IsChecked());
    Layout();
}

void OknoGlowne::OnImportKategoriiStartowych(wxCommandEvent&) {
    const std::string katalog = ManagerBazyDanych::pobierz().znajdzKatalogSQL();
    if (katalog.empty()) {
        wxMessageBox(TR("Nie znaleziono katalogu „sql” ze skryptami obok programu."),
                     TR("Import kategorii"), wxOK | wxICON_ERROR, this);
        return;
    }
    ImportujKategorie(katalog + "/kategorie_startowe.txt");
}

void OknoGlowne::OnImportKategoriiZPliku(wxCommandEvent&) {
    wxFileDialog dlg(this, TR("Wybierz plik z kategoriami"), wxEmptyString, wxEmptyString,
                     TR("Pliki tekstowe (*.txt;*.csv)|*.txt;*.csv|Wszystkie pliki|*"),
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() != wxID_OK) return;
    ImportujKategorie(dlg.GetPath().utf8_string());
}

void OknoGlowne::ImportujKategorie(const std::string& sciezka) {
    ImportKategorii::Wynik wynik;
    std::string blad;
    bool ok;
    {
        wxBusyCursor zajety;
        ok = ImportKategorii::importujZPliku(logika, sciezka, wynik, blad);
    }
    if (!ok) {
        Zapisz(TR("✗ Import kategorii nie powiódł się: ") + TekstZUTF8(blad));
        wxMessageBox(TekstZUTF8(blad), TR("Import kategorii"), wxOK | wxICON_ERROR, this);
        return;
    }
    const wxString opis = TR("Wczytano ścieżek: ") + wxString::Format("%d", wynik.sciezek) +
                          TR(", dodano nowych kategorii: ") + wxString::Format("%d", wynik.dodano);
    Zapisz(TR("✓ Import kategorii. ") + opis);
    panelKategorii->Odswiez();
    panelElementow->Odswiez();
    wxMessageBox(opis + (wynik.dodano == 0 ? TR("\n\nWszystkie kategorie już istniały.") : wxString()),
                 TR("Import kategorii"), wxOK | wxICON_INFORMATION, this);
}

void OknoGlowne::OnEtykiety(wxCommandEvent&) {
    const auto& elementy = panelElementow->Elementy();
    if (elementy.empty()) {
        wxMessageBox(TR("Lista elementów jest pusta, nie ma czego drukować."), TR("Etykiety"),
                     wxOK | wxICON_INFORMATION, this);
        return;
    }
    const Element* wybrany = panelElementow->Wybrany();
    DialogEtykiet dlg(this, elementy, wybrany ? wybrany->id : 0);
    dlg.ShowModal();
}

void OknoGlowne::OnNarzedziaKody(wxCommandEvent&) {
    DialogKodow dlg(this);
    dlg.ShowModal();
}

void OknoGlowne::OnNarzedziaKalkulatory(wxCommandEvent&) {
    DialogKalkulatorow dlg(this);
    dlg.ShowModal();
}

void OknoGlowne::OnGeneratorSzeregow(wxCommandEvent&) {
    DialogImportuElementow dlg(this, logika, DialogImportuElementow::Tryb::Szereg);
    if (dlg.ShowModal() != wxID_OK) return;
    const auto& w = dlg.Wynik();
    Zapisz(wxString::Format(TR("✓ Generator szeregów: dodano %d, dodano ilość %d, uzupełniono %d, pominięto %d"),
                            w.dodano, w.zsumowano, w.zaktualizowano, w.pominieto));
    panelKategorii->Odswiez();
    panelElementow->Odswiez();
}

void OknoGlowne::OnImportElementow(wxCommandEvent&) {
    DialogImportuElementow dlg(this, logika, DialogImportuElementow::Tryb::Plik);
    if (dlg.ShowModal() != wxID_OK) return;
    const auto& w = dlg.Wynik();
    Zapisz(wxString::Format(TR("✓ Import CSV: dodano %d, dodano ilość %d, uzupełniono %d, pominięto %d, błędów %zu"),
                            w.dodano, w.zsumowano, w.zaktualizowano, w.pominieto, w.bledy.size()));
    panelKategorii->Odswiez();
    panelElementow->Odswiez();
}

void OknoGlowne::OnOdczytKodu(wxCommandEvent&) {
    wxFileDialog dlg(this, TR("Wybierz zdjęcie etykiety z kodem kreskowym"), wxEmptyString, wxEmptyString,
                     TR("Obrazy (*.jpg;*.jpeg;*.png;*.bmp;*.gif)|*.jpg;*.jpeg;*.png;*.bmp;*.gif|Wszystkie pliki|*.*"),
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() != wxID_OK) return;
    wxString blad;
    const auto kod = OdczytajKodZPliku(dlg.GetPath(), blad);
    if (!kod) {
        Zapisz(TR("✗ ") + blad);
        wxMessageBox(blad, TR("Odczyt kodu"), wxOK | wxICON_WARNING, this);
        return;
    }
    zakladki->SetSelection(0);
    Zapisz(TR("Odczytano kod: ") + TekstZUTF8(*kod));
    panelElementow->Szukaj(TekstZUTF8(*kod));
}

void OknoGlowne::OnRaporty(wxCommandEvent&) {
    DialogRaportu dlg(this, logikaRaportow);
    dlg.ShowModal();
}

void OknoGlowne::OnSklepy(wxCommandEvent&) {
    DialogSklepow dlg(this, logikaSklepow);
    dlg.ShowModal();
}

void OknoGlowne::OnSklepyElementu(wxCommandEvent&) {
    const Element* e = panelElementow->Wybrany();
    if (!e) return;
    DialogLinkowElementu dlg(this, logikaSklepow, logika, e->id);
    dlg.ShowModal();
    panelElementow->Odswiez();
}

void OknoGlowne::OnZestawienie(wxCommandEvent&) {
    std::optional<long long> idProjektu;
    if (zakladki->GetSelection() == 1) idProjektu = panelProjektow->WybranyId();
    DialogZestawienia dlg(this, logikaProjektow, logikaSklepow, idProjektu);
    dlg.ShowModal();
}

void OknoGlowne::PokazKatalog() { zakladki->SetSelection(2); }

void OknoGlowne::OnKatalog(wxCommandEvent& event) {
    zakladki->SetSelection(2);
    switch (event.GetId()) {
        case ID_KATALOG_NOWA: panelKatalogu->Nowa(); break;
        case ID_KATALOG_EDYTUJ: panelKatalogu->Edytuj(); break;
        case ID_KATALOG_USUN: panelKatalogu->Usun(); break;
        case ID_KATALOG_DO_MAGAZYNU: panelKatalogu->DodajDoMagazynu(); break;
        case ID_KATALOG_IMPORT: panelKatalogu->ImportujCSV(); break;
        case ID_KATALOG_EKSPORT: panelKatalogu->EksportujCSV(); break;
        case ID_KATALOG_SZABLON: panelKatalogu->ZapiszSzablon(); break;
        case ID_KATALOG_DS_OTWORZ: panelKatalogu->OtworzDatasheet(); break;
        case ID_KATALOG_DS_ODCZYT: panelKatalogu->OdczytajDatasheet(); break;
    }
}

void OknoGlowne::OnProjekt(wxCommandEvent& event) {
    zakladki->SetSelection(1);
    switch (event.GetId()) {
        case ID_PROJ_NOWY: panelProjektow->NowyProjekt(); break;
        case ID_PROJ_EDYTUJ: panelProjektow->EdytujProjekt(); break;
        case ID_PROJ_USUN: panelProjektow->UsunProjekt(); break;
        case ID_PROJ_IMPORT: panelProjektow->ImportujBOM(); break;
        case ID_PROJ_DODAJ_CZESC: panelProjektow->DodajCzesc(); break;
        case ID_PROJ_WYDAJ_WSZYSTKO: panelProjektow->WydajWszystko(); break;
        case ID_PROJ_ZAKUPY: panelProjektow->ListaZakupow(); break;
        case ID_PROJ_EAGLE_SKRYPT: panelProjektow->SkryptEagle(); break;
        case ID_PROJ_EAGLE_ULP: panelProjektow->ZapiszUlp(); break;
        default: break;
    }
}

void OknoGlowne::OnEksportCSV(wxCommandEvent&) {
    const auto& elementy = panelElementow->Elementy();
    if (elementy.empty()) {
        wxMessageBox(TR("Lista elementów jest pusta, nie ma czego eksportować."), TR("Eksport do CSV"),
                     wxOK | wxICON_INFORMATION, this);
        return;
    }
    wxFileDialog dlg(this, TR("Eksport listy do CSV"), wxEmptyString,
                     "elementy_" + wxDateTime::Now().Format("%Y%m%d") + ".csv",
                     TR("Pliki CSV (*.csv)|*.csv"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() != wxID_OK) return;

    std::string blad;
    if (!EksportCSV::zapiszElementy(dlg.GetPath().utf8_string(), elementy, blad)) {
        Zapisz(TR("✗ Eksport nie powiódł się: ") + TekstZUTF8(blad));
        wxMessageBox(TekstZUTF8(blad), TR("Eksport do CSV"), wxOK | wxICON_ERROR, this);
        return;
    }
    Zapisz(TR("✓ Wyeksportowano ") + wxString::Format("%zu", elementy.size()) + TR(" elementów do: ") + dlg.GetPath());
    SetStatusText(TR("Wyeksportowano do CSV"), 0);
}


// ---------------------------------------------------------------------------
// Sprawdzanie aktualizacji
// ---------------------------------------------------------------------------
void OknoGlowne::SprawdzAktualizacjeNaStarcie() {
    Konfiguracja& k = Konfiguracja::pobierz();
    if (wxGetEnv("DM_BEZ_AKTUALIZACJI", nullptr)) return;          // testy automatyczne
    if (!k.pobierzBool("aktualizacje", "sprawdzaj", true)) return;
    // najwyżej raz dziennie, żeby nie odpytywać GitHuba przy każdym uruchomieniu
    const std::string dzis = wxDateTime::Today().FormatISODate().utf8_string();
    if (k.pobierz("aktualizacje", "ostatnie", "") == dzis) return;
    k.ustaw("aktualizacje", "ostatnie", dzis);
    k.zapisz();
    SprawdzAktualizacje(true);
}

void OknoGlowne::SprawdzAktualizacje(bool cicho) {
    const std::string adres = Konfiguracja::pobierz().pobierz("aktualizacje", "adres", Aktualizacje::adresWersji());
    wxWeakRef<OknoGlowne> okno(this);
    if (!cicho) Zapisz(TR("Sprawdzam, czy jest nowsza wersja..."));
    KlientHTTP::pobierzStrone(adres, [okno, cicho](const WynikHTTP& w) {
        if (!okno) return;
        auto blad = [&](const wxString& powod) {
            if (cicho) return;
            wxMessageBox(TR("Nie udało się sprawdzić aktualizacji.\n\n") + powod, TR("Aktualizacje"), wxOK | wxICON_WARNING, okno);
        };
        if (!w.ok) { blad(TekstZUTF8(w.blad.empty() ? "Brak odpowiedzi serwera (kod " + std::to_string(w.kod) + ")." : w.blad)); return; }
        const auto info = Aktualizacje::parsujPlikWersji(w.tresc);
        if (!info) { blad(TR("Plik z numerem wersji ma nieoczekiwaną treść.")); return; }
        if (!Aktualizacje::nowsza(info->wersja, DM_VERSION)) {
            if (!cicho)
                wxMessageBox(TR("Masz najnowszą wersję programu (") + wxString::FromUTF8(DM_VERSION) + ").", TR("Aktualizacje"),
                             wxOK | wxICON_INFORMATION, okno);
            return;
        }
        wxString tekst = TR("Dostępna jest nowa wersja programu: ") + TekstZUTF8(info->wersja) + TR(" (masz ") +
                         wxString::FromUTF8(DM_VERSION) + ").";
        if (!info->opis.empty()) tekst += "\n\n" + TekstZUTF8(info->opis);
        tekst += TR("\n\nOtworzyć stronę pobierania?");
        if (wxMessageBox(tekst, TR("Aktualizacje"), wxYES_NO | wxICON_INFORMATION, okno) == wxYES)
            wxLaunchDefaultBrowser(TekstZUTF8(info->adresPobierania.empty() ? Aktualizacje::adresStronyPobierania() : info->adresPobierania));
    });
}
