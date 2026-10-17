#ifndef KONFIGURACJA_H
#define KONFIGURACJA_H

#include <string>
#include <utility>
#include <vector>
#include "../BazaDanych/UstawieniaBazowe.h"

/**
 * @class Konfiguracja
 * @brief Prosty plik konfiguracyjny w formacie INI (UTF-8), bez zależności od wx.
 *
 * Format:
 *   # komentarz
 *   [sekcja]
 *   klucz=wartość
 *
 * Klucze zapisane przed pierwszą sekcją trafiają do sekcji "" (tak wyglądał
 * stary, płaski config.ini). Nieznane klucze są zachowywane przy zapisie, więc
 * nowe moduły programu mogą dopisywać własne sekcje bez zmian w tej klasie.
 *
 * Wartości są przycinane z białych znaków na początku i końcu, więc nie mogą
 * zaczynać się ani kończyć spacją. Ścieżki i teksty są w UTF-8.
 */
class Konfiguracja {
public:
    static Konfiguracja& pobierz();

    /// Ustaw ścieżkę pliku (UTF-8). Nie wczytuje pliku.
    void ustawSciezke(const std::string& sciezka);
    /// Katalog na zdjęcia i datasheety, gdy użytkownik nie ustawił własnych ścieżek (instalacja systemowa, np. pakiet DEB:
    /// katalog programu jest tylko do odczytu, a "./dokumenty" zależałoby od katalogu, z którego uruchomiono program).
    /// Pusty = zachowanie domyślne (ścieżki względne).
    static void ustawKatalogDanych(const std::string& katalog);
    const std::string& sciezka() const { return plik; }

    /// Zwraca pierwszą istniejącą ścieżkę z listy (UTF-8) lub "".
    static std::string znajdzPierwszyIstniejacy(const std::vector<std::string>& kandydaci);

    /// Wczytaj plik. Brak pliku to nie błąd (zwraca true i czyści dane).
    bool wczytaj();
    /// Zapisz plik (przez plik tymczasowy, więc awaria nie niszczy starego).
    bool zapisz();

    bool czyIstnieje(const std::string& sekcja, const std::string& klucz) const;
    std::string pobierz(const std::string& sekcja, const std::string& klucz,
                        const std::string& domyslna = "") const;
    int pobierzInt(const std::string& sekcja, const std::string& klucz, int domyslna) const;
    bool pobierzBool(const std::string& sekcja, const std::string& klucz, bool domyslna) const;

    void ustaw(const std::string& sekcja, const std::string& klucz, const std::string& wartosc);
    void ustawInt(const std::string& sekcja, const std::string& klucz, int wartosc);
    void ustawBool(const std::string& sekcja, const std::string& klucz, bool wartosc);
    void usun(const std::string& sekcja, const std::string& klucz);
    /// Usuwa całą sekcję
    void usunSekcje(const std::string& sekcja);
    /// Klucze i wartości sekcji, w kolejności z pliku
    std::vector<std::pair<std::string, std::string>> pobierzSekcje(const std::string& sekcja) const;

    /// Ustawienia aplikacji (ścieżki, EAN, czcionka, progi kolorów)
    UstawieniaBazowe pobierzUstawieniaAplikacji() const;
    void ustawUstawieniaAplikacji(const UstawieniaBazowe& us);

    const std::string& ostatniBlad() const { return blad; }

private:
    Konfiguracja() = default;

    struct Sekcja {
        std::string nazwa;
        std::vector<std::pair<std::string, std::string>> wpisy;
    };

    std::string plik = "config.ini";
    std::vector<Sekcja> sekcje;
    std::string blad;

    Sekcja* znajdzSekcje(const std::string& nazwa);
    const Sekcja* znajdzSekcje(const std::string& nazwa) const;
};

#endif // KONFIGURACJA_H
