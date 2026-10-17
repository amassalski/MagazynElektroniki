#ifndef KODYELEMENTOW_H
#define KODYELEMENTOW_H

#include <optional>
#include <string>
#include <vector>

/// Dekodowanie oznaczeń elementów: paski kolorów (rezystory, cewki) i kody SMD. Bez zależności od GUI i bazy.
namespace KodyElementow {

enum class Kolor { Czarny, Brazowy, Czerwony, Pomaranczowy, Zolty, Zielony, Niebieski, Fioletowy, Szary, Bialy,
                   Zloty, Srebrny, Brak };

std::string nazwa(Kolor k);                       ///< "brązowy"
std::string nazwaAngielska(Kolor k);              ///< "brown"
unsigned kolorRGB(Kolor k);                       ///< 0xRRGGBB do rysowania

/// Do czego służy pasek na danej pozycji (inaczej wygląda i ma inny zestaw dozwolonych kolorów)
enum class Rola { Cyfra, Mnoznik, Tolerancja, Tempco };
/// Rola paska nr @p i (od 0) na rezystorze z @p n paskami (3..6)
Rola rolaPaska(int i, int n);
/// Kolory, które mają sens na pasku o danej roli (np. dla tolerancji bez czarnego, żółtego, pomarańczowego i białego).
/// Dla tolerancji ostatni jest Kolor::Brak = brak paska = 20 %.
std::vector<Kolor> dozwoloneKolory(Rola r);
/// Współczynnik temperaturowy w ppm/K dla koloru paska (-1, gdy kolor go nie oznacza)
int wspTemperaturowy(Kolor k);
/// Krótki opis znaczenia koloru w roli: "×100", "±5 %", "100 ppm/K", cyfra
std::string znaczenieKoloru(Kolor k, Rola r);

struct WynikPaskow {
    double wartosc = 0;                           ///< Ω (rezystor) albo µH (cewka)
    double tolerancja = 20;                       ///< w procentach
    int tempco = -1;                              ///< ppm/K, -1 = brak paska
};

/// 3, 4, 5 lub 6 pasków od strony pierwszej cyfry. nullopt przy niedozwolonej kombinacji (@p blad opisuje powód).
std::optional<WynikPaskow> odczytajPaski(const std::vector<Kolor>& paski, std::string* blad = nullptr);

/// Paski dla wartości: 3 paski (2 cyfry, tolerancja 20 % bez paska), 4 (2 cyfry), 5 (3 cyfry) albo 6 (3 cyfry
/// i współczynnik temperaturowy @p tempco w ppm/K, domyślnie 100). Tolerancja 20 % w 4 i 5 paskach to brak paska
/// tolerancji (Kolor::Brak). Pusty wynik, gdy wartości nie da się zapisać na tylu paskach (np. 4,75 k na 4)
/// albo tolerancja / tempco nie ma koloru.
std::vector<Kolor> paskiDlaWartosci(double wartosc, double tolerancjaProc, int liczbaPaskow, int tempco = 100);

/// Kod SMD rezystora: 472, 1002, 4R7, R22, 01A (EIA-96), 000. Wynik w Ω.
std::optional<double> rezystorZKodu(const std::string& kod);
/// Kod SMD kondensatora w pF: 104 = 100 nF, 4R7 = 4,7 pF, 225 = 2,2 µF. Wynik w faradach.
std::optional<double> kondensatorZKodu(const std::string& kod);
/// Kod EIA-96 dla wartości z szeregu E96 ("01A" dla 100 Ω); pusty, gdy wartości nie ma w szeregu
std::string kodEIA96(double omy);

} // namespace KodyElementow

#endif // KODYELEMENTOW_H
