// Testy warstwy bazy danych i konfiguracji (bez wxWidgets).
//
// Wymagają działającego PostgreSQL. Parametry z zmiennych środowiskowych:
//   DM_TEST_HOST (localhost), DM_TEST_PORT (5432), DM_TEST_DB (dm_test),
//   DM_TEST_USER (postgres), DM_TEST_PASSWORD (puste), DM_TEST_SQL (../sql)
// Uwaga: test wykonuje skrypty SQL na wskazanej bazie i tworzy w niej dane
// testowe (sprząta po sobie). Używaj osobnej bazy testowej.

#include "BazaDanych/ManagerBazyDanych.h"
#include "Eksport/EksportCSV.h"
#include "Import/ImportKategorii.h"
#include "Logika/DopasowanieBOM.h"
#include "Logika/LogikaMagazynu.h"
#include "Logika/LogikaProjektow.h"
#include "Logika/LogikaRaportow.h"
#include "Logika/LogikaSklepow.h"
#include "Eksport/EksportRaport.h"
#include "Narzedzia/Liczby.h"
#include "Narzedzia/Miejsce.h"
#include "Logika/ImportCSVElementow.h"
#include "Logika/ImportElementow.h"
#include "Logika/LogikaKatalogu.h"
#include "Logika/SzeregiE.h"
#include "Pomoc/PomocKontekstowa.h"
#include "Narzedzia/Konfiguracja.h"

#include <algorithm>
#include <clocale>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

static int bledy = 0;
static int sprawdzenia = 0;

#define CHECK(warunek)                                                                  \
    do {                                                                                \
        ++sprawdzenia;                                                                  \
        if (!(warunek)) {                                                               \
            ++bledy;                                                                    \
            std::cerr << "BŁĄD " << __FILE__ << ":" << __LINE__ << "  " #warunek "\n"; \
        }                                                                               \
    } while (0)

static std::string env(const char* nazwa, const char* domyslna) {
    const char* v = std::getenv(nazwa);
    return v ? v : domyslna;
}

static void zapisz(const fs::path& p, const std::string& tresc) {
    std::ofstream(p, std::ios::binary) << tresc;
}

static std::string wczytaj(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

static void testKonfiguracji(const fs::path& tmp) {
    std::cout << "[konfiguracja]\n";
    Konfiguracja& k = Konfiguracja::pobierz();
    ManagerBazyDanych& m = ManagerBazyDanych::pobierz();

    // 1. Stary, płaski plik z BOM i CRLF oraz nieznaną sekcją
    const fs::path plik = tmp / "config.ini";
    zapisz(plik, "\xEF\xBB\xBF# stary\r\nhost=192.168.111.100\r\nport=5433\r\nbaza=elektronika\r\n"
                 "uzytkownik=postgres\r\nhaslo=\r\n\r\n[moduly]\r\nkamera=1\r\n");
    m.ustawSciezkeKonfiguracji(plik.u8string());
    CHECK(m.wczytajUstawienia());
    CHECK(m.pobierzUstawienia().host == "192.168.111.100");
    CHECK(m.pobierzUstawienia().port == 5433);
    CHECK(m.pobierzUstawienia().bazaDanych == "elektronika");
    CHECK(m.pobierzUstawienia().haslo.empty());

    // 2. Zapis zmienia format na sekcyjny, zachowuje nieznane sekcje
    UstawieniaBazy us = m.pobierzUstawienia();
    us.haslo = "tajne łąka'\"";
    m.ustawUstawienia(us);
    CHECK(m.zapiszUstawienia());
    const std::string po = wczytaj(plik);
    CHECK(po.find("[baza]") != std::string::npos);
    CHECK(po.find("[moduly]") != std::string::npos);
    CHECK(po.find("kamera=1") != std::string::npos);
    CHECK(po.find("\nhost=192.168.111.100\n\n") == std::string::npos); // brak starych płaskich kluczy
    CHECK(m.wczytajUstawienia());
    CHECK(m.pobierzUstawienia().haslo == "tajne łąka'\"");
    CHECK(!fs::exists(tmp / "config.ini.tmp"));

    // 3. Bez zapamiętywania hasła plik go nie zawiera
    us.zapamietajHaslo = false;
    m.ustawUstawienia(us);
    CHECK(m.zapiszUstawienia());
    CHECK(wczytaj(plik).find("tajne") == std::string::npos);
    CHECK(wczytaj(plik).find("\nhaslo=") == std::string::npos);

    // 4. Uszkodzone wartości nie przewracają programu
    zapisz(plik, "[baza]\nport=abc\nhost=serwer\n");
    CHECK(m.wczytajUstawienia());
    CHECK(m.pobierzUstawienia().port == 5432);
    CHECK(m.pobierzUstawienia().host == "serwer");
    zapisz(plik, "[baza]\nport=99999\n");
    CHECK(m.wczytajUstawienia());
    CHECK(m.pobierzUstawienia().port == 5432);

    // 5. Brak pliku = domyślne ustawienia
    m.ustawSciezkeKonfiguracji((tmp / "nie_ma.ini").u8string());
    CHECK(m.wczytajUstawienia());
    CHECK(m.pobierzUstawienia().host == "localhost");

    // 6. Ustawienia aplikacji (ścieżki z polskimi znakami, progi kolorów)
    m.ustawSciezkeKonfiguracji((tmp / "app.ini").u8string());
    UstawieniaBazowe ub;
    ub.sciezkaObrazki = "C:/Użytkownik/Zdjęcia";
    ub.prefixEAN = 29;
    ub.rozmiarFontu = 14;
    ub.kursywa = true;
    ub.koloryProgow[5] = "#FFAA00";
    ub.koloryProgow[0] = "#FF0000";
    k.ustawUstawieniaAplikacji(ub);
    CHECK(k.zapisz());
    CHECK(k.wczytaj());
    const UstawieniaBazowe w = k.pobierzUstawieniaAplikacji();
    CHECK(w.sciezkaObrazki == "C:/Użytkownik/Zdjęcia");
    CHECK(w.prefixEAN == 29 && w.rozmiarFontu == 14 && w.kursywa && !w.pogrubienie);
    CHECK(w.koloryProgow.size() == 2 && w.koloryProgow.at(5) == "#FFAA00");

    // 7. Ścieżka z polskimi znakami (UTF-8)
    const fs::path katalogPl = tmp / u8"zażółć_gęślą";
    m.ustawSciezkeKonfiguracji((katalogPl / "config.ini").u8string());
    CHECK(m.zapiszUstawienia());
    CHECK(fs::exists(katalogPl / "config.ini"));

    CHECK(Konfiguracja::znajdzPierwszyIstniejacy({"/nie/ma", plik.u8string()}) == plik.u8string());
    CHECK(Konfiguracja::znajdzPierwszyIstniejacy({"/nie/ma"}).empty());
}

static void testBazy(const fs::path& tmp) {
    std::cout << "[baza danych]\n";
    ManagerBazyDanych& m = ManagerBazyDanych::pobierz();

    UstawieniaBazy us;
    us.host = env("DM_TEST_HOST", "localhost");
    us.port = std::atoi(env("DM_TEST_PORT", "5432").c_str());
    us.bazaDanych = env("DM_TEST_DB", "dm_test");
    us.uzytkownik = env("DM_TEST_USER", "postgres");
    us.haslo = env("DM_TEST_PASSWORD", "");
    us.timeoutSek = 3;

    // Złe hasło / zła baza / zły port dają czytelny błąd i nie wieszają programu
    {
        UstawieniaBazy zle = us;
        zle.haslo = "zle haslo";
        std::string blad;
        CHECK(!ManagerBazyDanych::testujUstawienia(zle, blad));
        CHECK(!blad.empty());
        zle = us;
        zle.bazaDanych = "nie_ma_takiej_bazy";
        CHECK(!ManagerBazyDanych::testujUstawienia(zle, blad));
        CHECK(!blad.empty());
        zle = us;
        zle.port = 1;
        CHECK(!ManagerBazyDanych::testujUstawienia(zle, blad));
        CHECK(!blad.empty());
    }

    std::string blad, wersja;
    CHECK(ManagerBazyDanych::testujUstawienia(us, blad, &wersja));
    CHECK(!wersja.empty());
    CHECK(!m.czyPolaczony()); // test na osobnym połączeniu nie łączy singletona

    m.ustawUstawienia(us);
    CHECK(m.polacz());
    CHECK(m.czyPolaczony());
    CHECK(m.testPolaczenie());

    // Czysta baza testowa
    CHECK(m.wykonaj("DROP SCHEMA public CASCADE"));
    CHECK(m.wykonaj("CREATE SCHEMA public"));
    CHECK(!m.czyBazaZainicjalizowana());
    CHECK(m.wersjaSchematu() == 0);

    // Brak katalogu ze skryptami -> błąd z wyjaśnieniem
    m.ustawKatalogiSQL({(tmp / "brak").u8string()});
    std::string raport;
    CHECK(!m.inicjalizujBaze(&raport) || !m.znajdzKatalogSQL().empty());

    // Inicjalizacja z prawdziwych skryptów; dwukrotnie (idempotentność)
    m.ustawKatalogiSQL({(tmp / "brak").u8string(), env("DM_TEST_SQL", "../sql")});
    CHECK(!m.znajdzKatalogSQL().empty());
    CHECK(m.inicjalizujBaze(&raport));
    if (!m.pobierzBlad().empty()) std::cerr << "  błąd: " << m.pobierzBlad() << "\n";
    CHECK(raport.find("schema.sql: OK") != std::string::npos);
    CHECK(raport.find("dane_startowe.sql: OK") != std::string::npos);
    CHECK(m.czyBazaZainicjalizowana());
    CHECK(m.wersjaSchematu() == ManagerBazyDanych::WYMAGANA_WERSJA_SCHEMATU);
    CHECK(m.inicjalizujBaze(&raport));
    {
        auto w = m.zapytanie("SELECT (SELECT count(*) FROM tbl_sklepy), (SELECT count(*) FROM tbl_kategorie)");
        CHECK(w && w->liczba(0, 0) == 10 && w->liczba(0, 1) > 30);
    }

    const InformacjeOBazie info = m.pobierzInformacje();
    CHECK(info.polaczono && info.zainicjalizowana && info.baza == us.bazaDanych);
    CHECK(info.wersjaSchematu == ManagerBazyDanych::WYMAGANA_WERSJA_SCHEMATU && !info.wersjaSerwera.empty());

    // Zapytania z parametrami, NULL, polskie znaki, SQL injection w parametrze
    CHECK(m.wykonaj("INSERT INTO tbl_elementy (symbol, producent, opis) VALUES ($1, $2, $3)",
                    {std::string("Dławik 10µH"), std::nullopt, std::string("'; DROP TABLE tbl_elementy; --")}));
    {
        auto w = m.zapytanie("SELECT symbol, producent, opis FROM tbl_elementy WHERE symbol = $1",
                             {std::string("Dławik 10µH")});
        CHECK(w && w->liczbaWierszy() == 1);
        CHECK(w && w->tekst(0, 0) == "Dławik 10µH");
        CHECK(w && w->czyNull(0, 1));
        CHECK(w && w->tekst(0, "opis") == "'; DROP TABLE tbl_elementy; --");
        CHECK(w && w->nazwaKolumny(2) == "opis" && w->indeksKolumny("brak") == -1);
        CHECK(w && w->tekst(5, 5).empty() && w->liczba(5, 5, 7) == 7); // poza zakresem jest bezpieczne
    }
    {
        auto w = m.zapytanie("SELECT symbol FROM szukaj_elementow($1)", {std::string("dlawik")});
        CHECK(w && w->liczbaWierszy() == 1);
    }
    CHECK(!m.wykonaj("SELECT * FROM tabela_ktorej_nie_ma"));
    CHECK(m.pobierzBlad().find("tabela_ktorej_nie_ma") != std::string::npos);
    CHECK(m.wykonaj("SELECT 1")); // po błędzie połączenie nadal działa

    // Transakcje: wycofanie, zatwierdzenie, RAII
    PolaczenieBazy* pb = m.pobierzPolaczenie();
    {
        Transakcja t(*pb);
        CHECK(t.czyAktywna());
        CHECK(pb->wykonaj("INSERT INTO tbl_projekty (nazwa) VALUES ('wycofany')"));
    } // brak zatwierdzenia -> ROLLBACK
    CHECK(!pb->wTransakcji());
    {
        Transakcja t(*pb);
        CHECK(pb->wykonaj("INSERT INTO tbl_projekty (nazwa) VALUES ('zatwierdzony')"));
        CHECK(t.zatwierdz());
    }
    {
        auto w = m.zapytanie("SELECT count(*) FILTER (WHERE nazwa='wycofany'), count(*) FILTER (WHERE nazwa='zatwierdzony') FROM tbl_projekty");
        CHECK(w && w->liczba(0, 0) == 0 && w->liczba(0, 1) == 1);
    }
    CHECK(pb->rozpocznijTransakcje());
    CHECK(!pb->rozpocznijTransakcje());      // zagnieżdżenie jest błędem, nie zawiesza
    CHECK(!pb->wykonaj("SELECT bledne"));    // transakcja w stanie błędu...
    CHECK(pb->wycofajTransakcje());          // ...da się wycofać
    CHECK(m.wykonaj("SELECT 1"));

    // Triggery przez warstwę C++ (np. element dostaje linki do sklepów)
    {
        auto w = m.zapytanie("SELECT count(*) FROM tbl_elementy_sklepy");
        CHECK(w && w->liczba(0, 0) == 5);
    }

    // Zerwane połączenie jest odnawiane automatycznie
    {
        UstawieniaBazy us2 = us;
        PolaczenieBazy zabojca;
        CHECK(zabojca.polacz(us2.host, us2.port, us2.bazaDanych, us2.uzytkownik, us2.haslo));
        auto pid = m.zapytanie("SELECT pg_backend_pid()");
        CHECK(pid.has_value());
        CHECK(zabojca.wykonaj("SELECT pg_terminate_backend($1::int)", {pid->tekst(0, 0)}));
        // pierwsza próba po zerwaniu: albo działa dzięki odnowieniu, albo daje błąd, druga musi działać
        bool ok = m.wykonaj("SELECT 1") || m.wykonaj("SELECT 1");
        CHECK(ok);
        CHECK(m.czyPolaczony());
    }

    // Hasło ze spacjami i apostrofem (ucieczka w parametrach połączenia)
    {
        CHECK(m.wykonaj("DROP ROLE IF EXISTS dm_trudne_haslo"));
        CHECK(m.wykonaj("CREATE ROLE dm_trudne_haslo LOGIN PASSWORD 'a b\\'c'") ||
              m.wykonaj("CREATE ROLE dm_trudne_haslo LOGIN PASSWORD E'a b\\'c'"));
        UstawieniaBazy t = us;
        t.uzytkownik = "dm_trudne_haslo";
        t.haslo = "a b'c";
        std::string b;
        // zależnie od pg_hba może być odrzucone, ale nie z powodu błędnego parsowania parametrów
        bool ok = ManagerBazyDanych::testujUstawienia(t, b);
        CHECK(ok || b.find("invalid connection option") == std::string::npos);
        CHECK(b.find("missing \"=\"") == std::string::npos);
        m.wykonaj("DROP ROLE IF EXISTS dm_trudne_haslo");
    }

    // Skrypt SQL z BOM i polskimi znakami
    zapisz(tmp / "bom.sql", "\xEF\xBB\xBF" "CREATE TABLE tmp_test (t text); INSERT INTO tmp_test VALUES ('żółć');");
    CHECK(m.wykonajSkryptSQL((tmp / "bom.sql").u8string()));
    {
        auto w = m.zapytanie("SELECT t FROM tmp_test");
        CHECK(w && w->tekst(0, 0) == "żółć");
    }
    zapisz(tmp / "zly.sql", "CREATE TABLE a (x int); SELEKT 1;");
    CHECK(!m.wykonajSkryptSQL((tmp / "zly.sql").u8string()));
    CHECK(!m.czyTabelaIstnieje("a")); // skrypt jest wykonywany atomowo
    CHECK(!m.wykonajSkryptSQL((tmp / "nie_ma.sql").u8string()));
    zapisz(tmp / "pusty.sql", "  \n");
    CHECK(!m.wykonajSkryptSQL((tmp / "pusty.sql").u8string()));

    m.rozlacz();
    CHECK(!m.czyPolaczony());
    CHECK(!m.wykonaj("SELECT 1") || m.czyPolaczony()); // wykonaj() łączy ponownie wg ustawień
    m.rozlacz();
}


static void testMagazynu(const fs::path& tmp) {
    std::cout << "[magazyn: kategorie, elementy, CSV]\n";

    // Liczby nie mogą zależeć od ustawień regionalnych (program ustawia język polski)
    for (const char* nazwa : {"pl_PL.UTF-8", "pl_PL.utf8", "pl_PL", "Polish_Poland.1250"}) {
        if (std::setlocale(LC_ALL, nazwa)) break;
    }
    double x = 0;
    CHECK(tekstNaLiczbe("12,5", x) && x == 12.5);
    CHECK(tekstNaLiczbe(" 1 234,50 ", x) && x == 1234.5);
    CHECK(tekstNaLiczbe("0.25", x) && x == 0.25);
    CHECK(!tekstNaLiczbe("", x));
    CHECK(!tekstNaLiczbe("abc", x));
    CHECK(!tekstNaLiczbe("1.2.3", x));
    CHECK(!tekstNaLiczbe("12zł", x));
    CHECK(liczbaNaTekstKlasyczny(0.1) == "0.1");
    CHECK(liczbaNaTekstKlasyczny(1234.5) == "1234.5");

    // Parser ścieżek kategorii
    {
        const auto p = ImportKategorii::parsuj("\xEF\xBB\xBF# komentarz\r\n\r\nA / B / C\r\nD > E\r\n"
                                               "  Przetworniki ADC/DAC  \r\nF/G\r\n");
        CHECK(p.size() == 4);
        CHECK(p.size() == 4 && p[0] == std::vector<std::string>({"A", "B", "C"}));
        CHECK(p.size() == 4 && p[1] == std::vector<std::string>({"D", "E"}));
        CHECK(p.size() == 4 && p[2] == std::vector<std::string>({"Przetworniki ADC/DAC"}));
        CHECK(p.size() == 4 && p[3] == std::vector<std::string>({"F/G"}));
        CHECK(ImportKategorii::parsuj("# tylko komentarz\n\n").empty());
    }

    // CSV
    CHECK(EksportCSV::pole("abc") == "abc");
    CHECK(EksportCSV::pole("a;b") == "\"a;b\"");
    CHECK(EksportCSV::pole("a\"b") == "\"a\"\"b\"");
    CHECK(EksportCSV::pole("a\nb") == "\"a\nb\"");
    CHECK(EksportCSV::zbuduj({"x", "y"}, {{"1", "2;3"}}) == "x;y\r\n1;\"2;3\"\r\n");

    ManagerBazyDanych& m = ManagerBazyDanych::pobierz();
    UstawieniaBazy us;
    us.host = env("DM_TEST_HOST", "localhost");
    us.port = std::atoi(env("DM_TEST_PORT", "5432").c_str());
    us.bazaDanych = env("DM_TEST_DB", "dm_test");
    us.uzytkownik = env("DM_TEST_USER", "postgres");
    us.haslo = env("DM_TEST_PASSWORD", "");
    m.ustawUstawienia(us);
    m.ustawKatalogiSQL({env("DM_TEST_SQL", "../sql")});
    CHECK(m.polacz());
    CHECK(m.inicjalizujBaze());

    auto sprzatanie = [&] {
        m.wykonaj("DELETE FROM tbl_bom WHERE id_elementu IN (SELECT id FROM tbl_elementy WHERE symbol LIKE 'TEST\\_%')");
        m.wykonaj("DELETE FROM tbl_projekty WHERE nazwa = 'TEST_PROJEKT'");
        m.wykonaj("DELETE FROM tbl_transakcje WHERE id_elementu IN (SELECT id FROM tbl_elementy WHERE symbol LIKE 'TEST\\_%')");
        m.wykonaj("DELETE FROM tbl_elementy WHERE symbol LIKE 'TEST\\_%'");
        m.wykonaj("DELETE FROM tbl_kategorie WHERE nazwa LIKE 'TEST\\_%' AND id_rodzica IS NOT NULL");
        m.wykonaj("DELETE FROM tbl_kategorie WHERE nazwa LIKE 'TEST\\_%'");
    };
    sprzatanie();

    LogikaMagazynu logika(m);

    // --- kategorie startowe: plik .txt musi odpowiadać temu, co tworzy dane_startowe.sql
    CHECK(logika.odswiezKategorie());
    const size_t liczbaKategorii = logika.kategorie().size();
    CHECK(liczbaKategorii >= 40);
    {
        ImportKategorii::Wynik w;
        std::string blad;
        CHECK(ImportKategorii::importujZPliku(logika, env("DM_TEST_SQL", "../sql") + "/kategorie_startowe.txt", w, blad));
        CHECK(w.sciezek > 30);
        CHECK(w.dodano == 0); // lista startowa = kategorie z dane_startowe.sql
        if (!blad.empty()) std::cerr << blad << "\n";
        CHECK(!ImportKategorii::importujZTekstu(logika, "# nic\n", w, blad));
        CHECK(!ImportKategorii::importujZPliku(logika, (tmp / "nie_ma.txt").u8string(), w, blad));
    }
    // porządek drzewa: rodzic przed dziećmi, pełne nazwy i sumy
    {
        const Kategoria* bierne = nullptr;
        for (const Kategoria& k : logika.kategorie()) {
            if (k.nazwa == "Elementy bierne") bierne = &k;
        }
        CHECK(bierne != nullptr);
        CHECK(bierne && bierne->poziom == 0 && bierne->pelnaNazwa == "Elementy bierne");
        for (size_t i = 0; i < logika.kategorie().size(); ++i) {
            const Kategoria& k = logika.kategorie()[i];
            if (!k.idRodzica) continue;
            const Kategoria* r = logika.znajdzKategorie(*k.idRodzica);
            CHECK(r != nullptr);
            if (r) {
                CHECK(k.pelnaNazwa == r->pelnaNazwa + " / " + k.nazwa);
                CHECK(k.poziom == r->poziom + 1);
            }
        }
    }

    // --- dodawanie / zmiana nazwy / przenoszenie / usuwanie kategorii
    auto glowna = logika.dodajKategorie(std::nullopt, "TEST_Glowna");
    CHECK(glowna.has_value());
    auto dziecko = glowna ? logika.dodajKategorie(glowna, "TEST_Dziecko") : std::nullopt;
    CHECK(dziecko.has_value());
    auto druga = logika.dodajKategorie(std::nullopt, "TEST_Druga");
    CHECK(druga.has_value());
    CHECK(!logika.dodajKategorie(std::nullopt, "TEST_Glowna"));                 // duplikat
    CHECK(logika.pobierzBlad().find("już istnieje") != std::string::npos);
    CHECK(!logika.dodajKategorie(std::nullopt, "test_glowna"));                 // wielkość liter bez znaczenia
    CHECK(!logika.dodajKategorie(std::nullopt, "   "));                         // pusta nazwa
    CHECK(!logika.zmienNazweKategorii(*druga, "TEST_Glowna"));
    CHECK(logika.zmienNazweKategorii(*druga, "TEST_Druga zmieniona"));
    CHECK(!logika.przeniesKategorie(*glowna, dziecko));                          // cykl
    CHECK(!logika.przeniesKategorie(*glowna, glowna));
    CHECK(logika.przeniesKategorie(*dziecko, druga));
    CHECK(logika.odswiezKategorie());
    CHECK(logika.pelnaNazwaKategorii(*dziecko) == "TEST_Druga zmieniona / TEST_Dziecko");
    CHECK(!logika.usunKategorie(*druga));                                        // ma podkategorię
    CHECK(logika.przeniesKategorie(*dziecko, std::nullopt));
    CHECK(logika.odswiezKategorie());
    CHECK(logika.znajdzKategorie(*dziecko)->poziom == 0);
    CHECK(logika.przeniesKategorie(*dziecko, glowna));
    CHECK(logika.odswiezKategorie());
    CHECK(logika.pelnaNazwaKategorii(*dziecko) == "TEST_Glowna / TEST_Dziecko");

    // --- elementy
    Element e;
    e.symbol = "TEST_BC547";
    e.idKategorii = dziecko;
    e.producent = "Żółwik";
    e.opis = "Tranzystor NPN, mały sygnał";
    e.obudowa = "TO-92";
    e.iloscDostepna = 10;
    e.iloscMinimalna = 20;
    e.cena = 0.35;
    e.tagi = "tranzystor npn";
    auto id = logika.zapiszElement(e);
    CHECK(id.has_value());
    {
        auto w = logika.pobierzElement(id.value_or(0));
        CHECK(w.has_value());
        if (w) {
            CHECK(w->symbol == "TEST_BC547" && w->producent == "Żółwik");
            CHECK(w->iloscDostepna == 10 && w->iloscMinimalna == 20);
            CHECK(w->cena && *w->cena == 0.35);
            CHECK(w->waluta == "PLN");
            CHECK(w->idKategorii == dziecko);
            CHECK(w->kategoriaPelna == "TEST_Glowna / TEST_Dziecko");
            CHECK(w->niskiStan());
            CHECK(w->symbolProducenta.empty()); // pusty tekst = NULL = ""
        }
        auto n = m.zapytanie("SELECT count(*), min(typ_transakcji), min(ilosc_zmiana) FROM tbl_transakcje WHERE id_elementu = $1",
                             {std::to_string(id.value_or(0))});
        CHECK(n && n->liczba(0, 0) == 1 && n->tekst(0, 1) == "PRZYJECIE" && n->liczba(0, 2) == 10);
    }
    {   // walidacja
        Element zly;
        CHECK(!logika.zapiszElement(zly));                // brak symbolu
        zly.symbol = "TEST_zly";
        zly.iloscDostepna = -1;
        CHECK(!logika.zapiszElement(zly));
        zly.iloscDostepna = 0;
        zly.cena = -5;
        CHECK(!logika.zapiszElement(zly));
    }
    {   // edycja: zmiana ilości trafia do historii
        auto w = logika.pobierzElement(*id);
        CHECK(w.has_value());
        w->iloscDostepna = 7;
        w->opis = "Zmieniony opis";
        w->cena = std::nullopt;
        CHECK(logika.zapiszElement(*w).has_value());
        auto po = logika.pobierzElement(*id);
        CHECK(po && po->iloscDostepna == 7 && po->opis == "Zmieniony opis" && !po->cena);
        auto n = m.zapytanie("SELECT count(*), sum(ilosc_zmiana) FROM tbl_transakcje WHERE id_elementu = $1",
                             {std::to_string(*id)});
        CHECK(n && n->liczba(0, 0) == 2 && n->liczba(0, 1) == 7);
    }
    {   // wydanie i przyjęcie
        CHECK(!logika.zmienIlosc(*id, -100, "WYDANIE"));
        CHECK(logika.pobierzBlad().find("Niewystarczająca") != std::string::npos);
        CHECK(logika.pobierzElement(*id)->iloscDostepna == 7);
        CHECK(logika.zmienIlosc(*id, 5, "PRZYJECIE", "dostawa"));
        CHECK(logika.pobierzElement(*id)->iloscDostepna == 12);
        CHECK(logika.zmienIlosc(*id, -2, "WYDANIE"));
        CHECK(!logika.zmienIlosc(*id, 0, "KOREKTA"));
    }

    // --- wyszukiwanie i filtry
    Element drugi;
    drugi.symbol = "TEST_LM358";
    drugi.idKategorii = glowna;
    drugi.opis = "Wzmacniacz operacyjny, żółty znacznik";
    drugi.iloscDostepna = 100;
    CHECK(logika.zapiszElement(drugi).has_value());
    {
        std::vector<Element> lista;
        FiltrElementow f;
        f.tekst = "TEST_";
        CHECK(logika.pobierzElementy(f, lista) && lista.size() == 2);
        CHECK(lista.size() == 2 && lista[0].symbol == "TEST_BC547");   // sortowanie po symbolu
        f.tekst = "zolty";                                              // bez polskich znaków
        CHECK(logika.pobierzElementy(f, lista) && lista.size() == 1 && lista[0].symbol == "TEST_LM358");
        f.tekst = "ŻÓŁTY znacznik";                                     // wielkość liter, kilka słów
        CHECK(logika.pobierzElementy(f, lista) && lista.size() == 1);
        f.tekst = "zolty xyz";
        CHECK(logika.pobierzElementy(f, lista) && lista.empty());
        f.tekst = "TEST_'; DROP TABLE tbl_elementy; --";                // brak wstrzyknięcia SQL
        CHECK(logika.pobierzElementy(f, lista) && lista.empty());
        CHECK(m.czyTabelaIstnieje("tbl_elementy"));

        f = FiltrElementow();
        f.tekst = "TEST_";
        f.idKategorii = glowna;                                         // z podkategoriami
        CHECK(logika.pobierzElementy(f, lista) && lista.size() == 2);
        f.zPodkategoriami = false;
        CHECK(logika.pobierzElementy(f, lista) && lista.size() == 1 && lista[0].symbol == "TEST_LM358");
        f.idKategorii = dziecko;
        CHECK(logika.pobierzElementy(f, lista) && lista.size() == 1 && lista[0].symbol == "TEST_BC547");
    }
    // po przyjęciach i wydaniach BC547 ma 10 szt. przy minimum 20, więc jest na liście niskich stanów
    {
        std::vector<Element> lista;
        FiltrElementow f;
        f.tekst = "TEST_";
        f.tylkoNiskieStany = true;
        CHECK(logika.pobierzElementy(f, lista));
        CHECK(lista.size() == 1 && lista[0].symbol == "TEST_BC547");
        f.limit = 1;
        f.tylkoNiskieStany = false;
        CHECK(logika.pobierzElementy(f, lista) && lista.size() == 1);
    }

    // kategoria z elementami nie da się usunąć; sumy w drzewie
    CHECK(logika.odswiezKategorie());
    CHECK(logika.znajdzKategorie(*glowna)->liczbaElementow == 1);
    CHECK(logika.znajdzKategorie(*glowna)->liczbaElementowRazem == 2);
    CHECK(!logika.usunKategorie(*dziecko));
    CHECK(logika.pobierzBlad().find("elementy") != std::string::npos);

    // --- eksport CSV
    {
        std::vector<Element> lista;
        FiltrElementow f;
        f.tekst = "TEST_";
        CHECK(logika.pobierzElementy(f, lista));
        lista[0].cena = 1234.5;
        lista[0].opis = "z \"cudzysłowem\"; i średnikiem";
        std::string blad;
        const fs::path csv = tmp / "elementy.csv";
        CHECK(EksportCSV::zapiszElementy(csv.u8string(), lista, blad));
        const std::string tresc = wczytaj(csv);
        CHECK(tresc.compare(0, 3, "\xEF\xBB\xBF") == 0);
        CHECK(tresc.find("ID;Symbol;") != std::string::npos);
        CHECK(tresc.find("TEST_BC547") != std::string::npos);
        CHECK(tresc.find("\"z \"\"cudzysłowem\"\"; i średnikiem\"") != std::string::npos);
        CHECK(tresc.find(";1234,5;PLN;") != std::string::npos);
        CHECK(tresc.find("TEST_Glowna / TEST_Dziecko") != std::string::npos);
        CHECK(!EksportCSV::zapiszElementy((tmp / "nie_ma_katalogu" / "x.csv").u8string(), lista, blad));
        CHECK(!blad.empty());
    }

    // --- usuwanie elementów
    {
        // bez powiązań (tylko historia ilości) -> usunięty całkiem, razem z historią
        CHECK(logika.usunElement(*id) == WynikUsuniecia::Usuniety);
        CHECK(!logika.pobierzElement(*id).has_value());
        auto n = m.zapytanie("SELECT count(*) FROM tbl_transakcje WHERE id_elementu = $1", {std::to_string(*id)});
        CHECK(n && n->liczba(0, 0) == 0);

        // z powiązaniem w BOM -> archiwizacja (znika z listy, zostaje w bazie)
        std::vector<Element> lista;
        FiltrElementow f;
        f.tekst = "TEST_LM358";
        CHECK(logika.pobierzElementy(f, lista) && lista.size() == 1);
        const long long idLm = lista[0].id;
        CHECK(m.wykonaj("INSERT INTO tbl_projekty (nazwa) VALUES ('TEST_PROJEKT')"));
        CHECK(m.wykonaj("INSERT INTO tbl_bom (id_projektu, id_elementu, ilosc_wymagana) "
                        "SELECT p.id, $1::bigint, 3 FROM tbl_projekty p WHERE p.nazwa = 'TEST_PROJEKT'",
                        {std::to_string(idLm)}));
        CHECK(logika.usunElement(idLm) == WynikUsuniecia::Zarchiwizowany);
        CHECK(logika.pobierzElementy(f, lista) && lista.empty());
        auto k = m.zapytanie("SELECT czy_aktywny FROM tbl_elementy WHERE id = $1", {std::to_string(idLm)});
        CHECK(k && k->liczbaWierszy() == 1 && !k->logiczna(0, 0, true));
        CHECK(logika.usunElement(987654321) == WynikUsuniecia::Usuniety); // brak wiersza: nic do skasowania
    }

    // --- usuwanie kategorii po opróżnieniu
    CHECK(logika.usunKategorie(*dziecko));
    CHECK(!logika.usunKategorie(*glowna)); // zarchiwizowany element nadal jest w kategorii
    sprzatanie();
    CHECK(logika.odswiezKategorie());
    CHECK(logika.kategorie().size() == liczbaKategorii);

    // --- brak połączenia
    m.rozlacz();
    {
        std::vector<Element> lista;
        CHECK(!logika.pobierzElementy(FiltrElementow(), lista));
        CHECK(!logika.pobierzBlad().empty());
        CHECK(!logika.odswiezKategorie());
        CHECK(!logika.zapiszElement(Element{}).has_value());
    }
    std::setlocale(LC_ALL, "C");
}

static void testProjektow(const fs::path& tmp) {
    std::cout << "[projekty, BOM, wydawanie]\n";
    ManagerBazyDanych& m = ManagerBazyDanych::pobierz();
    UstawieniaBazy us;
    us.host = env("DM_TEST_HOST", "localhost");
    us.port = std::atoi(env("DM_TEST_PORT", "5432").c_str());
    us.bazaDanych = env("DM_TEST_DB", "dm_test");
    us.uzytkownik = env("DM_TEST_USER", "postgres");
    us.haslo = env("DM_TEST_PASSWORD", "");
    m.ustawUstawienia(us);
    m.ustawKatalogiSQL({env("DM_TEST_SQL", "../sql")});
    CHECK(m.polacz());
    CHECK(m.inicjalizujBaze());

    auto sprzatanie = [&] {
        m.wykonaj("DELETE FROM tbl_bom WHERE id_projektu IN (SELECT id FROM tbl_projekty WHERE nazwa LIKE 'TEST\\_P%')");
        m.wykonaj("DELETE FROM tbl_transakcje WHERE id_projektu IN (SELECT id FROM tbl_projekty WHERE nazwa LIKE 'TEST\\_P%')");
        m.wykonaj("DELETE FROM tbl_projekty WHERE nazwa LIKE 'TEST\\_P%'");
        m.wykonaj("DELETE FROM tbl_transakcje WHERE id_elementu IN (SELECT id FROM tbl_elementy WHERE symbol LIKE 'TESTP\\_%')");
        m.wykonaj("DELETE FROM tbl_elementy WHERE symbol LIKE 'TESTP\\_%'");
    };
    sprzatanie();

    LogikaMagazynu mag(m);
    LogikaProjektow proj(m, mag);

    auto nowyElement = [&](const std::string& symbol, const std::string& wartosc, const std::string& obudowa, int ilosc) {
        Element e;
        e.symbol = symbol;
        e.wartosc = wartosc;
        e.obudowa = obudowa;
        e.iloscDostepna = ilosc;
        auto id = mag.zapiszElement(e);
        CHECK(id.has_value());
        return id.value_or(0);
    };
    const long long r10k = nowyElement("TESTP_R10K", "10 kΩ", "R0805", 5);
    const long long c100 = nowyElement("TESTP_C100N", "100nF", "0603", 100);
    const long long ne555 = nowyElement("TESTP_NE555", "", "DIP-8", 0);

    // --- dopasowanie (bez bazy, ale na prawdziwych elementach)
    {
        std::vector<Element> magazyn;
        FiltrElementow f;
        f.tekst = "TESTP_";
        CHECK(mag.pobierzElementy(f, magazyn));
        CHECK(magazyn.size() == 3);
        PozycjaBOM p;
        p.wartosc = "10k";
        p.obudowa = "Resistor_SMD:R_0805_2012Metric";
        auto k = DopasowanieBOM::znajdz(p, magazyn);
        CHECK(k.size() == 1 && k[0].idElementu == r10k && k[0].punkty == 80);
        p.obudowa = "R1206";
        k = DopasowanieBOM::znajdz(p, magazyn);
        CHECK(k.size() == 1 && k[0].punkty == 40);
        p.obudowa = "";
        k = DopasowanieBOM::znajdz(p, magazyn);
        CHECK(k.size() == 1 && k[0].punkty == 50);
        PozycjaBOM u;
        u.symbol = "testp_ne-555";
        k = DopasowanieBOM::znajdz(u, magazyn);
        CHECK(k.size() == 1 && k[0].idElementu == ne555 && k[0].punkty == 100);
        PozycjaBOM nic;
        nic.wartosc = "7805";
        CHECK(DopasowanieBOM::znajdz(nic, magazyn).empty());
    }

    // --- projekty
    Projekt pr;
    CHECK(!proj.zapiszProjekt(pr).has_value());  // brak nazwy
    pr.nazwa = "TEST_PROJEKT_A";
    pr.opis = "Zasilacz";
    pr.status = "NIEZNANY";
    CHECK(!proj.zapiszProjekt(pr).has_value());
    pr.status = "PLANOWANY";
    auto idA = proj.zapiszProjekt(pr);
    CHECK(idA.has_value());
    if (!idA) return;
    {
        auto odczyt = proj.pobierzProjekt(*idA);
        CHECK(odczyt && odczyt->nazwa == "TEST_PROJEKT_A" && odczyt->status == "PLANOWANY" && odczyt->liczbaPozycji == 0);
        odczyt->status = "W_TRAKCIE";
        CHECK(proj.zapiszProjekt(*odczyt).has_value());
        CHECK(proj.pobierzProjekt(*idA)->status == "W_TRAKCIE");
    }

    // --- lista części
    PozycjaProjektu poz;
    poz.idProjektu = *idA;
    poz.idElementu = r10k;
    poz.oznaczenia = "R2, R1";
    poz.wymagana = 3;
    CHECK(!proj.dodajPozycje(PozycjaProjektu{}).has_value());
    auto p1 = proj.dodajPozycje(poz);
    CHECK(p1.has_value());
    poz.oznaczenia = "R10";
    poz.wymagana = 2;
    auto p1b = proj.dodajPozycje(poz);  // ten sam element: sumuje
    CHECK(p1b && p1 && *p1b == *p1);
    poz.idElementu = c100;
    poz.oznaczenia = "C1";
    poz.wymagana = 4;
    CHECK(proj.dodajPozycje(poz).has_value());
    poz.idElementu = ne555;
    poz.oznaczenia = "";
    poz.wymagana = 1;
    CHECK(proj.dodajPozycje(poz).has_value());

    std::vector<PozycjaProjektu> lista;
    CHECK(proj.pobierzPozycje(*idA, lista));
    CHECK(lista.size() == 3);
    if (lista.size() == 3) {
        CHECK(lista[0].oznaczenia == "C1");
        CHECK(lista[1].oznaczenia == "R1, R2, R10");  // oznaczenia połączone i posortowane naturalnie
        CHECK(lista[1].wymagana == 5 && lista[1].dostepna == 5 && lista[1].brakuje() == 0);
        CHECK(lista[2].oznaczenia.empty() && lista[2].brakuje() == 1);  // NE555: brak w magazynie
    }
    CHECK(proj.pobierzProjekt(*idA)->liczbaPozycji == 3);
    CHECK(proj.pobierzProjekt(*idA)->liczbaBrakow == 1);

    std::vector<PozycjaProjektu> braki;
    CHECK(proj.pobierzBraki(*idA, braki));
    CHECK(braki.size() == 1 && braki[0].symbol == "TESTP_NE555" && braki[0].brakuje() == 1);

    // lista zakupów do CSV
    {
        const fs::path csv = tmp / "zakupy.csv";
        CHECK(proj.eksportujListeZakupow(csv.u8string(), *idA));
        const std::string tresc = wczytaj(csv);
        CHECK(tresc.compare(0, 3, "\xEF\xBB\xBF") == 0);
        CHECK(tresc.find("TESTP_NE555") != std::string::npos && tresc.find("TESTP_R10K") == std::string::npos);
        CHECK(tresc.find("TEST_PROJEKT_A") != std::string::npos);
    }

    // zmiana pozycji
    PozycjaProjektu zm = lista[2];
    zm.zamowiona = 1;
    zm.uwagi = "zamówione w sklepie";
    CHECK(proj.zapiszPozycje(zm));
    zm.wymagana = 0;
    CHECK(!proj.zapiszPozycje(zm));

    // --- wydawanie
    const long long idR = lista[1].id;
    CHECK(!proj.wydajDoProjektu(idR, 0));
    CHECK(!proj.wydajDoProjektu(idR, 6));  // potrzeba tylko 5
    CHECK(proj.pobierzBlad().find("5") != std::string::npos);
    CHECK(proj.wydajDoProjektu(idR, 2));
    CHECK(mag.pobierzElement(r10k)->iloscDostepna == 3);
    CHECK(!proj.wydajDoProjektu(idR, 4));  // zostało 3 do wydania, więc 4 to za dużo
    CHECK(proj.pobierzPozycje(*idA, lista));
    CHECK(lista[1].zuzyta == 2 && lista[1].doWydania() == 3 && lista[1].brakuje() == 0);
    // za mało w magazynie: ilość nie może zejść poniżej zera, nic się nie zmienia
    CHECK(mag.zmienIlosc(r10k, -2, "KOREKTA"));
    CHECK(!proj.wydajDoProjektu(idR, 2));
    CHECK(mag.pobierzElement(r10k)->iloscDostepna == 1);
    CHECK(proj.pobierzPozycje(*idA, lista));
    CHECK(lista[1].zuzyta == 2);  // wycofane razem z transakcją
    CHECK(mag.zmienIlosc(r10k, 2, "PRZYJECIE"));

    int wydano = 0;
    CHECK(proj.wydajWszystkoDostepne(*idA, &wydano));
    CHECK(wydano == 3 + 4);  // reszta R10K (3) + C100N (4); NE555 niedostępny
    CHECK(mag.pobierzElement(c100)->iloscDostepna == 96);
    CHECK(proj.pobierzPozycje(*idA, lista));
    CHECK(lista[2].zuzyta == 0 && lista[1].zuzyta == 5);
    CHECK(!proj.usunPozycje(lista[1].id));  // wydano już części
    CHECK(proj.usunPozycje(lista[2].id));    // NE555: nic nie wydano

    // --- import BOM
    Projekt pb;
    pb.nazwa = "TEST_PROJEKT_B";
    auto idB = proj.zapiszProjekt(pb);
    CHECK(idB.has_value());
    if (!idB) return;
    std::vector<WierszImportuBOM> wiersze(4);
    wiersze[0].pozycja.oznaczenia = "R1, R2";
    wiersze[0].pozycja.ilosc = 2;
    wiersze[0].akcja = WierszImportuBOM::Akcja::UzyjIstniejacego;
    wiersze[0].idElementu = r10k;
    wiersze[1].pozycja.oznaczenia = "U1";
    wiersze[1].pozycja.symbol = "TESTP_LM358";
    wiersze[1].pozycja.producent = "TI";
    wiersze[1].akcja = WierszImportuBOM::Akcja::UtworzNowy;
    wiersze[2].pozycja.oznaczenia = "C7";
    wiersze[2].pozycja.wartosc = "TESTP_1u";
    wiersze[2].pozycja.obudowa = "0805";
    wiersze[2].akcja = WierszImportuBOM::Akcja::UtworzNowy;
    wiersze[3].pozycja.oznaczenia = "X1";
    wiersze[3].akcja = WierszImportuBOM::Akcja::Pomin;
    WynikImportuDoProjektu wi;
    CHECK(!proj.importujBOM(*idB, wiersze, false, 0, &wi));  // mnożnik
    CHECK(proj.importujBOM(*idB, wiersze, false, 3, &wi));
    CHECK(wi.dodanoPozycji == 3 && wi.utworzonoElementow == 2 && wi.pominieto == 1 && wi.zsumowano == 0);
    CHECK(proj.pobierzPozycje(*idB, lista));
    CHECK(lista.size() == 3);
    {
        bool znaleziono = false;
        for (const auto& p : lista) {
            if (p.idElementu == r10k) {
                znaleziono = true;
                CHECK(p.wymagana == 6 && p.oznaczenia == "R1, R2");  // 2 sztuki x 3 płytki
            }
            if (p.symbol == "TESTP_1u 0805") CHECK(p.wartosc == "TESTP_1u" && p.obudowa == "0805");
        }
        CHECK(znaleziono);
    }
    // powtórny import bez zastępowania sumuje; z zastępowaniem czyści
    CHECK(proj.importujBOM(*idB, wiersze, false, 1, &wi));
    CHECK(wi.zsumowano == 3 && wi.dodanoPozycji == 0 && wi.utworzonoElementow == 0);
    CHECK(proj.importujBOM(*idB, wiersze, true, 1, &wi));
    CHECK(wi.usunieto == 3 && wi.dodanoPozycji == 3);
    CHECK(proj.pobierzPozycje(*idB, lista) && lista.size() == 3);
    // błąd w środku (brak elementu) nie zmienia listy
    wiersze[0].idElementu = 0;
    CHECK(!proj.importujBOM(*idB, wiersze, true, 1, &wi));
    CHECK(proj.pobierzPozycje(*idB, lista) && lista.size() == 3);
    // element utworzony z importu trzeba posprzątać
    m.wykonaj("DELETE FROM tbl_bom WHERE id_projektu = " + std::to_string(*idB));
    m.wykonaj("DELETE FROM tbl_elementy WHERE symbol LIKE 'TESTP\\_%' AND id NOT IN (SELECT id_elementu FROM tbl_bom)"
              " AND symbol NOT IN ('TESTP_R10K','TESTP_C100N','TESTP_NE555')");

    // --- usuwanie projektów
    CHECK(proj.usunProjekt(*idB) == WynikUsuniecia::Usuniety);
    CHECK(!proj.pobierzProjekt(*idB).has_value());
    CHECK(proj.usunProjekt(*idA) == WynikUsuniecia::Zarchiwizowany);  // były wydania
    {
        std::vector<Projekt> wszystkie;
        CHECK(proj.pobierzProjekty(true, wszystkie));
        bool jest = false;
        for (const auto& p : wszystkie) jest = jest || p.id == *idA;
        CHECK(!jest);
        CHECK(proj.pobierzProjekty(false, wszystkie));
        jest = false;
        for (const auto& p : wszystkie) jest = jest || p.id == *idA;
        CHECK(jest);
    }
    CHECK(proj.usunProjekt(987654321) == WynikUsuniecia::Usuniety);

    sprzatanie();
    m.rozlacz();
    std::vector<Projekt> pustaLista;
    CHECK(!proj.pobierzProjekty(true, pustaLista));
    CHECK(proj.pobierzBlad().find("połączenia") != std::string::npos);
}

static void testRaportow(const fs::path& tmp) {
    std::cout << "[raporty]\n";
    ManagerBazyDanych& m = ManagerBazyDanych::pobierz();
    UstawieniaBazy us;
    us.host = env("DM_TEST_HOST", "localhost");
    us.port = std::atoi(env("DM_TEST_PORT", "5432").c_str());
    us.bazaDanych = env("DM_TEST_DB", "dm_test");
    us.uzytkownik = env("DM_TEST_USER", "postgres");
    us.haslo = env("DM_TEST_PASSWORD", "");
    m.ustawUstawienia(us);
    m.ustawKatalogiSQL({env("DM_TEST_SQL", "../sql")});
    CHECK(m.polacz());
    CHECK(m.inicjalizujBaze());

    auto sprzatanie = [&] {
        m.wykonaj("DELETE FROM tbl_bom WHERE id_projektu IN (SELECT id FROM tbl_projekty WHERE nazwa LIKE 'TEST\\_R%')");
        m.wykonaj("DELETE FROM tbl_transakcje WHERE id_projektu IN (SELECT id FROM tbl_projekty WHERE nazwa LIKE 'TEST\\_R%')");
        m.wykonaj("DELETE FROM tbl_projekty WHERE nazwa LIKE 'TEST\\_R%'");
        m.wykonaj("DELETE FROM tbl_transakcje WHERE id_elementu IN (SELECT id FROM tbl_elementy WHERE symbol LIKE 'TESTR\\_%')");
        m.wykonaj("DELETE FROM tbl_elementy WHERE symbol LIKE 'TESTR\\_%'");
        m.wykonaj("DELETE FROM tbl_kategorie WHERE nazwa LIKE 'TEST\\_R%'");
    };
    sprzatanie();

    LogikaMagazynu mag(m);
    LogikaProjektow proj(m, mag);
    LogikaRaportow rap(m, mag, proj);

    CHECK(LogikaRaportow::formatKwoty(1234.5) == "1234,50");
    CHECK(LogikaRaportow::formatKwoty(0.005 * 3) == "0,01" || LogikaRaportow::formatKwoty(0.015) == "0,01" ||
          LogikaRaportow::formatKwoty(0.015) == "0,02");

    auto kat = mag.dodajKategorie(std::nullopt, "TEST_RAP");
    CHECK(kat.has_value());
    Element a;
    a.symbol = "TESTR_A";
    a.idKategorii = kat;
    a.iloscDostepna = 10;
    a.iloscMinimalna = 4;
    a.cena = 0.25;
    Element b = a;
    b.symbol = "TESTR_B";
    b.iloscDostepna = 1;
    b.iloscMinimalna = 6;
    b.cena = 2.0;
    Element c = a;
    c.symbol = "TESTR_C";
    c.cena = std::nullopt;
    CHECK(mag.zapiszElement(a).has_value());
    auto idB = mag.zapiszElement(b);
    CHECK(idB.has_value());
    CHECK(mag.zapiszElement(c).has_value());

    Raport r;
    CHECK(rap.zbuduj(RodzajRaportu::StanMagazynu, {}, r));
    CHECK(r.naglowki.size() == 10 && r.wyrownajDoPrawej.size() == 10 && r.naglowki[5] == "Miejsce");
    int znalezione = 0;
    for (const auto& w : r.wiersze) {
        if (w[0] == "TESTR_A") { ++znalezione; CHECK(w[7] == "0,25" && w[8] == "2,50" && w[6] == "10"); }
        if (w[0] == "TESTR_B") { ++znalezione; CHECK(w[8] == "2,00"); }
        if (w[0] == "TESTR_C") { ++znalezione; CHECK(w[7].empty() && w[8].empty()); }
    }
    CHECK(znalezione == 3);
    CHECK(!r.podsumowanie.empty() && r.podsumowanie[0].find("Elementów:") == 0);

    CHECK(rap.zbuduj(RodzajRaportu::NiskieStany, {}, r));
    bool jestB = false, jestA = false;
    for (const auto& w : r.wiersze) {
        if (w[0] == "TESTR_B") { jestB = true; CHECK(w[8] == "5"); }  // 6 - 1
        if (w[0] == "TESTR_A") jestA = true;
    }
    CHECK(jestB && !jestA);

    CHECK(rap.zbuduj(RodzajRaportu::WartoscWgKategorii, {}, r));
    bool jestKat = false;
    for (const auto& w : r.wiersze) {
        if (w[0] == "TEST_RAP") { jestKat = true; CHECK(w[1] == "3" && w[2] == "21"); CHECK(w[3].find("4,50 PLN") != std::string::npos); }
    }
    CHECK(jestKat);

    // projekt z brakami + wydanie, żeby historia i braki miały dane
    Projekt pr;
    pr.nazwa = "TEST_RAP_PROJEKT";
    auto idP = proj.zapiszProjekt(pr);
    CHECK(idP.has_value());
    if (idP && idB) {
        PozycjaProjektu poz;
        poz.idProjektu = *idP;
        poz.idElementu = *idB;
        poz.wymagana = 5;
        poz.oznaczenia = "X1-X5";
        auto idPoz = proj.dodajPozycje(poz);
        CHECK(idPoz.has_value());
        CHECK(proj.wydajDoProjektu(idPoz.value_or(0), 1));
        CHECK(rap.zbuduj(RodzajRaportu::BrakiWProjektach, {}, r));
        bool brak = false;
        for (const auto& w : r.wiersze) {
            if (w[1] == "TESTR_B") { brak = true; CHECK(w[0] == "TEST_RAP_PROJEKT" && w[6] == "4"); }
        }
        CHECK(brak);
        ParametryRaportu par;
        CHECK(rap.zbuduj(RodzajRaportu::HistoriaZmian, par, r));
        bool wyd = false;
        for (const auto& w : r.wiersze) {
            if (w[1] == "WYDANIE" && w[2] == "TESTR_B" && w[3] == "-1" && w[6] == "TEST_RAP_PROJEKT") wyd = true;
        }
        CHECK(wyd);
        par.dataOd = "2999-01-01";
        CHECK(rap.zbuduj(RodzajRaportu::HistoriaZmian, par, r) && r.wiersze.empty());
        par.dataOd = "2000-01-01";
        par.dataDo = "2999-01-01";
        CHECK(rap.zbuduj(RodzajRaportu::HistoriaZmian, par, r) && !r.wiersze.empty());
        par.dataOd = "x'; DROP TABLE tbl_elementy;--";
        CHECK(!rap.zbuduj(RodzajRaportu::HistoriaZmian, par, r));
        CHECK(!rap.pobierzBlad().empty());
        par.dataOd = "";
        par.dataDo = "";
        par.limitWierszy = 1;
        CHECK(rap.zbuduj(RodzajRaportu::HistoriaZmian, par, r) && r.wiersze.size() == 1 && r.ucieto);
    }

    // zapis: CSV i HTML
    CHECK(rap.zbuduj(RodzajRaportu::NiskieStany, {}, r));
    r.wiersze.push_back({"<b>\"zły\" & znak</b>", "", "", "", "", "0", "0", "0"});
    std::string blad;
    const fs::path csv = tmp / "raport.csv", html = tmp / "raport.html";
    CHECK(EksportRaport::zapiszCSV(csv.u8string(), r, blad));
    CHECK(wczytaj(csv).compare(0, 3, "\xEF\xBB\xBF") == 0);
    CHECK(EksportRaport::zapiszHTML(html.u8string(), r, "2026-10-11", blad));
    const std::string h = wczytaj(html);
    CHECK(h.find("&lt;b&gt;&quot;zły&quot; &amp; znak&lt;/b&gt;") != std::string::npos);
    CHECK(h.find("<b>\"zły\"") == std::string::npos);
    CHECK(h.find("charset=\"utf-8\"") != std::string::npos && h.find("Niskie stany") != std::string::npos);
    CHECK(!EksportRaport::zapiszHTML((tmp / "brak_katalogu" / "x.html").u8string(), r, "", blad) && !blad.empty());

    sprzatanie();
    m.rozlacz();
    CHECK(!rap.zbuduj(RodzajRaportu::StanMagazynu, {}, r));
    CHECK(rap.pobierzBlad().find("połączenia") != std::string::npos);
}

static void testSklepow(const fs::path& tmp) {
    (void)tmp;
    std::cout << "[sklepy]\n";
    ManagerBazyDanych& m = ManagerBazyDanych::pobierz();
    UstawieniaBazy us;
    us.host = env("DM_TEST_HOST", "localhost");
    us.port = std::atoi(env("DM_TEST_PORT", "5432").c_str());
    us.bazaDanych = env("DM_TEST_DB", "dm_test");
    us.uzytkownik = env("DM_TEST_USER", "postgres");
    us.haslo = env("DM_TEST_PASSWORD", "");
    m.ustawUstawienia(us);
    m.ustawKatalogiSQL({env("DM_TEST_SQL", "../sql")});
    CHECK(m.polacz());
    CHECK(m.inicjalizujBaze());
    CHECK(m.wersjaSchematu() == ManagerBazyDanych::WYMAGANA_WERSJA_SCHEMATU);

    auto sprzatanie = [&] {
        m.wykonaj("DELETE FROM tbl_elementy_sklepy WHERE id_elementu IN (SELECT id FROM tbl_elementy WHERE symbol LIKE 'TESTS\\_%')");
        m.wykonaj("DELETE FROM tbl_elementy WHERE symbol LIKE 'TESTS\\_%'");
        m.wykonaj("DELETE FROM tbl_sklepy WHERE nazwa LIKE 'TESTS\\_%'");
        m.wykonaj("DELETE FROM tbl_kursy_walut WHERE waluta = 'CHF'");
    };
    sprzatanie();

    LogikaMagazynu mag(m);
    LogikaSklepow sk(m);

    // sklepy startowe mają szablony wyszukiwania
    std::vector<Sklep> wszystkie;
    CHECK(sk.pobierzSklepy(true, wszystkie));
    bool tme = false;
    for (const Sklep& s : wszystkie) if (s.nazwa == "TME") tme = s.szablonWyszukiwania.find("{q}") != std::string::npos;
    CHECK(tme);

    // parsowanie wiersza
    Sklep p; std::string blad;
    CHECK(LogikaSklepow::parsujWierszSklepu("TESTS_X; https://x.pl/ ;https://x.pl/s?q={q};eur;12,50;300", p, blad));
    CHECK(p.waluta == "EUR" && p.kosztDostawy == 12.5 && p.darmowaDostawaOd && *p.darmowaDostawaOd == 300);
    CHECK(LogikaSklepow::parsujWierszSklepu("TESTS_X\thttps://x.pl/", p, blad) && p.waluta == "PLN");
    CHECK(LogikaSklepow::parsujWierszSklepu("TESTS_X;;https://x.pl/szukaj?q={q}", p, blad) && p.urlBazowy == "https://x.pl/");
    CHECK(!LogikaSklepow::parsujWierszSklepu(";https://x.pl/", p, blad));
    CHECK(!LogikaSklepow::parsujWierszSklepu("TESTS_X;https://x.pl/;https://x.pl/s", p, blad));
    CHECK(!LogikaSklepow::parsujWierszSklepu("TESTS_X;https://x.pl/;;EURO", p, blad));
    CHECK(!LogikaSklepow::parsujWierszSklepu("TESTS_X;https://x.pl/;;PLN;abc", p, blad));
    CHECK(LogikaSklepow::parsujWierszSklepu("TESTS_X", p, blad) && p.urlBazowy.empty());

    // adres wyszukiwania
    Sklep sz; sz.szablonWyszukiwania = "https://s.pl/?q={q}&x=1";
    CHECK(LogikaSklepow::adresWyszukiwania(sz, "BC547 B/2") == "https://s.pl/?q=BC547%20B%2F2&x=1");
    CHECK(LogikaSklepow::adresWyszukiwania(Sklep(), "x").empty());

    // import listy
    WynikImportuSklepow wi;
    const std::string lista =
        "nazwa;adres strony;szablon wyszukiwania;waluta;koszt dostawy;darmowa dostawa od\n"
        "# komentarz\n"
        "TESTS_A;https://a.pl/;https://a.pl/szukaj?q={q};PLN;9,99;200\n"
        "TESTS_B;https://b.com/;;USD;5;\n"
        "\n"
        ";https://bez-nazwy.pl/\n"
        "TESTS_C;;https://c.pl/s?q={q}\n";
    CHECK(sk.importujSklepy(lista, wi));
    CHECK(wi.dodano == 3 && wi.zaktualizowano == 0 && wi.bledy.size() == 1);
    CHECK(wi.bledy.size() == 1 && wi.bledy[0].find("wiersz 6") == 0);
    WynikImportuSklepow wi2;
    CHECK(sk.importujSklepy("tests_a;;;EUR;3", wi2));   // ta sama nazwa, inna wielkość liter
    CHECK(wi2.dodano == 0 && wi2.zaktualizowano == 1);
    WynikImportuSklepow wi3;
    CHECK(sk.importujSklepy("TESTS_Z", wi3) && wi3.dodano == 0 && wi3.bledy.size() == 1);   // nowy sklep bez adresu
    CHECK(sk.pobierzSklepy(false, wszystkie));
    Sklep sa, sb;
    for (const Sklep& s : wszystkie) { if (s.nazwa == "TESTS_A") sa = s; if (s.nazwa == "TESTS_B") sb = s; }
    CHECK(sa.id > 0 && sa.waluta == "EUR" && sa.kosztDostawy == 3 && sa.szablonWyszukiwania.find("{q}") != std::string::npos);
    CHECK(sa.urlBazowy == "https://a.pl/" && sa.darmowaDostawaOd && *sa.darmowaDostawaOd == 200);
    CHECK(sb.id > 0 && sb.waluta == "USD" && !sb.darmowaDostawaOd);
    CHECK(sk.eksportujSklepy().find("TESTS_B;https://b.com/;;USD;5;") != std::string::npos);

    // zapis sklepu: walidacje
    Sklep n; n.nazwa = "TESTS_N"; n.urlBazowy = "https://n.pl/";
    n.waluta = "zl";
    CHECK(!sk.zapiszSklep(n));
    n.waluta = "pln";
    n.szablonWyszukiwania = "https://n.pl/s";
    CHECK(!sk.zapiszSklep(n) && sk.pobierzBlad().find("{q}") != std::string::npos);
    n.szablonWyszukiwania = "";
    n.kosztDostawy = -1;
    CHECK(!sk.zapiszSklep(n));
    n.kosztDostawy = 7.5;
    auto idN = sk.zapiszSklep(n);
    CHECK(idN.has_value());
    Sklep dup = n; dup.id = 0; dup.nazwa = "tests_n";
    CHECK(!sk.zapiszSklep(dup));
    auto wczytany = sk.pobierzSklep(*idN);
    CHECK(wczytany && wczytany->waluta == "PLN" && wczytany->kosztDostawy == 7.5);
    wczytany->priorytet = 9;
    wczytany->darmowaDostawaOd = 99.5;
    CHECK(sk.zapiszSklep(*wczytany).has_value());
    auto po = sk.pobierzSklep(*idN);
    CHECK(po && po->priorytet == 9 && po->darmowaDostawaOd && *po->darmowaDostawaOd == 99.5);
    CHECK(!sk.pobierzSklep(999999999).has_value());

    // elementy i linki
    auto kat = mag.dodajKategorie(std::nullopt, "TEST_SKL");
    (void)kat;
    Element e; e.symbol = "TESTS_E1"; e.iloscDostepna = 1;
    auto idE = mag.zapiszElement(e);
    CHECK(idE.has_value());
    LinkSklepu l; l.idElementu = *idE; l.idSklepu = sa.id;
    CHECK(!sk.zapiszLink(l));                         // brak adresu
    l.url = "ftp://x";
    CHECK(!sk.zapiszLink(l));                         // zły protokół
    l.url = "https://a.pl/produkt/1";
    l.cena = 2.5;
    auto idL = sk.zapiszLink(l);
    CHECK(idL.has_value());
    CHECK(!sk.zapiszLink(l));                         // ten sam sklep drugi raz
    // nowy element dostaje od wyzwalacza linki wyszukiwania do głównych sklepów; szukamy własnych
    std::vector<LinkSklepu> linki;
    auto dla = [&](long long idSklepu) -> const LinkSklepu* {
        for (const LinkSklepu& x : linki) if (x.idSklepu == idSklepu) return &x;
        return nullptr;
    };
    CHECK(sk.pobierzLinki(*idE, linki) && dla(sa.id));
    CHECK(dla(sa.id) && dla(sa.id)->nazwaSklepu == "TESTS_A" && dla(sa.id)->cena && *dla(sa.id)->cena == 2.5 &&
          dla(sa.id)->waluta == "EUR");
    CHECK(dla(sa.id) && !dla(sa.id)->ostatniaWeryfikacja.empty());

    LinkSklepu l2; l2.idElementu = *idE; l2.idSklepu = sb.id; l2.url = "https://b.com/x"; l2.domyslny = true;
    l2.minimalnaIlosc = 10;
    auto idL2 = sk.zapiszLink(l2);
    CHECK(idL2.has_value());
    l.id = *idL; l.domyslny = true; l.cena = 2.5;
    CHECK(sk.zapiszLink(l).has_value());              // zmiana na domyślny: drugi przestaje
    CHECK(sk.pobierzLinki(*idE, linki) && linki[0].id == *idL && linki[0].domyslny);
    int domyslnych = 0;
    for (const LinkSklepu& x : linki) domyslnych += x.domyslny;
    CHECK(domyslnych == 1);
    CHECK(dla(sb.id) && dla(sb.id)->minimalnaIlosc == 10 && !dla(sb.id)->cena);

    // ceny i historia
    std::vector<WpisHistoriiCeny> h;
    CHECK(sk.pobierzHistorie(*idL, h) && h.size() == 1 && h[0].zrodlo == "reczna");
    CHECK(sk.ustawCene(*idL, 2.5, "EUR", "", "strona"));          // ta sama cena tego samego dnia: bez nowego wpisu
    CHECK(sk.pobierzHistorie(*idL, h) && h.size() == 1);
    CHECK(sk.ustawCene(*idL, 2.2, "EUR", "Dostępny", "strona"));
    CHECK(sk.pobierzHistorie(*idL, h) && h.size() == 2 && h[0].cena == 2.2 && h[0].zrodlo == "strona");
    auto lk = sk.pobierzLink(*idL);
    CHECK(lk && lk->cena && *lk->cena == 2.2 && lk->status == "Dostępny");
    CHECK(!sk.ustawCene(*idL, -1, "EUR", "", "strona"));
    long long aktualne = 0;
    if (auto w = m.zapytanie("SELECT count(*) FROM tbl_historia_cen_sklepy WHERE id_elementu_sklepu = " + std::to_string(*idL) + " AND czy_aktualna"))
        aktualne = w->liczba(0, 0);
    CHECK(aktualne == 1);

    // oferty do zestawienia (tylko z ceną, tylko aktywne sklepy)
    std::vector<LinkSklepu> oferty;
    CHECK(sk.pobierzLinkiElementow({*idE}, oferty) && oferty.size() == 1 && oferty[0].idSklepu == sa.id);
    CHECK(sk.pobierzLinkiElementow({}, oferty) && oferty.empty());

    // kursy
    std::map<std::string, double> kursy;
    CHECK(sk.pobierzKursy(kursy) && kursy.count("PLN") && kursy.count("USD") && kursy.count("EUR"));
    CHECK(sk.zapiszKurs("chf", 4.65) && sk.pobierzKursy(kursy) && kursy["CHF"] == 4.65);
    CHECK(sk.zapiszKurs("CHF", 4.7) && sk.pobierzKursy(kursy) && kursy["CHF"] == 4.7);
    CHECK(!sk.zapiszKurs("CHF", 0) && !sk.zapiszKurs("PLN", 2) && !sk.zapiszKurs("zl", 1));

    // usuwanie: sklep z linkami jest tylko wyłączany
    CHECK(sk.usunSklep(sa.id) == WynikUsuniecia::Zarchiwizowany);
    auto wy = sk.pobierzSklep(sa.id);
    CHECK(wy && !wy->aktywny && wy->liczbaLinkow == 1);
    CHECK(sk.pobierzLinkiElementow({*idE}, oferty) && oferty.empty());   // wyłączony sklep znika z ofert
    CHECK(sk.usunSklep(*idN) == WynikUsuniecia::Usuniety);  // priorytet 9: bez automatycznych linków
    CHECK(sk.usunLink(*idL) && sk.usunLink(*idL2));
    CHECK(sk.pobierzLinki(*idE, linki) && !dla(sa.id) && !dla(sb.id));
    CHECK(sk.usunSklep(sa.id) == WynikUsuniecia::Usuniety);

    // historia kasuje się razem z linkiem
    long long zostalo = -1;
    if (auto w = m.zapytanie("SELECT count(*) FROM tbl_historia_cen_sklepy WHERE id_elementu_sklepu = " + std::to_string(*idL)))
        zostalo = w->liczba(0, 0);
    CHECK(zostalo == 0);

    sprzatanie();
    m.wykonaj("DELETE FROM tbl_kategorie WHERE nazwa = 'TEST_SKL'");

    // bez połączenia
    m.rozlacz();
    CHECK(!sk.pobierzSklepy(false, wszystkie) && sk.pobierzBlad().find("połączenia") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Pomoc kontekstowa: parser, plik pola.txt, tabela tbl_pomoc_pol (v4)
// ---------------------------------------------------------------------------
static void testPomocy(const fs::path&) {
    std::cout << "== pomoc kontekstowa\n";
    PomocKontekstowa p;
    std::vector<std::string> uwagi;
    const int n = p.wczytajZTekstu(
        "\xEF\xBB\xBF# komentarz\n\nkl.a\tTytuł A\tOpis A\tkotwica-a\r\nkl.b\tB\tOpis B\n"
        "zly\tbez opisu\nkl.a\tA2\tOpis A2\tk2\n", &uwagi);
    CHECK(n == 3 && uwagi.size() == 1 && uwagi[0].find("wiersz 5") == 0);
    CHECK(p.liczba() == 2);
    const WpisPomocy* a = p.znajdz("kl.a");
    CHECK(a && a->tytul == "A2" && a->opis == "Opis A2" && a->kotwica == "k2");
    const WpisPomocy* b = p.znajdz("kl.b");
    CHECK(b && b->kotwica.empty());
    CHECK(p.znajdz("brak") == nullptr);
    CHECK(p.wczytajZPliku("/nie/ma/takiego.txt") == -1);

    // plik źródłowy opisów
    PomocKontekstowa plik;
    const std::string sciezka = env("DM_TEST_POMOC", "../pomoc/pola.txt");
    std::vector<std::string> bl;
    const int wczytano = plik.wczytajZPliku(sciezka, &bl);
    CHECK(wczytano > 40 && bl.empty());
    const WpisPomocy* sym = plik.znajdz("element.symbol");
    CHECK(sym && !sym->opis.empty() && sym->kotwica == "pole-symbol");

    // tabela w bazie, zasilona skryptem pomoc_pola.sql
    ManagerBazyDanych& m = ManagerBazyDanych::pobierz();
    if (!m.czyPolaczony()) CHECK(m.polacz());   // poprzedni test rozłącza
    CHECK(m.inicjalizujBaze());
    PolaczenieBazy* pol = m.pobierzPolaczenie();
    CHECK(pol != nullptr);
    if (!pol) return;
    auto licz = [&] {
        auto o = pol->zapytanie("SELECT count(*) FROM tbl_pomoc_pol");
        return o ? std::atoi(o->tekst(0, 0).c_str()) : -1;
    };
    CHECK(licz() == wczytano);

    PomocKontekstowa zBazy;
    CHECK(zBazy.wczytajZBazy(m) == wczytano);
    const WpisPomocy* ds = zBazy.znajdz("element.symbol");
    CHECK(ds && ds->opis == sym->opis);

    // ręczna zmiana opisu ustawia flagę i przeżywa ponowną aktualizację struktury
    CHECK(m.wykonaj("UPDATE tbl_pomoc_pol SET opis = 'Mój opis' WHERE klucz = 'element.symbol'"));
    auto fl = pol->zapytanie("SELECT zmienione FROM tbl_pomoc_pol WHERE klucz = 'element.symbol'");
    CHECK(fl && fl->tekst(0, 0) == "t");
    CHECK(m.inicjalizujBaze());
    auto op = pol->zapytanie("SELECT opis, zmienione FROM tbl_pomoc_pol WHERE klucz = 'element.symbol'");
    CHECK(op && op->tekst(0, 0) == "Mój opis" && op->tekst(0, 1) == "t");
    PomocKontekstowa zmieniona;
    zmieniona.wczytajZTekstu("element.symbol\tx\tz pliku\tpole-symbol\n");
    zmieniona.wczytajZBazy(m);   // baza nadpisuje plik
    CHECK(zmieniona.znajdz("element.symbol")->opis == "Mój opis");

    // zwykła aktualizacja nadpisuje opisy nieruszane ręcznie
    CHECK(m.wykonaj("DELETE FROM tbl_pomoc_pol WHERE klucz IN ('element.symbol', 'element.tagi')"));
    CHECK(m.inicjalizujBaze());
    CHECK(licz() == wczytano);
    auto po = pol->zapytanie("SELECT opis, zmienione FROM tbl_pomoc_pol WHERE klucz = 'element.symbol'");
    CHECK(po && po->tekst(0, 0) == sym->opis && po->tekst(0, 1) == "f");
}

// ---------------------------------------------------------------------------
// Miejsce magazynowania (schemat 5)
// ---------------------------------------------------------------------------
static void testMiejsc(const fs::path&) {
    std::cout << "== miejsca magazynowania\n";
    auto p = MiejsceMagazynowe::parsuj("Kontener 1 szufladka A1/1");
    CHECK(p.kontener == "Kontener 1" && p.szuflada == "A1" && p.przegroda == "1");
    p = MiejsceMagazynowe::parsuj("Kontener 1, A1/1");
    CHECK(p.kontener == "Kontener 1" && p.szuflada == "A1" && p.przegroda == "1");
    p = MiejsceMagazynowe::parsuj("A1");
    CHECK(p.kontener.empty() && p.szuflada == "A1" && p.przegroda.empty());
    p = MiejsceMagazynowe::parsuj("Kontener 2 B12");
    CHECK(p.kontener == "Kontener 2" && p.szuflada == "B12");
    p = MiejsceMagazynowe::parsuj("K2/B3/4");
    CHECK(p.kontener == "K2" && p.szuflada == "B3" && p.przegroda == "4");
    p = MiejsceMagazynowe::parsuj("Regał 3 półka górna");
    CHECK(p.kontener == "Regał 3 półka górna" && p.szuflada.empty());
    CHECK(MiejsceMagazynowe::parsuj("   ").puste());
    CHECK((MiejsceMagazynowe{"Kontener 1", "A1", "1"}).naTekst() == "Kontener 1, A1/1");
    CHECK((MiejsceMagazynowe{"", "A1", ""}).naTekst() == "A1");
    CHECK((MiejsceMagazynowe{"Kontener 1", "", ""}).naTekst() == "Kontener 1");
    CHECK(MiejsceMagazynowe::parsuj(MiejsceMagazynowe{"Kontener 7", "C3", "2"}.naTekst()).naTekst() == "Kontener 7, C3/2");

    ManagerBazyDanych& m = ManagerBazyDanych::pobierz();
    if (!m.czyPolaczony()) CHECK(m.polacz());
    CHECK(m.inicjalizujBaze());
    CHECK(m.wersjaSchematu() == ManagerBazyDanych::WYMAGANA_WERSJA_SCHEMATU);
    LogikaMagazynu mag(m);
    m.wykonaj("DELETE FROM tbl_elementy WHERE symbol LIKE 'TESTM\\_%'");

    Element a; a.symbol = "TESTM_A"; a.wartosc = "10k"; a.ustawMiejsce({"TESTM Kontener 1", "A1", "1"});
    Element b; b.symbol = "TESTM_B"; b.wartosc = "22k"; b.ustawMiejsce({"TESTM Kontener 1", "A2", ""});
    Element c; c.symbol = "TESTM_C"; c.wartosc = "47k";   // bez miejsca
    auto ia = mag.zapiszElement(a), ib = mag.zapiszElement(b), ic = mag.zapiszElement(c);
    CHECK(ia && ib && ic);
    if (!ia || !ib || !ic) return;

    auto wa = mag.pobierzElement(*ia);
    CHECK(wa && wa->kontener == "TESTM Kontener 1" && wa->szuflada == "A1" && wa->przegroda == "1");
    CHECK(wa && wa->miejsceTekst() == "TESTM Kontener 1, A1/1");
    auto wc = mag.pobierzElement(*ic);
    CHECK(wc && wc->miejsce().puste() && wc->miejsceTekst().empty());

    // edycja miejsca
    wc->ustawMiejsce({"", "B7", ""});
    CHECK(mag.zapiszElement(*wc).has_value());
    wc = mag.pobierzElement(*ic);
    CHECK(wc && wc->kontener.empty() && wc->szuflada == "B7");

    // wyszukiwanie po szufladzie, po "A1/1" i po kontenerze
    auto znajdz = [&](const std::string& tekst) {
        FiltrElementow f; f.tekst = tekst;
        std::vector<Element> w;
        mag.pobierzElementy(f, w);
        std::vector<std::string> sym;
        for (const Element& e : w) if (e.symbol.rfind("TESTM_", 0) == 0) sym.push_back(e.symbol);
        return sym;
    };
    CHECK(znajdz("testm kontener 1").size() == 2);
    CHECK(znajdz("A1/1") == std::vector<std::string>{"TESTM_A"});
    CHECK(znajdz("b7") == std::vector<std::string>{"TESTM_C"});

    // podpowiedzi
    const auto kontenery = mag.pobierzKontenery();
    CHECK(std::find(kontenery.begin(), kontenery.end(), "TESTM Kontener 1") != kontenery.end());
    const auto szuflady = mag.pobierzSzuflady("TESTM Kontener 1");
    CHECK(szuflady == (std::vector<std::string>{"A1/1", "A2"}));
    CHECK(mag.pobierzSzuflady("").size() >= 1);   // elementy bez kontenera: B7

    // zmiana nazwy kontenera
    auto ile = mag.zmienNazweKontenera("TESTM Kontener 1", "TESTM Kontener 9");
    CHECK(ile && *ile == 2);
    wa = mag.pobierzElement(*ia);
    CHECK(wa && wa->kontener == "TESTM Kontener 9" && wa->szuflada == "A1");
    CHECK(!mag.zmienNazweKontenera("", "x").has_value());

    // CSV: miejsce w ostatnich kolumnach
    { auto nag = EksportCSV::naglowkiElementow(); CHECK(nag.size() >= 3 && nag[nag.size() - 3] == "Przegroda"); }
    std::vector<Element> lista{*wa};
    { auto w0 = EksportCSV::wierszeElementow(lista)[0]; CHECK(w0.size() >= 3 && w0[w0.size() - 3] == "1"); }

    m.wykonaj("DELETE FROM tbl_elementy WHERE symbol LIKE 'TESTM\\_%'");
}

// ---------------------------------------------------------------------------
// Import zestawów elementów (generator szeregów, CSV)
// ---------------------------------------------------------------------------
static void testImportuElementow(const fs::path&) {
    std::cout << "== import elementów\n";
    ManagerBazyDanych& m = ManagerBazyDanych::pobierz();
    if (!m.czyPolaczony()) CHECK(m.polacz());
    CHECK(m.inicjalizujBaze());
    LogikaMagazynu mag(m);
    ImportElementow imp(mag);
    auto sprzatanie = [&] {
        m.wykonaj("DELETE FROM tbl_transakcje WHERE id_elementu IN (SELECT id FROM tbl_elementy WHERE obudowa = 'TESTI' OR symbol LIKE 'TESTI\\_%')");
        m.wykonaj("DELETE FROM tbl_elementy_sklepy WHERE id_elementu IN (SELECT id FROM tbl_elementy WHERE obudowa = 'TESTI' OR symbol LIKE 'TESTI\\_%')");
        m.wykonaj("DELETE FROM tbl_elementy WHERE obudowa = 'TESTI' OR symbol LIKE 'TESTI\\_%'");
        for (int i = 0; i < 4; ++i)
            m.wykonaj("DELETE FROM tbl_kategorie k WHERE k.nazwa LIKE 'TESTI%' AND NOT EXISTS (SELECT 1 FROM tbl_kategorie c WHERE c.id_rodzica = k.id)");
    };
    sprzatanie();

    // rozbijanie ścieżek kategorii
    CHECK((ImportElementow::rozbijSciezke("A / B > C") == std::vector<std::string>{"A", "B", "C"}));
    CHECK((ImportElementow::rozbijSciezke("Przetworniki ADC/DAC") == std::vector<std::string>{"Przetworniki ADC/DAC"}));
    CHECK(ImportElementow::rozbijSciezke("  ").empty());

    SzeregiE::ParametrySzeregu p;
    p.obudowa = "TESTI"; p.od = 100; p.doo = 1000; p.ilosc = 10; p.minimum = 3; p.kategoria = "TESTI Rezystory / SMD";
    p.miejsce = {"TESTI Kontener", "A1", ""}; p.szufladaNaDekade = true;
    auto gen = SzeregiE::generujSzereg(p);
    CHECK(gen.size() == 25);

    WynikImportuElementow w;
    CHECK(imp.wykonaj(gen, TrybDuplikatow::Pomin, w));
    CHECK(w.dodano == 25 && w.pominieto == 0 && w.bledy.empty() && w.utworzonoKategorii == 2);
    auto e = mag.znajdzPoSymbolu("r-470r-testi-5%");          // bez rozróżniania wielkości liter
    CHECK(e && e->iloscDostepna == 10 && e->iloscMinimalna == 3 && e->tolerancja == "5%" && e->wartosc == "470");
    CHECK(e && e->kontener == "TESTI Kontener" && e->szuflada == "A1" && e->kategoriaPelna == "TESTI Rezystory / SMD");
    auto e1k = mag.znajdzPoSymbolu("R-1K-TESTI-5%");
    CHECK(e1k && e1k->szuflada == "A2");
    // stan początkowy zapisany w historii
    auto hist = m.zapytanie("SELECT count(*) FROM tbl_transakcje WHERE id_elementu = " + std::to_string(e ? e->id : 0) + " AND typ_transakcji = 'PRZYJECIE'");
    CHECK(hist && hist->liczba(0, 0) == 1);

    // ponowny import: nic się nie dubluje
    CHECK(imp.wykonaj(gen, TrybDuplikatow::Pomin, w));
    CHECK(w.dodano == 0 && w.pominieto == 25 && w.utworzonoKategorii == 0);
    CHECK(mag.pobierzSymbole().count("r-100r-testi-5%") == 1);

    // dodanie ilości
    CHECK(imp.wykonaj(gen, TrybDuplikatow::DodajIlosc, w));
    CHECK(w.zsumowano == 25 && w.dodano == 0);
    e = mag.znajdzPoSymbolu("R-470R-TESTI-5%");
    CHECK(e && e->iloscDostepna == 20);

    // aktualizacja danych bez zmiany stanu, tylko niepuste pola
    SzeregiE::ParametrySzeregu p2 = p;
    p2.tolerancja = "1%"; p2.ilosc = 99; p2.producent = "Yageo"; p2.wzorSymbolu = "R-{rkm}-{obudowa}-5%";
    p2.miejsce = {"", "", ""};
    auto gen2 = SzeregiE::generujSzereg(p2);
    CHECK(imp.wykonaj(gen2, TrybDuplikatow::Aktualizuj, w));
    CHECK(w.zaktualizowano == 25);
    e = mag.znajdzPoSymbolu("R-470R-TESTI-5%");
    CHECK(e && e->iloscDostepna == 20 && e->tolerancja == "1%" && e->producent == "Yageo" && e->szuflada == "A1");

    // CSV: pozycje błędne są raportowane, poprawne importowane; brakujące kategorie powstają
    auto csv = ImportCSVElementow::parsuj(
        "Symbol;Wartość;Obudowa;Ilość;Kategoria;Miejsce\n"
        "TESTI_BC547;;TO-92;25;TESTI Półprzewodniki / Tranzystory;Kontener 2 szufladka C3/1\n"
        ";;;5;;\n"
        "TESTI_BC557;;TO-92;abc;;\n");
    CHECK(csv.blad.empty() && csv.pozycje.size() == 3);
    CHECK(imp.wykonaj(csv.pozycje, TrybDuplikatow::Pomin, w));
    CHECK(w.dodano == 1 && w.bledy.size() == 2 && w.utworzonoKategorii == 2);
    CHECK(w.bledy.size() == 2 && w.bledy[0].find("wiersz 3") == 0 && w.bledy[1].find("wiersz 4") == 0);
    auto bc = mag.znajdzPoSymbolu("TESTI_BC547");
    CHECK(bc && bc->iloscDostepna == 25 && bc->kontener == "Kontener 2" && bc->szuflada == "C3" && bc->przegroda == "1");
    CHECK(bc && bc->kategoriaPelna == "TESTI Półprzewodniki / Tranzystory");
    CHECK(!mag.znajdzPoSymbolu("TESTI_BC557").has_value());

    sprzatanie();
}

// ---------------------------------------------------------------------------
// Katalog elementów
// ---------------------------------------------------------------------------
static void testKatalogu(const fs::path&) {
    std::cout << "== katalog\n";
    ManagerBazyDanych& m = ManagerBazyDanych::pobierz();
    if (!m.czyPolaczony()) CHECK(m.polacz());
    CHECK(m.inicjalizujBaze());
    LogikaMagazynu mag(m);
    LogikaKatalogu kat(m);
    auto sprzatanie = [&] {
        m.wykonaj("DELETE FROM tbl_transakcje WHERE id_elementu IN (SELECT id FROM tbl_elementy WHERE symbol LIKE 'TESTK\\_%')");
        m.wykonaj("DELETE FROM tbl_elementy WHERE symbol LIKE 'TESTK\\_%'");
        m.wykonaj("DELETE FROM tbl_katalog WHERE symbol LIKE 'TESTK\\_%'");
    };
    sprzatanie();

    PozycjaKatalogu k;
    k.symbol = "TESTK_LM358"; k.producent = "TI"; k.opis = "Podwójny wzmacniacz"; k.obudowa = "DIP-8";
    k.rodzina = "Wzmacniacze operacyjne";
    k.parametry = {{"Napięcie zasilania", "3-32 V", "reczny"}, {"Pasmo", "1 MHz", "datasheet"}, {"Wartość", "LM358", "reczny"}};
    auto id = kat.zapisz(k);
    CHECK(id.has_value());
    if (!id) return;
    // ten sam symbol i producent (inne wielkości liter) jest duplikatem, inny producent nie
    PozycjaKatalogu d = k; d.symbol = "testk_lm358"; d.producent = "ti";
    CHECK(!kat.zapisz(d).has_value() && !kat.pobierzBlad().empty());
    d.producent = "ST";
    auto id2 = kat.zapisz(d);
    CHECK(id2.has_value() && *id2 != *id);

    auto z = kat.pobierzPozycje(*id);
    CHECK(z && z->symbol == "TESTK_LM358" && z->parametry.size() == 3 && z->parametry[1].zrodlo == "datasheet");
    CHECK(z && z->parametr("pasmo") && z->liczbaElementow == 0 && z->wMagazynie == 0);

    // powiązanie z magazynem po symbolu i po symbolu producenta
    Element e; e.symbol = "TESTK_LM358"; e.iloscDostepna = 7;
    Element e2; e2.symbol = "TESTK_INNY"; e2.symbolProducenta = "testk_lm358"; e2.iloscDostepna = 3;
    CHECK(mag.zapiszElement(e).has_value() && mag.zapiszElement(e2).has_value());
    z = kat.pobierzPozycje(*id);
    CHECK(z && z->liczbaElementow == 2 && z->wMagazynie == 10);
    auto dla = kat.dlaElementu(e);
    CHECK(dla && dla->symbol == "TESTK_LM358");
    FiltrKatalogu f; f.tylkoWMagazynie = true; f.tekst = "TESTK_LM";
    std::vector<PozycjaKatalogu> lista;
    CHECK(kat.pobierz(f, lista) && lista.size() == 2);       // oba wpisy (TI i ST) pasują do symbolu
    f.tylkoWMagazynie = false; f.tekst = "wzmacniacz"; f.rodzina = "wzmacniacze OPERACYJNE";
    CHECK(kat.pobierz(f, lista) && lista.size() == 2);
    f.rodzina = "inna";
    CHECK(kat.pobierz(f, lista) && lista.empty());
    CHECK(!kat.rodziny().empty());

    // parametry: bez nadpisywania i z nadpisywaniem
    CHECK(kat.dodajParametry(*id, {{"Pasmo", "2 MHz", "datasheet"}, {"Temperatura pracy", "0...70 °C", "datasheet"}}, false));
    z = kat.pobierzPozycje(*id);
    CHECK(z && z->parametry.size() == 4 && z->parametr("Pasmo")->wartosc == "1 MHz");
    CHECK(kat.dodajParametry(*id, {{"pasmo", "2 MHz", "datasheet"}}, true));
    z = kat.pobierzPozycje(*id);
    CHECK(z && z->parametry.size() == 4 && z->parametr("Pasmo")->wartosc == "2 MHz");

    // z katalogu do magazynu: element dostaje dane i parametry, drugi raz się nie da
    auto el = LogikaKatalogu::elementZKatalogu(*z);
    CHECK(el.symbol == "TESTK_LM358" && el.symbolProducenta == "TESTK_LM358" && el.producent == "TI" && el.wartosc == "LM358");
    CHECK(el.parametry.find("Napięcie zasilania: 3-32 V") != std::string::npos && el.parametry.find("Wartość") == std::string::npos);
    CHECK(!kat.dodajDoMagazynu(mag, *id, 5, {"K", "A1", ""}, std::nullopt).has_value());    // symbol już jest
    m.wykonaj("DELETE FROM tbl_transakcje WHERE id_elementu IN (SELECT id FROM tbl_elementy WHERE symbol LIKE 'TESTK\\_%')");
    m.wykonaj("DELETE FROM tbl_elementy WHERE symbol LIKE 'TESTK\\_%'");
    auto ide = kat.dodajDoMagazynu(mag, *id, 5, {"TESTK K", "A1", "2"}, std::nullopt);
    CHECK(ide.has_value());
    auto nowy = ide ? mag.pobierzElement(*ide) : std::nullopt;
    CHECK(nowy && nowy->iloscDostepna == 5 && nowy->szuflada == "A1" && nowy->przegroda == "2" && nowy->obudowa == "DIP-8");

    // import CSV: dodanie, pominięcie, aktualizacja, błąd wiersza
    auto csv = KatalogCSV::parsuj(
        "Symbol;Producent;Opis;Napięcie wejściowe\n"
        "TESTK_NOWY;ABC;Nowy element;5 V\n"
        "TESTK_LM358;TI;Zmieniony opis;\n"
        ";;Bez symbolu;\n");
    CHECK(csv.pozycje.size() == 3);
    WynikImportuKatalogu w;
    CHECK(kat.importuj(csv.pozycje, TrybKatalogu::Pomin, w));
    CHECK(w.dodano == 1 && w.pominieto == 1 && w.bledy.size() == 1 && w.bledy[0].find("wiersz 4") == 0);
    z = kat.pobierzPozycje(*id);
    CHECK(z && z->opis == "Podwójny wzmacniacz");
    CHECK(kat.importuj(csv.pozycje, TrybKatalogu::Aktualizuj, w));
    CHECK(w.dodano == 0 && w.zaktualizowano == 2 && w.bledy.size() == 1);
    z = kat.pobierzPozycje(*id);
    CHECK(z && z->opis == "Zmieniony opis" && z->parametry.size() == 4);      // parametry zachowane

    // usunięcie pozycji katalogu nie rusza magazynu, a parametry znikają kaskadowo
    CHECK(kat.usun(*id) && !kat.pobierzPozycje(*id).has_value());
    auto par = m.zapytanie("SELECT count(*) FROM tbl_katalog_parametry WHERE id_katalogu = " + std::to_string(*id));
    CHECK(par && par->liczba(0, 0) == 0);
    CHECK(mag.znajdzPoSymbolu("TESTK_LM358").has_value());
    sprzatanie();
}

int main() {
    const fs::path tmp = fs::temp_directory_path() / "dm_test_tmp";
    fs::remove_all(tmp);
    fs::create_directories(tmp);

    testKonfiguracji(tmp);
    testBazy(tmp);
    testMagazynu(tmp);
    testProjektow(tmp);
    testRaportow(tmp);
    testSklepow(tmp);
    testPomocy(tmp);
    testMiejsc(tmp);
    testImportuElementow(tmp);
    testKatalogu(tmp);

    fs::remove_all(tmp);
    std::cout << sprawdzenia << " sprawdzeń, " << bledy << " błędów\n";
    return bledy == 0 ? 0 : 1;
}
