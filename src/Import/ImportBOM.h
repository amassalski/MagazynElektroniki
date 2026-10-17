#ifndef IMPORTBOM_H
#define IMPORTBOM_H

#include <string>
#include <vector>

/// Jedna (zgrupowana) pozycja listy części wczytana z pliku projektu
struct PozycjaBOM {
    std::string oznaczenia;   ///< np. "R1, R2, R5"
    std::string wartosc;      ///< np. "10k", "100nF"
    std::string obudowa;      ///< np. "Resistor_SMD:R_0805_2012Metric" albo "R0805"
    std::string symbol;       ///< numer części producenta (MPN), jeśli plik go zawiera
    std::string producent;
    std::string opis;
    int ilosc = 1;
};

struct WynikImportuBOM {
    std::string format;                    ///< "KiCad (CSV)", "Eagle (schemat)" itp.
    std::vector<PozycjaBOM> pozycje;       ///< pozycje zgrupowane (te same części razem)
    std::vector<std::string> ostrzezenia;  ///< np. pominięte wiersze

    int liczbaSztuk() const;
};

/**
 * Wczytywanie list części (BOM) z KiCada i Eagle'a:
 *  - CSV (KiCad, Eagle, dowolny edytor z nagłówkami typu Reference/Value/Footprint/Qty),
 *  - KiCad: lista sieciowa XML (netlist, .xml),
 *  - Eagle: schemat (.sch, XML).
 * Format jest rozpoznawany po zawartości.
 */
namespace ImportBOM {

/// Ścieżka w UTF-8
bool wczytajPlik(const std::string& sciezka, WynikImportuBOM& wynik, std::string& blad);
bool parsujTekst(const std::string& tekst, WynikImportuBOM& wynik, std::string& blad);

/// CSV z nagłówkami (rozpoznaje kolumny po nazwach)
bool parsujCSV(const std::string& tekst, WynikImportuBOM& wynik, std::string& blad);

/// Łączy pozycje o tej samej wartości, obudowie, symbolu i producencie (sumuje ilości)
std::vector<PozycjaBOM> zgrupuj(const std::vector<PozycjaBOM>& surowe,
                                std::vector<std::string>* ostrzezenia = nullptr);

/// Porządek "naturalny": R2 < R10
bool porownajNaturalnie(const std::string& a, const std::string& b);
/// Rozbija "R1, R2, R5-R7" na pojedyncze oznaczenia
std::vector<std::string> rozbijOznaczenia(const std::string& tekst);

/// Postać porównywalna: "10 kΩ" -> "10k", "4k7" -> "4.7k", "100nF" -> "100n"
std::string normalizujWartosc(const std::string& wartosc);
/// Postać porównywalna: "Resistor_SMD:R_0805_2012Metric" -> "0805", "SOT-23" -> "sot23"
std::string normalizujObudowe(const std::string& obudowa);

} // namespace ImportBOM

#endif // IMPORTBOM_H
