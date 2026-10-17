#ifndef KATALOG_H
#define KATALOG_H

#include <string>
#include <vector>

/// Parametr elementu katalogowego, np. {"Napięcie zasilania", "2,7 ... 5,5 V", "datasheet"}
struct ParametrKatalogu {
    std::string nazwa;
    std::string wartosc;
    std::string zrodlo = "reczny";   ///< reczny, csv albo datasheet
};

/// Pozycja katalogu elementów (tbl_katalog) z parametrami
struct PozycjaKatalogu {
    long long id = 0;                ///< 0 = jeszcze nie zapisana
    std::string symbol;              ///< np. "LM358", "NE555P"; wymagany
    std::string producent;           ///< razem z symbolem tworzy unikalny klucz
    std::string opis;
    std::string obudowa;
    std::string rodzina;             ///< np. "Wzmacniacze operacyjne"
    std::string datasheetUrl;
    std::string datasheetPlik;       ///< pobrany plik PDF
    std::string uwagi;
    std::vector<ParametrKatalogu> parametry;

    // tylko do odczytu: powiązanie z magazynem (po symbolu lub symbolu producenta elementu)
    int liczbaElementow = 0;
    int wMagazynie = 0;              ///< suma stanów dopasowanych elementów

    /// Parametr o podanej nazwie (bez rozróżniania wielkości liter) albo nullptr
    const ParametrKatalogu* parametr(const std::string& nazwa) const;
};

#endif // KATALOG_H
