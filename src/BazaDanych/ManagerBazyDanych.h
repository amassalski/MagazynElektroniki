#ifndef MANAGERBAZYDANYCH_H
#define MANAGERBAZYDANYCH_H

#include "PolaczenieBazy.h"

#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <vector>

/**
 * @struct UstawieniaBazy
 * @brief Parametry połączenia z PostgreSQL
 */
struct UstawieniaBazy {
    std::string host = "localhost";
    int port = 5432;
    std::string bazaDanych = "domowy_magazyn";
    std::string uzytkownik = "postgres";
    std::string haslo = "";
    /// Czy zapisywać hasło w pliku konfiguracji (jako zwykły tekst)
    bool zapamietajHaslo = true;
    /// Maksymalny czas oczekiwania na serwer przy łączeniu (sekundy)
    int timeoutSek = 5;
};

/**
 * @struct InformacjeOBazie
 * @brief Podsumowanie stanu bazy do wyświetlenia w programie
 */
struct InformacjeOBazie {
    bool polaczono = false;
    std::string wersjaSerwera;
    std::string baza;
    std::string uzytkownik;
    std::string host;
    int port = 0;
    bool zainicjalizowana = false; ///< czy istnieją tabele programu
    int wersjaSchematu = 0;        ///< 0 = brak informacji
};

/**
 * @class ManagerBazyDanych
 * @brief Wysokopoziomowy dostęp do bazy: ustawienia, połączenie, struktura bazy.
 *
 * Singleton. Ustawienia są trzymane w pliku konfiguracji (sekcja [baza]) przez
 * klasę Konfiguracja. Klasa nie zależy od wxWidgets, więc można ją testować
 * bez GUI.
 *
 * Wątki: polacz() można wywołać w osobnym wątku, żeby nie blokować okna. W tym
 * czasie nie wolno używać pozostałych metod (czyPolaczony() i czyLaczenie()
 * są bezpieczne).
 */
class ManagerBazyDanych {
public:
    /// Wersja struktury bazy, której oczekuje ten program
    static constexpr int WYMAGANA_WERSJA_SCHEMATU = 6;

    static ManagerBazyDanych& pobierz();

    ManagerBazyDanych(const ManagerBazyDanych&) = delete;
    ManagerBazyDanych& operator=(const ManagerBazyDanych&) = delete;

    // ============ USTAWIENIA ============

    /// Plik konfiguracji (UTF-8). Domyślnie "config.ini" w bieżącym katalogu.
    void ustawSciezkeKonfiguracji(const std::string& sciezka);
    std::string sciezkaKonfiguracji() const;

    /// Wczytaj ustawienia z pliku. Brak pliku = ustawienia domyślne (nie błąd).
    bool wczytajUstawienia();
    /// Zapisz ustawienia do pliku (zachowuje pozostałe sekcje pliku).
    bool zapiszUstawienia();

    void ustawUstawienia(const UstawieniaBazy& ustawienia);
    const UstawieniaBazy& pobierzUstawienia() const;

    // ============ POŁĄCZENIE ============

    /// Nawiąż połączenie wg bieżących ustawień (zamyka poprzednie)
    bool polacz();
    void rozlacz();
    /// Czy aktualnie trwa łączenie (bezpieczne z każdego wątku)
    bool czyLaczenie() const { return laczenie_.load(); }
    /// Czy połączenie jest aktywne i sprawne
    bool czyPolaczony() const;
    /// Połącz (jeśli trzeba) i sprawdź, że serwer odpowiada
    bool testPolaczenie();
    /// Sprawdź ustawienia na osobnym połączeniu; nie zmienia bieżącego połączenia.
    static bool testujUstawienia(const UstawieniaBazy& ustawienia, std::string& blad,
                                 std::string* wersjaSerwera = nullptr);

    std::string pobierzBlad() const;
    PolaczenieBazy* pobierzPolaczenie();

    // ============ ZAPYTANIA ============

    std::optional<WynikSQL> zapytanie(const std::string& sql,
                                      const std::vector<Parametr>& parametry = {});
    bool wykonaj(const std::string& sql, const std::vector<Parametr>& parametry = {});

    // ============ STRUKTURA BAZY ============

    bool czyTabelaIstnieje(const std::string& nazwaTabeli);
    /// Czy w bazie są tabele programu (kategorie, elementy, wersja schematu)
    bool czyBazaZainicjalizowana();
    /// Wersja struktury zapisana w bazie (0 gdy brak)
    int wersjaSchematu();
    InformacjeOBazie pobierzInformacje();

    /// Katalogi, w których szukane są pliki .sql (pierwszy z plikiem schema.sql wygrywa)
    void ustawKatalogiSQL(const std::vector<std::string>& katalogi);
    /// Znaleziony katalog ze skryptami albo "" (UTF-8)
    std::string znajdzKatalogSQL() const;

    /**
     * @brief Utwórz lub zaktualizuj strukturę bazy: schema, funkcje_i_trigger,
     *        widoki, dane_startowe. Skrypty są idempotentne, więc można to
     *        wywoływać wielokrotnie, także na bazie już używanej.
     * @param raport (opcjonalnie) wynik krok po kroku, do pokazania użytkownikowi
     */
    bool inicjalizujBaze(std::string* raport = nullptr);

    /// Wczytaj plik SQL (UTF-8) i wykonaj w całości
    bool wykonajSkryptSQL(const std::string& sciezkaPliku);

private:
    ManagerBazyDanych();
    ~ManagerBazyDanych();

    std::unique_ptr<PolaczenieBazy> polaczenie_;
    UstawieniaBazy ustawienia_;
    std::string ostatniBlad_;
    std::vector<std::string> katalogiSQL_;
    std::atomic<bool> laczenie_{false};

    void ustawBlad(const std::string& blad);
    bool zapewnijPolaczenie();
    static bool wczytajPlik(const std::string& sciezka, std::string& zawartosc);
};

#endif // MANAGERBAZYDANYCH_H
