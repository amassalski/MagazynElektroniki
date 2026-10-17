// Testy szeregów E6-E96, zapisu wartości (RKM) i importu elementów z CSV. Bez bazy danych i wxWidgets.

#include "Logika/ImportCSVElementow.h"
#include "Logika/SzeregiE.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>

static int bledy = 0;
static int sprawdzenia = 0;

#define CHECK(warunek)                                                                  \
    do {                                                                                \
        ++sprawdzenia;                                                                  \
        if (!(warunek)) {                                                               \
            ++bledy;                                                                    \
            std::cerr << "BŁĄD " << __FILE__ << ":" << __LINE__ << "  " #warunek "\n"; \
        }                                                                               \
    } while (0)

using namespace SzeregiE;
static const RodzajElementu R = RodzajElementu::Rezystor, C = RodzajElementu::Kondensator, L = RodzajElementu::Cewka;

static bool bliskie(double a, double b) { return std::fabs(a - b) <= 1e-9 * std::max(std::fabs(a), std::fabs(b)); }

static void testSzeregow() {
    std::cout << "[szeregi]\n";
    CHECK(mantysy(Szereg::E6).size() == 6 && mantysy(Szereg::E12).size() == 12 && mantysy(Szereg::E24).size() == 24);
    CHECK(mantysy(Szereg::E48).size() == 48 && mantysy(Szereg::E96).size() == 96);
    // dekada: 6/12/24/48/96 wartości, a w zakresie 10 Ω - 1 MΩ jest 5 pełnych dekad + wartość 1M
    CHECK(wartosci(Szereg::E24, 10, 1e6).size() == 5 * 24 + 1);
    CHECK(wartosci(Szereg::E96, 100, 1e6).size() == 4 * 96 + 1);
    CHECK(wartosci(Szereg::E12, 1, 10).size() == 13);   // 1,0 ... 8,2 i 10

    // znane wartości (IEC 60063)
    auto ma = [](Szereg s, double v) { for (double w : wartosci(s, v * 0.99, v * 1.01)) if (bliskie(w, v)) return true; return false; };
    CHECK(ma(Szereg::E24, 27) && ma(Szereg::E24, 30) && ma(Szereg::E24, 33) && ma(Szereg::E24, 91));
    CHECK(!ma(Szereg::E24, 26) && !ma(Szereg::E24, 29));                 // E24 nie wynika ze wzoru
    CHECK(ma(Szereg::E96, 102) && ma(Szereg::E96, 976) && ma(Szereg::E96, 604) && ma(Szereg::E96, 249));
    CHECK(ma(Szereg::E48, 215) && ma(Szereg::E48, 316) && ma(Szereg::E48, 953) && !ma(Szereg::E48, 102));
    // E24 zawiera E12, a E96 zawiera E48
    auto w12 = wartosci(Szereg::E12, 10, 99), w24 = wartosci(Szereg::E24, 10, 99);
    for (double v : w12) CHECK(std::find_if(w24.begin(), w24.end(), [&](double x) { return bliskie(x, v); }) != w24.end());
    auto w48 = wartosci(Szereg::E48, 100, 999), w96 = wartosci(Szereg::E96, 100, 999);
    for (double v : w48) CHECK(std::find_if(w96.begin(), w96.end(), [&](double x) { return bliskie(x, v); }) != w96.end());

    // zakres jest włączony po obu stronach, wartości rosną i nie mają szumu
    auto z = wartosci(Szereg::E24, 4.7, 47);
    CHECK(!z.empty() && bliskie(z.front(), 4.7) && bliskie(z.back(), 47));
    CHECK(std::is_sorted(z.begin(), z.end()));
    CHECK(wartosci(Szereg::E24, 100, 10).empty() && wartosci(Szereg::E24, 0, 10).empty());
    CHECK(wartosci(Szereg::E24, 0.1, 1).size() == 25);                   // 0,1 ... 0,91 i 1

    CHECK(domyslnaTolerancja(Szereg::E24) == "5%" && domyslnaTolerancja(Szereg::E96) == "1%");
    CHECK(zNazwy("e24") == Szereg::E24 && zNazwy(" E96 ") == Szereg::E96 && !zNazwy("E192"));
}

static void testZapisu() {
    std::cout << "[zapis wartości]\n";
    CHECK(formatRKM(4.7, R) == "4R7" && formatRKM(47, R) == "47R" && formatRKM(470, R) == "470R");
    CHECK(formatRKM(4700, R) == "4K7" && formatRKM(100000, R) == "100K" && formatRKM(2.2e6, R) == "2M2");
    CHECK(formatRKM(4750, R) == "4K75" && formatRKM(0.1, R) == "0R1" && formatRKM(0.47, R) == "0R47");
    CHECK(formatRKM(4.7e-9, C) == "4n7" && formatRKM(100e-9, C) == "100n" && formatRKM(1e-6, C) == "1u");
    CHECK(formatRKM(10e-12, C) == "10p" && formatRKM(4.7e-6, C) == "4u7" && formatRKM(1000e-6, C) == "1000u");
    CHECK(formatRKM(4.7e-6, L) == "4u7" && formatRKM(2.2e-3, L) == "2m2" && formatRKM(100e-9, L) == "100n");
    CHECK(formatWartosci(4700, R) == "4,7k" && formatWartosci(47, R) == "47" && formatWartosci(2.2e6, R) == "2,2M");
    CHECK(formatWartosci(100e-9, C) == "100n" && formatWartosci(4.7e-6, L) == "4,7u");
    // pole "Wartość": litera zamiast przecinka
    CHECK(formatPolaWartosci(4700, R) == "4k7" && formatPolaWartosci(47, R) == "47" && formatPolaWartosci(2.2e6, R) == "2M2");
    CHECK(formatPolaWartosci(4.7e-9, C) == "4n7" && formatPolaWartosci(4.7e-6, L) == "4u7" && formatPolaWartosci(0.47, R) == "0R47");
    for (double v : {0.47, 4.7, 4700.0, 2.2e6, 100.0}) {
        const std::string t = formatPolaWartosci(v, R);
        CHECK(t.find(',') == std::string::npos && parsujWartosc(t, R) && bliskie(*parsujWartosc(t, R), v));
    }
    CHECK(formatZJednostka(4700, R) == "4,7 kΩ" && formatZJednostka(100e-9, C) == "100 nF" && formatZJednostka(4.7e-6, L) == "4,7 µH");

    // wczytywanie: obie notacje i jednostki
    auto p = [](const char* t, RodzajElementu r) { auto v = parsujWartosc(t, r); return v ? *v : -1.0; };
    CHECK(bliskie(p("4k7", R), 4700) && bliskie(p("4K7", R), 4700) && bliskie(p("4,7k", R), 4700) && bliskie(p("4.7k", R), 4700));
    CHECK(bliskie(p("100", R), 100) && bliskie(p("100R", R), 100) && bliskie(p("4R7", R), 4.7) && bliskie(p("0R1", R), 0.1));
    CHECK(bliskie(p("1M", R), 1e6) && bliskie(p("2M2", R), 2.2e6) && bliskie(p("10 kΩ", R), 1e4) && bliskie(p("47kohm", R), 47000));
    CHECK(bliskie(p("100nF", C), 100e-9) && bliskie(p("4n7", C), 4.7e-9) && bliskie(p("1µF", C), 1e-6) && bliskie(p("22p", C), 22e-12));
    CHECK(bliskie(p("1m", L), 1e-3) && bliskie(p("4u7", L), 4.7e-6) && bliskie(p("1m", C), 1e-3));
    CHECK(p("", R) < 0 && p("abc", R) < 0 && p("4,7,7k", R) < 0 && p("0", R) < 0 && p("4x7", R) < 0);
    // zapis i odczyt dają tę samą wartość dla całych szeregów
    for (Szereg s : {Szereg::E24, Szereg::E48, Szereg::E96})
        for (double v : wartosci(s, 0.1, 10e6)) {
            CHECK(bliskie(p(formatRKM(v, R).c_str(), R), v));
            CHECK(bliskie(p(formatWartosci(v, R).c_str(), R), v));
        }
    for (double v : wartosci(Szereg::E12, 1e-12, 4.7e-3)) {
        CHECK(bliskie(p(formatRKM(v, C).c_str(), C), v));
        CHECK(bliskie(p(formatWartosci(v, L).c_str(), L), v));
    }
}

static void testGeneratora() {
    std::cout << "[generator]\n";
    CHECK(zbudujSymbol("", R, 4700, "0603", "1%", "", "", "") == "R-4K7-0603-1%");
    CHECK(zbudujSymbol("", C, 100e-9, "0603", "", "", "50 V", "X7R") == "C-100n-0603-50V-X7R");
    CHECK(zbudujSymbol("", C, 100e-9, "", "", "", "", "") == "C-100n");              // puste człony znikają
    CHECK(zbudujSymbol("{typ}{wartosc}_{obudowa}", L, 4.7e-6, "1210", "", "", "", "") == "L4,7u_1210");
    CHECK(kategoriaDomyslna(R, "0603") == "Elementy bierne / Rezystory / Rezystory SMD");
    CHECK(kategoriaDomyslna(R, "0207") == "Elementy bierne / Rezystory / Rezystory THT");
    CHECK(kategoriaDomyslna(C, "0603") == "Elementy bierne / Kondensatory / Ceramiczne");
    CHECK(kategoriaDomyslna(L, "") == "Elementy bierne / Cewki i dławiki");
    CHECK(kolejnaSzuflada("A1", 2) == "A3" && kolejnaSzuflada("A01", 3) == "A04" && kolejnaSzuflada("B9", 1) == "B10");
    CHECK(kolejnaSzuflada("X", 2) == "X");

    ParametrySzeregu p;
    p.obudowa = "0603"; p.od = 10; p.doo = 1000; p.ilosc = 50; p.minimum = 10; p.moc = "0,1 W";
    p.miejsce = {"Kontener 1", "A1", ""}; p.szufladaNaDekade = true; p.cena = 0.02; p.tagi = "smd";
    std::string blad;
    auto w = generujSzereg(p, &blad);
    CHECK(blad.empty() && w.size() == 2 * 24 + 1);
    if (w.size() == 49) {
        const Element& a = w.front().element;
        CHECK(a.symbol == "R-10R-0603-5%" && a.wartosc == "10" && a.jednostka == "Ω" && a.tolerancja == "5%");
        CHECK(a.obudowa == "0603" && a.parametry == "0,1 W" && a.iloscDostepna == 50 && a.iloscMinimalna == 10);
        CHECK(a.tagi == "E24, smd" && a.opis == "Rezystor 10 Ω 5% 0,1 W 0603" && a.waluta == "PLN");
        CHECK(a.cena && *a.cena == 0.02);
        CHECK(w.front().kategoria == "Elementy bierne / Rezystory / Rezystory SMD");
        // dekady: 10-91 Ω -> A1, 100-910 Ω -> A2, 1k -> A3
        CHECK(a.miejsceTekst() == "Kontener 1, A1");
        CHECK(w[24].element.symbol == "R-100R-0603-5%" && w[24].element.miejsceTekst() == "Kontener 1, A2");
        CHECK(w[48].element.symbol == "R-1K-0603-5%" && w[48].element.miejsceTekst() == "Kontener 1, A3");
    }
    std::set<std::string> symbole;
    for (const auto& x : w) symbole.insert(x.element.symbol);
    CHECK(symbole.size() == w.size());                                // symbole są niepowtarzalne

    p.szufladaNaDekade = false;
    CHECK(generujSzereg(p)[30].element.szuflada == "A1");
    p.szereg = Szereg::E96; p.tolerancja = "0,5%";
    auto w96 = generujSzereg(p);
    CHECK(w96.size() == 2 * 96 + 1 && w96[1].element.tolerancja == "0,5%" && w96[1].element.symbol == "R-10R2-0603-0,5%");

    ParametrySzeregu c;
    c.rodzaj = C; c.szereg = Szereg::E12; c.od = 1e-9; c.doo = 1e-6; c.obudowa = "0805"; c.napiecie = "50 V"; c.dielektryk = "X7R";
    auto wc = generujSzereg(c);
    CHECK(wc.size() == 3 * 12 + 1 && wc[0].element.symbol == "C-1n-0805-50V-X7R");
    CHECK(wc[0].element.jednostka == "F" && wc[0].element.parametry == "50 V; X7R" && wc[0].element.tolerancja == "10%");

    // błędne parametry
    ParametrySzeregu z; z.od = 0;
    CHECK(generujSzereg(z, &blad).empty() && !blad.empty());
    z.od = 100; z.doo = 10;
    CHECK(generujSzereg(z, &blad).empty());
    z.od = 1e-30; z.doo = 1e30; z.szereg = Szereg::E96;
    CHECK(generujSzereg(z, &blad).empty() && blad.find("zbyt duży") != std::string::npos);
    z.od = 10.5; z.doo = 10.6; z.szereg = Szereg::E6;
    CHECK(generujSzereg(z, &blad).empty());
}

static void testCSV() {
    std::cout << "[CSV elementów]\n";
    using namespace ImportCSVElementow;
    // szablon wczytuje się bez błędów i bez ostrzeżeń
    auto s = parsuj(szablon());
    CHECK(s.blad.empty() && s.ostrzezenia.empty() && s.pozycje.size() == 3);
    if (s.pozycje.size() == 3) {
        const Element& r = s.pozycje[0].element;
        CHECK(r.symbol == "R-4K7-0603-1%" && r.tolerancja == "1%" && r.iloscDostepna == 100 && r.iloscMinimalna == 20);
        CHECK(r.kontener == "Kontener 1" && r.szuflada == "A1" && r.parametry == "0,1 W");
        CHECK(s.pozycje[0].kategoria == "Elementy bierne / Rezystory / Rezystory SMD" && s.pozycje[0].nrWiersza == 2);
        CHECK(s.pozycje[2].element.symbol == "BC547B" && s.pozycje[2].element.producent == "ON Semiconductor");
    }

    // różne nagłówki, separator przecinek, miejsce zapisem swobodnym, cena z przecinkiem
    auto a = parsuj("Part,Value,Tol,Package,Qty,Min,Location,Price,Currency\n"
                    "R-10K,10k,1%,0603,\"1 000\",100,\"Kontener 1 szufladka A1/1\",\"0,02\",eur\n");
    CHECK(a.blad.empty() && a.pozycje.size() == 1);
    if (!a.pozycje.empty()) {
        const PozycjaImportu& p = a.pozycje[0];
        CHECK(p.blad.empty() && p.element.symbol == "R-10K" && p.element.tolerancja == "1%");
        CHECK(p.element.iloscDostepna == 1000 && p.element.iloscMinimalna == 100);
        CHECK(p.element.kontener == "Kontener 1" && p.element.szuflada == "A1" && p.element.przegroda == "1");
        CHECK(p.element.cena && *p.element.cena == 0.02 && p.element.waluta == "EUR");
    }

    // Typ + Wartość bez symbolu: symbol, jednostka i zapis wartości powstają automatycznie; Moc/Napięcie idą do parametrów
    auto t = parsuj("Typ;Wartość;Obudowa;Tolerancja;Moc;Ilość\nR;4k7;0603;1%;0,1 W;10\nC;100nF;0603;;;5\nL;4u7;1210;;;2\n");
    CHECK(t.blad.empty() && t.pozycje.size() == 3);
    if (t.pozycje.size() == 3) {
        CHECK(t.pozycje[0].element.symbol == "R-4K7-0603-1%" && t.pozycje[0].element.wartosc == "4k7");
        CHECK(t.pozycje[0].element.jednostka == "Ω" && t.pozycje[0].element.parametry == "0,1 W");
        CHECK(t.pozycje[1].element.symbol == "C-100n-0603" && t.pozycje[1].element.jednostka == "F");
        CHECK(t.pozycje[2].element.symbol == "L-4u7-1210" && t.pozycje[2].element.jednostka == "H");
    }

    // błędy w wierszach nie psują reszty; puste wiersze znikają; nieznane kolumny dają ostrzeżenie
    auto b = parsuj("Symbol;Ilość;Kolor\nA;5;czerwony\n;3;x\nB;abc;y\nC;-2;z\nD;1,5;w\nE;7;v\n;;\n");
    CHECK(b.blad.empty() && b.ostrzezenia.size() == 1 && b.ostrzezenia[0].find("Kolor") != std::string::npos);
    CHECK(b.pozycje.size() == 6);
    if (b.pozycje.size() == 6) {
        CHECK(b.pozycje[0].blad.empty() && b.pozycje[0].element.iloscDostepna == 5);
        CHECK(b.pozycje[1].blad == "brak symbolu" && b.pozycje[1].nrWiersza == 3);
        CHECK(!b.pozycje[2].blad.empty() && !b.pozycje[3].blad.empty() && !b.pozycje[4].blad.empty());
        CHECK(b.pozycje[5].blad.empty() && b.pozycje[5].element.symbol == "E");
    }
    CHECK(!parsuj("").blad.empty());
    CHECK(!parsuj("Wartość;Ilość\n10k;5\n").blad.empty());              // brak symbolu i typu
    CHECK(!parsuj("Typ;Wartość\nX;10k\n").pozycje[0].blad.empty());     // nieznany typ
    CHECK(!parsuj("Typ;Wartość\nR;abc\n").pozycje[0].blad.empty());     // zła wartość

    // zapis i ponowny odczyt (Excel: UTF-8 z BOM, średnik, przecinek dziesiętny)
    ParametrySzeregu p; p.obudowa = "0603"; p.od = 10; p.doo = 100; p.ilosc = 7; p.cena = 0.125;
    p.miejsce = {"Kontener 1", "A1", "2"}; p.producent = "Yageo; \"test\"";
    auto gen = generujSzereg(p);
    const std::string csv = naCSV(gen);
    CHECK(csv.compare(0, 3, "\xEF\xBB\xBF") == 0 && csv.find("0,125") != std::string::npos);
    auto wr = parsuj(csv);
    CHECK(wr.blad.empty() && wr.ostrzezenia.empty() && wr.pozycje.size() == gen.size());
    if (wr.pozycje.size() == gen.size() && !gen.empty()) {
        const Element &x = gen[3].element, &y = wr.pozycje[3].element;
        CHECK(x.symbol == y.symbol && x.wartosc == y.wartosc && x.opis == y.opis && x.tolerancja == y.tolerancja);
        CHECK(x.miejsceTekst() == y.miejsceTekst() && y.producent == "Yageo; \"test\"");
        CHECK(y.cena && *y.cena == 0.125 && y.iloscDostepna == 7 && wr.pozycje[3].kategoria == gen[3].kategoria);
    }
}

int main() {
    testSzeregow();
    testZapisu();
    testGeneratora();
    testCSV();
    std::cout << sprawdzenia << " sprawdzeń, " << bledy << " błędów\n";
    return bledy == 0 ? 0 : 1;
}
