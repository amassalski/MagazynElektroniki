// Testy katalogu elementów z/do CSV. Bez bazy danych i wxWidgets.
#include "Logika/KatalogCSV.h"

#include <iostream>

static int bledy = 0, sprawdzenia = 0;
#define CHECK(w) do { ++sprawdzenia; if (!(w)) { ++bledy; std::cerr << "BŁĄD " << __FILE__ << ":" << __LINE__ << "  " #w "\n"; } } while (0)

int main() {
    auto p = KatalogCSV::parsujParametry("Napięcie=3-32 V; Pasmo = 1 MHz;;bez równości; Pusty=; =pusta nazwa");
    CHECK(p.size() == 2 && p[0].nazwa == "Napięcie" && p[0].wartosc == "3-32 V" && p[0].zrodlo == "csv");
    CHECK(p.size() == 2 && p[1].nazwa == "Pasmo" && p[1].wartosc == "1 MHz");
    CHECK(KatalogCSV::parametryNaTekst(p) == "Napięcie=3-32 V; Pasmo=1 MHz");

    // dowolne kolumny stają się parametrami, aliasy nagłówków działają
    auto w = KatalogCSV::parsuj(
        "MPN;Manufacturer;Description;Package;Category;Datasheet;Napięcie zasilania;Parametry\n"
        "LM358;TI;Wzmacniacz;DIP-8;Wzmacniacze;http://x/lm358.pdf;3-32 V;Pasmo=1 MHz\n"
        ";TI;bez symbolu;;;;;\n"
        "NE555P;;;;;;4,5-16 V;\n");
    CHECK(w.blad.empty() && w.pozycje.size() == 3);
    const auto& a = w.pozycje[0].katalog;
    CHECK(a.symbol == "LM358" && a.producent == "TI" && a.opis == "Wzmacniacz" && a.obudowa == "DIP-8" &&
          a.rodzina == "Wzmacniacze" && a.datasheetUrl == "http://x/lm358.pdf");
    CHECK(a.parametry.size() == 2 && a.parametry[0].nazwa == "Napięcie zasilania" && a.parametry[0].wartosc == "3-32 V" &&
          a.parametry[1].nazwa == "Pasmo");
    CHECK(a.parametr("pasmo") && a.parametr("PASMO")->wartosc == "1 MHz" && !a.parametr("nie ma"));
    CHECK(w.pozycje[1].blad == "brak symbolu" && w.pozycje[1].nrWiersza == 3);
    CHECK(w.pozycje[2].katalog.symbol == "NE555P" && w.pozycje[2].katalog.parametry.size() == 1);

    // brak kolumny Symbol
    CHECK(!KatalogCSV::parsuj("Opis;Obudowa\nx;y\n").blad.empty());
    CHECK(!KatalogCSV::parsuj("").blad.empty());

    // obieg: eksport -> import
    std::vector<PozycjaKatalogu> lista = {a, w.pozycje[2].katalog};
    lista[0].uwagi = "z \"cudzysłowem\"; i średnikiem";
    const std::string csv = KatalogCSV::naCSV(lista);
    CHECK(csv.compare(0, 3, "\xEF\xBB\xBF") == 0);
    auto z = KatalogCSV::parsuj(csv);
    CHECK(z.blad.empty() && z.pozycje.size() == 2);
    CHECK(z.pozycje[0].katalog.uwagi == lista[0].uwagi && z.pozycje[0].katalog.parametry.size() == 2);

    // szablon da się wczytać
    auto s = KatalogCSV::parsuj(KatalogCSV::szablon());
    CHECK(s.blad.empty() && s.pozycje.size() == 3 && s.pozycje[0].blad.empty());
    CHECK(s.pozycje[2].katalog.parametr("hFE") && s.pozycje[2].katalog.parametr("hFE")->wartosc == "200-450");

    std::cout << sprawdzenia << " sprawdzeń, " << bledy << " błędów\n";
    return bledy ? 1 : 0;
}
