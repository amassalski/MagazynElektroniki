// Testy parserów list części (CSV, KiCad, Eagle). Nie wymagają bazy danych ani wxWidgets.

#include "Import/ImportBOM.h"
#include "Import/ImportEagle.h"
#include "Import/ImportKiCad.h"
#include "Import/MiniXML.h"
#include "Import/ParserCSV.h"

#include <iostream>

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

#define CHECK_EQ(a, b)                                                                              \
    do {                                                                                            \
        ++sprawdzenia;                                                                              \
        const auto va = (a);                                                                        \
        const auto vb = (b);                                                                        \
        if (!(va == vb)) {                                                                          \
            ++bledy;                                                                                \
            std::cerr << "BŁĄD " << __FILE__ << ":" << __LINE__ << "  " #a " == " #b "\n  jest: [" \
                      << va << "] oczekiwano: [" << vb << "]\n";                                    \
        }                                                                                           \
    } while (0)

static const PozycjaBOM* znajdz(const WynikImportuBOM& w, const std::string& oznaczenieZawiera) {
    for (const PozycjaBOM& p : w.pozycje) {
        if (p.oznaczenia.find(oznaczenieZawiera) != std::string::npos) return &p;
    }
    return nullptr;
}

static void testCSV() {
    std::cout << "[csv]\n";
    using ParserCSV::parsuj;
    auto w = parsuj("a,b,c\r\n1,\"x,y\",3\r\n");
    CHECK_EQ(w.size(), size_t(2));
    CHECK_EQ(w[1][1], std::string("x,y"));
    w = parsuj("\xEF\xBB\xBF" "a;b;c\n1;\"mówi \"\"cześć\"\"\";3\n\n4;5;6\n");
    CHECK_EQ(w.size(), size_t(3));
    CHECK_EQ(w[0][0], std::string("a"));  // BOM usunięty
    CHECK_EQ(w[1][1], std::string("mówi \"cześć\""));
    w = parsuj("a\tb\n1\t2\n");
    CHECK_EQ(w[1][1], std::string("2"));
    w = parsuj("a,b\n\"wiele\nlinii\",2\n");
    CHECK_EQ(w.size(), size_t(2));
    CHECK_EQ(w[1][0], std::string("wiele\nlinii"));
    CHECK_EQ(ParserCSV::wykryjSeparator("\"R1, R2\";x;y\n"), ';');
    CHECK(parsuj("").empty());
}

static void testXML() {
    std::cout << "[xml]\n";
    std::string blad;
    auto d = MiniXML::parsuj("<?xml version=\"1.0\"?><!-- k --><a x=\"1 &amp; 2\"><b/><b y='z'>t&lt;e</b></a>", blad);
    CHECK(d != nullptr);
    if (d) {
        CHECK_EQ(d->atrybut("x"), std::string("1 & 2"));
        CHECK_EQ(d->dzieciONazwie("b").size(), size_t(2));
        CHECK_EQ(d->dzieciONazwie("b")[1]->atrybut("y"), std::string("z"));
        CHECK_EQ(d->dzieciONazwie("b")[1]->tekst, std::string("t<e"));
    }
    CHECK(MiniXML::parsuj("<a><b></a>", blad) == nullptr);
    CHECK(!blad.empty());
}

static void testNormalizacji() {
    std::cout << "[normalizacja]\n";
    using namespace ImportBOM;
    CHECK_EQ(normalizujWartosc("4k7"), std::string("4.7k"));
    CHECK_EQ(normalizujWartosc("4.7 kΩ"), std::string("4.7k"));
    CHECK_EQ(normalizujWartosc("10K"), std::string("10k"));
    CHECK_EQ(normalizujWartosc("10 kΩ"), std::string("10k"));
    CHECK_EQ(normalizujWartosc("100nF"), std::string("100n"));
    CHECK_EQ(normalizujWartosc("0.1uF"), normalizujWartosc("100nF") == "100n" ? std::string("0.1u") : std::string("?"));
    CHECK_EQ(normalizujWartosc("1µF"), std::string("1u"));
    CHECK_EQ(normalizujWartosc("LM358"), std::string("lm358"));
    CHECK_EQ(normalizujWartosc("  22R "), std::string("22"));
    CHECK_EQ(normalizujObudowe("Resistor_SMD:R_0805_2012Metric"), std::string("0805"));
    CHECK_EQ(normalizujObudowe("Capacitor_SMD:C_0603_1608Metric"), std::string("0603"));
    CHECK_EQ(normalizujObudowe("SOT-23"), std::string("sot23"));
    CHECK_EQ(normalizujObudowe("R0805"), std::string("0805"));
    CHECK_EQ(normalizujObudowe("C0603K"), std::string("0603"));

    CHECK(porownajNaturalnie("R2", "R10"));
    CHECK(!porownajNaturalnie("R10", "R2"));
    CHECK(porownajNaturalnie("C9", "R1"));

    auto r = rozbijOznaczenia("R1, R2,R5-R7 C3-5");
    CHECK_EQ(r.size(), size_t(8));
    if (r.size() == 8) {
        CHECK_EQ(r[2], std::string("R5"));
        CHECK_EQ(r[4], std::string("R7"));
        CHECK_EQ(r[7], std::string("C5"));
    }
}

static const char* KICAD_CSV =
    "\"Reference\",\"Value\",\"Footprint\",\"Qty\",\"DNP\"\n"
    "\"C1, C2\",\"100nF\",\"Capacitor_SMD:C_0603_1608Metric\",\"2\",\"\"\n"
    "\"R1, R3, R2\",\"10k\",\"Resistor_SMD:R_0805_2012Metric\",\"3\",\"\"\n"
    "\"R4\",\"10k\",\"Resistor_SMD:R_0805_2012Metric\",\"1\",\"DNP\"\n"
    "\"U1\",\"LM358\",\"Package_SO:SOIC-8_3.9x4.9mm_P1.27mm\",\"1\",\"\"\n";

static const char* POLSKI_CSV =
    "Oznaczenia;Wartość;Obudowa;Ilość;Producent;Symbol producenta\r\n"
    "R1-R3;4k7;0805;;Yageo;RC0805FR-074K7L\r\n"
    "C1;1µF;0603;5;;\r\n";

static void testCSVBOM() {
    std::cout << "[bom csv]\n";
    WynikImportuBOM w;
    std::string blad;
    CHECK(ImportBOM::parsujTekst(KICAD_CSV, w, blad));
    CHECK_EQ(w.pozycje.size(), size_t(3));  // C, R (bez DNP), U
    CHECK_EQ(w.liczbaSztuk(), 6);
    const PozycjaBOM* r = znajdz(w, "R1");
    CHECK(r != nullptr);
    if (r) {
        CHECK_EQ(r->oznaczenia, std::string("R1, R2, R3"));  // posortowane naturalnie
        CHECK_EQ(r->ilosc, 3);
        CHECK_EQ(r->wartosc, std::string("10k"));
    }
    CHECK(!w.ostrzezenia.empty());  // informacja o pominiętym DNP

    WynikImportuBOM p;
    CHECK(ImportBOM::parsujTekst(POLSKI_CSV, p, blad));
    CHECK_EQ(p.pozycje.size(), size_t(2));
    r = znajdz(p, "R1");
    CHECK(r != nullptr);
    if (r) {
        CHECK_EQ(r->ilosc, 3);  // z liczby oznaczeń, gdy kolumna Ilość pusta
        CHECK_EQ(r->producent, std::string("Yageo"));
        CHECK_EQ(r->symbol, std::string("RC0805FR-074K7L"));
    }
    r = znajdz(p, "C1");
    CHECK(r != nullptr);
    if (r) CHECK_EQ(r->ilosc, 5);

    WynikImportuBOM zly;
    CHECK(!ImportBOM::parsujTekst("coś;innego\n1;2\n", zly, blad));
    CHECK(!blad.empty());
    CHECK(!ImportBOM::parsujTekst("", zly, blad));
    CHECK(!ImportBOM::parsujTekst("(kicad_sch (version 2023))", zly, blad));
    CHECK(blad.find("netlist") != std::string::npos || blad.find("Netlist") != std::string::npos ||
          blad.find("list") != std::string::npos);
}

static const char* KICAD_NETLISTA =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<export version=\"E\"><design><source>x.kicad_sch</source></design>\n"
    "<components>\n"
    "<comp ref=\"R1\"><value>10k</value><footprint>Resistor_SMD:R_0805_2012Metric</footprint></comp>\n"
    "<comp ref=\"R2\"><value>10k</value><footprint>Resistor_SMD:R_0805_2012Metric</footprint></comp>\n"
    "<comp ref=\"U1\"><value>NE555</value><footprint>Package_DIP:DIP-8_W7.62mm</footprint>\n"
    "  <fields><field name=\"MPN\">NE555P</field><field name=\"Manufacturer\">TI</field></fields></comp>\n"
    "<comp ref=\"TP1\"><value>TP</value><footprint>TestPoint:TestPoint_Pad_D1.0mm</footprint>\n"
    "  <property name=\"exclude_from_bom\"/></comp>\n"
    "<comp ref=\"C9\"><value>1u</value><footprint>Capacitor_SMD:C_0805_2012Metric</footprint>\n"
    "  <property name=\"dnp\"/></comp>\n"
    "</components></export>\n";

static void testKiCadNetlista() {
    std::cout << "[kicad xml]\n";
    WynikImportuBOM w;
    std::string blad;
    CHECK(ImportBOM::parsujTekst(KICAD_NETLISTA, w, blad));
    CHECK_EQ(w.pozycje.size(), size_t(2));
    const PozycjaBOM* r = znajdz(w, "R1");
    CHECK(r != nullptr);
    if (r) {
        CHECK_EQ(r->ilosc, 2);
        CHECK_EQ(r->oznaczenia, std::string("R1, R2"));
    }
    const PozycjaBOM* u = znajdz(w, "U1");
    CHECK(u != nullptr);
    if (u) {
        CHECK_EQ(u->symbol, std::string("NE555P"));
        CHECK_EQ(u->producent, std::string("TI"));
    }
    CHECK(znajdz(w, "TP1") == nullptr);
    CHECK(znajdz(w, "C9") == nullptr);
    CHECK(w.format.find("KiCad") != std::string::npos);
}

static const char* EAGLE_SCH =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<!DOCTYPE eagle SYSTEM \"eagle.dtd\">\n"
    "<eagle version=\"9.6.2\"><drawing><schematic>\n"
    "<libraries>\n"
    "<library name=\"rcl\"><packages/><symbols/><devicesets>\n"
    "  <deviceset name=\"R-EU_\" prefix=\"R\"><devices>\n"
    "    <device name=\"R0805\" package=\"R0805\"/><device name=\"R0603\" package=\"R0603\"/></devices></deviceset>\n"
    "  <deviceset name=\"GND\" prefix=\"\"><devices><device name=\"\"/></devices></deviceset>\n"
    "</devicesets></library>\n"
    "<library name=\"linear\"><devicesets>\n"
    "  <deviceset name=\"LM358\" prefix=\"IC\"><devices><device name=\"D\" package=\"SO08\"/></devices></deviceset>\n"
    "</devicesets></library>\n"
    "</libraries>\n"
    "<parts>\n"
    "<part name=\"R1\" library=\"rcl\" deviceset=\"R-EU_\" device=\"R0805\" value=\"10k\"/>\n"
    "<part name=\"R2\" library=\"rcl\" deviceset=\"R-EU_\" device=\"R0805\" value=\"10k\"/>\n"
    "<part name=\"R3\" library=\"rcl\" deviceset=\"R-EU_\" device=\"R0603\" value=\"4k7\"/>\n"
    "<part name=\"GND1\" library=\"rcl\" deviceset=\"GND\" device=\"\"/>\n"
    "<part name=\"IC1\" library=\"linear\" deviceset=\"LM358\" device=\"D\">\n"
    "  <attribute name=\"MPN\" value=\"LM358DR\"/><attribute name=\"MANUFACTURER\" value=\"TI\"/></part>\n"
    "</parts></schematic></drawing></eagle>\n";

static void testEagle() {
    std::cout << "[eagle]\n";
    WynikImportuBOM w;
    std::string blad;
    CHECK(ImportBOM::parsujTekst(EAGLE_SCH, w, blad));
    CHECK(w.format.find("Eagle") != std::string::npos);
    CHECK_EQ(w.pozycje.size(), size_t(3));  // R 10k 0805 x2, R 4k7 0603, IC1; bez GND
    const PozycjaBOM* r = znajdz(w, "R1");
    CHECK(r != nullptr);
    if (r) {
        CHECK_EQ(r->ilosc, 2);
        CHECK_EQ(r->obudowa, std::string("R0805"));
    }
    const PozycjaBOM* ic = znajdz(w, "IC1");
    CHECK(ic != nullptr);
    if (ic) {
        CHECK_EQ(ic->symbol, std::string("LM358DR"));
        CHECK_EQ(ic->producent, std::string("TI"));
        CHECK_EQ(ic->obudowa, std::string("SO08"));
    }
    CHECK(znajdz(w, "GND") == nullptr);
}

int main() {
    testCSV();
    testXML();
    testNormalizacji();
    testCSVBOM();
    testKiCadNetlista();
    testEagle();
    std::cout << sprawdzenia << " sprawdzeń, " << bledy << " błędów\n";
    return bledy == 0 ? 0 : 1;
}
