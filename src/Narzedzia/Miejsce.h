#ifndef MIEJSCE_H
#define MIEJSCE_H

#include <string>

/**
 * @struct MiejsceMagazynowe
 * @brief Gdzie leży element: kontener, szuflada (np. "A1") i opcjonalna przegroda (np. "1").
 *
 * Zapis tekstowy: "Kontener 1, A1/1" (kontener, szuflada/przegroda). Każda część jest opcjonalna,
 * więc może być sama szuflada ("A1") albo szuflada z przegrodą ("A1/1").
 */
struct MiejsceMagazynowe {
    std::string kontener;
    std::string szuflada;
    std::string przegroda;

    bool puste() const { return kontener.empty() && szuflada.empty() && przegroda.empty(); }

    /// "Kontener 1, A1/1", "A1", "Kontener 1", "A1/1"; pusty tekst, gdy nic nie podano
    std::string naTekst() const;

    /// Rozpoznaje zapis swobodny, m.in.:
    ///   "Kontener 1 szufladka A1/1", "Kontener 1, A1/1", "Kontener 1 A1", "A1/1", "A1", "K2/B3/4"
    /// Nierozpoznany tekst trafia w całości do kontenera.
    static MiejsceMagazynowe parsuj(const std::string& tekst);
};

#endif // MIEJSCE_H
