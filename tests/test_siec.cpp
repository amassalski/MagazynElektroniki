// Testy warstwy sieciowej: klient HTTP, sprawdzanie cen, datasheety.
// Serwer testowy działa lokalnie (gniazda POSIX), więc test nie dotyka prawdziwych sklepów.
#include "BazaDanych/ManagerBazyDanych.h"
#include "Logika/Aktualizacje.h"
#include "Logika/LogikaMagazynu.h"
#include "Logika/LogikaSklepow.h"
#include "Wyszukiwanie/KlientHTTP.h"
#include "Wyszukiwanie/SprawdzanieCen.h"
#include "Wyszukiwanie/WyszukiwanieDatasheet.h"

#include <wx/apptrait.h>
#include <wx/app.h>
#include <wx/evtloop.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/utils.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <thread>

static int sprawdzenia = 0, bledy = 0;
#define CHECK(w) do { ++sprawdzenia; if (!(w)) { ++bledy; std::cerr << "BŁĄD " << __FILE__ << ":" << __LINE__ << ": " #w "\n"; } } while (0)

static std::string env(const char* n, const char* d) { const char* v = std::getenv(n); return v ? v : d; }

// ---------------------------------------------------------------------------
// Serwer testowy
// ---------------------------------------------------------------------------
struct Odpowiedz {
    int kod = 200;
    std::string typ = "text/html; charset=utf-8";
    std::string tresc;
    std::string naglowki;   ///< dodatkowe, każdy zakończony \r\n
    int opoznienieMs = 0;   ///< przed odpowiedzią
};

class Serwer {
public:
    std::map<std::string, Odpowiedz> strony;
    std::atomic<int> zapytan{0};
    std::string ostatniAgent;
    int port = 0;

    bool start() {
        fd_ = socket(AF_INET, SOCK_STREAM, 0);
        int jeden = 1;
        setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &jeden, sizeof jeden);
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        a.sin_port = 0;
        if (bind(fd_, reinterpret_cast<sockaddr*>(&a), sizeof a) != 0 || listen(fd_, 16) != 0) return false;
        socklen_t dl = sizeof a;
        getsockname(fd_, reinterpret_cast<sockaddr*>(&a), &dl);
        port = ntohs(a.sin_port);
        watek_ = std::thread([this] { petla(); });
        return true;
    }
    void stop() {
        stop_ = true;
        if (watek_.joinable()) watek_.join();
        if (fd_ >= 0) close(fd_);
    }
    std::string adres(const std::string& sciezka) const { return "http://127.0.0.1:" + std::to_string(port) + sciezka; }

private:
    int fd_ = -1;
    std::atomic<bool> stop_{false};
    std::thread watek_;

    void petla() {
        while (!stop_) {
            pollfd p{fd_, POLLIN, 0};
            if (poll(&p, 1, 50) <= 0) continue;
            int k = accept(fd_, nullptr, nullptr);
            if (k < 0) continue;
            std::thread([this, k] { obsluz(k); }).detach();
        }
    }
    void obsluz(int k) {
        std::string zad;
        char b[2048];
        while (zad.find("\r\n\r\n") == std::string::npos) {
            const ssize_t n = recv(k, b, sizeof b, 0);
            if (n <= 0) { close(k); return; }
            zad.append(b, static_cast<size_t>(n));
            if (zad.size() > 65536) break;
        }
        ++zapytan;
        const size_t sp1 = zad.find(' ');
        const size_t sp2 = zad.find(' ', sp1 + 1);
        const std::string sciezka = zad.substr(sp1 + 1, sp2 - sp1 - 1);
        std::string male = zad;
        for (char& ch : male) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        const size_t ua = male.find("\r\nuser-agent:");
        if (ua != std::string::npos) {
            const size_t kon = zad.find("\r\n", ua + 2);
            ostatniAgent = zad.substr(ua + 14, kon - ua - 14);
            while (!ostatniAgent.empty() && ostatniAgent[0] == ' ') ostatniAgent.erase(0, 1);
        }
        auto it = strony.find(sciezka);
        Odpowiedz o;
        if (it == strony.end()) { o.kod = 404; o.tresc = "<html>brak</html>"; }
        else o = it->second;
        if (o.opoznienieMs > 0) std::this_thread::sleep_for(std::chrono::milliseconds(o.opoznienieMs));
        const char* opis = o.kod == 200 ? "OK" : o.kod == 302 ? "Found" : o.kod == 404 ? "Not Found" : "X";
        std::string glowa = "HTTP/1.1 " + std::to_string(o.kod) + " " + opis + "\r\nContent-Type: " + o.typ +
                            "\r\nContent-Length: " + std::to_string(o.tresc.size()) + "\r\nConnection: close\r\n" + o.naglowki + "\r\n";
        send(k, glowa.data(), glowa.size(), MSG_NOSIGNAL);
        size_t poz = 0;
        while (poz < o.tresc.size()) {
            const ssize_t n = send(k, o.tresc.data() + poz, std::min<size_t>(65536, o.tresc.size() - poz), MSG_NOSIGNAL);
            if (n <= 0) break;
            poz += static_cast<size_t>(n);
        }
        shutdown(k, SHUT_WR);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        close(k);
    }
};

// ---------------------------------------------------------------------------
// Pętla zdarzeń na potrzeby testu
// ---------------------------------------------------------------------------
static wxEventLoopBase* petla = nullptr;

static bool czekaj(const std::function<bool()>& warunek, int limitMs = 15000) {
    const auto start = std::chrono::steady_clock::now();
    while (!warunek()) {
        if (std::chrono::steady_clock::now() - start > std::chrono::milliseconds(limitMs)) return false;
        petla->DispatchTimeout(20);
        wxTheApp->ProcessPendingEvents();
    }
    return true;
}

static WynikHTTP stronaSync(const std::string& url, int limit = 20000) {
    WynikHTTP w; bool gotowe = false;
    KlientHTTP::pobierzStrone(url, [&](const WynikHTTP& x) { w = x; gotowe = true; }, limit);
    CHECK(czekaj([&] { return gotowe; }, limit + 5000));
    return w;
}
static WynikHTTP plikSync(const std::string& url, const wxString& sciezka, const std::string& poczatek = "") {
    WynikHTTP w; bool gotowe = false;
    KlientHTTP::pobierzPlik(url, sciezka, [&](const WynikHTTP& x) { w = x; gotowe = true; }, 20000, poczatek);
    CHECK(czekaj([&] { return gotowe; }));
    return w;
}

static std::string czytaj(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

static const char* STRONA_PRODUKTU = R"(<html><head><meta charset="utf-8"><title>Rezystor</title>
<script type="application/ld+json">{"@type":"Product","name":"R10k","offers":{"price":"3,25","priceCurrency":"PLN","availability":"https://schema.org/InStock"}}</script>
</head><body><a href="/pliki/r10k.pdf">Karta katalogowa</a><a href="/inne.html">inne</a></body></html>)";

static void testKlienta(Serwer& srv, const std::filesystem::path& tmp) {
    std::cout << "[klient HTTP]\n";
    // czysta logika
    CHECK(KlientHTTP::adresDozwolony("https://a.pl/x") && KlientHTTP::adresDozwolony("HTTP://a.pl"));
    CHECK(!KlientHTTP::adresDozwolony("ftp://a.pl") && !KlientHTTP::adresDozwolony("file:///etc/passwd") && !KlientHTTP::adresDozwolony("http://") && !KlientHTTP::adresDozwolony(""));
    CHECK(KlientHTTP::naUTF8("a\xB3" "b", "text/html; charset=windows-1250") == "a\xC5\x82" "b");
    CHECK(KlientHTTP::naUTF8("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=ISO-8859-2\">z\xB3", "") .find("z\xC5\x82") != std::string::npos);
    CHECK(KlientHTTP::naUTF8("z\xC5\x82", "text/html; charset=utf-8") == "z\xC5\x82");
    CHECK(KlientHTTP::naUTF8("z\xC5\x82", "text/html") == "z\xC5\x82");
    CHECK(KlientHTTP::naUTF8("x", "text/html; charset=nieistniejace-kodowanie") == "x");

    srv.strony["/produkt"].tresc = STRONA_PRODUKTU;
    srv.strony["/przekieruj"] = {302, "text/html", "", "Location: /produkt\r\n", 0};
    srv.strony["/cp1250"] = {200, "text/html; charset=windows-1250", "<html>z\xB3" "oty</html>", "", 0};
    srv.strony["/wolna"] = {200, "text/html", "x", "", 3000};
    srv.strony["/duza"] = {200, "text/html", std::string(KlientHTTP::MAKS_STRONA + 100000, 'a'), "", 0};
    srv.strony["/dobry.pdf"] = {200, "application/pdf", "%PDF-1.4\n1 0 obj<<>>endobj\n%%EOF", "", 0};
    srv.strony["/falszywy.pdf"] = {200, "application/pdf", "<html>to nie pdf</html>", "", 0};
    srv.strony["/pusty.pdf"] = {200, "application/pdf", "", "", 0};

    WynikHTTP w = stronaSync(srv.adres("/produkt"));
    CHECK(w.ok && w.kod == 200 && w.tresc.find("Rezystor") != std::string::npos && w.typ.find("text/html") != std::string::npos);
    CHECK(srv.ostatniAgent.find("DomowyMagazyn") == 0);

    w = stronaSync(srv.adres("/nie-ma"));
    CHECK(!w.ok && w.kod == 404 && w.blad.find("404") != std::string::npos);

    // plik wersja.txt: nowsza wersja z opisem i adresem; brak pliku (404) nie może być wzięty za wersję
    srv.strony["/wersja.txt"] = {200, "text/plain", "v9.9.9\nhttps://example.com/pobierz\nCo nowego: testy\n", "", 0};
    w = stronaSync(srv.adres("/wersja.txt"));
    {
        const auto info = Aktualizacje::parsujPlikWersji(w.tresc);
        CHECK(w.ok && info && info->wersja == "9.9.9" && Aktualizacje::nowsza(info->wersja, "1.0.0") &&
              info->adresPobierania == "https://example.com/pobierz" && info->opis == "Co nowego: testy");
        const WynikHTTP brak = stronaSync(srv.adres("/brak-wersji.txt"));
        CHECK(!brak.ok && !Aktualizacje::parsujPlikWersji(brak.tresc));
    }

    w = stronaSync(srv.adres("/przekieruj"));
    CHECK(w.ok && w.adres == srv.adres("/produkt") && w.tresc.find("Rezystor") != std::string::npos);

    w = stronaSync(srv.adres("/cp1250"));
    CHECK(w.ok && w.tresc.find("z\xC5\x82oty") != std::string::npos);

    w = stronaSync(srv.adres("/wolna"), 800);
    CHECK(!w.ok && w.blad.find("czasie") != std::string::npos);

    w = stronaSync(srv.adres("/duza"));
    CHECK(!w.ok && w.blad.find("zbyt duża") != std::string::npos);

    w = stronaSync("ftp://127.0.0.1/x");
    CHECK(!w.ok && w.blad.find("http") != std::string::npos);

    w = stronaSync("http://127.0.0.1:1/zamkniety");   // port, na którym nikt nie słucha
    CHECK(!w.ok && !w.blad.empty());

    // pliki
    const wxString dobry = wxString::FromUTF8((tmp / "dobry.pdf").string());
    w = plikSync(srv.adres("/dobry.pdf"), dobry, "%PDF");
    CHECK(w.ok && std::filesystem::exists(tmp / "dobry.pdf") && !std::filesystem::exists(tmp / "dobry.pdf.part"));
    CHECK(czytaj(tmp / "dobry.pdf").rfind("%PDF-1.4", 0) == 0);

    const wxString falszywy = wxString::FromUTF8((tmp / "falszywy.pdf").string());
    w = plikSync(srv.adres("/falszywy.pdf"), falszywy, "%PDF");
    CHECK(!w.ok && w.blad.find("%PDF") != std::string::npos);
    CHECK(!std::filesystem::exists(tmp / "falszywy.pdf") && !std::filesystem::exists(tmp / "falszywy.pdf.part"));

    w = plikSync(srv.adres("/pusty.pdf"), wxString::FromUTF8((tmp / "pusty.pdf").string()), "");
    CHECK(!w.ok && !std::filesystem::exists(tmp / "pusty.pdf"));

    w = plikSync(srv.adres("/nie-ma.pdf"), wxString::FromUTF8((tmp / "nie-ma.pdf").string()), "%PDF");
    CHECK(!w.ok && w.kod == 404 && !std::filesystem::exists(tmp / "nie-ma.pdf.part"));

    // istniejący plik jest zastępowany tylko po udanym pobraniu
    { std::ofstream(tmp / "stary.pdf") << "%PDF-stary"; }
    w = plikSync(srv.adres("/falszywy.pdf"), wxString::FromUTF8((tmp / "stary.pdf").string()), "%PDF");
    CHECK(!w.ok && czytaj(tmp / "stary.pdf") == "%PDF-stary");
    w = plikSync(srv.adres("/dobry.pdf"), wxString::FromUTF8((tmp / "stary.pdf").string()), "%PDF");
    CHECK(w.ok && czytaj(tmp / "stary.pdf").find("obj") != std::string::npos);

    // nieosiągalny katalog
    w = plikSync(srv.adres("/dobry.pdf"), wxString::FromUTF8((tmp / "brak_katalogu" / "x.pdf").string()), "%PDF");
    CHECK(!w.ok && !w.blad.empty());
}

static void testDatasheetow(Serwer& srv, const std::filesystem::path& tmp) {
    std::cout << "[datasheety]\n";
    using namespace WyszukiwanieDatasheet;
    CHECK(nazwaPliku("BC547 B/2") == "BC547_B_2.pdf");
    CHECK(nazwaPliku("../../etc/passwd") == "etc_passwd.pdf");
    CHECK(nazwaPliku("Dławik 10µH") == "Dławik_10µH.pdf");
    CHECK(nazwaPliku("///") == "element.pdf" && nazwaPliku("") == "element.pdf");
    CHECK(nazwaPliku(std::string(300, 'a')).size() == 104);
    CHECK(nazwaPliku("a:b*c?.pdf").find_first_of("/\\:*?\"<>|") == std::string::npos);
    auto ad = adresySerwisow("BC547 B");
    CHECK(ad.size() == 3 && ad[0].url.find("BC547%20B") != std::string::npos && ad[0].url.rfind("https://", 0) == 0);

    srv.strony["/pliki/r10k.pdf"] = {200, "application/pdf", "%PDF-1.5 r10k", "", 0};
    bool gotowe = false, ok = false; std::string blad; std::vector<LinkDatasheet> linki;
    szukajNaStronie(srv.adres("/produkt"), [&](bool o, const std::string& b, std::vector<LinkDatasheet> l) { ok = o; blad = b; linki = l; gotowe = true; });
    CHECK(czekaj([&] { return gotowe; }));
    CHECK(ok && linki.size() == 2 || (ok && linki.size() == 1));
    CHECK(!linki.empty() && linki[0].url == srv.adres("/pliki/r10k.pdf"));

    gotowe = false;
    szukajNaStronie(srv.adres("/nie-ma"), [&](bool o, const std::string& b, std::vector<LinkDatasheet>) { ok = o; blad = b; gotowe = true; });
    CHECK(czekaj([&] { return gotowe; }));
    CHECK(!ok && !blad.empty());

    const wxString katalog = wxString::FromUTF8((tmp / "dokumenty" / "podkatalog").string());
    CHECK(!czyPobrany(katalog, "R 10k/0805"));
    gotowe = false; wxString sciezka;
    pobierz(srv.adres("/pliki/r10k.pdf"), katalog, "R 10k/0805", [&](bool o, const std::string& b, const wxString& s) { ok = o; blad = b; sciezka = s; gotowe = true; });
    CHECK(czekaj([&] { return gotowe; }));
    CHECK(ok && czyPobrany(katalog, "R 10k/0805") && sciezka == sciezkaPliku(katalog, "R 10k/0805"));
    CHECK(czytaj(std::filesystem::path(sciezka.utf8_string())) == "%PDF-1.5 r10k");

    gotowe = false;
    pobierz(srv.adres("/falszywy.pdf"), katalog, "Zly", [&](bool o, const std::string& b, const wxString&) { ok = o; blad = b; gotowe = true; });
    CHECK(czekaj([&] { return gotowe; }));
    CHECK(!ok && !czyPobrany(katalog, "Zly"));
    CHECK(!otworz(sciezkaPliku(katalog, "Zly")));   // nie ma pliku
}

static void testCen(Serwer& srv) {
    std::cout << "[sprawdzanie cen]\n";
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
        m.wykonaj("DELETE FROM tbl_elementy_sklepy WHERE id_elementu IN (SELECT id FROM tbl_elementy WHERE symbol LIKE 'TESTN\\_%')");
        m.wykonaj("DELETE FROM tbl_elementy WHERE symbol LIKE 'TESTN\\_%'");
        m.wykonaj("DELETE FROM tbl_sklepy WHERE nazwa LIKE 'TESTN\\_%'");
    };
    sprzatanie();

    LogikaMagazynu mag(m);
    LogikaSklepow sk(m);
    srv.strony["/bez-ceny"].tresc = "<html><body>Brak informacji o cenie</body></html>";
    srv.strony["/wlasny"].tresc = "<html><body><b class=\"cena-brutto\">7,50&nbsp;zł</b></body></html>";
    srv.strony["/usd"].tresc = "<meta property=\"product:price:amount\" content=\"1.25\"><meta property=\"product:price:currency\" content=\"USD\">";

    Sklep a; a.nazwa = "TESTN_A"; a.urlBazowy = "http://127.0.0.1/"; a.priorytet = 9;
    Sklep b = a; b.nazwa = "TESTN_B"; b.regexCeny = R"(cena-brutto">([0-9 ,.]+))";
    Sklep c = a; c.nazwa = "TESTN_C"; c.waluta = "EUR";
    Sklep d = a; d.nazwa = "TESTN_D";
    auto ia = sk.zapiszSklep(a), ib = sk.zapiszSklep(b), ic = sk.zapiszSklep(c), id = sk.zapiszSklep(d);
    CHECK(ia && ib && ic && id);
    Element e; e.symbol = "TESTN_E";
    auto ie = mag.zapiszElement(e);
    CHECK(ie.has_value());

    auto link = [&](long long sklep, const std::string& sciezka) {
        LinkSklepu l; l.idElementu = *ie; l.idSklepu = sklep; l.url = srv.adres(sciezka);
        auto i = sk.zapiszLink(l);
        CHECK(i.has_value());
        return i.value_or(0);
    };
    const long long l1 = link(*ia, "/produkt");      // JSON-LD: 3,25 PLN
    const long long l2 = link(*ib, "/wlasny");       // wzorzec sklepu
    const long long l3 = link(*ic, "/bez-ceny");     // brak ceny
    const long long l4 = link(*id, "/nie-ma");       // 404

    std::vector<Sklep> sklepy;
    CHECK(sk.pobierzSklepy(false, sklepy));
    std::vector<LinkSklepu> linki;
    CHECK(sk.pobierzLinki(*ie, linki));
    std::vector<LinkSklepu> moje;
    for (const LinkSklepu& l : linki) if (l.id == l1 || l.id == l2 || l.id == l3 || l.id == l4) moje.push_back(l);
    auto zadania = SprawdzaczCen::ZadaniaDlaLinkow(moje, sklepy, "TESTN_E");
    CHECK(zadania.size() == 4);
    bool zWzorcem = false;
    for (const auto& z : zadania) if (z.idLinku == l2) zWzorcem = !z.wzorzec.empty() && z.walutaSklepu == "PLN";
    CHECK(zWzorcem);

    SprawdzaczCen spr(sk);
    std::vector<WynikSprawdzenia> wyniki;
    bool koniec = false, anul = true;
    CHECK(spr.Start(zadania, [&](int n, int r, const WynikSprawdzenia& w) { CHECK(n == static_cast<int>(wyniki.size()) + 1 && r == 4); wyniki.push_back(w); },
                    [&](bool a2) { koniec = true; anul = a2; }, 5));
    CHECK(spr.wToku() && !spr.Start(zadania, nullptr, nullptr));   // drugi start w trakcie odrzucony
    CHECK(czekaj([&] { return koniec; }));
    CHECK(!anul && !spr.wToku() && wyniki.size() == 4);
    auto wynik = [&](long long id) -> const WynikSprawdzenia* { for (const auto& w : wyniki) if (w.zadanie.idLinku == id) return &w; return nullptr; };
    CHECK(wynik(l1) && wynik(l1)->zapisano && wynik(l1)->cena == 3.25 && wynik(l1)->waluta == "PLN");
    CHECK(wynik(l2) && wynik(l2)->zapisano && wynik(l2)->cena == 7.5 && wynik(l2)->waluta == "PLN");
    CHECK(wynik(l3) && !wynik(l3)->zapisano && wynik(l3)->komunikat.find("Nie znaleziono ceny") == 0);
    CHECK(wynik(l4) && !wynik(l4)->zapisano && wynik(l4)->komunikat.find("404") != std::string::npos);

    auto k1 = sk.pobierzLink(l1);
    CHECK(k1 && k1->cena && *k1->cena == 3.25 && k1->status == "Dostępny" && !k1->ostatniaWeryfikacja.empty());
    auto k3 = sk.pobierzLink(l3);
    CHECK(k3 && !k3->cena);
    std::vector<WpisHistoriiCeny> h;
    CHECK(sk.pobierzHistorie(l1, h) && h.size() == 1 && h[0].zrodlo == "strona" && h[0].cena == 3.25);

    // druga kontrola tej samej ceny nie dubluje historii; zmiana ceny dopisuje wpis
    srv.strony["/produkt"].tresc = std::string(STRONA_PRODUKTU).replace(std::string(STRONA_PRODUKTU).find("3,25"), 4, "2,99");
    wyniki.clear(); koniec = false;
    CHECK(spr.Start({zadania[0]}, [&](int, int, const WynikSprawdzenia& w) { wyniki.push_back(w); }, [&](bool) { koniec = true; }, 0));
    CHECK(czekaj([&] { return koniec; }));
    CHECK(sk.pobierzHistorie(l1, h) && h.size() == 2 && h[0].cena == 2.99);

    // anulowanie po pierwszej stronie
    wyniki.clear(); koniec = false; anul = false;
    SprawdzaczCen* sp = &spr;
    CHECK(spr.Start(zadania, [&](int, int, const WynikSprawdzenia& w) { wyniki.push_back(w); sp->Anuluj(); }, [&](bool a2) { koniec = true; anul = a2; }, 5));
    CHECK(czekaj([&] { return koniec; }));
    CHECK(anul && wyniki.size() == 1 && !spr.wToku());

    // przerwa między zapytaniami jest zachowywana
    const int przed = srv.zapytan;
    wyniki.clear(); koniec = false;
    const auto t0 = std::chrono::steady_clock::now();
    CHECK(spr.Start({zadania[0], zadania[0], zadania[0]}, [&](int, int, const WynikSprawdzenia& w) { wyniki.push_back(w); }, [&](bool) { koniec = true; }, 300));
    CHECK(czekaj([&] { return koniec; }));
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    CHECK(wyniki.size() == 3 && srv.zapytan - przed == 3 && ms >= 590);

    CHECK(!spr.Start({}, nullptr, nullptr));   // pusta lista

    sprzatanie();
}

class TestSiec : public wxAppConsole {
public:
    int OnRun() override {
        wxAppTraits* tr = GetTraits();
        petla = tr->CreateEventLoop();
        wxEventLoopActivator aktywator(petla);

        namespace fs = std::filesystem;
        const fs::path tmp = fs::temp_directory_path() / ("dm_siec_" + std::to_string(getpid()));
        fs::create_directories(tmp);

        Serwer srv;
        if (!srv.start()) { std::cerr << "Nie można uruchomić serwera testowego\n"; return 2; }
        testKlienta(srv, tmp);
        testDatasheetow(srv, tmp);
        testCen(srv);
        srv.stop();
        fs::remove_all(tmp);
        delete petla;
        std::cout << sprawdzenia << " sprawdzeń, " << bledy << " błędów\n";
        return bledy == 0 ? 0 : 1;
    }
};

wxIMPLEMENT_APP_CONSOLE(TestSiec);
