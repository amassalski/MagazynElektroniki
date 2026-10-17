#ifndef PARSERSTRONY_H
#define PARSERSTRONY_H

#include <string>
#include <vector>

/// Cena znaleziona na stronie sklepu
struct WynikCeny {
    bool znaleziono = false;
    double cena = 0;
    std::string waluta;        ///< "" gdy strona jej nie podaje (wtedy waluta sklepu)
    std::string dostepnosc;    ///< po polsku albo tekst ze strony; może być pusta
    std::string zrodlo;        ///< "wzorzec sklepu", "json-ld", "meta", "itemprop"
    std::string blad;          ///< np. błędny wzorzec sklepu
};

/// Odnośnik do karty katalogowej
struct LinkDatasheet {
    std::string url;
    std::string tekst;
    int ocena = 0;             ///< im więcej, tym pewniejszy (do sortowania)
};

/// Wyciąganie danych z HTML (bez sieci i wxWidgets)
namespace ParserStrony {

/**
 * Szuka ceny w kolejności: własny wzorzec sklepu (pierwsza grupa przechwytująca), dane strukturalne JSON-LD
 * (schema.org Offer), znaczniki meta (product:price:amount, og:price:amount), atrybuty itemprop="price".
 * Nie zgaduje ceny z przypadkowych liczb na stronie.
 */
WynikCeny znajdzCene(const std::string& html, const std::string& wzorzecSklepu = "");

/// Liczba z tekstu ceny: "1 234,56 zł", "1,234.56", "12.5", "0,0123"
bool liczbaZCeny(const std::string& tekst, double& wynik);

/// Linki do PDF-ów i odnośniki "datasheet"/"karta katalogowa", najpewniejsze pierwsze, bez powtórzeń
std::vector<LinkDatasheet> znajdzDatasheety(const std::string& html, const std::string& adresStrony);

/// Adres względny -> bezwzględny względem adresu strony
std::string rozwiazAdres(const std::string& adresStrony, const std::string& href);

/// &amp; &quot; &#39; &#x27; &nbsp; itd.
std::string odkodujEncje(const std::string& tekst);

/// Dostępność schema.org ("https://schema.org/InStock") po polsku; inne teksty bez zmian
std::string nazwaDostepnosci(const std::string& wartosc);

} // namespace ParserStrony

#endif // PARSERSTRONY_H
