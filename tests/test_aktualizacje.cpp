#include "../src/Logika/Aktualizacje.h"
#include <iostream>

static int sprawdzenia = 0, bledy = 0;
#define CHECK(w) do { ++sprawdzenia; if (!(w)) { ++bledy; std::cerr << "BŁĄD " << __FILE__ << ":" << __LINE__ << "  " #w "\n"; } } while (0)

using namespace Aktualizacje;

int main() {
    CHECK(nowsza("1.0.1", "1.0.0") && nowsza("1.10.0", "1.9.9") && nowsza("2", "1.99.99") && nowsza("v1.2", "1.1.9"));
    CHECK(!nowsza("1.0.0", "1.0.0") && !nowsza("1.0", "1.0.0") && !nowsza("1.0.0", "1.0.1") && !nowsza("0.9", "1.0.0"));
    CHECK(!nowsza("abc", "1.0.0") && !nowsza("1.0.1", "") && !nowsza("1.2.", "1.0.0") && !nowsza("1..2", "1.0.0"));
    CHECK(parsujNumer(" v1.2.3 ") && *parsujNumer("1.2.3") == (std::vector<int>{1, 2, 3}));
    CHECK(!parsujNumer("1.2.3.4.5") && !parsujNumer("<html>") && !parsujNumer("1.0-beta"));

    auto i = parsujPlikWersji("\xEF\xBB\xBFv1.0.1\r\nhttps://example.com/pobierz\r\nCo nowego:\r\n- poprawki\r\n- nowe ikony\r\n");
    CHECK(i && i->wersja == "1.0.1" && i->adresPobierania == "https://example.com/pobierz");
    CHECK(i && i->opis == "Co nowego:\n- poprawki\n- nowe ikony");
    auto j = parsujPlikWersji("\n1.2.0\n");
    CHECK(j && j->wersja == "1.2.0" && j->adresPobierania.empty() && j->opis.empty());
    // strona błędu zamiast pliku (np. 404 w HTML) nie może być wzięta za wersję
    CHECK(!parsujPlikWersji("<!DOCTYPE html><html>404: Not Found</html>"));
    CHECK(!parsujPlikWersji("404: Not Found"));
    CHECK(!parsujPlikWersji("") && !parsujPlikWersji("\n\n"));
    CHECK(adresWersji().rfind("https://", 0) == 0 && adresWersji().find("DomowyMagazyn/wersja.txt") != std::string::npos);

    std::cout << sprawdzenia << " sprawdzeń, " << bledy << " błędów\n";
    return bledy ? 1 : 0;
}
