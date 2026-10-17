#ifndef LOGIKAKATALOGU_H
#define LOGIKAKATALOGU_H

#include "../BazaDanych/ManagerBazyDanych.h"
#include "../Model/Katalog.h"
#include "KatalogCSV.h"
#include "LogikaMagazynu.h"

#include <optional>
#include <string>
#include <vector>

struct FiltrKatalogu {
    std::string tekst;            ///< szuka w symbolu, producencie, opisie, rodzinie i obudowie
    std::string rodzina;          ///< pusta = wszystkie
    bool tylkoWMagazynie = false; ///< tylko pozycje, które mają odpowiednik w magazynie
    int limit = 2000;
};

enum class TrybKatalogu {
    Pomin,        ///< istniejące pozycje (symbol + producent) zostają bez zmian
    Aktualizuj    ///< uzupełnij puste pola i dopisz parametry (istniejące parametry nadpisane wartością z pliku)
};

struct WynikImportuKatalogu {
    int dodano = 0;
    int zaktualizowano = 0;
    int pominieto = 0;
    std::vector<std::string> bledy;   ///< "wiersz 5 (LM358): powód"
};

/**
 * @class LogikaKatalogu
 * @brief Katalog elementów (wzorce z parametrami) powiązany z magazynem po symbolu.
 */
class LogikaKatalogu {
public:
    explicit LogikaKatalogu(ManagerBazyDanych& manager) : manager_(manager) {}

    std::string pobierzBlad() const { return blad_; }

    /// Lista pozycji bez parametrów (z liczbą dopasowanych elementów i sumą stanów)
    bool pobierz(const FiltrKatalogu& filtr, std::vector<PozycjaKatalogu>& wynik);
    std::optional<PozycjaKatalogu> pobierzPozycje(long long id);
    /// Pozycja o symbolu (bez rozróżniania wielkości liter); przy pustym producencie pierwsza pasująca
    std::optional<PozycjaKatalogu> znajdz(const std::string& symbol, const std::string& producent = "");
    /// Pozycja katalogu pasująca do elementu magazynu (po symbolu albo symbolu producenta)
    std::optional<PozycjaKatalogu> dlaElementu(const Element& element);
    std::vector<std::string> rodziny();

    /// Dodaje albo zapisuje pozycję razem z parametrami (parametry są zastępowane podanymi)
    std::optional<long long> zapisz(const PozycjaKatalogu& pozycja);
    bool usun(long long id);
    /// Dopisuje parametry; @p nadpisz decyduje, czy istniejące o tej samej nazwie dostają nową wartość
    bool dodajParametry(long long id, const std::vector<ParametrKatalogu>& parametry, bool nadpisz);
    bool ustawDatasheetPlik(long long id, const std::string& sciezka);

    bool importuj(const std::vector<KatalogCSV::Pozycja>& pozycje, TrybKatalogu tryb, WynikImportuKatalogu& wynik);

    /// Element magazynu zbudowany z pozycji katalogu (bez zapisu)
    static Element elementZKatalogu(const PozycjaKatalogu& pozycja);
    /// Dodaje do magazynu nowy element z katalogu. Gdy symbol już jest w magazynie, zwraca nullopt (z opisem błędu).
    std::optional<long long> dodajDoMagazynu(LogikaMagazynu& magazyn, long long idKatalogu, int ilosc,
                                             const MiejsceMagazynowe& miejsce, std::optional<long long> idKategorii);

private:
    ManagerBazyDanych& manager_;
    std::string blad_;
    PolaczenieBazy* polaczenie();
    bool ustawBladBazy(PolaczenieBazy* pol, const std::string& zaZnaczeniem = "");
    bool wczytajParametry(PolaczenieBazy* pol, PozycjaKatalogu& pozycja);
};

#endif // LOGIKAKATALOGU_H
