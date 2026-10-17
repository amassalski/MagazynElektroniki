#ifndef KODKRESKOWY_H
#define KODKRESKOWY_H

#include <optional>
#include <string>
#include <vector>

/**
 * Kody kreskowe bez zewnętrznych bibliotek: EAN-13, EAN-8 i Code 128 (zestaw B).
 * "Moduły" to najwęższe paski: true = kreska, false = przerwa (bez marginesów).
 *
 * Kod elementu w magazynie:
 *  - EAN-13: 2 cyfry prefiksu (z ustawień, np. 20-29 to zakres do użytku wewnętrznego)
 *            + 10 cyfr numeru elementu + cyfra kontrolna,
 *  - EAN-8:  7 cyfr numeru elementu + cyfra kontrolna,
 *  - Code128: ten sam 13-cyfrowy tekst co EAN-13, tylko inaczej narysowany.
 */
namespace KodKreskowy {

enum class Typ { EAN13, EAN8, Code128 };

/// "EAN13", "EAN8", "Code128" (z ustawień); nieznany tekst daje EAN13
Typ typZNazwy(const std::string& nazwa);

/// Cyfra kontrolna EAN dla samych cyfr bez niej (12 albo 7 cyfr)
char cyfraKontrolna(const std::string& cyfry);
bool poprawnyEAN(const std::string& kod);  ///< 13 albo 8 cyfr z dobrą sumą kontrolną

/// Tekst kodu dla elementu o danym numerze; pusty, gdy numer nie mieści się w kodzie
std::string tekstKoduElementu(Typ typ, long long idElementu, int prefiks);
/// Numer elementu z zeskanowanego tekstu (13 cyfr z naszym prefiksem albo 8 cyfr); brak = to nie nasz kod
std::optional<long long> idZKodu(const std::string& tekst, int prefiks);

/// Moduły do narysowania (EAN-13: 95, EAN-8: 67). Pusty wynik = błędny tekst.
std::vector<bool> moduly(Typ typ, const std::string& tekst);
std::vector<bool> modulyEAN(const std::string& cyfry);
std::vector<bool> modulyCode128(const std::string& tekst);

/**
 * Odczyt kodu z linii jasności pikseli (0 = czarny, 255 = biały; jedna linia obrazu).
 * Rozpoznaje EAN-13 i EAN-8 (także odwrócone o 180 stopni) niezależnie od skali.
 * Zwraca tekst kodu albo nic.
 */
std::optional<std::string> odczytajLinie(const std::vector<unsigned char>& jasnosc);
/// Odczyt z obrazu w odcieniach szarości (wiersz po wierszu); próbuje wielu linii
std::optional<std::string> odczytajObraz(const std::vector<unsigned char>& szarosc, int szerokosc, int wysokosc);

} // namespace KodKreskowy

#endif // KODKRESKOWY_H
