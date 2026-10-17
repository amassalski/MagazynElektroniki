#ifndef KALKULATORY_H
#define KALKULATORY_H

#include "SzeregiE.h"

#include <optional>
#include <string>

/// Proste kalkulatory elektroniczne (bez GUI i bazy): dzielnik napięcia, rezystor do diody LED, filtr RC.
namespace Kalkulatory {

/// Najbliższa wartość z szeregu (w skali logarytmicznej); 0 dla wartości <= 0
double najblizsza(double wartosc, Szereg s);
/// Najbliższa NIE MNIEJSZA wartość z szeregu (np. rezystor ograniczający prąd LED nie może być za mały)
double najblizszaWieksza(double wartosc, Szereg s);

// --- dzielnik napięcia: Uwy = Uwe * R2 / (R1 + R2)
std::optional<double> napiecieDzielnika(double uwe, double r1, double r2);

struct WynikDzielnika {
    double r1 = 0, r2 = 0;      ///< Ω
    double uwy = 0;             ///< V przy tych rezystorach
    double bladProc = 0;        ///< (uwy - cel) / cel * 100
    double pradMA = 0;          ///< prąd płynący przez dzielnik bez obciążenia
};
/// Dobiera parę rezystorów z szeregu dającą napięcie zbliżone do @p uwy. R1 przebiega przez zakres
/// [rMin, rMax] (domyślnie 1 k..1 M), więc prąd dzielnika da się ograniczyć. nullopt, gdy uwy >= uwe lub dane są błędne.
std::optional<WynikDzielnika> dobierzDzielnik(double uwe, double uwy, Szereg s, double rMin = 1e3, double rMax = 1e6);

// --- rezystor szeregowy do diody LED
struct WynikLED {
    double rIdealny = 0;        ///< Ω: (Uzas - Uf) / I
    double rZSzeregu = 0;       ///< Ω: najbliższa większa wartość z szeregu
    double pradRzeczywistyMA = 0;
    double mocRezystoraMW = 0;  ///< moc wydzielana na rezystorze z szeregu
    double mocLEDMW = 0;        ///< Uf * I
};
/// @p uzas V, @p uf V (napięcie przewodzenia), @p pradMA mA. nullopt, gdy uzas <= uf lub prąd <= 0.
std::optional<WynikLED> rezystorLED(double uzas, double uf, double pradMA, Szereg s = Szereg::E24);

// --- filtr RC pierwszego rzędu
double czestotliwoscGranicznaRC(double rOm, double cF);   ///< Hz: 1 / (2 pi R C)
double stalaCzasowaRC(double rOm, double cF);             ///< s: R * C
double rDlaFiltru(double fHz, double cF);                 ///< Ω
double cDlaFiltru(double fHz, double rOm);                ///< F

/// Częstotliwość do opisów: "1,59 kHz", "340 Hz", "2,2 MHz"
std::string formatCzestotliwosci(double hz);
/// Czas do opisów: "4,7 ms", "100 µs", "1,2 s"
std::string formatCzasu(double s);

} // namespace Kalkulatory

#endif // KALKULATORY_H
