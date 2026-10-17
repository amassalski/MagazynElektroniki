#ifndef LINKSKLEPU_H
#define LINKSKLEPU_H

#include <optional>
#include <string>

/// Strona elementu w sklepie (wiersz tbl_elementy_sklepy)
struct LinkSklepu {
    long long id = 0;
    long long idElementu = 0;
    long long idSklepu = 0;
    std::string nazwaSklepu;                ///< tylko do odczytu
    std::string url;
    std::string kodWSklepie;
    std::optional<double> cena;             ///< cena jednostkowa w walucie sklepu
    std::string waluta;
    std::string ostatniaWeryfikacja;        ///< tylko do odczytu
    std::string status;                     ///< dostępność, tekst ze sklepu
    int minimalnaIlosc = 1;
    bool domyslny = false;
    std::string uwagi;
};

/// Jedna zapisana cena z historii
struct WpisHistoriiCeny {
    std::string data;
    double cena = 0;
    std::string waluta;
    std::string zrodlo;                     ///< "reczna", "strona"...
};

#endif // LINKSKLEPU_H
