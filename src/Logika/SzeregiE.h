#ifndef SZEREGIE_H
#define SZEREGIE_H

#include "PozycjaImportu.h"

#include <optional>
#include <string>
#include <vector>

/// Szeregi wartości znormalizowanych IEC 60063
enum class Szereg { E6, E12, E24, E48, E96 };

enum class RodzajElementu { Rezystor, Kondensator, Cewka };

namespace SzeregiE {

std::string nazwa(Szereg s);                              ///< "E24"
std::optional<Szereg> zNazwy(const std::string& nazwa);   ///< "e24" -> E24
/// Tolerancja zwykle przypisywana szeregowi: E6 20%, E12 10%, E24 5%, E48 2%, E96 1%
std::string domyslnaTolerancja(Szereg s);

/// Mantysy szeregu w jednej dekadzie: E6/E12/E24 jako dwie cyfry (10..91), E48/E96 jako trzy (100..976)
std::vector<int> mantysy(Szereg s);
/// Wszystkie wartości szeregu z przedziału [od, do] (włącznie), rosnąco, bez szumu zmiennoprzecinkowego
std::vector<double> wartosci(Szereg s, double od, double doo);

std::string jednostka(RodzajElementu r);                  ///< "Ω", "F", "H"
std::string nazwaRodzaju(RodzajElementu r);               ///< "Rezystor", "Kondensator", "Cewka"
/// Zapis kodem RKM: 4K7, 47R, 100K, 2M2 (rezystory); 4n7, 100n, 1u, 10p (kondensatory, cewki)
std::string formatRKM(double wartosc, RodzajElementu r);
/// Wartość do pola "Wartość": "4,7k", "47", "100n", "4,7u"
std::string formatWartosci(double wartosc, RodzajElementu r);
/// Zapis do pola "Wartość" elementu: litera przedrostka zamiast przecinka (4k7, 2M2, 4n7, 0R47; bez przecinka)
std::string formatPolaWartosci(double wartosc, RodzajElementu r);
/// Wartość z jednostką do opisów: "4,7 kΩ", "100 nF", "4,7 µH"
std::string formatZJednostka(double wartosc, RodzajElementu r);
/// Rozpoznaje zapis wartości: "4k7", "4,7k", "100n", "2M2", "4R7", "10 kΩ", "100nF", "1u". nullopt, gdy nie rozumie.
std::optional<double> parsujWartosc(const std::string& tekst, RodzajElementu r);

/// Kategoria z kategorie_startowe.txt pasująca do rodzaju i obudowy (SMD / THT)
std::string kategoriaDomyslna(RodzajElementu r, const std::string& obudowa);
/// "A1" + 2 -> "A3"; tekst bez numeru na końcu zostaje bez zmian
std::string kolejnaSzuflada(const std::string& baza, int przesuniecie);

/// Domyślny wzór symbolu dla rodzaju
std::string domyslnyWzorSymbolu(RodzajElementu r);
/// Podstawia {typ} {rkm} {wartosc} {obudowa} {tol} {moc} {napiecie} {dielektryk}; puste człony znikają razem z myślnikiem
std::string zbudujSymbol(const std::string& wzor, RodzajElementu r, double wartosc, const std::string& obudowa,
                         const std::string& tol, const std::string& moc, const std::string& napiecie,
                         const std::string& dielektryk);

struct ParametrySzeregu {
    RodzajElementu rodzaj = RodzajElementu::Rezystor;
    Szereg szereg = Szereg::E24;
    double od = 10, doo = 1e6;               ///< zakres wartości w jednostkach podstawowych (Ω, F, H)
    std::string tolerancja;                  ///< pusta = domyślna dla szeregu
    std::string obudowa;
    std::string moc;                         ///< np. "0,1 W" (rezystory)
    std::string napiecie;                    ///< np. "50 V" (kondensatory)
    std::string dielektryk;                  ///< np. "X7R" (kondensatory)
    std::string producent;
    std::string tagi;                        ///< dopisywane do tagu z nazwą szeregu
    std::string wzorSymbolu;                 ///< pusty = domyślny
    std::string kategoria;                   ///< pusta = domyślna dla rodzaju i obudowy
    int ilosc = 0;
    int minimum = 0;
    std::optional<double> cena;
    std::string waluta = "PLN";
    MiejsceMagazynowe miejsce;
    bool szufladaNaDekade = false;           ///< każda dekada wartości do kolejnej szuflady (A1, A2, ...)
};

/// Tworzy pozycje całego szeregu w zakresie. Przy błędnych parametrach zwraca pustą listę i uzupełnia @p blad.
std::vector<PozycjaImportu> generujSzereg(const ParametrySzeregu& p, std::string* blad = nullptr);

} // namespace SzeregiE

#endif // SZEREGIE_H
