#ifndef BOM_H
#define BOM_H

#include <string>

/// Pozycja listy części projektu (tbl_bom) razem z danymi elementu z magazynu
struct PozycjaProjektu {
    long long id = 0;             ///< 0 = jeszcze nie zapisana
    long long idProjektu = 0;
    long long idElementu = 0;
    std::string oznaczenia;       ///< np. "R1, R2, R5"
    int wymagana = 1;
    int zamowiona = 0;
    int zuzyta = 0;               ///< ile już wydano z magazynu do projektu
    int priorytet = 5;
    std::string uwagi;

    // tylko do odczytu: dane elementu
    std::string symbol;
    std::string producent;
    std::string symbolProducenta; ///< MPN
    std::string miejsce;          ///< np. "Kontener 1, A1/1"
    std::string wartosc;
    std::string obudowa;
    std::string opis;
    int dostepna = 0;             ///< aktualny stan magazynu

    /// Ile jeszcze trzeba wydać do projektu
    int doWydania() const { return wymagana > zuzyta ? wymagana - zuzyta : 0; }
    /// Ile trzeba dokupić (z uwzględnieniem tego, co jest w magazynie)
    int brakuje() const {
        const int d = doWydania() - dostepna;
        return d > 0 ? d : 0;
    }
};

#endif // BOM_H
