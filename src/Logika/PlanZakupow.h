#ifndef PLANZAKUPOW_H
#define PLANZAKUPOW_H

#include "../Model/LinkSklepu.h"
#include "../Model/Raport.h"
#include "../Model/Sklep.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

/// Co trzeba kupić
struct PotrzebaZakupu {
    long long idElementu = 0;
    std::string symbol;
    int ilosc = 0;
};

/// Jedna pozycja planu: ten element, w tym sklepie
struct PozycjaPlanu {
    long long idElementu = 0;
    std::string symbol;
    long long idSklepu = 0;
    std::string nazwaSklepu;
    int iloscPotrzebna = 0;
    int iloscKupowana = 0;      ///< może być większa (minimalna ilość w sklepie)
    double cenaPLN = 0;         ///< za sztukę
    double wartoscPLN = 0;
    std::string url;
};

/// Podsumowanie jednego sklepu w planie
struct KoszykSklepu {
    long long idSklepu = 0;
    std::string nazwaSklepu;
    int liczbaPozycji = 0;
    double towarPLN = 0;
    double dostawaPLN = 0;
    double razemPLN() const { return towarPLN + dostawaPLN; }
};

struct PlanZakupow {
    std::vector<PozycjaPlanu> pozycje;
    std::vector<KoszykSklepu> koszyki;
    std::vector<PotrzebaZakupu> bezOferty;   ///< elementy, których nie ma w żadnym sklepie z ceną
    std::vector<std::string> ostrzezenia;    ///< np. brak kursu waluty
    double towarPLN = 0;
    double dostawaPLN = 0;
    double razemPLN() const { return towarPLN + dostawaPLN; }
    bool kompletny() const { return bezOferty.empty(); }
};

enum class TrybPlanu {
    Najtanszy,      ///< najniższa suma razem z dostawą (dzieli zakupy między sklepy, gdy się opłaca)
    NajtanszeCeny,  ///< każda część tam, gdzie jest najtańsza; dostawa nie ma znaczenia
    JedenSklep      ///< wszystko w jednym sklepie (wybierany najtańszy, który ma całość)
};

namespace PlanZakupowLogika {

/// Dostawa dla koszyka o wartości `towarPLN` w tym sklepie (w złotych)
double kosztDostawyPLN(const Sklep& sklep, double towarPLN, const std::map<std::string, double>& kursy);

/**
 * Układa listę zakupów. Do planu trafiają tylko oferty z ceną w aktywnych sklepach i znanym kursem waluty.
 * Gdy nie ma żadnego sklepu mającego całość, tryb JedenSklep zwraca plan pusty z wszystkim w `bezOferty`.
 */
PlanZakupow zaplanuj(const std::vector<PotrzebaZakupu>& potrzeby, const std::vector<LinkSklepu>& oferty,
                     const std::vector<Sklep>& sklepy, const std::map<std::string, double>& kursy, TrybPlanu tryb);

/// Raport do wyświetlenia i zapisu (wiersze: sklep, symbol, ilości, cena, wartość)
Raport naRaport(const PlanZakupow& plan, const std::string& tytul);

/// Zwykła lista do wklejenia do zamówienia: "ilość x symbol" w sekcjach sklepów
std::string naListeZamowien(const PlanZakupow& plan);

} // namespace PlanZakupowLogika

#endif // PLANZAKUPOW_H
