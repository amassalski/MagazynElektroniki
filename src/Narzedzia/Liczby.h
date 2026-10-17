#ifndef LICZBY_H
#define LICZBY_H

#include <cstring>
#include <locale>
#include <sstream>
#include <string>

/// Liczba -> tekst z kropką dziesiętną, niezależnie od ustawień regionalnych
std::string liczbaNaTekstKlasyczny(double wartosc);

/// Tekst z przecinkiem lub kropką (spacje są ignorowane) -> liczba.
/// Zwraca false dla pustego lub błędnego tekstu.
bool tekstNaLiczbe(const std::string& tekst, double& wartosc);

/// Liczba z przecinkiem dziesiętnym, bez zbędnych zer: (1.5, 4, 2) -> "1,50"; (0.0123, 4, 2) -> "0,0123"
std::string liczbaNaTekstPolski(double wartosc, int maxMiejsc = 4, int minMiejsc = 2);

/// Odpowiednik strtod(), który ZAWSZE rozumie kropkę dziesiętną. Zwykłe strtod()/stod() po ustawieniu polskich
/// ustawień regionalnych (wxLocale) czytają "4.7" jako 4 - stąd ta wersja. Przecinka nie rozpoznaje.
inline double strtodKlasyczny(const char* s, char** koniec = nullptr) {
    std::istringstream is{std::string(s)};
    is.imbue(std::locale::classic());
    double v = 0.0;
    is >> v;
    if (is.fail()) {
        if (koniec) *koniec = const_cast<char*>(s);
        return 0.0;
    }
    if (koniec) *koniec = const_cast<char*>(s) + (is.eof() ? std::strlen(s) : static_cast<size_t>(is.tellg()));
    return v;
}

/// Liczba -> 9 cyfr znaczących (bez szumu typu 4.7000000000000002), niezależnie od ustawień regionalnych
inline double zaokraglij9(double v) {
    std::ostringstream os;
    os.imbue(std::locale::classic());
    os.precision(9);
    os << v;
    return strtodKlasyczny(os.str().c_str());
}

#endif // LICZBY_H
