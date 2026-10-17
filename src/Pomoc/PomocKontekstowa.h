#ifndef POMOCKONTEKSTOWA_H
#define POMOCKONTEKSTOWA_H

#include "../BazaDanych/ManagerBazyDanych.h"

#include <map>
#include <string>
#include <vector>

/// Opis jednego pola formularza (dymek, F1)
struct WpisPomocy {
    std::string tytul;
    std::string opis;
    std::string kotwica;   ///< miejsce w pliku pomocy (bez #)
};

/**
 * @class PomocKontekstowa
 * @brief Słownik opisów pól: klucz (np. "element.symbol") -> tytuł, opis, kotwica w pomocy.
 *
 * Źródła, w kolejności ładowania: plik pomoc/pola.txt (działa bez bazy), potem tabela
 * tbl_pomoc_pol (nadpisuje wpisy z pliku, więc opisy można poprawiać w bazie).
 */
class PomocKontekstowa {
public:
    static PomocKontekstowa& pobierz();

    void wyczysc();

    /// Wczytuje tekst w formacie "klucz TAB tytuł TAB opis TAB kotwica"; zwraca liczbę wczytanych wpisów.
    /// Wiersze z # i puste są pomijane, błędne trafiają do @p bledy ("wiersz N: ...").
    int wczytajZTekstu(const std::string& tekst, std::vector<std::string>* bledy = nullptr);
    int wczytajZPliku(const std::string& sciezka, std::vector<std::string>* bledy = nullptr);
    /// Nadpisuje wpisy tym, co jest w tabeli tbl_pomoc_pol; -1 gdy brak połączenia lub tabeli
    int wczytajZBazy(ManagerBazyDanych& manager);

    const WpisPomocy* znajdz(const std::string& klucz) const;
    size_t liczba() const { return wpisy_.size(); }

private:
    std::map<std::string, WpisPomocy> wpisy_;
};

#endif
