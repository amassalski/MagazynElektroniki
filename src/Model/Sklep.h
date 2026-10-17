#ifndef SKLEP_H
#define SKLEP_H

#include <optional>
#include <string>

/// Sklep z częściami (wiersz tabeli tbl_sklepy)
struct Sklep {
    long long id = 0;                       ///< 0 = jeszcze nie zapisany
    std::string nazwa;                      ///< wymagana
    std::string opis;
    std::string urlBazowy;                  ///< strona główna sklepu
    std::string kraj;
    std::string waluta = "PLN";
    bool aktywny = true;
    int priorytet = 5;                      ///< 1 = najchętniej
    std::string szablonWyszukiwania;        ///< adres z {q} w miejscu szukanej frazy
    double kosztDostawy = 0;                ///< w walucie sklepu
    std::optional<double> darmowaDostawaOd; ///< próg darmowej dostawy (waluta sklepu)
    std::string regexCeny;                  ///< własny wzorzec ceny (pierwsza grupa), opcjonalny

    int liczbaLinkow = 0;                   ///< tylko do odczytu
};

/// Kurs waluty do złotego
struct KursWaluty {
    std::string waluta;
    double kursPLN = 1.0;
};

#endif // SKLEP_H
