#ifndef ODCZYTDATASHEETU_H
#define ODCZYTDATASHEETU_H

#include <string>
#include <vector>

/**
 * Odczyt parametrów z tekstu karty katalogowej. To heurystyka (wyrażenia regularne), więc wyniki
 * są tylko PROPOZYCJAMI - użytkownik potwierdza je przed zapisem do bazy.
 *
 * Co jest rozpoznawane:
 *  - sekcje "Absolute Maximum Ratings" (nazwy parametrów z przedrostkiem "Maks.: ") i
 *    "Electrical Characteristics" (przedrostek "Elektr.: "), w tym hFE tranzystorów;
 *    znane parametry mają polską nazwę i symbol, pozostałe wiersze z liczbą i jednostką
 *    są proponowane ogólnie (pewny == false);
 *  - kilka parametrów ogólnych z całego tekstu (zasilanie, temperatura, prąd, moc...);
 *  - opis zastosowania ("Applications") lub opis ogólny - jako propozycja do pola Uwagi.
 */
namespace OdczytDatasheetu {

struct Propozycja {
    std::string nazwa;     ///< np. "Napięcie zasilania"
    std::string wartosc;   ///< np. "2.7 V ... 5.5 V"
    std::string fragment;  ///< linia tekstu, z której wzięto wartość (do sprawdzenia)
    bool pewny = true;     ///< false = ogólne zgadywanie (parametr spoza znanej listy) - domyślnie nie zaznaczany
};

struct Wynik {
    std::string opis;                  ///< propozycja opisu (pierwsza sensowna linia)
    std::string obudowa;               ///< propozycja obudowy
    std::string uwagi;                 ///< propozycja do pola Uwagi (opis zastosowania lub ogólny opis układu)
    std::vector<Propozycja> parametry; ///< pozostałe parametry
};

/// Wyciąga propozycje z tekstu (np. z pdftotext)
Wynik wyciagnij(const std::string& tekst);

/// Tekst z pliku PDF przez zewnętrzny program pdftotext (poppler). Pusty wynik + @p blad, gdy się nie da.
std::string tekstZPDF(const std::string& sciezka, std::string& blad, int maxStron = 10);

} // namespace OdczytDatasheetu

#endif
