#ifndef ELEMENT_H
#define ELEMENT_H

#include <optional>
#include <string>

#include "../Narzedzia/Miejsce.h"

/// Element magazynu (wiersz tabeli tbl_elementy)
struct Element {
    long long id = 0;                       ///< 0 = jeszcze nie zapisany
    std::optional<long long> idKategorii;
    std::string symbol;                     ///< wymagany
    std::string symbolProducenta;
    std::string producent;
    std::string opis;
    std::string wartosc;
    std::string jednostka;
    std::string obudowa;
    std::string datasheetUrl;
    std::string zdjecieSciezka;
    int iloscDostepna = 0;
    int iloscMinimalna = 0;
    std::optional<double> cena;             ///< cena jednostkowa
    std::string waluta = "PLN";
    std::string tagi;
    std::string tolerancja;                 ///< np. "1%"
    std::string parametry;                  ///< pozostałe parametry techniczne, np. "0,1 W; 50 V; X7R"
    std::string kontener;                   ///< miejsce magazynowania: kontener
    std::string szuflada;                   ///< np. "A1"
    std::string przegroda;                  ///< np. "1" (opcjonalnie)
    bool aktywny = true;

    std::string kategoriaPelna;             ///< tylko do odczytu: nazwa kategorii z drzewa

    MiejsceMagazynowe miejsce() const { return {kontener, szuflada, przegroda}; }
    void ustawMiejsce(const MiejsceMagazynowe& m) { kontener = m.kontener; szuflada = m.szuflada; przegroda = m.przegroda; }
    std::string miejsceTekst() const { return miejsce().naTekst(); }

    bool niskiStan() const { return iloscDostepna < iloscMinimalna; }
};

#endif // ELEMENT_H
