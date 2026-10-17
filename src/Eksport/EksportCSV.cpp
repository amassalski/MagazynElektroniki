#include "EksportCSV.h"

#include "../Narzedzia/Liczby.h"

#include <filesystem>
#include <fstream>

namespace EksportCSV {

std::string pole(const std::string& tekst, char separator) {
    const bool trzebaCudzyslow = tekst.find_first_of(std::string("\"\r\n") + separator) != std::string::npos;
    if (!trzebaCudzyslow) return tekst;
    std::string wynik = "\"";
    for (char c : tekst) {
        if (c == '"') wynik += '"';
        wynik += c;
    }
    wynik += '"';
    return wynik;
}

std::string zbuduj(const std::vector<std::string>& naglowki,
                   const std::vector<std::vector<std::string>>& wiersze, char separator) {
    std::string wynik;
    auto dodajWiersz = [&](const std::vector<std::string>& w) {
        for (size_t i = 0; i < w.size(); ++i) {
            if (i) wynik += separator;
            wynik += pole(w[i], separator);
        }
        wynik += "\r\n";
    };
    dodajWiersz(naglowki);
    for (const auto& w : wiersze) dodajWiersz(w);
    return wynik;
}

bool zapisz(const std::string& sciezka, const std::vector<std::string>& naglowki,
            const std::vector<std::vector<std::string>>& wiersze, std::string& blad, char separator) {
    std::ofstream plik(std::filesystem::u8path(sciezka), std::ios::binary | std::ios::trunc);
    if (!plik) {
        blad = "Nie można otworzyć pliku do zapisu: " + sciezka;
        return false;
    }
    plik << "\xEF\xBB\xBF" << zbuduj(naglowki, wiersze, separator);
    plik.flush();
    if (!plik) {
        blad = "Błąd zapisu pliku: " + sciezka;
        return false;
    }
    return true;
}

std::vector<std::string> naglowkiElementow() {
    return {"ID", "Symbol", "Symbol producenta", "Producent", "Opis", "Wartość", "Jednostka",
            "Obudowa", "Kategoria", "Ilość", "Minimum", "Cena", "Waluta", "Tagi", "Datasheet", "Kontener", "Szuflada", "Przegroda", "Tolerancja", "Parametry"};
}

std::vector<std::vector<std::string>> wierszeElementow(const std::vector<Element>& elementy) {
    std::vector<std::vector<std::string>> wiersze;
    wiersze.reserve(elementy.size());
    for (const Element& e : elementy) {
        std::string cena;
        if (e.cena) {
            cena = liczbaNaTekstKlasyczny(*e.cena);
            for (char& c : cena) {
                if (c == '.') c = ',';
            }
        }
        wiersze.push_back({std::to_string(e.id), e.symbol, e.symbolProducenta, e.producent, e.opis,
                           e.wartosc, e.jednostka, e.obudowa, e.kategoriaPelna,
                           std::to_string(e.iloscDostepna), std::to_string(e.iloscMinimalna), cena,
                           e.waluta, e.tagi, e.datasheetUrl, e.kontener, e.szuflada, e.przegroda, e.tolerancja, e.parametry});
    }
    return wiersze;
}

bool zapiszElementy(const std::string& sciezka, const std::vector<Element>& elementy,
                    std::string& blad) {
    return zapisz(sciezka, naglowkiElementow(), wierszeElementow(elementy), blad);
}

} // namespace EksportCSV
