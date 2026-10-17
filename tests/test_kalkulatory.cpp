#include "Logika/Kalkulatory.h"
#include <cmath>
#include <iostream>

static int bledy = 0, sprawdzenia = 0;
#define CHECK(w) do { ++sprawdzenia; if (!(w)) { ++bledy; std::cerr << "BŁĄD " << __FILE__ << ":" << __LINE__ << "  " #w "\n"; } } while (0)
static bool bl(double a, double b, double tol = 1e-6) { return std::fabs(a - b) <= tol * std::fabs(b) + 1e-12; }

using namespace Kalkulatory;

int main() {
    // szeregi
    CHECK(bl(najblizsza(4800, Szereg::E24), 4700) || bl(najblizsza(4800, Szereg::E24), 5100));
    CHECK(bl(najblizsza(4700, Szereg::E24), 4700));
    CHECK(bl(najblizszaWieksza(4800, Szereg::E24), 5100));
    CHECK(bl(najblizszaWieksza(470, Szereg::E12), 470));
    CHECK(bl(najblizszaWieksza(0.5, Szereg::E12), 0.56));
    CHECK(najblizsza(-1, Szereg::E12) == 0);

    // dzielnik
    auto u = napiecieDzielnika(12, 10000, 10000);
    CHECK(u && bl(*u, 6));
    CHECK(!napiecieDzielnika(12, 1000, 0));
    auto d = dobierzDzielnik(5, 3.3, Szereg::E24);
    CHECK(d && std::fabs(d->bladProc) < 1.0);
    if (d) CHECK(bl(d->uwy, 5 * d->r2 / (d->r1 + d->r2)) && d->r1 >= 1e3 && d->r1 <= 1e6);
    auto d96 = dobierzDzielnik(5, 3.3, Szereg::E96);
    CHECK(d96 && std::fabs(d96->bladProc) < 0.5);
    if (d) CHECK(d->r1 + d->r2 >= 5e3 && d->r1 + d->r2 <= 100e3);   // rozsądny prąd, nie megaomy
    CHECK(!dobierzDzielnik(5, 6, Szereg::E24));
    CHECK(!dobierzDzielnik(5, 0, Szereg::E24));

    // LED: 5 V, 2 V, 20 mA -> 150 Ω
    auto l = rezystorLED(5, 2, 20);
    CHECK(l && bl(l->rIdealny, 150) && bl(l->rZSzeregu, 150) && bl(l->pradRzeczywistyMA, 20));
    if (l) CHECK(bl(l->mocRezystoraMW, 60) && bl(l->mocLEDMW, 40));
    auto l2 = rezystorLED(5, 2.1, 20);                      // 145 Ω -> 150 Ω
    CHECK(l2 && bl(l2->rZSzeregu, 150) && l2->pradRzeczywistyMA < 20);
    CHECK(!rezystorLED(2, 2, 20) && !rezystorLED(3, 2, 0));

    // RC: 10 k i 100 nF -> 159,15 Hz, tau 1 ms
    CHECK(bl(czestotliwoscGranicznaRC(10e3, 100e-9), 159.1549, 1e-4));
    CHECK(bl(stalaCzasowaRC(10e3, 100e-9), 1e-3));
    CHECK(bl(rDlaFiltru(159.1549, 100e-9), 10e3, 1e-4));
    CHECK(bl(cDlaFiltru(159.1549, 10e3), 100e-9, 1e-4));
    CHECK(czestotliwoscGranicznaRC(0, 1e-9) == 0);

    CHECK(formatCzestotliwosci(159.15) == "159 Hz");
    CHECK(formatCzestotliwosci(1591.5) == "1,59 kHz");
    CHECK(formatCzestotliwosci(15915) == "15,9 kHz");
    CHECK(formatCzestotliwosci(2.2e6) == "2,2 MHz");
    CHECK(formatCzasu(0.001) == "1 ms");
    CHECK(formatCzasu(4.7e-6) == "4,7 \xC2\xB5s");
    CHECK(formatCzasu(1.2) == "1,2 s");

    std::cerr << sprawdzenia << " sprawdzeń, " << bledy << " błędów\n";
    return bledy ? 1 : 0;
}
