#ifndef KLIENTHTTP_H
#define KLIENTHTTP_H

#include <wx/string.h>

#include <functional>
#include <string>

/// Wynik pobrania strony albo pliku
struct WynikHTTP {
    bool ok = false;           ///< odpowiedź 2xx i dane w całości
    int kod = 0;               ///< kod HTTP (0 = brak odpowiedzi)
    std::string tresc;         ///< dla stron: treść w UTF-8
    std::string adres;         ///< adres po przekierowaniach
    std::string typ;           ///< Content-Type
    std::string blad;          ///< opis po polsku
};

/**
 * @class KlientHTTP
 * @brief Asynchroniczne pobieranie stron i plików (wxWebRequest). Wymaga działającej pętli zdarzeń;
 *        wynik trafia do funkcji zwrotnej w wątku okna. Tylko http i https.
 */
class KlientHTTP {
public:
    using Po = std::function<void(const WynikHTTP&)>;

    static constexpr size_t MAKS_STRONA = 4u * 1024 * 1024;
    static constexpr size_t MAKS_PLIK = 50u * 1024 * 1024;

    /// Nazwa programu wysyłana serwerom
    static std::string agent();

    /// Pobiera stronę; treść jest zamieniana na UTF-8 wg nagłówka lub <meta charset>
    static void pobierzStrone(const std::string& url, Po po, int limitCzasuMs = 20000);
    /// Pobiera plik; zapisuje najpierw do "<sciezka>.part", po udanym pobraniu zmienia nazwę
    /// (gdy `wymaganyPoczatek` niepusty, plik musi się nim zaczynać, np. "%PDF")
    static void pobierzPlik(const std::string& url, const wxString& sciezka, Po po, int limitCzasuMs = 120000,
                            const std::string& wymaganyPoczatek = "");

    /// Zamiana treści strony na UTF-8 (UTF-8, ISO-8859-2, windows-1250 itd.); bez zmian, gdy już UTF-8
    static std::string naUTF8(const std::string& tresc, const std::string& typ);
    static bool adresDozwolony(const std::string& url);
};

#endif // KLIENTHTTP_H
