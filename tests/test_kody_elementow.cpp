// Testy dekodera pasków kolorów i kodów SMD. Bez bazy danych i wxWidgets.
#include "Logika/KodyElementow.h"
#include "Logika/SzeregiE.h"

#include <clocale>
#include <cmath>
#include <iostream>

static int bledy = 0, sprawdzenia = 0;
#define CHECK(w) do { ++sprawdzenia; if (!(w)) { ++bledy; std::cerr << "BŁĄD " << __FILE__ << ":" << __LINE__ << "  " #w "\n"; } } while (0)

using namespace KodyElementow;
using K = Kolor;
static bool bl(double a, double b) { return std::fabs(a - b) <= 1e-9 * std::fabs(b) + 1e-15; }

int main() {
    // 4 paski: 4,7 kΩ 5% = żółty fioletowy czerwony złoty
    auto w = odczytajPaski({K::Zolty, K::Fioletowy, K::Czerwony, K::Zloty});
    CHECK(w && bl(w->wartosc, 4700) && w->tolerancja == 5 && w->tempco == -1);
    // 5 pasków: 100 kΩ 1% = brązowy czarny czarny pomarańczowy brązowy
    w = odczytajPaski({K::Brazowy, K::Czarny, K::Czarny, K::Pomaranczowy, K::Brazowy});
    CHECK(w && bl(w->wartosc, 100000) && w->tolerancja == 1);
    // 6 pasków z tempco: 1 kΩ 1% 50 ppm
    w = odczytajPaski({K::Brazowy, K::Czarny, K::Czarny, K::Brazowy, K::Brazowy, K::Czerwony});
    CHECK(w && bl(w->wartosc, 1000) && w->tempco == 50);
    // 0,22 Ω: czerwony czerwony srebrny złoty; 4,7 Ω: żółty fioletowy złoty złoty
    w = odczytajPaski({K::Czerwony, K::Czerwony, K::Srebrny, K::Zloty});
    CHECK(w && bl(w->wartosc, 0.22));
    w = odczytajPaski({K::Zolty, K::Fioletowy, K::Zloty, K::Zloty});
    CHECK(w && bl(w->wartosc, 4.7));
    // 3 paski = 20%
    w = odczytajPaski({K::Brazowy, K::Czarny, K::Pomaranczowy});
    CHECK(w && bl(w->wartosc, 10000) && w->tolerancja == 20);
    // błędy
    std::string blad;
    CHECK(!odczytajPaski({K::Zloty, K::Czarny, K::Czarny, K::Zloty}, &blad) && !blad.empty());
    CHECK(!odczytajPaski({K::Czarny, K::Czarny}, &blad));
    CHECK(!odczytajPaski({K::Brazowy, K::Czarny, K::Czarny, K::Czarny}, &blad));   // czarny nie jest tolerancją

    // 4 paski bez paska tolerancji (Kolor::Brak) = 20 %
    w = odczytajPaski({K::Zolty, K::Fioletowy, K::Czerwony, K::Brak});
    CHECK(w && bl(w->wartosc, 4700) && w->tolerancja == 20);
    // role pasków i dozwolone kolory
    CHECK(rolaPaska(0, 4) == Rola::Cyfra && rolaPaska(2, 4) == Rola::Mnoznik && rolaPaska(3, 4) == Rola::Tolerancja);
    CHECK(rolaPaska(2, 5) == Rola::Cyfra && rolaPaska(3, 5) == Rola::Mnoznik && rolaPaska(4, 5) == Rola::Tolerancja);
    CHECK(rolaPaska(4, 6) == Rola::Tolerancja && rolaPaska(5, 6) == Rola::Tempco);
    {
        auto t = dozwoloneKolory(Rola::Tolerancja);
        auto jest = [&](Kolor k) { for (Kolor x : t) if (x == k) return true; return false; };
        CHECK(!jest(K::Czarny) && !jest(K::Zolty) && !jest(K::Pomaranczowy) && !jest(K::Bialy));
        CHECK(jest(K::Zloty) && jest(K::Srebrny) && jest(K::Brak) && t.back() == K::Brak);
        CHECK(znaczenieKoloru(K::Zloty, Rola::Tolerancja) == "\xC2\xB1" "5 %");
        CHECK(znaczenieKoloru(K::Czerwony, Rola::Mnoznik) == "\xC3\x97" "100");
        for (Rola r : {Rola::Cyfra, Rola::Mnoznik, Rola::Tolerancja, Rola::Tempco})
            for (Kolor k : dozwoloneKolory(r)) {   // każdy dozwolony kolor musi dać się odczytać
                const int n = r == Rola::Tempco ? 6 : 4;
                std::vector<Kolor> p = n == 6 ? std::vector<Kolor>{K::Brazowy, K::Czarny, K::Czarny, K::Brazowy, K::Brazowy, k}
                                              : std::vector<Kolor>{K::Brazowy, K::Czarny, K::Brazowy, K::Zloty};
                if (r == Rola::Cyfra) p[0] = k; else if (r == Rola::Mnoznik) p[2] = k; else if (r == Rola::Tolerancja) p[3] = k;
                if (r == Rola::Cyfra && k == K::Czarny) p[0] = K::Czarny;
                CHECK(odczytajPaski(p).has_value());
            }
    }
    // odwrotnie
    CHECK((paskiDlaWartosci(4700, 20, 4) == std::vector<Kolor>{K::Zolty, K::Fioletowy, K::Czerwony, K::Brak}));
    CHECK((paskiDlaWartosci(4700, 20, 3) == std::vector<Kolor>{K::Zolty, K::Fioletowy, K::Czerwony}));
    CHECK(paskiDlaWartosci(4700, 5, 3).empty());
    CHECK((paskiDlaWartosci(1000, 1, 6, 50) ==
           std::vector<Kolor>{K::Brazowy, K::Czarny, K::Czarny, K::Brazowy, K::Brazowy, K::Czerwony}));
    CHECK(paskiDlaWartosci(1000, 1, 6, 7).empty());
    {
        auto p = paskiDlaWartosci(4750, 2, 6);
        auto o = odczytajPaski(p);
        CHECK(p.size() == 6 && o && bl(o->wartosc, 4750) && o->tolerancja == 2 && o->tempco == 100);
    }
    CHECK((paskiDlaWartosci(4700, 5, 4) == std::vector<Kolor>{K::Zolty, K::Fioletowy, K::Czerwony, K::Zloty}));
    CHECK((paskiDlaWartosci(100000, 1, 5) ==
           std::vector<Kolor>{K::Brazowy, K::Czarny, K::Czarny, K::Pomaranczowy, K::Brazowy}));
    CHECK((paskiDlaWartosci(0.22, 10, 4) == std::vector<Kolor>{K::Czerwony, K::Czerwony, K::Srebrny, K::Srebrny}));
    CHECK(paskiDlaWartosci(4750, 5, 4).empty());
    CHECK(!paskiDlaWartosci(4750, 1, 5).empty());
    CHECK(paskiDlaWartosci(4700, 3, 4).empty());
    // pełny obieg dla wielu wartości
    for (double v : {1.0, 10.0, 47.0, 330.0, 2200.0, 15000.0, 680000.0, 1e7}) {
        auto p = paskiDlaWartosci(v, 5, 4);
        auto o = odczytajPaski(p);
        CHECK(!p.empty() && o && bl(o->wartosc, v));
    }

    // kody SMD rezystorów
    CHECK(bl(*rezystorZKodu("472"), 4700));
    CHECK(bl(*rezystorZKodu("1002"), 10000));
    CHECK(bl(*rezystorZKodu("4R7"), 4.7));
    CHECK(bl(*rezystorZKodu("R22"), 0.22));
    CHECK(bl(*rezystorZKodu("000"), 0));
    CHECK(bl(*rezystorZKodu("01A"), 100));
    CHECK(bl(*rezystorZKodu("01C"), 10000));
    CHECK(bl(*rezystorZKodu("68X"), 0.1 * 499));   // 68. mantysa E96 to 499
    CHECK(bl(*rezystorZKodu("96Z"), 0.976));
    CHECK(!rezystorZKodu("xyz").has_value() && !rezystorZKodu("").has_value());
    CHECK(kodEIA96(100) == "01A" && kodEIA96(10000) == "01C" && kodEIA96(4.7).empty());

    // kody kondensatorów
    CHECK(bl(*kondensatorZKodu("104"), 100e-9));
    CHECK(bl(*kondensatorZKodu("225"), 2.2e-6));
    CHECK(bl(*kondensatorZKodu("101"), 100e-12));
    CHECK(bl(*kondensatorZKodu("4R7"), 4.7e-12));
    CHECK(bl(*kondensatorZKodu("n47"), 470e-12));
    CHECK(!kondensatorZKodu("abc").has_value());

    std::cout << sprawdzenia << " sprawdzeń, " << bledy << " błędów\n";
    // Regresja: po ustawieniu polskich ustawień regionalnych strtod() czytało "4.7" jako 4, więc 4k7 dawało 4 kΩ
    // i paski bez fioletowego. Test działa tylko tam, gdzie takie ustawienia są zainstalowane.
    for (const char* nazwa : {"pl_PL.UTF-8", "pl_PL.utf8", "pl_PL", "de_DE.UTF-8", "de_DE.utf8"}) {
        if (!std::setlocale(LC_ALL, nazwa)) continue;
        auto v = SzeregiE::parsujWartosc("4k7", RodzajElementu::Rezystor);
        CHECK(v && bl(*v, 4700));
        if (v) {
            CHECK((paskiDlaWartosci(*v, 5, 4) == std::vector<K>{K::Zolty, K::Fioletowy, K::Czerwony, K::Zloty}));
            CHECK((paskiDlaWartosci(*v, 5, 5) == std::vector<K>{K::Zolty, K::Fioletowy, K::Czarny, K::Brazowy, K::Zloty}));
        }
        auto m = SzeregiE::parsujWartosc("2n2", RodzajElementu::Kondensator);
        CHECK(m && bl(*m, 2.2e-9));
        auto r = SzeregiE::parsujWartosc("0,47", RodzajElementu::Rezystor);
        CHECK(r && bl(*r, 0.47));
        std::setlocale(LC_ALL, "C");
        break;
    }
    return bledy ? 1 : 0;
}
