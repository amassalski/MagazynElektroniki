#ifndef MINIJSON_H
#define MINIJSON_H

#include <string>
#include <utility>
#include <vector>

/// Minimalna wartość JSON (do danych strukturalnych ze stron sklepów)
struct JsonWartosc {
    enum class Typ { Null, Bool, Liczba, Tekst, Tablica, Obiekt };
    Typ typ = Typ::Null;
    bool logiczna = false;
    double liczba = 0;
    std::string tekst;
    std::vector<JsonWartosc> elementy;                          ///< dla tablicy
    std::vector<std::pair<std::string, JsonWartosc>> pola;      ///< dla obiektu, w kolejności z tekstu

    bool czyObiekt() const { return typ == Typ::Obiekt; }
    bool czyTablica() const { return typ == Typ::Tablica; }
    /// Pole obiektu albo nullptr
    const JsonWartosc* pole(const std::string& nazwa) const;
    /// Tekst lub liczba jako tekst ("" dla innych)
    std::string jakoTekst() const;
};

namespace MiniJSON {
/// Zwraca false przy błędzie składni (blad opisuje miejsce). Głębokość ograniczona do 64.
bool parsuj(const std::string& tekst, JsonWartosc& wynik, std::string& blad);
}

#endif // MINIJSON_H
