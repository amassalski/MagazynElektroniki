#ifndef KATEGORIA_H
#define KATEGORIA_H

#include <optional>
#include <string>

/// Kategoria elementów (węzeł drzewa kategorii)
struct Kategoria {
    long long id = 0;
    std::optional<long long> idRodzica;   ///< brak = kategoria główna
    std::string nazwa;
    std::string opis;
    int poziom = 0;                       ///< 0 = główna
    int liczbaElementow = 0;              ///< elementy bezpośrednio w tej kategorii
    int liczbaElementowRazem = 0;         ///< z podkategoriami (liczone w programie)
    std::string pelnaNazwa;               ///< "Elementy bierne / Rezystory" (liczone w programie)
};

#endif // KATEGORIA_H
