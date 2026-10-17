#ifndef POLACZENIEBAZY_H
#define POLACZENIEBAZY_H

#include <libpq-fe.h>

#include <optional>
#include <string>
#include <vector>

/// Parametr zapytania ($1, $2, ...). std::nullopt oznacza NULL.
using Parametr = std::optional<std::string>;

/**
 * @class WynikSQL
 * @brief Wynik zapytania (RAII nad PGresult). Tylko do przenoszenia.
 *
 * Wartości liczone są od zera: wiersz 0..liczbaWierszy()-1, kolumna 0..liczbaKolumn()-1.
 */
class WynikSQL {
public:
    WynikSQL() = default;
    explicit WynikSQL(PGresult* wynik) : wynik_(wynik) {}
    ~WynikSQL();

    WynikSQL(const WynikSQL&) = delete;
    WynikSQL& operator=(const WynikSQL&) = delete;
    WynikSQL(WynikSQL&& inny) noexcept;
    WynikSQL& operator=(WynikSQL&& inny) noexcept;

    int liczbaWierszy() const;
    int liczbaKolumn() const;
    std::string nazwaKolumny(int kolumna) const;
    /// Indeks kolumny o danej nazwie albo -1
    int indeksKolumny(const std::string& nazwa) const;

    bool czyNull(int wiersz, int kolumna) const;
    /// Wartość jako tekst UTF-8 (NULL daje pusty tekst)
    std::string tekst(int wiersz, int kolumna) const;
    std::string tekst(int wiersz, const std::string& kolumna) const;
    long long liczba(int wiersz, int kolumna, long long domyslna = 0) const;
    double rzeczywista(int wiersz, int kolumna, double domyslna = 0.0) const;
    bool logiczna(int wiersz, int kolumna, bool domyslna = false) const;

    /// Liczba wierszy zmienionych przez INSERT/UPDATE/DELETE
    long long wierszeZmienione() const;

private:
    PGresult* wynik_ = nullptr;
    bool poprawnaKomorka(int wiersz, int kolumna) const;
};

/**
 * @class PolaczenieBazy
 * @brief Cienka nakładka na libpq: połączenie, zapytania z parametrami, transakcje.
 *
 * Obiekt nie jest bezpieczny wątkowo: jednego połączenia w danej chwili używa
 * jeden wątek.
 */
class PolaczenieBazy {
public:
    PolaczenieBazy();
    ~PolaczenieBazy();

    PolaczenieBazy(const PolaczenieBazy&) = delete;
    PolaczenieBazy& operator=(const PolaczenieBazy&) = delete;

    /**
     * @brief Nawiąż połączenie. Wartości mogą zawierać spacje, apostrofy itp.
     * @param timeoutSek maksymalny czas oczekiwania na serwer
     */
    bool polacz(const std::string& host, int port, const std::string& baza,
                const std::string& uzytkownik, const std::string& haslo,
                int timeoutSek = 10);

    void rozlacz();

    /// Czy obiekt ma otwarte połączenie (nie sprawdza stanu sieci)
    bool czyPolaczony() const;
    /// Czy połączenie jest sprawne (PQstatus)
    bool czyPolaczenieDobre() const;

    /**
     * @brief Zapytanie z parametrami ($1, $2...). Zwraca wynik albo std::nullopt
     *        (wtedy komunikat jest w pobierzBlad()).
     * Gdy połączenie zostało zerwane, próbuje je raz odnowić.
     */
    std::optional<WynikSQL> zapytanie(const std::string& sql,
                                      const std::vector<Parametr>& parametry = {});

    /// Polecenie bez wyniku (INSERT/UPDATE/DELETE/DDL)
    bool wykonaj(const std::string& sql, const std::vector<Parametr>& parametry = {});

    /// Skrypt z wieloma poleceniami (bez parametrów); wykonywany jako jedna całość
    bool wykonajSkrypt(const std::string& sql);

    bool rozpocznijTransakcje();
    bool zatwierdzTransakcje();
    bool wycofajTransakcje();
    bool wTransakcji() const { return wTransakcji_; }

    std::string pobierzBlad() const { return blad_; }

    // Informacje o połączeniu
    int wersjaSerwera() const;          ///< np. 160015 dla 16.15, 90624 dla 9.6.24
    std::string wersjaSerweraTekst() const; ///< np. "16.15"
    std::string nazwaBazy() const;
    std::string uzytkownik() const;
    std::string host() const;
    int port() const;

private:
    PGconn* polaczenie_ = nullptr;
    bool wTransakcji_ = false;
    std::string blad_;

    // dane do odnowienia połączenia
    std::string host_, baza_, uzytkownik_, haslo_;
    int port_ = 5432;
    int timeout_ = 10;

    bool zapewnijPolaczenie();
    void ustawBlad(const std::string& blad);
    void ustawBladZLibpq();
    bool wykonajProste(const char* sql);
};

/**
 * @class Transakcja
 * @brief RAII: BEGIN w konstruktorze, ROLLBACK w destruktorze, o ile nie było zatwierdzenia.
 */
class Transakcja {
public:
    explicit Transakcja(PolaczenieBazy& polaczenie);
    ~Transakcja();
    Transakcja(const Transakcja&) = delete;
    Transakcja& operator=(const Transakcja&) = delete;

    bool czyAktywna() const { return aktywna_; }
    bool zatwierdz();

private:
    PolaczenieBazy& polaczenie_;
    bool aktywna_ = false;
};

#endif // POLACZENIEBAZY_H
