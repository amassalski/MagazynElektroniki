// Testy parserów stron sklepów (JSON, cena, datasheet) - bez sieci
#include "Wyszukiwanie/MiniJSON.h"
#include "Wyszukiwanie/ParserStrony.h"

#include <cmath>
#include <iostream>

static int sprawdzenia = 0, bledy = 0;
#define CHECK(w) do { ++sprawdzenia; if (!(w)) { ++bledy; std::cerr << "BŁĄD " << __FILE__ << ":" << __LINE__ << ": " #w "\n"; } } while (0)
static bool bl(double a, double b) { return std::fabs(a - b) < 1e-9; }

static double liczba(const std::string& t, bool* ok = nullptr) {
    double v = -1;
    const bool o = ParserStrony::liczbaZCeny(t, v);
    if (ok) *ok = o;
    return o ? v : -1;
}

int main() {
    using namespace ParserStrony;

    // --- JSON ---
    JsonWartosc j; std::string blad;
    CHECK(MiniJSON::parsuj(R"({"a":[1,2.5,-3e2,true,null,"xą\n\"y"],"b":{"c":"d"}})", j, blad));
    CHECK(j.pole("a") && j.pole("a")->elementy.size() == 6 && bl(j.pole("a")->elementy[2].liczba, -300));
    CHECK(j.pole("a")->elementy[5].tekst == "xą\n\"y" && j.pole("b")->pole("c")->tekst == "d");
    CHECK(MiniJSON::parsuj("\"\\ud83d\\ude00\"", j, blad) && j.tekst == "\xF0\x9F\x98\x80");
    CHECK(!MiniJSON::parsuj("{\"a\":", j, blad) && !blad.empty());
    CHECK(!MiniJSON::parsuj("{\"a\":1} x", j, blad));
    CHECK(!MiniJSON::parsuj("[1,2", j, blad));
    CHECK(!MiniJSON::parsuj("{a:1}", j, blad));
    CHECK(!MiniJSON::parsuj(std::string(100, '[') + std::string(100, ']'), j, blad));  // za głęboko
    CHECK(MiniJSON::parsuj("[]", j, blad) && j.czyTablica() && j.elementy.empty());
    CHECK(MiniJSON::parsuj("  {}  ", j, blad) && j.czyObiekt());

    // --- liczby cen ---
    CHECK(bl(liczba("12,50 zł"), 12.5) && bl(liczba("12.5"), 12.5) && bl(liczba("1 234,56 zł"), 1234.56));
    CHECK(bl(liczba("1,234.56"), 1234.56) && bl(liczba("1.234,56"), 1234.56) && bl(liczba("0,0123"), 0.0123));
    CHECK(bl(liczba("1,234"), 1234) && bl(liczba("0,123"), 0.123) && bl(liczba("0.123"), 0.123) && bl(liczba("12,345"), 12345));
    CHECK(bl(liczba("1\xC2\xA0" "234,00 PLN"), 1234) && bl(liczba("$3.99"), 3.99) && bl(liczba("€ 5"), 5));
    CHECK(bl(liczba("1,234,567"), 1234567) && bl(liczba("cena: 7,"), 7));
    bool ok = true;
    liczba("", &ok); CHECK(!ok);
    liczba("brak ceny", &ok); CHECK(!ok);
    liczba(",", &ok); CHECK(!ok);

    // --- encje ---
    CHECK(odkodujEncje("a &amp; b &quot;c&quot; &#39;d&#39; &#x142; &nbsp;e &nieznana; &") == "a & b \"c\" 'd' ł  e &nieznana; &");
    CHECK(nazwaDostepnosci("https://schema.org/InStock") == "Dostępny" && nazwaDostepnosci("http://schema.org/OutOfStock") == "Niedostępny");
    CHECK(nazwaDostepnosci(" Na magazynie ") == "Na magazynie");

    // --- cena: JSON-LD ---
    const std::string ld = R"(<html><head><script type="application/ld+json">
      {"@context":"https://schema.org","@type":"Product","name":"BC547",
       "offers":{"@type":"Offer","price":"0,35","priceCurrency":"PLN","availability":"https://schema.org/InStock"}}
      </script></head><body><span class="price">99,99</span></body></html>)";
    WynikCeny c = znajdzCene(ld);
    CHECK(c.znaleziono && bl(c.cena, 0.35) && c.waluta == "PLN" && c.dostepnosc == "Dostępny" && c.zrodlo == "json-ld");

    // @graph, kilka ofert -> najniższa, AggregateOffer
    c = znajdzCene(R"(<script type='application/ld+json'>{"@graph":[{"@type":"WebPage"},{"@type":"Product",
        "offers":[{"price":4.5,"priceCurrency":"EUR"},{"price":3.2,"priceCurrency":"EUR"},{"price":"x"}]}]}</script>)");
    CHECK(c.znaleziono && bl(c.cena, 3.2) && c.waluta == "EUR");
    c = znajdzCene(R"(<script type="application/ld+json">[{"@type":"Product","offers":{"@type":"AggregateOffer","lowPrice":"1.10","highPrice":"2","priceCurrency":"USD"}}]</script>)");
    CHECK(c.znaleziono && bl(c.cena, 1.1) && c.waluta == "USD");
    // uszkodzony JSON-LD jest pomijany, dalej szuka w meta
    c = znajdzCene(R"(<script type="application/ld+json">{"offers":</script><meta property="product:price:amount" content="7.77"><meta property="product:price:currency" content="PLN">)");
    CHECK(c.znaleziono && bl(c.cena, 7.77) && c.zrodlo == "meta" && c.waluta == "PLN");
    c = znajdzCene(R"(<meta content="2,50" property="og:price:amount"/>)");
    CHECK(c.znaleziono && bl(c.cena, 2.5));

    // --- itemprop ---
    c = znajdzCene(R"(<div itemprop="offers"><meta itemprop="priceCurrency" content="PLN"><span itemprop="price" content="12.30">12,30 zł</span>
                    <link itemprop="availability" href="https://schema.org/OutOfStock"></div>)");
    CHECK(c.znaleziono && bl(c.cena, 12.3) && c.zrodlo == "itemprop" && c.waluta == "PLN" && c.dostepnosc == "Niedostępny");
    c = znajdzCene(R"(<span itemprop=price>1 299,00&nbsp;zł</span>)");
    CHECK(c.znaleziono && bl(c.cena, 1299));

    // --- skrypty i komentarze nie udają ceny ---
    c = znajdzCene(R"(<!-- <meta property="og:price:amount" content="1"> --><script>var x = '<meta property="og:price:amount" content="2">';</script>)");
    CHECK(!c.znaleziono);
    CHECK(!znajdzCene("<html><body>Cena: 12,50 zł</body></html>").znaleziono);   // nie zgadujemy
    CHECK(!znajdzCene("").znaleziono);

    // --- własny wzorzec sklepu ma pierwszeństwo ---
    const std::string sklep = R"(<span class="cena-brutto">3,99&nbsp;zł</span><script type="application/ld+json">{"offers":{"price":"100"}}</script>)";
    c = znajdzCene(sklep, R"(cena-brutto">([0-9 ,.]+))");
    CHECK(c.znaleziono && bl(c.cena, 3.99) && c.zrodlo == "wzorzec sklepu");
    c = znajdzCene(sklep, "nie-ma-takiego(.*)");
    CHECK(c.znaleziono && bl(c.cena, 100) && c.zrodlo == "json-ld");                // wzorzec nie pasuje: zapasowo dane strukturalne
    c = znajdzCene("<b>nic</b>", "(zły");
    CHECK(!c.znaleziono && !c.blad.empty());
    c = znajdzCene("<b>cena abc</b>", "cena (\\w+)");
    CHECK(!c.znaleziono && c.blad.find("nie jest ceną") != std::string::npos);
    c = znajdzCene("<b>cena 5</b>", "cena 5");                                        // wzorzec bez grupy
    CHECK(!c.znaleziono && !c.blad.empty());

    // --- adresy ---
    const std::string s = "https://sklep.pl/produkty/rezystory/r10k.html?x=1#a";
    CHECK(rozwiazAdres(s, "https://inny.pl/a.pdf") == "https://inny.pl/a.pdf");
    CHECK(rozwiazAdres(s, "//cdn.pl/a.pdf") == "https://cdn.pl/a.pdf");
    CHECK(rozwiazAdres(s, "/pliki/a.pdf") == "https://sklep.pl/pliki/a.pdf");
    CHECK(rozwiazAdres(s, "a.pdf") == "https://sklep.pl/produkty/rezystory/a.pdf");
    CHECK(rozwiazAdres(s, "../a.pdf?v=2") == "https://sklep.pl/produkty/a.pdf?v=2");
    CHECK(rozwiazAdres(s, "./b/../a.pdf") == "https://sklep.pl/produkty/rezystory/a.pdf");
    CHECK(rozwiazAdres("https://sklep.pl", "a.pdf") == "https://sklep.pl/a.pdf");
    CHECK(rozwiazAdres(s, "javascript:void(0)").empty() && rozwiazAdres(s, "#top").empty() && rozwiazAdres(s, "mailto:a@b.c").empty());
    CHECK(rozwiazAdres("brak", "a.pdf").empty());

    // --- datasheety ---
    const std::string strona = R"(<html><body>
        <a href="/kontakt">Kontakt</a>
        <a href="/pliki/bc547.pdf">Instrukcja</a>
        <a href="https://cdn.pl/ds/BC547_datasheet.PDF?v=3" title="x">Pobierz</a>
        <a href="/download?id=7">Karta katalogowa</a>
        <a href="/pliki/bc547.pdf">Instrukcja drugi raz</a>
        <a href='javascript:void(0)'>Datasheet</a>
        <a href="/zdjecie.jpg">foto</a>
        <!-- <a href="/ukryty.pdf">x</a> -->
        </body></html>)";
    auto d = znajdzDatasheety(strona, "https://sklep.pl/p/bc547");
    CHECK(d.size() == 3);
    CHECK(d.size() >= 3 && d[0].url == "https://cdn.pl/ds/BC547_datasheet.PDF?v=3" && d[0].ocena > d[1].ocena);
    CHECK(d.size() >= 3 && d[1].url == "https://sklep.pl/pliki/bc547.pdf" && d[2].url == "https://sklep.pl/download?id=7");
    CHECK(znajdzDatasheety("<p>nic</p>", "https://x.pl/").empty());

    std::cout << sprawdzenia << " sprawdzeń, " << bledy << " błędów\n";
    return bledy == 0 ? 0 : 1;
}
