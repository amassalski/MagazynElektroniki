#include "ManagerBazyDanych.h"
#include "../Narzedzia/Konfiguracja.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace {

fs::path P(const std::string& s) { return fs::u8path(s); }

const char* const SEKCJA = "baza";

// Pliki skryptów w kolejności wykonywania
const char* const SKRYPTY[] = {"schema.sql", "funkcje_i_trigger.sql", "widoki.sql",
                               "dane_startowe.sql", "pomoc_pola.sql"};

} // namespace

ManagerBazyDanych::ManagerBazyDanych()
    : polaczenie_(std::make_unique<PolaczenieBazy>()) {}

ManagerBazyDanych::~ManagerBazyDanych() = default;

ManagerBazyDanych& ManagerBazyDanych::pobierz() {
    static ManagerBazyDanych instancja;
    return instancja;
}

void ManagerBazyDanych::ustawBlad(const std::string& blad) {
    ostatniBlad_ = blad;
}

std::string ManagerBazyDanych::pobierzBlad() const {
    return ostatniBlad_;
}

// ---------------------------------------------------------------------------
// Ustawienia
// ---------------------------------------------------------------------------

void ManagerBazyDanych::ustawSciezkeKonfiguracji(const std::string& sciezka) {
    Konfiguracja::pobierz().ustawSciezke(sciezka);
}

std::string ManagerBazyDanych::sciezkaKonfiguracji() const {
    return Konfiguracja::pobierz().sciezka();
}

bool ManagerBazyDanych::wczytajUstawienia() {
    Konfiguracja& kfg = Konfiguracja::pobierz();
    if (!kfg.wczytaj()) {
        ustawBlad(kfg.ostatniBlad());
        return false;
    }

    // Stary config.ini nie miał sekcji; klucze z "" są traktowane jako wartości zapasowe
    const UstawieniaBazy domyslne;
    auto tekst = [&](const char* klucz, const std::string& domyslna) {
        return kfg.pobierz(SEKCJA, klucz, kfg.pobierz("", klucz, domyslna));
    };

    UstawieniaBazy us;
    us.host = tekst("host", domyslne.host);
    us.bazaDanych = tekst("baza", domyslne.bazaDanych);
    us.uzytkownik = tekst("uzytkownik", domyslne.uzytkownik);
    us.haslo = tekst("haslo", domyslne.haslo);

    int port = kfg.pobierzInt(SEKCJA, "port", kfg.pobierzInt("", "port", domyslne.port));
    us.port = (port >= 1 && port <= 65535) ? port : domyslne.port;

    us.zapamietajHaslo = kfg.pobierzBool(SEKCJA, "zapamietaj_haslo", domyslne.zapamietajHaslo);
    int timeout = kfg.pobierzInt(SEKCJA, "timeout", domyslne.timeoutSek);
    us.timeoutSek = (timeout >= 1 && timeout <= 120) ? timeout : domyslne.timeoutSek;

    ustawienia_ = us;
    ustawBlad("");
    return true;
}

bool ManagerBazyDanych::zapiszUstawienia() {
    Konfiguracja& kfg = Konfiguracja::pobierz();

    kfg.ustaw(SEKCJA, "host", ustawienia_.host);
    kfg.ustawInt(SEKCJA, "port", ustawienia_.port);
    kfg.ustaw(SEKCJA, "baza", ustawienia_.bazaDanych);
    kfg.ustaw(SEKCJA, "uzytkownik", ustawienia_.uzytkownik);
    kfg.ustawBool(SEKCJA, "zapamietaj_haslo", ustawienia_.zapamietajHaslo);
    kfg.ustawInt(SEKCJA, "timeout", ustawienia_.timeoutSek);
    if (ustawienia_.zapamietajHaslo) {
        kfg.ustaw(SEKCJA, "haslo", ustawienia_.haslo);
    } else {
        kfg.usun(SEKCJA, "haslo"); // nie zostawiaj starego hasła w pliku
    }

    // Stare, płaskie klucze są już zbędne
    for (const char* k : {"host", "port", "baza", "uzytkownik", "haslo"}) kfg.usun("", k);

    if (!kfg.zapisz()) {
        ustawBlad(kfg.ostatniBlad());
        return false;
    }
    ustawBlad("");
    return true;
}

void ManagerBazyDanych::ustawUstawienia(const UstawieniaBazy& ustawienia) {
    ustawienia_ = ustawienia;
}

const UstawieniaBazy& ManagerBazyDanych::pobierzUstawienia() const {
    return ustawienia_;
}

// ---------------------------------------------------------------------------
// Połączenie
// ---------------------------------------------------------------------------

bool ManagerBazyDanych::polacz() {
    laczenie_ = true;
    const bool ok = polaczenie_->polacz(ustawienia_.host, ustawienia_.port, ustawienia_.bazaDanych,
                                        ustawienia_.uzytkownik, ustawienia_.haslo,
                                        ustawienia_.timeoutSek);
    ustawBlad(ok ? "" : polaczenie_->pobierzBlad());
    laczenie_ = false;
    return ok;
}

void ManagerBazyDanych::rozlacz() {
    polaczenie_->rozlacz();
}

bool ManagerBazyDanych::czyPolaczony() const {
    if (laczenie_.load()) return false;
    return polaczenie_ && polaczenie_->czyPolaczenieDobre();
}

bool ManagerBazyDanych::zapewnijPolaczenie() {
    if (czyPolaczony()) return true;
    return polacz();
}

bool ManagerBazyDanych::testPolaczenie() {
    if (!zapewnijPolaczenie()) return false;
    if (!polaczenie_->zapytanie("SELECT 1")) {
        ustawBlad(polaczenie_->pobierzBlad());
        return false;
    }
    ustawBlad("");
    return true;
}

bool ManagerBazyDanych::testujUstawienia(const UstawieniaBazy& ustawienia, std::string& blad,
                                         std::string* wersjaSerwera) {
    PolaczenieBazy proba;
    if (!proba.polacz(ustawienia.host, ustawienia.port, ustawienia.bazaDanych,
                      ustawienia.uzytkownik, ustawienia.haslo, ustawienia.timeoutSek)) {
        blad = proba.pobierzBlad();
        return false;
    }
    if (!proba.zapytanie("SELECT 1")) {
        blad = proba.pobierzBlad();
        return false;
    }
    if (wersjaSerwera) *wersjaSerwera = proba.wersjaSerweraTekst();
    blad.clear();
    return true;
}

PolaczenieBazy* ManagerBazyDanych::pobierzPolaczenie() {
    return polaczenie_.get();
}

// ---------------------------------------------------------------------------
// Zapytania
// ---------------------------------------------------------------------------

std::optional<WynikSQL> ManagerBazyDanych::zapytanie(const std::string& sql,
                                                     const std::vector<Parametr>& parametry) {
    if (!zapewnijPolaczenie()) return std::nullopt;
    auto wynik = polaczenie_->zapytanie(sql, parametry);
    ustawBlad(wynik ? "" : polaczenie_->pobierzBlad());
    return wynik;
}

bool ManagerBazyDanych::wykonaj(const std::string& sql, const std::vector<Parametr>& parametry) {
    return zapytanie(sql, parametry).has_value();
}

// ---------------------------------------------------------------------------
// Struktura bazy
// ---------------------------------------------------------------------------

bool ManagerBazyDanych::czyTabelaIstnieje(const std::string& nazwaTabeli) {
    auto wynik = zapytanie(
        "SELECT EXISTS (SELECT 1 FROM information_schema.tables "
        "WHERE table_schema = 'public' AND table_name = $1)",
        {nazwaTabeli});
    return wynik && wynik->logiczna(0, 0);
}

bool ManagerBazyDanych::czyBazaZainicjalizowana() {
    return czyTabelaIstnieje("tbl_wersja_schematu") && czyTabelaIstnieje("tbl_kategorie") &&
           czyTabelaIstnieje("tbl_elementy");
}

int ManagerBazyDanych::wersjaSchematu() {
    if (!czyTabelaIstnieje("tbl_wersja_schematu")) return 0;
    auto wynik = zapytanie("SELECT COALESCE(max(wersja), 0) FROM tbl_wersja_schematu");
    return wynik ? static_cast<int>(wynik->liczba(0, 0)) : 0;
}

InformacjeOBazie ManagerBazyDanych::pobierzInformacje() {
    InformacjeOBazie info;
    if (!czyPolaczony()) return info;

    info.polaczono = true;
    info.wersjaSerwera = polaczenie_->wersjaSerweraTekst();
    info.baza = polaczenie_->nazwaBazy();
    info.uzytkownik = polaczenie_->uzytkownik();
    info.host = polaczenie_->host();
    info.port = polaczenie_->port();
    info.zainicjalizowana = czyBazaZainicjalizowana();
    info.wersjaSchematu = info.zainicjalizowana ? wersjaSchematu() : 0;
    return info;
}

void ManagerBazyDanych::ustawKatalogiSQL(const std::vector<std::string>& katalogi) {
    katalogiSQL_ = katalogi;
}

std::string ManagerBazyDanych::znajdzKatalogSQL() const {
    std::vector<std::string> kandydaci = katalogiSQL_;
    kandydaci.push_back("sql");
    kandydaci.push_back("../sql");

    for (const auto& k : kandydaci) {
        std::error_code ec;
        if (fs::is_regular_file(P(k) / "schema.sql", ec)) return k;
    }
    return "";
}

bool ManagerBazyDanych::wczytajPlik(const std::string& sciezka, std::string& zawartosc) {
    std::ifstream plik(P(sciezka), std::ios::binary);
    if (!plik.is_open()) return false;

    std::stringstream bufor;
    bufor << plik.rdbuf();
    zawartosc = bufor.str();

    if (zawartosc.compare(0, 3, "\xEF\xBB\xBF") == 0) zawartosc.erase(0, 3); // BOM
    return true;
}

bool ManagerBazyDanych::wykonajSkryptSQL(const std::string& sciezkaPliku) {
    std::string sql;
    if (!wczytajPlik(sciezkaPliku, sql)) {
        ustawBlad("Nie można otworzyć pliku " + sciezkaPliku);
        return false;
    }
    if (sql.find_first_not_of(" \t\r\n") == std::string::npos) {
        ustawBlad("Plik " + sciezkaPliku + " jest pusty");
        return false;
    }
    if (!zapewnijPolaczenie()) return false;

    if (!polaczenie_->wykonajSkrypt(sql)) {
        ustawBlad(polaczenie_->pobierzBlad());
        return false;
    }
    ustawBlad("");
    return true;
}

bool ManagerBazyDanych::inicjalizujBaze(std::string* raport) {
    std::ostringstream log;
    auto zakoncz = [&](bool ok) {
        if (raport) *raport = log.str();
        return ok;
    };

    if (!zapewnijPolaczenie()) return zakoncz(false);

    const std::string katalog = znajdzKatalogSQL();
    if (katalog.empty()) {
        std::string szukano;
        for (const auto& k : katalogiSQL_) szukano += "\n  " + k;
        ustawBlad("Nie znaleziono katalogu ze skryptami SQL (plik schema.sql)." + szukano);
        return zakoncz(false);
    }

    for (const char* nazwa : SKRYPTY) {
        const std::string sciezka = (P(katalog) / nazwa).u8string();
        if (!wykonajSkryptSQL(sciezka)) {
            const std::string powod = ostatniBlad_;
            ustawBlad(std::string(nazwa) + ": " + powod);
            log << nazwa << ": BŁĄD\n";
            return zakoncz(false);
        }
        log << nazwa << ": OK\n";
    }

    const int wersja = wersjaSchematu();
    log << "Wersja struktury bazy: " << wersja << "\n";
    ustawBlad("");
    return zakoncz(true);
}
