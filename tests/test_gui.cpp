// Test dymny GUI: tworzy okno główne i dialogi, sprawdza, że wxWidgets nie
// zgłasza asercji (np. błędnych rodziców kontrolek), że przyciski robią to, co
// trzeba, i zapisuje zrzuty ekranu do katalogu z DM_TEST_SHOTS.
//
// Uruchamianie wymaga ekranu (na serwerze: xvfb-run) oraz PostgreSQL:
//   DM_TEST_HOST, DM_TEST_PORT, DM_TEST_DB, DM_TEST_USER, DM_TEST_PASSWORD
//   DM_TEST_SQL  - katalog ze skryptami SQL
//   DM_TEST_SHOTS - katalog na zrzuty (opcjonalnie)

#include <wx/wx.h>
#include <wx/dcscreen.h>
#include <wx/filename.h>
#include <wx/ffile.h>
#include <wx/evtloop.h>
#include <wx/timer.h>

#include <cstdlib>
#include <functional>
#include <vector>
#include <fstream>
#include <iostream>

#include "BazaDanych/ManagerBazyDanych.h"
#include "GUI/DialogElementu.h"
#include "GUI/DialogEtykiet.h"
#include "GUI/DialogImportuElementow.h"
#include "GUI/DialogKodow.h"
#include "GUI/DialogKalkulatorow.h"
#include "GUI/DialogKamery.h"
#include "GUI/PanelPodgladu.h"
#include "GUI/DialogImportBOM.h"
#include "GUI/DialogRaportu.h"
#include "GUI/DialogSklepow.h"
#include "GUI/DialogDodajSklep.h"
#include "GUI/DialogLinkowElementu.h"
#include "GUI/DialogZestawienia.h"
#include "GUI/OdczytKoduZObrazu.h"
#include "GUI/DialogProjektu.h"
#include "GUI/PanelProjektow.h"
#include "GUI/PanelKatalogu.h"
#include "GUI/DialogKatalogu.h"
#include "Logika/LogikaKatalogu.h"
#include "Logika/OdczytDatasheetu.h"
#include "GUI/DialogPolaczenia.h"
#include "GUI/DialogUstawien.h"
#include "GUI/OknoGlowne.h"
#include "GUI/PanelElementow.h"
#include "GUI/PanelKategorii.h"
#include "Logika/LogikaMagazynu.h"
#include "Narzedzia/Konfiguracja.h"
#include "Narzedzia/KodKreskowy.h"
#include "Pomoc/PomocGUI.h"

static int bledy = 0;
#define CHECK(w)                                                                   \
    do {                                                                           \
        if (!(w)) {                                                                \
            ++bledy;                                                               \
            std::cerr << "BŁĄD " << __FILE__ << ":" << __LINE__ << "  " #w "\n";  \
        }                                                                          \
    } while (0)

static std::string env(const char* n, const char* d) {
    const char* v = std::getenv(n);
    return v ? v : d;
}

/// Uruchamia na chwilę zagnieżdżoną pętlę zdarzeń, żeby okna zdążyły się narysować
static void PrzerwaNaRysowanie(int ms) {
    wxEvtHandler wlasciciel;
    wxTimer timer(&wlasciciel);
    wxEventLoop petla;
    wlasciciel.Bind(wxEVT_TIMER, [&petla](wxTimerEvent&) { petla.Exit(); });
    timer.StartOnce(ms);
    petla.Run();
}

static void Zrzut(wxWindow* okno, const wxString& nazwa) {
    std::cerr << "krok: " << nazwa.utf8_string() << std::endl;
    const std::string katalog = env("DM_TEST_SHOTS", "");
    if (katalog.empty() || !okno) return;
    // GTK rysuje dopiero w prawdziwej pętli zdarzeń (samo wxYield nie wystarcza)
    okno->Raise();
    PrzerwaNaRysowanie(500);
    wxScreenDC ekran;
    const wxRect r = okno->GetScreenRect();
    wxBitmap bmp(r.width, r.height);
    wxMemoryDC mem(bmp);
    mem.Blit(0, 0, r.width, r.height, &ekran, r.x, r.y);
    mem.SelectObject(wxNullBitmap);
    bmp.SaveFile(wxString::FromUTF8(katalog) + "/" + nazwa + ".png", wxBITMAP_TYPE_PNG);
}

/// Zrzut całego ekranu zewnętrznym programem (ImageMagick "import"): pewniejszy dla okien modalnych bez menedżera okien
static void ZrzutEkranu(const wxString& nazwa) {
    std::cerr << "krok: " << nazwa.utf8_string() << std::endl;
    const std::string katalog = env("DM_TEST_SHOTS", "");
    if (katalog.empty()) return;
    PrzerwaNaRysowanie(1200);
    wxExecute("import -window root \"" + wxString::FromUTF8(katalog) + "/" + nazwa + ".png\"", wxEXEC_SYNC);
}

static wxWindow* Znajdz(wxWindow* rodzic, const wxString& etykieta) {
    for (wxWindow* dziecko : rodzic->GetChildren()) {
        if (auto* kontrolka = wxDynamicCast(dziecko, wxControl)) {
            if (kontrolka->GetLabelText() == etykieta) return kontrolka;
        }
        if (wxWindow* w = Znajdz(dziecko, etykieta)) return w;
    }
    return nullptr;
}

static wxWindow* ZnajdzZawierajacy(wxWindow* rodzic, const wxString& fragment) {
    for (wxWindow* dziecko : rodzic->GetChildren()) {
        if (auto* tekst = wxDynamicCast(dziecko, wxStaticText)) {
            if (tekst->GetLabel().Contains(fragment)) return tekst;
        }
        if (wxWindow* w = ZnajdzZawierajacy(dziecko, fragment)) return w;
    }
    return nullptr;
}

template <class T>
static T* ZnajdzTyp(wxWindow* rodzic) {
    for (wxWindow* dziecko : rodzic->GetChildren()) {
        if (auto* t = dynamic_cast<T*>(dziecko)) return t;
        if (T* w = ZnajdzTyp<T>(dziecko)) return w;
    }
    return nullptr;
}

static wxWindow* Gorne() {
    return wxTopLevelWindows.GetLast()->GetData();
}

static long long Liczba(ManagerBazyDanych& m, const char* sql) {
    auto w = m.zapytanie(sql);
    return (w && w->liczbaWierszy() == 1) ? w->liczba(0, 0) : -1;
}

static void Kliknij(wxWindow* przycisk) {
    wxCommandEvent zdarzenie(wxEVT_BUTTON, przycisk->GetId());
    zdarzenie.SetEventObject(przycisk);
    przycisk->GetEventHandler()->ProcessEvent(zdarzenie);
}

class TestApp : public wxApp {
public:
    bool OnInit() override {
        setenv("DM_BEZ_AKTUALIZACJI", "1", 1);   // test nie odpytuje GitHuba
        wxInitAllImageHandlers();
        const wxString tmp = wxFileName::GetTempDir() + "/dm_gui_test";
        wxFileName::Mkdir(tmp, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
        const wxString plik = tmp + "/config.ini";
        wxRemoveFile(plik);

        ManagerBazyDanych& m = ManagerBazyDanych::pobierz();
        m.ustawSciezkeKonfiguracji(plik.utf8_string());
        m.ustawKatalogiSQL({env("DM_TEST_SQL", "../sql")});

        UstawieniaBazy us;
        us.host = env("DM_TEST_HOST", "localhost");
        us.port = std::atoi(env("DM_TEST_PORT", "5432").c_str());
        us.bazaDanych = env("DM_TEST_DB", "dm_test");
        us.uzytkownik = env("DM_TEST_USER", "postgres");
        us.haslo = env("DM_TEST_PASSWORD", "");
        us.timeoutSek = 3;
        m.ustawUstawienia(us);
        CHECK(m.zapiszUstawienia());

        CallAfter([this] { Uruchom(); });
        return true;
    }

    void OnAssertFailure(const wxChar* plik, int linia, const wxChar* funkcja,
                         const wxChar* warunek, const wxChar* komunikat) override {
        ++bledy;
        std::cerr << "ASERCJA wx: " << wxString(plik) << ":" << linia << " " << wxString(warunek)
                  << " " << wxString(komunikat) << "\n";
    }

    int OnExit() override {
        ManagerBazyDanych::pobierz().rozlacz();
        return wxApp::OnExit();
    }

private:
    // =====================================================================
    // Etap 2: drzewo kategorii, lista elementów, formularze
    // =====================================================================
    // =====================================================================
    // Pomoc: menu, opisy pól (dymki), F1, okno pomocy
    // =====================================================================
    void TestPomocy(OknoGlowne* okno) {
        ManagerBazyDanych& m = ManagerBazyDanych::pobierz();
        LogikaMagazynu logika(m);
        CHECK(!PomocGUI::ZnajdzKatalogPomocy().empty());
        PomocGUI::WczytajOpisyZPliku();
        PomocGUI::WczytajOpisyZBazy();
        CHECK(okno->GetMenuBar()->FindItem(wxID_HELP) != nullptr);

        // dymki i klucze F1 w formularzu elementu
        {
            Element e;
            DialogElementu d(okno, logika, e);
            auto* symbol = ZnajdzTyp<wxTextCtrl>(&d);   // pierwsze pole tekstowe to symbol
            CHECK(symbol != nullptr);
            if (symbol) {
                CHECK(PomocGUI::KluczKontrolki(symbol) == "element.symbol");
                CHECK(symbol->GetToolTipText().Contains("Nazwa elementu"));
            }
        }

        // F1 w polu: okienko z opisem pola, z przyciskiem "Więcej w pomocy"
        {
            Element e;
            DialogElementu d(okno, logika, e);
            CallAfter([&] {
                if (auto* t = ZnajdzTyp<wxTextCtrl>(&d)) t->SetFocus();
                CallAfter([&] {
                    auto* opis = dynamic_cast<wxDialog*>(Gorne());
                    CHECK(opis != nullptr && opis != &d);
                    if (!opis) return;
                    CHECK(opis->GetTitle().Contains("Symbol"));
                    ZrzutEkranu("13_f1_opis_pola");
                    opis->EndModal(wxID_OK);
                });
                PomocGUI::PokazPomocDlaFokusu();
                d.EndModal(wxID_CANCEL);
            });
            d.ShowModal();
        }

        // okno pomocy na kotwicy
        CHECK(PomocGUI::PokazPomoc("pole-wartosc"));
        wxFrame* ramka = nullptr;
        for (wxWindowList::compatibility_iterator it = wxTopLevelWindows.GetFirst(); it; it = it->GetNext()) {
            auto* f = dynamic_cast<wxFrame*>(it->GetData());
            if (f && f->GetTitle().Contains("pomoc")) ramka = f;
        }
        CHECK(ramka != nullptr);
        if (ramka) {
            wxYield();
            ZrzutEkranu("14_pomoc");
            ramka->Close(true);
        }
        wxYield();
    }

    // =====================================================================
    // Miejsce magazynowania w formularzu elementu
    // =====================================================================
    void TestMiejsca(OknoGlowne* okno) {
        ManagerBazyDanych& m = ManagerBazyDanych::pobierz();
        LogikaMagazynu logika(m);
        m.wykonaj("DELETE FROM tbl_elementy WHERE symbol LIKE 'GUIM\\_%'");
        auto kombinacje = [](wxWindow* rodzic) {
            std::vector<wxComboBox*> v;
            std::function<void(wxWindow*)> idz = [&](wxWindow* w) {
                for (wxWindow* d : w->GetChildren()) {
                    if (auto* c = dynamic_cast<wxComboBox*>(d)) v.push_back(c);
                    idz(d);
                }
            };
            idz(rodzic);
            return v;
        };
        // istniejące miejsce trafia do pól, a swobodny zapis w polu kontenera zostaje rozdzielony
        Element e; e.symbol = "GUIM_A"; e.ustawMiejsce({"GUIM Kontener 1", "A1", "1"});
        auto id = logika.zapiszElement(e);
        CHECK(id.has_value());
        if (!id) return;
        {
            auto el = logika.pobierzElement(*id);
            DialogElementu d(okno, logika, *el);
            auto combos = kombinacje(&d);
            CHECK(combos.size() == 2);   // waluta i kontener
            if (combos.size() == 2) {
                CHECK(combos[1]->GetValue() == "GUIM Kontener 1");
                CHECK(PomocGUI::KluczKontrolki(combos[1]) == "element.kontener");
                Element z = d.PobierzElement();
                CHECK(z.kontener == "GUIM Kontener 1" && z.szuflada == "A1" && z.przegroda == "1");
                combos[1]->SetValue("GUIM Kontener 2 szufladka B3/4");
                for (wxWindow* w : d.GetChildren())
                    if (auto* t = dynamic_cast<wxTextCtrl*>(w)) {
                        if (t->GetHint() == "np. A1" || t->GetHint() == "np. 1") t->SetValue("");
                    }
                z = d.PobierzElement();
                CHECK(z.kontener == "GUIM Kontener 2" && z.szuflada == "B3" && z.przegroda == "4");
                CallAfter([&] { ZrzutEkranu("15_miejsce_formularz"); d.EndModal(wxID_CANCEL); });
                d.ShowModal();
            }
        }
        // lista: kolumna Miejsce
        FiltrElementow f; f.tekst = "GUIM Kontener 1";
        std::vector<Element> w;
        logika.pobierzElementy(f, w);
        CHECK(w.size() == 1 && w[0].miejsceTekst() == "GUIM Kontener 1, A1/1");
        m.wykonaj("DELETE FROM tbl_elementy WHERE symbol LIKE 'GUIM\\_%'");
    }

    // =====================================================================
    // Generator szeregów E i import CSV
    // =====================================================================
    void TestImportuElementow(OknoGlowne* okno) {
        ManagerBazyDanych& m = ManagerBazyDanych::pobierz();
        LogikaMagazynu logika(m);
        auto sprzatanie = [&] {
            m.wykonaj("DELETE FROM tbl_transakcje WHERE id_elementu IN (SELECT id FROM tbl_elementy WHERE obudowa = 'GUII' OR symbol LIKE 'GUII\\_%')");
            m.wykonaj("DELETE FROM tbl_elementy WHERE obudowa = 'GUII' OR symbol LIKE 'GUII\\_%'");
            for (int i = 0; i < 4; ++i)
                m.wykonaj("DELETE FROM tbl_kategorie k WHERE k.nazwa LIKE 'GUII%' AND NOT EXISTS (SELECT 1 FROM tbl_kategorie c WHERE c.id_rodzica = k.id)");
        };
        sprzatanie();
        {
            DialogImportuElementow d(okno, logika, DialogImportuElementow::Tryb::Szereg);
            CHECK(d.Pozycje().size() > 50);              // domyślnie E24, 10 Ω...1 MΩ
            d.UstawPole("obudowa", "GUII");
            d.UstawPole("od", "100");
            d.UstawPole("do", "1k");
            d.UstawPole("ilosc", "7");
            d.UstawPole("kategoria", "GUII Rezystory / SMD");
            d.UstawPole("miejsce", "GUII Kontener 1 szufladka A1");
            CHECK(d.Pozycje().size() == 25 && d.LiczbaPoprawnych() == 25);
            d.UstawPole("od", "zle");
            CHECK(d.Pozycje().empty());                  // zły zakres: pusty podgląd, komunikat zamiast zapisu
            d.UstawPole("od", "100");
            CallAfter([&] { ZrzutEkranu("16_generator_szeregow"); d.EndModal(wxID_CANCEL); });
            d.ShowModal();
            CHECK(d.Zapisz());
            CHECK(d.Wynik().dodano == 25);
            d.UstawPole("duplikaty", "1");
            CHECK(d.Zapisz());
            CHECK(d.Wynik().zsumowano == 25);
            auto e = logika.znajdzPoSymbolu("R-470R-GUII-5%");
            CHECK(e && e->iloscDostepna == 14 && e->szuflada == "A1");
        }
        {
            DialogImportuElementow d(okno, logika, DialogImportuElementow::Tryb::Plik);
            const wxString sciezka = wxFileName::GetTempDir() + "/guii.csv";
            {
                wxFFile f(sciezka, "wb");
                const std::string csv = "Symbol;Obudowa;Ilość;Kategoria\nGUII_BC547;TO-92;10;GUII Tranzystory\n;;5;\n";
                f.Write(csv.data(), csv.size());
            }
            CHECK(d.WczytajPlik(sciezka, true));
            CHECK(d.Pozycje().size() == 2 && d.LiczbaPoprawnych() == 1);
            CHECK(!d.WczytajPlik("/nie/ma/takiego.csv", true));
            CHECK(d.WczytajPlik(sciezka, true));
            CHECK(d.Zapisz() && d.Wynik().dodano == 1 && d.Wynik().bledy.size() == 1);
            wxRemoveFile(sciezka);
        }
        sprzatanie();
    }

    void TestKodow(OknoGlowne* okno) {
        using K = KodyElementow::Kolor;
        DialogKodow d(okno);
        d.UstawPaski({K::Zolty, K::Fioletowy, K::Czerwony, K::Zloty});
        CHECK(d.OpisPaskow().Contains("4,7") && d.OpisPaskow().Contains("5"));
        CHECK(d.Wartosc() == "4k7");
        // 4 paski bez paska tolerancji = 20 %
        d.UstawPaski({K::Zolty, K::Fioletowy, K::Czerwony, K::Brak});
        CHECK(d.OpisPaskow().Contains("4,7") && d.OpisPaskow().Contains("20"));
        // 6 pasków: 1 kΩ 1 % 50 ppm/K
        d.UstawPaski({K::Brazowy, K::Czarny, K::Czarny, K::Brazowy, K::Brazowy, K::Czerwony});
        CHECK(d.OpisPaskow().Contains("50 ppm/K") && d.Wartosc() == "1k");
        d.UstawWartosc("4k7", 20, 3);
        CHECK(d.PaskiZWartosci().size() == 3);
        d.UstawWartosc("4k7", 20, 4);
        CHECK(d.PaskiZWartosci().size() == 4 && d.PaskiZWartosci()[3] == K::Brak);
        d.UstawWartosc("4k7", 1, 6);
        CHECK(d.PaskiZWartosci().size() == 6);
        d.UstawWartosc("330", 1, 4);
        CHECK(d.PaskiZWartosci().size() == 4);
        d.UstawWartosc("4k75", 1, 4);
        CHECK(d.PaskiZWartosci().empty());
        d.UstawWartosc("4k75", 1, 5);
        CHECK(d.PaskiZWartosci().size() == 5);
        d.UstawKodSMD("104");
        CHECK(d.OpisSMD().Contains("100 nF"));
        d.UstawKodSMD("01A");
        CHECK(d.OpisSMD().Contains("100 Ω"));
        d.UstawKodSMD("zzz");
        CHECK(d.OpisSMD().Contains("Nie rozpoznaję"));
        d.UstawPaski({K::Zolty, K::Fioletowy, K::Czerwony, K::Zloty});
        d.PokazStrone(1);
        d.UstawWartosc("4k7", 5, 4);
        CallAfter([&] { ZrzutEkranu("17b_wartosc4"); d.UstawWartosc("4k7", 5, 5); ZrzutEkranu("17c_wartosc5"); d.UstawWartosc("4k7", 1, 6); ZrzutEkranu("17d_wartosc6"); d.PokazStrone(0); d.EndModal(wxID_CLOSE); });
        d.ShowModal();
        d.UstawPaski({K::Zolty, K::Fioletowy, K::Czerwony, K::Zloty});
        CallAfter([&] { ZrzutEkranu("17_narzedzia_kody"); d.EndModal(wxID_CLOSE); });
        d.ShowModal();
    }

    // =====================================================================
    // Kamera USB (kamera testowa) i zdjęcie w panelu po prawej
    // =====================================================================
    void TestKamery(OknoGlowne* okno) {
        setenv("DM_KAMERA_TEST", "1", 1);
        ManagerBazyDanych& m = ManagerBazyDanych::pobierz();
        LogikaMagazynu logika(m);
        auto sprzatanie = [&] {
            m.wykonaj("DELETE FROM tbl_transakcje WHERE id_elementu IN (SELECT id FROM tbl_elementy WHERE symbol LIKE 'GUIK\\_%')");
            m.wykonaj("DELETE FROM tbl_elementy WHERE symbol LIKE 'GUIK\\_%'");
        };
        sprzatanie();
        Konfiguracja& kf = Konfiguracja::pobierz();
        UstawieniaBazowe stare = kf.pobierzUstawieniaAplikacji(), us = stare;
        const wxString katalog = wxFileName::GetTempDir() + "/guik_zdjecia";
        us.sciezkaObrazki = katalog.utf8_string();
        kf.ustawUstawieniaAplikacji(us);

        CHECK(Kamera::obslugiwana());
        Element e; e.symbol = "GUIK_A"; e.opis = "Element ze zdjęciem"; e.iloscDostepna = 5; e.iloscMinimalna = 2;
        auto id = logika.zapiszElement(e);
        CHECK(id.has_value());
        PanelElementow* pe = ZnajdzTyp<PanelElementow>(okno);
        PanelPodgladu* pp = ZnajdzTyp<PanelPodgladu>(okno);
        CHECK(pe && pp);
        if (!id || !pe || !pp) { kf.ustawUstawieniaAplikacji(stare); return; }
        pe->UstawKategorie(std::nullopt);   // wszystkie elementy
        pe->Szukaj("GUIK_A");
        pe->WybierzPierwszy();
        CHECK(pp->IdElementu() == *id);
        CHECK(pp->Opis().Contains("Stan: 5") && pp->Opis().Contains("minimum 2"));
        CHECK(!pp->ZdjecieWidoczne());

        wxString sciezka;
        {
            DialogKamery d(okno, "GUIK_A");
            CHECK(d.Otwarta());
            for (int i = 0; i < 3; ++i) d.Odswiez();
            CHECK(d.LiczbaKlatek() == 4);               // jedna z otwarcia plus trzy
            d.ZrobZdjecie();
            CHECK(d.Zamrozony());
            const int po = d.LiczbaKlatek();
            d.Odswiez();
            CHECK(d.LiczbaKlatek() == po);               // zamrożony obraz się nie zmienia
            d.Powtorz();
            CHECK(!d.Zamrozony());
            d.ZrobZdjecie();
            CHECK(d.Zatwierdz());
            sciezka = d.Sciezka();
            CHECK(sciezka.EndsWith(".jpg") && sciezka.Contains("GUIK_A") && wxFileExists(sciezka));
            wxImage im(sciezka);
            CHECK(im.IsOk() && im.GetWidth() == 320 && im.GetHeight() == 240);
            CallAfter([&] { ZrzutEkranu("18_kamera"); d.EndModal(wxID_CANCEL); });
            d.ShowModal();
        }
        CHECK(pp->UstawZdjecie(sciezka));
        CHECK(pp->ZdjecieWidoczne() && pp->Sciezka() == sciezka);
        auto z = logika.pobierzElement(*id);
        CHECK(z && z->zdjecieSciezka == sciezka.utf8_string() && z->iloscDostepna == 5);   // stan bez zmian
        auto hist = m.zapytanie("SELECT count(*) FROM tbl_transakcje WHERE id_elementu = " + std::to_string(*id) +
                                " AND typ_transakcji = 'KOREKTA'");
        CHECK(hist && hist->liczba(0, 0) == 0);
        CHECK(pe->Wybrany() && pe->Wybrany()->zdjecieSciezka == sciezka.utf8_string());   // lista odświeżona
        wxYield();
        ZrzutEkranu("19_panel_zdjecia");

        CHECK(pp->UstawZdjecie(""));
        CHECK(!pp->ZdjecieWidoczne());
        z = logika.pobierzElement(*id);
        CHECK(z && z->zdjecieSciezka.empty());
        pe->Szukaj("");
        pe->Wyczysc();
        CHECK(pp->IdElementu() == 0);                    // brak zaznaczenia czyści panel

        wxRemoveFile(sciezka);
        wxFileName::Rmdir(katalog);
        kf.ustawUstawieniaAplikacji(stare);
        sprzatanie();
    }

    void TestKalkulatorow(OknoGlowne* okno) {
        DialogKalkulatorow d(okno);
        d.UstawDzielnik("5", "3,3", "10k", "20k");
        CHECK(d.OpisDzielnika().Contains("R1 = ") && d.OpisDzielnika().Contains("Uwy = 3,"));
        CHECK(d.SprawdzenieDzielnika().Contains("3,333"));
        d.UstawDzielnik("5", "6", "10k", "0");
        CHECK(d.OpisDzielnika().Contains("mniejsze"));
        d.UstawDzielnik("5", "3,3", "10k", "20k");
        d.UstawLED("5", "2", "20");
        CHECK(d.OpisLED().Contains("150 \u03a9") && d.OpisLED().Contains("20 mA"));
        d.UstawLED("2", "2", "20");
        CHECK(d.OpisLED().Contains("musi być większe"));
        d.UstawLED("5", "2", "20");
        d.UstawRC("10k", "100n", "1k");
        CHECK(d.OpisRC().Contains("159 Hz") && d.OpisRC().Contains("1 ms"));
        CHECK(d.OpisRCDobor().Contains("R = ") && d.OpisRCDobor().Contains("C = "));
        d.PokazStrone(0);
        CallAfter([&] {
            ZrzutEkranu("22_kalkulator_dzielnik");
            d.PokazStrone(1); ZrzutEkranu("23_kalkulator_led");
            d.PokazStrone(2); ZrzutEkranu("24_kalkulator_rc");
            d.EndModal(wxID_CLOSE);
        });
        d.ShowModal();
    }

    void TestEagle(OknoGlowne* okno) {
        ManagerBazyDanych& m = ManagerBazyDanych::pobierz();
        LogikaMagazynu logika(m);
        LogikaProjektow lp(m, logika);
        auto sprzatanie = [&] {
            m.wykonaj("DELETE FROM tbl_bom WHERE id_projektu IN (SELECT id FROM tbl_projekty WHERE nazwa LIKE 'GUIE2\\_%')");
            m.wykonaj("DELETE FROM tbl_projekty WHERE nazwa LIKE 'GUIE2\\_%'");
            m.wykonaj("DELETE FROM tbl_transakcje WHERE id_elementu IN (SELECT id FROM tbl_elementy WHERE symbol LIKE 'GUIE2\\_%')");
            m.wykonaj("DELETE FROM tbl_elementy WHERE symbol LIKE 'GUIE2\\_%'");
        };
        sprzatanie();
        Element e; e.symbol = "GUIE2_R10K"; e.symbolProducenta = "RC0603-10K"; e.producent = "Yageo"; e.iloscDostepna = 12;
        e.ustawMiejsce({"Kontener 1", "A1", ""});
        auto ide = logika.zapiszElement(e);
        Projekt p; p.nazwa = "GUIE2_Zasilacz";
        auto idp = lp.zapiszProjekt(p);
        CHECK(ide && idp);
        if (!ide || !idp) { sprzatanie(); return; }
        PozycjaProjektu poz; poz.idProjektu = *idp; poz.idElementu = *ide; poz.oznaczenia = "R1, R2"; poz.wymagana = 2;
        CHECK(lp.dodajPozycje(poz).has_value());

        PanelProjektow* pp = ZnajdzTyp<PanelProjektow>(okno);
        CHECK(pp != nullptr);
        if (pp) {
            pp->Odswiez();
            // wybór projektu na liście
            pp->WybierzProjekt(*idp);
            CHECK(pp->WybranyId() == idp);
            const wxString scr = wxFileName::GetTempDir() + "/guie2.scr", ulp = wxFileName::GetTempDir() + "/guie2.ulp";
            CHECK(pp->SkryptEagle(scr) == 2);
            wxFFile f(scr, "rb"); wxString tresc;
            CHECK(f.IsOpened() && f.ReadAll(&tresc));
            CHECK(tresc.Contains("ATTRIBUTE R1 DM_SYMBOL 'GUIE2_R10K';") && tresc.Contains("ATTRIBUTE R2 DM_STAN '12';") &&
                  tresc.Contains("ATTRIBUTE R1 DM_MIEJSCE 'Kontener 1, A1';") && tresc.Contains("ATTRIBUTE R1 MPN 'RC0603-10K';"));
            CHECK(pp->ZapiszUlp(ulp) && wxFileExists(ulp));
            wxRemoveFile(scr); wxRemoveFile(ulp);
        }
        sprzatanie();
    }


    void TestKatalogu(OknoGlowne* okno) {
        ManagerBazyDanych& m = ManagerBazyDanych::pobierz();
        LogikaMagazynu logika(m);
        LogikaKatalogu lk(m);
        auto sprzatanie = [&] {
            m.wykonaj("DELETE FROM tbl_transakcje WHERE id_elementu IN (SELECT id FROM tbl_elementy WHERE symbol LIKE 'GUIK2\\_%')");
            m.wykonaj("DELETE FROM tbl_elementy WHERE symbol LIKE 'GUIK2\\_%'");
            m.wykonaj("DELETE FROM tbl_katalog WHERE symbol LIKE 'GUIK2\\_%'");
        };
        sprzatanie();
        PanelKatalogu* pk = ZnajdzTyp<PanelKatalogu>(okno);
        CHECK(pk != nullptr);
        if (!pk) return;

        PozycjaKatalogu a; a.symbol = "GUIK2_LM358"; a.producent = "TI"; a.opis = "Dual Op-Amp"; a.obudowa = "DIP-8"; a.rodzina = "Wzmacniacze";
        a.parametry = {{"Napięcie zasilania", "3 V ... 32 V", "reczny"}};
        const auto ida = lk.zapisz(a);
        CHECK(ida.has_value());

        // import CSV przez panel
        const wxString csv = wxFileName::GetTempDir() + "/guik2.csv";
        {
            wxFFile f(csv, "wb");
            const std::string t = "\xEF\xBB\xBFSymbol;Producent;Opis;Obudowa;Rodzina;Parametry\n"
                                  "GUIK2_NE555;TI;Timer;DIP-8;Timery;\"Częstotliwość=100 kHz\"\n"
                                  "GUIK2_BC547;;NPN;TO-92;Tranzystory;\n";
            f.Write(t.data(), t.size());
        }
        CHECK(pk->ImportujCSV(csv) == 2);
        CHECK(pk->ImportujCSV(csv) == 0);                     // drugi raz: wszystko pominięte
        wxRemoveFile(csv);
        pk->Szukaj("GUIK2_");
        CHECK(pk->Lista().size() == 3);
        auto ne = lk.znajdz("GUIK2_NE555", "TI");
        CHECK(ne && ne->parametr("Częstotliwość") && ne->parametr("Częstotliwość")->wartosc == "100 kHz");

        // dodanie do magazynu i powiązanie
        pk->Wybierz(*ida);
        CHECK(pk->Wybrana() && pk->Wybrana()->symbol == "GUIK2_LM358" && pk->Wybrana()->wMagazynie == 0);
        const auto idm = lk.dodajDoMagazynu(logika, *ida, 7, MiejsceMagazynowe{"Kontener 1", "B2", ""}, std::nullopt);
        CHECK(idm.has_value());
        CHECK(!lk.dodajDoMagazynu(logika, *ida, 1, MiejsceMagazynowe{}, std::nullopt).has_value());   // duplikat symbolu
        pk->Odswiez();
        pk->Wybierz(*ida);
        CHECK(pk->Wybrana() && pk->Wybrana()->wMagazynie == 7);
        PanelPodgladu* pp = ZnajdzTyp<PanelPodgladu>(okno);
        PanelElementow* pe = ZnajdzTyp<PanelElementow>(okno);
        if (pp && pe && idm) {
            pe->UstawKategorie(std::nullopt);
            pe->Szukaj("GUIK2_LM358");
            pe->WybierzPierwszy();
            CHECK(pp->Opis().Contains("Z katalogu") && pp->Opis().Contains("3 V ... 32 V"));
            pe->Szukaj("");
        }

        // dialogi
        {
            DialogKatalogu d(okno, *lk.pobierzPozycje(*ida), lk.rodziny());
            CHECK(d.LiczbaParametrow() == 1);
            d.DodajParametr(wxString::FromUTF8("Prąd"), "20 mA");
            const PozycjaKatalogu w = d.Pobierz();
            CHECK(w.symbol == "GUIK2_LM358" && w.parametry.size() == 2 && w.parametry[1].nazwa == "Prąd");
            CallAfter([&] { ZrzutEkranu("20_katalog_pozycja"); d.EndModal(wxID_CANCEL); });
            d.ShowModal();
        }
        {
            DialogWyboruZKatalogu d(okno, lk, "GUIK2_");
            CHECK(d.LiczbaWynikow() == 3);
            d.Szukaj("GUIK2_NE");
            CHECK(d.LiczbaWynikow() == 1 && d.Wybierz(0) && d.Wybrana()->symbol == "GUIK2_NE555");
            DialogElementu de(okno, logika, Element{});
            de.UzupelnijZKatalogu(*d.Wybrana());
            const Element el = de.PobierzElement();
            CHECK(el.symbol == "GUIK2_NE555" && el.producent == "TI" && el.obudowa == "DIP-8" && el.parametry.find("100 kHz") != std::string::npos);
        }

        // odczyt datasheetu z PDF (wymaga pdftotext; bez niego pomijamy)
        {
            const wxString pdf = wxFileName::GetTempDir() + "/guik2.pdf";
            const std::string tresc =
                "BT /F1 11 Tf 50 740 Td 14 TL (LM358 Dual Operational Amplifier) Tj T* (Supply voltage 3 V to 32 V) Tj "
                "T* (Operating temperature -40 C to 85 C) Tj T* (Available in SOIC-8 package) Tj ET";
            std::string pdfTekst = "%PDF-1.4\n1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj\n"
                "3 0 obj<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]/Contents 4 0 R/Resources<</Font<</F1 5 0 R>>>>>>endobj\n"
                "4 0 obj<</Length " + std::to_string(tresc.size()) + ">>stream\n" + tresc + "\nendstream endobj\n"
                "5 0 obj<</Type/Font/Subtype/Type1/BaseFont/Helvetica>>endobj\ntrailer<</Root 1 0 R/Size 6>>\n%%EOF\n";
            { wxFFile f(pdf, "wb"); f.Write(pdfTekst.data(), pdfTekst.size()); }
            std::string blad;
            if (!OdczytDatasheetu::tekstZPDF(pdf.utf8_string(), blad).empty()) {
                pk->Wybierz(*ida);
                CHECK(pk->OdczytajDatasheet(pdf, true) >= 2);
                auto po = lk.pobierzPozycje(*ida);
                CHECK(po && po->parametr("Temperatura pracy") && po->parametr("Temperatura pracy")->wartosc == "-40 ... +85 °C");
                bool zrodlo = false;
                if (po) for (const auto& p : po->parametry) if (p.nazwa == "Temperatura pracy" && p.zrodlo == "datasheet") zrodlo = true;
                CHECK(zrodlo);
                CHECK(po && po->parametr("Prąd") == nullptr);                      // nie dopisano niczego z dialogu, który anulowano
                CHECK(po && po->parametr("Napięcie zasilania") != nullptr);
            } else {
                std::cout << "  (pomijam odczyt PDF: " << blad << ")\n";
            }
            wxRemoveFile(pdf);
        }
        // eksport
        const wxString out = wxFileName::GetTempDir() + "/guik2_out.csv";
        CHECK(pk->EksportujCSV(out) && wxFileExists(out));
        wxRemoveFile(out);

        okno->PokazKatalog();
        pk->Szukaj("GUIK2_");
        pk->Wybierz(*ida);
        wxYield();
        ZrzutEkranu("21_zakladka_katalog");
        pk->Szukaj("");
        sprzatanie();
    }

    void TestSklepow(OknoGlowne* okno) {
        ManagerBazyDanych& m = ManagerBazyDanych::pobierz();
        LogikaMagazynu logika(m);
        LogikaProjektow lp(m, logika);
        LogikaSklepow ls(m);
        auto sprzatanie = [&] {
            m.wykonaj("DELETE FROM tbl_bom WHERE id_projektu IN (SELECT id FROM tbl_projekty WHERE nazwa LIKE 'GUIS\\_%')");
            m.wykonaj("DELETE FROM tbl_projekty WHERE nazwa LIKE 'GUIS\\_%'");
            m.wykonaj("DELETE FROM tbl_elementy_sklepy WHERE id_elementu IN (SELECT id FROM tbl_elementy WHERE symbol LIKE 'GUIS\\_%')");
            m.wykonaj("DELETE FROM tbl_transakcje WHERE id_elementu IN (SELECT id FROM tbl_elementy WHERE symbol LIKE 'GUIS\\_%')");
            m.wykonaj("DELETE FROM tbl_elementy WHERE symbol LIKE 'GUIS\\_%'");
            m.wykonaj("DELETE FROM tbl_sklepy WHERE nazwa LIKE 'GUIS\\_%'");
        };
        sprzatanie();

        // menu Sklepy
        CHECK(okno->GetMenuBar()->FindMenu("Sklepy") != wxNOT_FOUND);

        // --- okno sklepów: dodanie przez okno, lista
        Sklep a; a.nazwa = "GUIS_A"; a.urlBazowy = "https://a.example/"; a.kosztDostawy = 10; a.priorytet = 9;
        Sklep b; b.nazwa = "GUIS_B"; b.urlBazowy = "https://b.example/"; b.kosztDostawy = 15; b.darmowaDostawaOd = 100; b.priorytet = 9;
        auto ia = ls.zapiszSklep(a), ib = ls.zapiszSklep(b);
        CHECK(ia && ib);
        if (!ia || !ib) return;
        {
            DialogSklepow dlg(okno, ls);
            bool jestA = false;
            for (const Sklep& s : dlg.Sklepy()) jestA = jestA || s.nazwa == "GUIS_A";
            CHECK(jestA);
            const size_t przed = dlg.Sklepy().size();
            CallAfter([&] {
                auto* d = dynamic_cast<DialogDodajSklep*>(Gorne());
                CHECK(d != nullptr);
                if (!d) return;
                d->tcNazwa->SetValue("GUIS_C");
                d->tcAdres->SetValue("https://c.example/");
                d->tcSzablon->SetValue("https://c.example/s?q={q}");
                d->tcWaluta->SetValue("usd");
                d->tcDostawa->SetValue("5,5");
                wxWindow* ok = Znajdz(d, "OK");
                CHECK(ok != nullptr);
                if (ok) Kliknij(ok);
            });
            CHECK(dlg.Dodaj());
            CHECK(dlg.Sklepy().size() == przed + 1);
            bool c = false;
            for (const Sklep& s : dlg.Sklepy()) if (s.nazwa == "GUIS_C") c = s.waluta == "USD" && s.kosztDostawy == 5.5 && s.szablonWyszukiwania.find("{q}") != std::string::npos;
            CHECK(c);
            // zła waluta: okno zostaje otwarte (zapis się nie udaje)
            {   // zamykane przed ShowModal, bo Gorne() wskazuje ostatnio utworzone okno
                DialogDodajSklep zle(okno, ls);
                zle.tcNazwa->SetValue("GUIS_Z");
                zle.tcAdres->SetValue("https://z/");
                zle.tcWaluta->SetValue("zl");
                CHECK(zle.Pobierz().waluta == "ZL");           // okno przekazuje wartość, a baza ją odrzuca
                CHECK(!ls.zapiszSklep(zle.Pobierz()));
                CHECK(zle.IdZapisanego() == 0);
            }
            CallAfter([&] { ZrzutEkranu("10_sklepy"); if (auto* d = dynamic_cast<wxDialog*>(Gorne())) d->EndModal(wxID_CANCEL); });
            dlg.ShowModal();
        }
        {
            std::map<std::string, double> k;
            std::string blad;
            CHECK(DialogSklepow::TekstNaKursy("USD = 4,05\n# komentarz\n\neur=4.3\n", k, blad) && k.size() == 2 && k["EUR"] == 4.3 && k["USD"] == 4.05);
            CHECK(!DialogSklepow::TekstNaKursy("USD 4", k, blad) && !blad.empty());
            CHECK(!DialogSklepow::TekstNaKursy("USD = abc", k, blad));
            CHECK(!DialogSklepow::TekstNaKursy("PLN = 2", k, blad));
            CHECK(!DialogSklepow::TekstNaKursy("USD = 0", k, blad));
            CHECK(DialogSklepow::KursyNaTekst({{"PLN", 1}, {"USD", 4.05}}) == "USD = 4,05\n");
        }

        // --- elementy, linki z cenami, projekt z brakami
        Element e1; e1.symbol = "GUIS_E1"; e1.producent = "Test";
        Element e2; e2.symbol = "GUIS_E2";
        auto i1 = logika.zapiszElement(e1), i2 = logika.zapiszElement(e2);
        CHECK(i1 && i2);
        if (!i1 || !i2) { sprzatanie(); return; }
        auto link = [&](long long el, long long sk, double cena) {
            LinkSklepu l; l.idElementu = el; l.idSklepu = sk; l.url = "https://x.example/p"; l.cena = cena;
            CHECK(ls.zapiszLink(l).has_value());
        };
        link(*i1, *ia, 1.00); link(*i1, *ib, 1.20);
        link(*i2, *ia, 2.00); link(*i2, *ib, 1.80);

        Projekt pr; pr.nazwa = "GUIS_Projekt";
        auto ip = lp.zapiszProjekt(pr);
        CHECK(ip.has_value());
        if (!ip) { sprzatanie(); return; }
        PozycjaProjektu p1; p1.idProjektu = *ip; p1.idElementu = *i1; p1.wymagana = 10;
        PozycjaProjektu p2; p2.idProjektu = *ip; p2.idElementu = *i2; p2.wymagana = 5;
        CHECK(lp.dodajPozycje(p1).has_value() && lp.dodajPozycje(p2).has_value());

        {
            DialogZestawienia dlg(okno, lp, ls, *ip);
            CHECK(dlg.Potrzeby().size() == 2);
            // tylko oferty dwóch testowych sklepów; inne sklepy (z cenami) nie wchodzą, bo elementy testowe mają tylko te linki
            const PlanZakupow& najt = dlg.PlanDlaTrybu(TrybPlanu::Najtanszy);
            const PlanZakupow& ceny = dlg.PlanDlaTrybu(TrybPlanu::NajtanszeCeny);
            const PlanZakupow& jeden = dlg.PlanDlaTrybu(TrybPlanu::JedenSklep);
            CHECK(najt.kompletny() && std::abs(najt.razemPLN() - 30.0) < 0.005);    // wszystko w A: 10 + 10 + 10 zł dostawy
            CHECK(std::abs(ceny.razemPLN() - 44.0) < 0.005);                        // E1 w A (10), E2 w B (9), dostawy 10 + 15
            CHECK(jeden.kompletny() && std::abs(jeden.razemPLN() - 30.0) < 0.005);
            CHECK(dlg.TekstPorownania().Contains("30,00") && dlg.TekstPorownania().Contains("44,00"));
            CHECK(dlg.TekstPorownania().Contains("14,00"));                         // oszczędność na łączeniu zakupów

            dlg.WybierzTryb(TrybPlanu::NajtanszeCeny);
            CHECK(std::abs(dlg.Plan().razemPLN() - 44.0) < 0.005);
            const Raport r = dlg.RaportZPlanu();
            CHECK(r.wiersze.size() == 2 && r.naglowki.size() == 6);
            dlg.WybierzTryb(TrybPlanu::Najtanszy);

            // już zamówione: 4 z 10 sztuk E1
            std::vector<PozycjaProjektu> poz;
            CHECK(lp.pobierzPozycje(*ip, poz));
            for (PozycjaProjektu& x : poz) if (x.idElementu == *i1) { x.zamowiona = 10; CHECK(lp.zapiszPozycje(x)); }
            CHECK(dlg.Przelicz());
            CHECK(dlg.Potrzeby().size() == 1 && dlg.Potrzeby()[0].symbol == "GUIS_E2");   // E1 zamówione w całości
            dlg.OdejmujZamowione(false);
            CHECK(dlg.Potrzeby().size() == 2);
            dlg.WybierzProjekt(std::nullopt);                                              // wszystkie aktywne projekty
            CHECK(dlg.Potrzeby().size() >= 2);
            dlg.WybierzProjekt(*ip);

            CallAfter([&] { ZrzutEkranu("11_zestawienie"); if (auto* d = dynamic_cast<wxDialog*>(Gorne())) d->EndModal(wxID_CANCEL); });
            dlg.ShowModal();
        }

        // --- okno linków elementu
        {
            DialogLinkowElementu dlg(okno, ls, logika, *i1);
            bool a1 = false, b1 = false;
            for (const LinkSklepu& l : dlg.Linki()) { a1 = a1 || (l.idSklepu == *ia && l.cena && *l.cena == 1.0); b1 = b1 || l.idSklepu == *ib; }
            CHECK(a1 && b1);
            CHECK(dlg.StatusDatasheetu().Contains("Brak datasheetu"));
            CHECK(!dlg.Sprawdzanie());
            CallAfter([&] { ZrzutEkranu("12_linki_elementu"); if (auto* d = dynamic_cast<wxDialog*>(Gorne())) d->EndModal(wxID_CANCEL); });
            dlg.ShowModal();
        }

        sprzatanie();
    }

    void TestRaportowIEtykiet(OknoGlowne* okno) {
        ManagerBazyDanych& m = ManagerBazyDanych::pobierz();
        LogikaMagazynu logika(m);
        LogikaProjektow lp(m, logika);
        LogikaRaportow lr(m, logika, lp);
        auto sprzatanie = [&] {
            m.wykonaj("DELETE FROM tbl_transakcje WHERE id_elementu IN (SELECT id FROM tbl_elementy WHERE symbol LIKE 'GUIE\\_%')");
            m.wykonaj("DELETE FROM tbl_elementy WHERE symbol LIKE 'GUIE\\_%'");
        };
        sprzatanie();
        Element e;
        e.symbol = "GUIE_LM358";
        e.wartosc = "LM358";
        e.obudowa = "SO-8";
        e.producent = "Texas Instruments";
        e.opis = "Podwójny wzmacniacz operacyjny";
        e.iloscDostepna = 3;
        e.iloscMinimalna = 10;
        e.cena = 1.5;
        auto id = logika.zapiszElement(e);
        CHECK(id.has_value());
        if (!id) return;

        // --- raport
        {
            DialogRaportu dlg(okno, lr);
            dlg.WybierzRodzaj(RodzajRaportu::NiskieStany);
            CHECK(dlg.Generuj());
            bool jest = false;
            for (const auto& w : dlg.AktualnyRaport().wiersze) jest = jest || w[0] == "GUIE_LM358";
            CHECK(jest);
            dlg.WybierzRodzaj(RodzajRaportu::HistoriaZmian);
            dlg.UstawDaty("2000-01-01", "2999-01-01");
            CHECK(dlg.Generuj());
        }

        // --- etykiety: PNG, a z PNG odczyt kodu (pełna pętla: rysowanie -> obraz -> odczyt)
        std::vector<Element> lista;
        FiltrElementow f;
        f.tekst = "GUIE_";
        CHECK(logika.pobierzElementy(f, lista) && lista.size() == 1);
        const wxString bazowa = wxFileName::GetTempDir() + "/gui_etykiety.png";
        {
            DialogEtykiet dlg(okno, lista, *id);
            dlg.WybierzTylkoZaznaczony(true);
            CHECK(dlg.Etykiety().size() == 1);
            CHECK(dlg.Etykiety()[0].kod.size() == 13);
            const int zapisano = dlg.ZapiszPNG(bazowa, 300);
            if (zapisano != 1) std::cerr << "zapisano=" << zapisano << " sciezka=" << bazowa.utf8_string() << "\n";
            CHECK(zapisano == 1);
            const UstawieniaDruku u = dlg.Ustawienia();
            CHECK(u.format.naStrone() >= 1);
        }
        wxString blad;
        const auto kod = OdczytajKodZPliku(bazowa, blad);
        CHECK(kod.has_value());
        if (kod) {
            const int prefiks = Konfiguracja::pobierz().pobierzUstawieniaAplikacji().prefixEAN;
            CHECK(KodKreskowy::idZKodu(*kod, prefiks) == std::optional<long long>(*id));
        }
        wxString blad2;
        CHECK(!OdczytajKodZPliku(wxFileName::GetTempDir() + "/nie_ma_takiego.png", blad2).has_value());
        CHECK(!blad2.empty());

        // --- szukanie po kodzie wskazuje element
        PanelElementow* pe = ZnajdzTyp<PanelElementow>(okno);
        if (pe && kod) {
            pe->Szukaj(wxString::FromUTF8(kod->c_str()));
            CHECK(pe->Elementy().size() == 1);
            CHECK(!pe->Elementy().empty() && pe->Elementy()[0].id == *id);
            CHECK(pe->MaWybor());
            pe->Szukaj("");
        }
        if (!env("DM_TEST_SHOTS", "").empty()) wxCopyFile(bazowa, wxString::FromUTF8(env("DM_TEST_SHOTS", "")) + "/12_etykieta_strona.png");
        wxRemoveFile(bazowa);
        sprzatanie();
    }

    void TestProjektow(OknoGlowne* okno) {
        ManagerBazyDanych& m = ManagerBazyDanych::pobierz();
        LogikaMagazynu logika(m);
        auto sprzatanie = [&] {
            m.wykonaj("DELETE FROM tbl_bom WHERE id_projektu IN (SELECT id FROM tbl_projekty WHERE nazwa LIKE 'GUIP\\_%')");
            m.wykonaj("DELETE FROM tbl_transakcje WHERE id_projektu IN (SELECT id FROM tbl_projekty WHERE nazwa LIKE 'GUIP\\_%')");
            m.wykonaj("DELETE FROM tbl_projekty WHERE nazwa LIKE 'GUIP\\_%'");
            m.wykonaj("DELETE FROM tbl_transakcje WHERE id_elementu IN (SELECT id FROM tbl_elementy WHERE symbol LIKE 'GUIP\\_%')");
            m.wykonaj("DELETE FROM tbl_elementy WHERE symbol LIKE 'GUIP\\_%'");
        };
        sprzatanie();

        PanelProjektow* pp = ZnajdzTyp<PanelProjektow>(okno);
        wxNotebook* zakladki = ZnajdzTyp<wxNotebook>(okno);
        CHECK(pp != nullptr && zakladki != nullptr);
        if (!pp || !zakladki) return;
        CHECK(zakladki->GetPageCount() == 3);
        CHECK(zakladki->GetPageText(1) == "Projekty");
        CHECK(pp->IsEnabled());

        // element, który program dopasuje po wartości i obudowie
        Element e;
        e.symbol = "GUIP_R10k0805";
        e.wartosc = "10k";
        e.obudowa = "R0805";
        e.iloscDostepna = 4;
        CHECK(logika.zapiszElement(e).has_value());

        const wxString plik = wxFileName::GetTempDir() + "/gui_bom.csv";
        {
            std::ofstream f(plik.utf8_string(), std::ios::binary);
            f << "Reference,Value,Footprint,Qty\n"
                 "\"R1, R2, R3\",10k,Resistor_SMD:R_0805_2012Metric,3\n"
                 "\"C1\",GUIP_1u,Capacitor_SMD:C_0805_2012Metric,1\n";
        }

        zakladki->SetSelection(1);
        pp->Odswiez();

        // --- nowy projekt
        CallAfter([&] {
            wxWindow* dlg = Gorne();
            CHECK(dynamic_cast<DialogProjektu*>(dlg) != nullptr);
            auto* nazwa = ZnajdzTyp<wxTextCtrl>(dlg);
            CHECK(nazwa != nullptr);
            if (nazwa) nazwa->SetValue("GUIP_Zasilacz");
            wxWindow* ok = Znajdz(dlg, "OK");
            CHECK(ok != nullptr);
            if (ok) Kliknij(ok);
        });
        pp->NowyProjekt();
        CHECK(pp->MaProjekt());
        CHECK(!pp->Projekty().empty());
        bool jest = false;
        for (const Projekt& p : pp->Projekty()) jest = jest || p.nazwa == "GUIP_Zasilacz";
        CHECK(jest);

        // --- import BOM z pliku: pierwsza pozycja z magazynu, druga jako nowy element
        CallAfter([&] {
            wxWindow* dlg = Gorne();
            auto* imp = dynamic_cast<DialogImportBOM*>(dlg);
            CHECK(imp != nullptr);
            if (imp) CHECK(imp->LiczbaWierszy() == 2);
            Zrzut(dlg, "08_import_bom");
            auto* plytki = ZnajdzTyp<wxSpinCtrl>(dlg);
            if (plytki) plytki->SetValue(2);
            wxWindow* ok = Znajdz(dlg, wxString::FromUTF8("Importuj"));
            CHECK(ok != nullptr);
            if (ok) Kliknij(ok);
        });
        pp->ImportujBOM(plik);
        CHECK(pp->Pozycje().size() == 2);
        bool rezystory = false, nowy = false;
        for (const PozycjaProjektu& z : pp->Pozycje()) {
            if (z.symbol == "GUIP_R10k0805") rezystory = z.wymagana == 6 && z.brakuje() == 2;
            if (z.symbol == "GUIP_1u C0805" || z.wartosc == "GUIP_1u") nowy = z.wymagana == 2 && z.dostepna == 0;
        }
        CHECK(rezystory);
        CHECK(nowy);
        CHECK(Liczba(m, "SELECT count(*) FROM tbl_elementy WHERE wartosc = 'GUIP_1u'") == 1);
        Zrzut(okno, "09_projekty");

        // --- lista zakupów jest policzona z brakujących części
        LogikaProjektow lp(m, logika);
        std::vector<PozycjaProjektu> braki;
        CHECK(lp.pobierzBraki(pp->Projekty()[0].id, braki));
        CHECK(braki.size() == 2);

        // --- wydanie jednej pozycji: okno wyboru ilości (domyślnie tyle, ile można)
        sprzatanie();
        pp->Odswiez();
        zakladki->SetSelection(0);
    }

    void TestMagazynu(OknoGlowne* okno) {
        ManagerBazyDanych& m = ManagerBazyDanych::pobierz();
        LogikaMagazynu logika(m);
        auto sprzatanie = [&] {
            m.wykonaj("DELETE FROM tbl_transakcje WHERE id_elementu IN (SELECT id FROM tbl_elementy WHERE symbol LIKE 'GUI\\_%')");
            m.wykonaj("DELETE FROM tbl_elementy WHERE symbol LIKE 'GUI\\_%'");
            m.wykonaj("DELETE FROM tbl_kategorie WHERE nazwa LIKE 'GUI\\_%' AND id_rodzica IS NOT NULL");
            m.wykonaj("DELETE FROM tbl_kategorie WHERE nazwa LIKE 'GUI\\_%'");
        };
        sprzatanie();

        PanelKategorii* pk = ZnajdzTyp<PanelKategorii>(okno);
        PanelElementow* pe = ZnajdzTyp<PanelElementow>(okno);
        CHECK(pk != nullptr);
        CHECK(pe != nullptr);
        if (!pk || !pe) return;
        pk->Odswiez();
        pe->Odswiez();

        // Menu: Magazyn i Widok istnieją, a pozycje są dostępne po połączeniu
        {
            wxMenuBar* pasek = okno->GetMenuBar();
            int magazyn = -1, widok = -1;
            for (size_t i = 0; i < pasek->GetMenuCount(); ++i) {
                const wxString t = pasek->GetMenuLabelText(i);
                if (t == "Magazyn") magazyn = static_cast<int>(i);
                if (t == "Widok") widok = static_cast<int>(i);
            }
            CHECK(magazyn >= 0);
            CHECK(widok >= 0);
            if (magazyn >= 0) {
                wxMenu* menu = pasek->GetMenu(static_cast<size_t>(magazyn));
                CHECK(menu->GetMenuItemCount() >= 10);
                bool jestNowy = false;
                for (wxMenuItem* pozycja : menu->GetMenuItems()) {
                    if (pozycja->GetItemLabelText().StartsWith("Nowy element")) jestNowy = true;
                }
                CHECK(jestNowy);
            }
        }
        CHECK(pk->WybranaKategoria() == std::nullopt);
        CHECK(pk->IsEnabled() && pe->IsEnabled());
        CHECK(okno->GetStatusBar()->GetStatusText(0).Contains(wxString::FromUTF8("Elementów")));

        // --- kategoria główna przez okienko wpisywania nazwy
        CallAfter([] {
            auto* d = dynamic_cast<wxTextEntryDialog*>(Gorne());
            CHECK(d != nullptr);
            if (d) { d->SetValue(wxString::FromUTF8("GUI_Zażółć")); d->EndModal(wxID_OK); }
        });
        pk->DodajKategorieGlowna();
        CHECK(Liczba(m, "SELECT count(*) FROM tbl_kategorie WHERE nazwa = 'GUI_Zażółć'") == 1);
        CHECK(logika.odswiezKategorie());
        long long idGlowna = 0;
        for (const Kategoria& k : logika.kategorie()) if (k.nazwa == "GUI_Zażółć") idGlowna = k.id;
        CHECK(idGlowna != 0);

        // --- podkategoria (rodzic = zaznaczona kategoria)
        pk->WybierzKategorie(idGlowna);
        CHECK(pk->WybranaKategoria() == std::optional<long long>(idGlowna));
        CHECK(pe->Kategoria() == std::optional<long long>(idGlowna));   // zaznaczenie przekazane do listy
        CallAfter([] {
            auto* d = dynamic_cast<wxTextEntryDialog*>(Gorne());
            CHECK(d != nullptr);
            if (d) { d->SetValue("GUI_Podkategoria"); d->EndModal(wxID_OK); }
        });
        pk->DodajPodkategorie();
        CHECK(Liczba(m, "SELECT count(*) FROM tbl_kategorie k JOIN tbl_kategorie r ON r.id = k.id_rodzica "
                        "WHERE k.nazwa = 'GUI_Podkategoria' AND r.nazwa = 'GUI_Zażółć'") == 1);
        logika.odswiezKategorie();
        long long idPod = 0;
        for (const Kategoria& k : logika.kategorie()) if (k.nazwa == "GUI_Podkategoria") idPod = k.id;
        CHECK(idPod != 0);

        // --- zmiana nazwy
        pk->WybierzKategorie(idPod);
        CallAfter([] {
            auto* d = dynamic_cast<wxTextEntryDialog*>(Gorne());
            CHECK(d != nullptr);
            if (d) { CHECK(d->GetValue() == "GUI_Podkategoria"); d->SetValue("GUI_Pod2"); d->EndModal(wxID_OK); }
        });
        pk->ZmienNazwe();
        CHECK(Liczba(m, "SELECT count(*) FROM tbl_kategorie WHERE nazwa = 'GUI_Pod2'") == 1);

        // --- nowy element: formularz (pierwsze pole tekstowe to symbol, pierwsze pokrętło to ilość)
        pk->WybierzKategorie(idPod);
        CallAfter([&] {
            wxWindow* dlg = Gorne();
            CHECK(dynamic_cast<DialogElementu*>(dlg) != nullptr);
            Zrzut(dlg, "06_dialog_elementu");
            auto* symbol = ZnajdzTyp<wxTextCtrl>(dlg);
            auto* ilosc = ZnajdzTyp<wxSpinCtrl>(dlg);
            CHECK(symbol != nullptr && ilosc != nullptr);
            if (symbol) symbol->SetValue("GUI_Rezystor 10k");
            if (ilosc) ilosc->SetValue(5);
            wxWindow* ok = Znajdz(dlg, "OK");
            CHECK(ok != nullptr);
            if (ok) Kliknij(ok);
        });
        pe->NowyElement();
        CHECK(pe->Elementy().size() == 1);
        CHECK(pe->MaWybor());                                   // nowy element jest zaznaczony
        if (!pe->Elementy().empty()) {
            const Element& e = pe->Elementy()[0];
            CHECK(e.symbol == "GUI_Rezystor 10k" && e.iloscDostepna == 5);
            CHECK(e.idKategorii == std::optional<long long>(idPod));   // kategoria z drzewa
        }
        CHECK(okno->GetStatusBar()->GetStatusText(0).Contains("1"));

        // --- wyszukiwanie
        pe->Szukaj("gui_REZ");
        CHECK(pe->Elementy().size() == 1);
        pe->Szukaj("nie_ma_takiego_xyz");
        CHECK(pe->Elementy().empty());
        CHECK(!pe->MaWybor());
        pe->Szukaj("");
        CHECK(pe->Elementy().size() == 1);
        pe->Szukaj("gui_rez");
        pe->WybierzPierwszy();
        CHECK(pe->MaWybor());

        // --- zmiana stanu: wydanie 3 sztuk
        CallAfter([&] {
            wxWindow* dlg = Gorne();
            CHECK(dynamic_cast<DialogZmianyIlosci*>(dlg) != nullptr);
            Zrzut(dlg, "07_zmiana_stanu");
            auto* wydanie = dynamic_cast<wxRadioButton*>(Znajdz(dlg, wxString::FromUTF8("Wydanie (odejmij od stanu)")));
            auto* ilosc = ZnajdzTyp<wxSpinCtrl>(dlg);
            CHECK(wydanie != nullptr && ilosc != nullptr);
            if (wydanie) wydanie->SetValue(true);
            if (ilosc) ilosc->SetValue(3);
            wxWindow* ok = Znajdz(dlg, "OK");
            CHECK(ok != nullptr);
            if (ok) Kliknij(ok);
        });
        pe->ZmienStanWybranego();
        CHECK(!pe->Elementy().empty() && pe->Elementy()[0].iloscDostepna == 2);
        CHECK(Liczba(m, "SELECT count(*) FROM tbl_transakcje t JOIN tbl_elementy e ON e.id = t.id_elementu "
                        "WHERE e.symbol = 'GUI_Rezystor 10k' AND t.typ_transakcji = 'WYDANIE' AND t.ilosc_zmiana = -3") == 1);

        // --- edycja: zmiana symbolu i ceny (z przecinkiem)
        CallAfter([&] {
            wxWindow* dlg = Gorne();
            CHECK(dynamic_cast<DialogElementu*>(dlg) != nullptr);
            auto* symbol = ZnajdzTyp<wxTextCtrl>(dlg);
            CHECK(symbol != nullptr);
            if (symbol) { CHECK(symbol->GetValue() == "GUI_Rezystor 10k"); symbol->SetValue("GUI_Rezystor 22k"); }
            // pola tekstowe w kolejności tworzenia: symbol, symbol producenta, producent, opis,
            // wartość, jednostka, obudowa, cena
            std::vector<wxTextCtrl*> pola;
            for (wxWindow* d : dlg->GetChildren()) if (auto* t = dynamic_cast<wxTextCtrl*>(d)) pola.push_back(t);
            CHECK(pola.size() >= 8);
            if (pola.size() >= 8) pola[7]->SetValue("0,35");
            wxWindow* ok = Znajdz(dlg, "OK");
            if (ok) Kliknij(ok);
        });
        pe->EdytujWybrany();
        CHECK(Liczba(m, "SELECT count(*) FROM tbl_elementy WHERE symbol = 'GUI_Rezystor 22k' AND cena_jednostkowa = 0.35 "
                        "AND ilosc_dostepna = 2") == 1);
        CHECK(Liczba(m, "SELECT count(*) FROM tbl_elementy WHERE symbol = 'GUI_Rezystor 10k'") == 0);

        // --- przeniesienie kategorii do poziomu głównego (logika używana przez przeciąganie)
        CHECK(logika.przeniesKategorie(idPod, std::nullopt));
        pk->Odswiez();
        pe->Odswiez();
        CHECK(!pe->Elementy().empty() && pe->Elementy()[0].kategoriaPelna == "GUI_Pod2");
        CHECK(logika.przeniesKategorie(idPod, idGlowna));

        // --- dane przykładowe do zrzutu ekranu
        auto idKat = [&](const std::string& pelna) -> std::optional<long long> {
            logika.odswiezKategorie();
            for (const Kategoria& k : logika.kategorie()) if (k.pelnaNazwa == pelna) return k.id;
            return std::nullopt;
        };
        struct Przyklad { const char* symbol; const char* producent; const char* opis; const char* wartosc;
                          const char* obudowa; const char* kat; int ilosc; int min; double cena; };
        const Przyklad przyklady[] = {
            {"GUI_RC0805-10k", "Yageo", "Rezystor 10 kΩ 1% 0,125 W", "10k", "0805", "Elementy bierne / Rezystory / Rezystory SMD", 480, 100, 0.012},
            {"GUI_MF-1k", "Vishay", "Rezystor metalizowany 1 kΩ 1% 0,6 W", "1k", "THT", "Elementy bierne / Rezystory / Rezystory THT", 12, 50, 0.08},
            {"GUI_Elko 100u/25V", "Panasonic", "Kondensator elektrolityczny 100 µF 25 V", "100u", "Ø6,3", "Elementy bierne / Kondensatory / Elektrolityczne", 34, 10, 0.45},
            {"GUI_X7R 100n", "Murata", "Kondensator ceramiczny 100 nF 50 V", "100n", "0603", "Elementy bierne / Kondensatory / Ceramiczne", 900, 200, 0.02},
        };
        for (const Przyklad& p : przyklady) {
            Element e;
            e.symbol = p.symbol; e.producent = p.producent; e.opis = p.opis; e.wartosc = p.wartosc;
            e.obudowa = p.obudowa; e.iloscDostepna = p.ilosc; e.iloscMinimalna = p.min; e.cena = p.cena;
            e.idKategorii = idKat(p.kat);
            CHECK(e.idKategorii.has_value());
            CHECK(logika.zapiszElement(e).has_value());
        }
        pk->Odswiez();
        pe->Szukaj("");
        pk->WybierzKategorie(idKat("Elementy bierne"));
        CHECK(pe->Elementy().size() >= 4);
        Zrzut(okno, "05_okno_z_danymi");
        pe->Szukaj("zolty"); // nic nie pasuje do przykładów
        CHECK(pe->Elementy().empty());
        pe->Szukaj("gui_ kondensator 100");
        CHECK(pe->Elementy().size() == 2);
        Zrzut(okno, "08_wyszukiwanie");
        pe->Szukaj("");

        // --- dziennik zdarzeń: każde zdarzenie zapisane dokładnie raz
        {
            wxString tekstDziennika;
            for (wxWindow* dziecko : okno->GetChildren()) {
                if (auto* t = dynamic_cast<wxTextCtrl*>(dziecko)) tekstDziennika = t->GetValue();
            }
            auto ile = [&](const wxString& fragment) {
                int n = 0;
                for (size_t poz = tekstDziennika.find(fragment); poz != wxString::npos;
                     poz = tekstDziennika.find(fragment, poz + 1)) ++n;
                return n;
            };
            CHECK(ile(wxString::FromUTF8("Dodano kategorię: GUI_Zażółć")) == 1);
            CHECK(ile(wxString::FromUTF8("Dodano podkategorię: GUI_Podkategoria")) == 1);
            CHECK(ile(wxString::FromUTF8("Zmieniono nazwę kategorii na: GUI_Pod2")) == 1);
            CHECK(ile(wxString::FromUTF8("Dodano element: GUI_Rezystor 10k")) == 1);
            CHECK(ile(wxString::FromUTF8("Zmieniono stan elementu GUI_Rezystor 10k: -3")) == 1);
            CHECK(ile(wxString::FromUTF8("Zapisano element: GUI_Rezystor 22k")) == 1);
        }

        // --- sprzątanie
        sprzatanie();
        pk->Odswiez();
        pe->Odswiez();
        CHECK(Liczba(m, "SELECT count(*) FROM tbl_elementy WHERE symbol LIKE 'GUI\\_%'") == 0);
    }

    void Uruchom() {
        // --- Okno główne łączy się samo ---
        auto* okno = new OknoGlowne("Domowy Magazyn");
        okno->Show();
        for (int i = 0; i < 60 && !ManagerBazyDanych::pobierz().czyPolaczony(); ++i) {
            wxMilliSleep(100);
            wxYield();
        }
        wxYield();
        CHECK(ManagerBazyDanych::pobierz().czyPolaczony());
        CHECK(okno->GetStatusBar()->GetStatusText(1).Contains(wxString::FromUTF8(env("DM_TEST_DB", "dm_test"))));
        Zrzut(okno, "01_okno_glowne");

        // --- Dialog połączenia: test, zapis ---
        wxWindow* dlgPol = nullptr;
        CallAfter([&] {
            dlgPol = wxTopLevelWindows.GetLast()->GetData();
            Zrzut(dlgPol, "02_dialog_polaczenia");
            wxWindow* test = Znajdz(dlgPol, wxString::FromUTF8("Test połączenia"));
            CHECK(test != nullptr);
            if (test) Kliknij(test);
            CHECK(ZnajdzZawierajacy(dlgPol, wxString::FromUTF8("Połączenie udane")) != nullptr);
            Zrzut(dlgPol, "03_dialog_polaczenia_po_tescie");
            wxWindow* zapisz = Znajdz(dlgPol, wxString::FromUTF8("Zapisz ustawienia"));
            CHECK(zapisz != nullptr);
            std::cerr << "klik zapisz" << std::endl;
            if (zapisz) Kliknij(zapisz);
            std::cerr << "po zapisz" << std::endl;
            CHECK(ZnajdzZawierajacy(dlgPol, wxString::FromUTF8("zapisane")) != nullptr);
            wxWindow* ok = Znajdz(dlgPol, "OK");
            CHECK(ok != nullptr);
            std::cerr << "klik ok" << std::endl;
            if (ok) Kliknij(ok);
            std::cerr << "po ok" << std::endl;
        });
        {
            DialogPolaczenia dlg(okno);
            dlg.ShowModal();
        }

        // --- Dialog ustawień aplikacji: zmiana, zapis, odczyt ---
        CallAfter([&] {
            wxWindow* dlg = wxTopLevelWindows.GetLast()->GetData();
            Zrzut(dlg, "04_dialog_ustawien");
            wxWindow* ok = Znajdz(dlg, "OK");
            CHECK(ok != nullptr);
            std::cerr << "klik ok" << std::endl;
            if (ok) Kliknij(ok);
            std::cerr << "po ok" << std::endl;
        });
        {
            Konfiguracja& k = Konfiguracja::pobierz();
            UstawieniaBazowe us = k.pobierzUstawieniaAplikacji();
            us.sciezkaObrazki = "/tmp/zażółć/obrazy";
            us.prefixEAN = 27;
            us.nazwaFontu = "Verdana";
            us.pogrubienie = true;
            us.koloryProgow[3] = "#00FF00";
            k.ustawUstawieniaAplikacji(us);
            k.zapisz();

            DialogUstawien dlg(okno);
            dlg.ShowModal();
            CHECK(!dlg.ZmienionoPolaczenie());
        }
        {
            Konfiguracja& k = Konfiguracja::pobierz();
            k.wczytaj();
            const UstawieniaBazowe w = k.pobierzUstawieniaAplikacji();
            CHECK(w.sciezkaObrazki == "/tmp/zażółć/obrazy");
            CHECK(w.prefixEAN == 27 && w.nazwaFontu == "Verdana" && w.pogrubienie);
            CHECK(w.koloryProgow.count(3) == 1);        // OK nie gubi nieedytowanych ustawień
            // OK w oknie ustawień nie może skasować hasła do bazy z pliku
            ManagerBazyDanych::pobierz().wczytajUstawienia();
            CHECK(ManagerBazyDanych::pobierz().pobierzUstawienia().haslo ==
                  env("DM_TEST_PASSWORD", ""));
        }

        TestMagazynu(okno);
        TestProjektow(okno);
        TestRaportowIEtykiet(okno);
        TestSklepow(okno);
        TestPomocy(okno);
        TestMiejsca(okno);
        TestImportuElementow(okno);
        TestKodow(okno);
        TestKalkulatorow(okno);
        TestKamery(okno);
        TestEagle(okno);
        TestKatalogu(okno);

        okno->Close(true);
        ExitMainLoop();
    }
};

wxIMPLEMENT_APP_NO_MAIN(TestApp);

int main(int argc, char** argv) {
    const int wynik = wxEntry(argc, argv);
    std::cout << (bledy == 0 ? "GUI: wszystko OK" : "GUI: są błędy") << "\n";
    return wynik != 0 ? wynik : (bledy == 0 ? 0 : 1);
}
