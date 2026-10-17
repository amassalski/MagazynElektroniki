#ifndef PROJEKT_H
#define PROJEKT_H

#include <string>

/// Projekt elektroniczny (wiersz tabeli tbl_projekty)
struct Projekt {
    long long id = 0;            ///< 0 = jeszcze nie zapisany
    std::string nazwa;           ///< wymagana
    std::string opis;
    std::string status = "W_TRAKCIE";  ///< PLANOWANY, W_TRAKCIE, WSTRZYMANY, ZAKONCZONY
    bool aktywny = true;
    std::string dataUtworzenia;  ///< tylko do odczytu

    // tylko do odczytu (podsumowanie listy części)
    int liczbaPozycji = 0;
    int liczbaBrakow = 0;        ///< pozycje, których brakuje w magazynie
};

/// Dozwolone statusy projektu (kolejność z listy w oknie)
inline const char* const STATUSY_PROJEKTU[] = {"PLANOWANY", "W_TRAKCIE", "WSTRZYMANY", "ZAKONCZONY"};

#endif // PROJEKT_H
