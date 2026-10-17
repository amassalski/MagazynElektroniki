#ifndef LOGIKASKLEPOW_H
#define LOGIKASKLEPOW_H

#include "../BazaDanych/ManagerBazyDanych.h"
#include "../Model/LinkSklepu.h"
#include "../Model/Sklep.h"
#include "LogikaMagazynu.h"

#include <map>
#include <string>
#include <vector>

/// Wynik wczytania listy sklepów z tekstu
struct WynikImportuSklepow {
    int dodano = 0;
    int zaktualizowano = 0;
    std::vector<std::string> bledy;   ///< "wiersz 3: ..."
};

/**
 * @class LogikaSklepow
 * @brief Sklepy, linki elementów do sklepów, ceny i ich historia, kursy walut.
 */
class LogikaSklepow {
public:
    explicit LogikaSklepow(ManagerBazyDanych& manager);

    std::string pobierzBlad() const { return blad_; }

    // ---- sklepy ----
    bool pobierzSklepy(bool tylkoAktywne, std::vector<Sklep>& wynik);
    std::optional<Sklep> pobierzSklep(long long id);
    std::optional<long long> zapiszSklep(const Sklep& sklep);
    /// Sklep bez linków jest kasowany, z linkami tylko wyłączany
    WynikUsuniecia usunSklep(long long id);

    /**
     * Wczytuje listę sklepów z tekstu: jeden sklep w wierszu, pola oddzielone średnikiem lub tabulatorem:
     * nazwa; adres strony; szablon wyszukiwania; waluta; koszt dostawy; darmowa dostawa od.
     * Wymagana tylko nazwa; sklep o tej samej nazwie jest aktualizowany. Wiersz zaczynający się od # jest pomijany.
     */
    bool importujSklepy(const std::string& tekst, WynikImportuSklepow& wynik);
    /// Wiersz pola jak wyżej (do testów i podglądu)
    static bool parsujWierszSklepu(const std::string& wiersz, Sklep& sklep, std::string& blad);
    std::string eksportujSklepy();

    // ---- linki ----
    bool pobierzLinki(long long idElementu, std::vector<LinkSklepu>& wynik);
    std::optional<LinkSklepu> pobierzLink(long long idLinku);
    /// Dodaje (id == 0) albo zmienia link; jeśli podano cenę, wpada ona do historii. Zwraca id.
    std::optional<long long> zapiszLink(const LinkSklepu& link);
    bool usunLink(long long idLinku);
    /// Zapisuje cenę ze sprawdzenia (aktualizuje link i dopisuje do historii, gdy zmieniła się lub minął dzień)
    bool ustawCene(long long idLinku, double cena, const std::string& waluta, const std::string& status,
                   const std::string& zrodlo);
    bool pobierzHistorie(long long idLinku, std::vector<WpisHistoriiCeny>& wynik);
    /// Adres wyszukiwania frazy w sklepie ("" gdy sklep nie ma szablonu)
    static std::string adresWyszukiwania(const Sklep& sklep, const std::string& fraza);
    static std::string kodujURL(const std::string& tekst);

    // ---- kursy ----
    bool pobierzKursy(std::map<std::string, double>& wynik);
    bool zapiszKurs(const std::string& waluta, double kursPLN);

    /// Linki wszystkich aktywnych sklepów dla podanych elementów (do zestawienia)
    /// (tylkoZCena = false: także linki bez ceny, np. do ich sprawdzenia)
    bool pobierzLinkiElementow(const std::vector<long long>& idElementow, std::vector<LinkSklepu>& wynik,
                               bool tylkoZCena = true);

private:
    ManagerBazyDanych& manager_;
    std::string blad_;
    PolaczenieBazy* polaczenie();
    bool ustawBladBazy(PolaczenieBazy* pol, const std::string& zaZnaczeniem = "");
    bool ustawBlad(const std::string& t) { blad_ = t; return false; }
};

#endif // LOGIKASKLEPOW_H
