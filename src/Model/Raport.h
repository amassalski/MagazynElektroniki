#ifndef RAPORT_H
#define RAPORT_H

#include <string>
#include <vector>

/// Gotowy raport: tabela tekstowa (wartości sformatowane po polsku) z podsumowaniem
struct Raport {
    std::string tytul;
    std::string opis;                              ///< np. zakres dat
    std::vector<std::string> naglowki;
    std::vector<bool> wyrownajDoPrawej;            ///< równolegle do nagłówków (kolumny liczbowe)
    std::vector<std::vector<std::string>> wiersze;
    std::vector<std::string> podsumowanie;         ///< linie pod tabelą, np. "Wartość razem: 1 234,50 PLN"
    bool ucieto = false;                           ///< wynik przycięty do limitu wierszy
};

enum class RodzajRaportu {
    StanMagazynu,
    NiskieStany,
    WartoscWgKategorii,
    BrakiWProjektach,
    HistoriaZmian
};

#endif // RAPORT_H
