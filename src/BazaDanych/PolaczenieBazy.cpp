#include "PolaczenieBazy.h"

#include <locale>
#include <sstream>

#include <cstdlib>
#include <utility>

namespace {

// Komunikaty informacyjne serwera (np. "relation already exists, skipping")
// nie powinny trafiać na stderr.
void ignorujKomunikaty(void*, const char*) {}

std::string przytnijKoniec(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
    return s;
}

} // namespace

// ---------------------------------------------------------------------------
// WynikSQL
// ---------------------------------------------------------------------------

WynikSQL::~WynikSQL() {
    if (wynik_) PQclear(wynik_);
}

WynikSQL::WynikSQL(WynikSQL&& inny) noexcept : wynik_(std::exchange(inny.wynik_, nullptr)) {}

WynikSQL& WynikSQL::operator=(WynikSQL&& inny) noexcept {
    if (this != &inny) {
        if (wynik_) PQclear(wynik_);
        wynik_ = std::exchange(inny.wynik_, nullptr);
    }
    return *this;
}

int WynikSQL::liczbaWierszy() const { return wynik_ ? PQntuples(wynik_) : 0; }
int WynikSQL::liczbaKolumn() const { return wynik_ ? PQnfields(wynik_) : 0; }

std::string WynikSQL::nazwaKolumny(int kolumna) const {
    if (!wynik_ || kolumna < 0 || kolumna >= PQnfields(wynik_)) return "";
    return PQfname(wynik_, kolumna);
}

int WynikSQL::indeksKolumny(const std::string& nazwa) const {
    return wynik_ ? PQfnumber(wynik_, nazwa.c_str()) : -1;
}

bool WynikSQL::poprawnaKomorka(int wiersz, int kolumna) const {
    return wynik_ && wiersz >= 0 && wiersz < PQntuples(wynik_) &&
           kolumna >= 0 && kolumna < PQnfields(wynik_);
}

bool WynikSQL::czyNull(int wiersz, int kolumna) const {
    return !poprawnaKomorka(wiersz, kolumna) || PQgetisnull(wynik_, wiersz, kolumna) != 0;
}

std::string WynikSQL::tekst(int wiersz, int kolumna) const {
    if (czyNull(wiersz, kolumna)) return "";
    return std::string(PQgetvalue(wynik_, wiersz, kolumna),
                       static_cast<size_t>(PQgetlength(wynik_, wiersz, kolumna)));
}

std::string WynikSQL::tekst(int wiersz, const std::string& kolumna) const {
    return tekst(wiersz, indeksKolumny(kolumna));
}

long long WynikSQL::liczba(int wiersz, int kolumna, long long domyslna) const {
    if (czyNull(wiersz, kolumna)) return domyslna;
    const std::string t = tekst(wiersz, kolumna);
    char* koniec = nullptr;
    const long long v = std::strtoll(t.c_str(), &koniec, 10);
    return (koniec == t.c_str() || *koniec != '\0') ? domyslna : v;
}

double WynikSQL::rzeczywista(int wiersz, int kolumna, double domyslna) const {
    if (czyNull(wiersz, kolumna)) return domyslna;
    // Nie strtod(): po ustawieniu polskich ustawień regionalnych (wxLocale) oczekuje
    // przecinka dziesiętnego, a PostgreSQL zwraca kropkę.
    const std::string t = tekst(wiersz, kolumna);
    std::istringstream strumien(t);
    strumien.imbue(std::locale::classic());
    double v = 0.0;
    strumien >> v;
    if (strumien.fail() || !strumien.eof()) return domyslna;
    return v;
}

bool WynikSQL::logiczna(int wiersz, int kolumna, bool domyslna) const {
    if (czyNull(wiersz, kolumna)) return domyslna;
    const std::string t = tekst(wiersz, kolumna);
    return t == "t" || t == "true" || t == "1";
}

long long WynikSQL::wierszeZmienione() const {
    if (!wynik_) return 0;
    return std::strtoll(PQcmdTuples(wynik_), nullptr, 10);
}

// ---------------------------------------------------------------------------
// PolaczenieBazy
// ---------------------------------------------------------------------------

PolaczenieBazy::PolaczenieBazy() = default;

PolaczenieBazy::~PolaczenieBazy() {
    rozlacz();
}

void PolaczenieBazy::ustawBlad(const std::string& blad) {
    blad_ = blad;
}

void PolaczenieBazy::ustawBladZLibpq() {
    blad_ = polaczenie_ ? przytnijKoniec(PQerrorMessage(polaczenie_)) : "Brak połączenia z bazą danych";
}

bool PolaczenieBazy::polacz(const std::string& host, int port, const std::string& baza,
                            const std::string& uzytkownik, const std::string& haslo,
                            int timeoutSek) {
    rozlacz();

    host_ = host;
    port_ = port;
    baza_ = baza;
    uzytkownik_ = uzytkownik;
    haslo_ = haslo;
    timeout_ = timeoutSek;

    const std::string portTekst = std::to_string(port);
    const std::string timeoutTekst = std::to_string(timeoutSek);

    // PQconnectdbParams nie wymaga zamieniania spacji i apostrofów w wartościach
    const char* klucze[] = {"host", "port", "dbname", "user", "password",
                            "connect_timeout", "client_encoding", nullptr};
    const char* wartosci[] = {host.c_str(), portTekst.c_str(), baza.c_str(), uzytkownik.c_str(),
                              haslo.c_str(), timeoutTekst.c_str(), "UTF8", nullptr};

    polaczenie_ = PQconnectdbParams(klucze, wartosci, 0);
    if (polaczenie_ == nullptr) {
        ustawBlad("Brak pamięci na połączenie z bazą danych");
        return false;
    }

    if (PQstatus(polaczenie_) != CONNECTION_OK) {
        ustawBladZLibpq();
        PQfinish(polaczenie_);
        polaczenie_ = nullptr;
        return false;
    }

    PQsetNoticeProcessor(polaczenie_, ignorujKomunikaty, nullptr);
    wTransakcji_ = false;
    ustawBlad("");
    return true;
}

void PolaczenieBazy::rozlacz() {
    if (polaczenie_ != nullptr) {
        PQfinish(polaczenie_); // serwer sam wycofa otwartą transakcję
        polaczenie_ = nullptr;
    }
    wTransakcji_ = false;
}

bool PolaczenieBazy::czyPolaczony() const {
    return polaczenie_ != nullptr;
}

bool PolaczenieBazy::czyPolaczenieDobre() const {
    return polaczenie_ != nullptr && PQstatus(polaczenie_) == CONNECTION_OK;
}

bool PolaczenieBazy::zapewnijPolaczenie() {
    if (polaczenie_ == nullptr) {
        ustawBlad("Brak połączenia z bazą danych");
        return false;
    }
    if (PQstatus(polaczenie_) == CONNECTION_OK) return true;

    // Połączenie zerwane (restart serwera, problem sieci). Poza transakcją
    // próbujemy je odnowić; w transakcji jej stan i tak by przepadł.
    if (wTransakcji_) {
        wTransakcji_ = false;
        ustawBlad("Połączenie z bazą zostało zerwane w trakcie transakcji");
        return false;
    }
    PQreset(polaczenie_);
    if (PQstatus(polaczenie_) != CONNECTION_OK) {
        ustawBladZLibpq();
        return false;
    }
    PQsetNoticeProcessor(polaczenie_, ignorujKomunikaty, nullptr);
    return true;
}

std::optional<WynikSQL> PolaczenieBazy::zapytanie(const std::string& sql,
                                                  const std::vector<Parametr>& parametry) {
    if (!zapewnijPolaczenie()) return std::nullopt;

    std::vector<const char*> wartosci;
    wartosci.reserve(parametry.size());
    for (const auto& p : parametry) {
        wartosci.push_back(p ? p->c_str() : nullptr);
    }

    PGresult* wynik = PQexecParams(polaczenie_, sql.c_str(), static_cast<int>(parametry.size()),
                                   nullptr, wartosci.empty() ? nullptr : wartosci.data(),
                                   nullptr, nullptr, 0);
    if (wynik == nullptr) {
        ustawBladZLibpq();
        return std::nullopt;
    }

    const ExecStatusType status = PQresultStatus(wynik);
    if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK && status != PGRES_EMPTY_QUERY) {
        blad_ = przytnijKoniec(PQresultErrorMessage(wynik));
        PQclear(wynik);
        return std::nullopt;
    }

    ustawBlad("");
    return WynikSQL(wynik);
}

bool PolaczenieBazy::wykonaj(const std::string& sql, const std::vector<Parametr>& parametry) {
    return zapytanie(sql, parametry).has_value();
}

bool PolaczenieBazy::wykonajSkrypt(const std::string& sql) {
    if (!zapewnijPolaczenie()) return false;

    PGresult* wynik = PQexec(polaczenie_, sql.c_str());
    if (wynik == nullptr) {
        ustawBladZLibpq();
        return false;
    }
    const ExecStatusType status = PQresultStatus(wynik);
    const bool ok = status == PGRES_COMMAND_OK || status == PGRES_TUPLES_OK ||
                    status == PGRES_EMPTY_QUERY;
    if (!ok) {
        blad_ = przytnijKoniec(PQresultErrorMessage(wynik));
    } else {
        ustawBlad("");
    }
    PQclear(wynik);
    return ok;
}

bool PolaczenieBazy::wykonajProste(const char* sql) {
    if (!zapewnijPolaczenie()) return false;
    PGresult* wynik = PQexec(polaczenie_, sql);
    const bool ok = wynik != nullptr && PQresultStatus(wynik) == PGRES_COMMAND_OK;
    if (!ok) {
        blad_ = wynik ? przytnijKoniec(PQresultErrorMessage(wynik)) : "Błąd połączenia z bazą";
    } else {
        ustawBlad("");
    }
    if (wynik) PQclear(wynik);
    return ok;
}

bool PolaczenieBazy::rozpocznijTransakcje() {
    if (wTransakcji_) {
        ustawBlad("Transakcja jest już rozpoczęta");
        return false;
    }
    if (!wykonajProste("BEGIN")) return false;
    wTransakcji_ = true;
    return true;
}

bool PolaczenieBazy::zatwierdzTransakcje() {
    if (!wTransakcji_) {
        ustawBlad("Brak aktywnej transakcji");
        return false;
    }
    // COMMIT zamknie transakcję także wtedy, gdy była w stanie błędu (wtedy robi ROLLBACK)
    const bool ok = wykonajProste("COMMIT");
    wTransakcji_ = false;
    return ok;
}

bool PolaczenieBazy::wycofajTransakcje() {
    if (!wTransakcji_) return true;
    const bool ok = wykonajProste("ROLLBACK");
    wTransakcji_ = false;
    return ok;
}

int PolaczenieBazy::wersjaSerwera() const {
    return polaczenie_ ? PQserverVersion(polaczenie_) : 0;
}

std::string PolaczenieBazy::wersjaSerweraTekst() const {
    const int v = wersjaSerwera();
    if (v == 0) return "";
    if (v >= 100000) return std::to_string(v / 10000) + "." + std::to_string(v % 10000);
    return std::to_string(v / 10000) + "." + std::to_string((v / 100) % 100) + "." +
           std::to_string(v % 100);
}

std::string PolaczenieBazy::nazwaBazy() const {
    const char* v = polaczenie_ ? PQdb(polaczenie_) : nullptr;
    return v ? v : "";
}

std::string PolaczenieBazy::uzytkownik() const {
    const char* v = polaczenie_ ? PQuser(polaczenie_) : nullptr;
    return v ? v : "";
}

std::string PolaczenieBazy::host() const {
    const char* v = polaczenie_ ? PQhost(polaczenie_) : nullptr;
    return v ? v : "";
}

int PolaczenieBazy::port() const {
    const char* v = polaczenie_ ? PQport(polaczenie_) : nullptr;
    return v ? std::atoi(v) : 0;
}

// ---------------------------------------------------------------------------
// Transakcja
// ---------------------------------------------------------------------------

Transakcja::Transakcja(PolaczenieBazy& polaczenie) : polaczenie_(polaczenie) {
    aktywna_ = polaczenie_.rozpocznijTransakcje();
}

Transakcja::~Transakcja() {
    if (aktywna_) polaczenie_.wycofajTransakcje();
}

bool Transakcja::zatwierdz() {
    if (!aktywna_) return false;
    aktywna_ = false;
    return polaczenie_.zatwierdzTransakcje();
}
