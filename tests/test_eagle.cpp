// Testy eksportu do Eagle (skrypt atrybutów, ULP) i zgodności listy CSV z ULP z importem listy części.
#include "Eksport/EksportEagle.h"
#include "Import/ImportBOM.h"

#include <iostream>

static int bledy = 0, sprawdzenia = 0;
#define CHECK(w) do { ++sprawdzenia; if (!(w)) { ++bledy; std::cerr << "BŁĄD " << __FILE__ << ":" << __LINE__ << "  " #w "\n"; } } while (0)

static bool zawiera(const std::string& s, const std::string& t) { return s.find(t) != std::string::npos; }

int main() {
    PozycjaProjektu a; a.symbol = "R-10K-0603-1%"; a.oznaczenia = "R1, R2"; a.dostepna = 40; a.producent = "Yageo";
    a.symbolProducenta = "RC0603FR-0710KL"; a.miejsce = "Kontener 1, A1/1";
    PozycjaProjektu b; b.symbol = "Kondensator 100µF/16V"; b.oznaczenia = "C1"; b.dostepna = 0; b.miejsce = "";
    PozycjaProjektu c; c.symbol = "Śruba M3"; c.oznaczenia = "";
    auto w = EksportEagle::skrypt({a, b, c}, "Zasilacz łazienkowy");
    CHECK(w.czesci == 3);
    CHECK(w.bezOznaczen.size() == 1 && w.bezOznaczen[0] == "Śruba M3");
    CHECK(zawiera(w.tekst, "ATTRIBUTE R1 DM_SYMBOL 'R-10K-0603-1%';\n"));
    CHECK(zawiera(w.tekst, "ATTRIBUTE R2 MPN 'RC0603FR-0710KL';\n"));
    CHECK(zawiera(w.tekst, "ATTRIBUTE R1 MANUFACTURER 'Yageo';\n"));
    CHECK(zawiera(w.tekst, "ATTRIBUTE R2 DM_MIEJSCE 'Kontener 1, A1/1';\n"));
    CHECK(zawiera(w.tekst, "ATTRIBUTE R1 DM_STAN '40';\n"));
    CHECK(zawiera(w.tekst, "ATTRIBUTE C1 DM_SYMBOL 'Kondensator 100uF/16V';\n"));   // bez znaków spoza ASCII
    CHECK(zawiera(w.tekst, "ATTRIBUTE C1 DM_STAN '0';\n"));
    CHECK(!zawiera(w.tekst, "C1 MPN") && !zawiera(w.tekst, "C1 DM_MIEJSCE"));          // puste atrybuty pomijane
    CHECK(zawiera(w.tekst, "Zasilacz lazienkowy"));
    for (unsigned char ch : w.tekst) CHECK(ch < 0x80);

    CHECK(EksportEagle::tekstDlaEagle("O'Neil\nżółć") == "O\\'Neil zolc");
    CHECK(EksportEagle::tekstDlaEagle("±5% 1Ω") == "+-5% 1R");

    // ULP istnieje i ma najważniejsze elementy
    const std::string ulp = EksportEagle::ulpBOM();
    CHECK(zawiera(ulp, "#usage") && zawiera(ulp, "SCH.allparts (P)") && zawiera(ulp, "P.attribute[\"MPN\"]"));
    CHECK(zawiera(ulp, "P.populate") && zawiera(ulp, "SCH.name") && !zawiera(ulp, "SCH.filename"));
    CHECK(zawiera(ulp, "\"Reference\", \"Value\", \"Package\", \"MPN\", \"Manufacturer\", \"Description\", \"Qty\""));
    CHECK(!zawiera(ulp, "int2str"));
    for (unsigned char ch : ulp) CHECK(ch < 0x80);

    // CSV z ULP jest rozpoznawany przez import listy części
    const std::string csv =
        "\"Reference\";\"Value\";\"Package\";\"MPN\";\"Manufacturer\";\"Description\";\"Qty\"\n"
        "\"R1\";\"10k\";\"R0603\";\"RC0603FR-0710KL\";\"Yageo\";\"\";\"1\"\n"
        "\"R2\";\"10k\";\"R0603\";\"RC0603FR-0710KL\";\"Yageo\";\"\";\"1\"\n"
        "\"C1\";\"100n\";\"C0603\";\"\";\"\";\"\";\"1\"\n"
        // tryb "wartosci": jedna linia na wartosc, oznaczenia po przecinku, Qty = liczba sztuk
        ;
    const std::string csvWartosci =
        "\"Reference\";\"Value\";\"Package\";\"MPN\";\"Manufacturer\";\"Description\";\"Qty\"\n"
        "\"R1, R2\";\"10k\";\"R0603\";\"\";\"\";\"\";\"2\"\n";
    {
        WynikImportuBOM w2; std::string b2;
        CHECK(ImportBOM::parsujTekst(csvWartosci, w2, b2));
        CHECK(w2.pozycje.size() == 1 && w2.pozycje[0].ilosc == 2);
    }
    WynikImportuBOM wynik;
    std::string blad;
    CHECK(ImportBOM::parsujTekst(csv, wynik, blad));
    CHECK(wynik.pozycje.size() == 2);
    int dziesiec = 0;
    for (const auto& p : wynik.pozycje) if (p.wartosc == "10k") dziesiec = p.ilosc;
    CHECK(dziesiec == 2);

    std::cout << sprawdzenia << " sprawdzeń, " << bledy << " błędów\n";
    return bledy ? 1 : 0;
}
