#include "ImportKategorii.h"

#include "../Logika/LogikaMagazynu.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>

namespace ImportKategorii {

namespace {
std::string przytnij(const std::string& s) {
    const char* spacje = " \t\r\n";
    const size_t a = s.find_first_not_of(spacje);
    if (a == std::string::npos) return std::string();
    return s.substr(a, s.find_last_not_of(spacje) - a + 1);
}
} // namespace

std::vector<std::vector<std::string>> parsuj(const std::string& tekst) {
    std::string t = tekst;
    if (t.compare(0, 3, "\xEF\xBB\xBF") == 0) t.erase(0, 3);

    std::vector<std::vector<std::string>> wynik;
    std::istringstream strumien(t);
    std::string linia;
    while (std::getline(strumien, linia)) {
        linia = przytnij(linia);
        if (linia.empty() || linia[0] == '#') continue;

        std::vector<std::string> sciezka;
        std::string nazwa;
        for (size_t i = 0; i < linia.size(); ++i) {
            const char c = linia[i];
            // ">" zawsze rozdziela; "/" tylko ze spacjami wokół (żeby "ADC/DAC" zostało nazwą)
            const bool ukosnik = c == '/' && i > 0 && i + 1 < linia.size() &&
                                 (linia[i - 1] == ' ' || linia[i - 1] == '\t') &&
                                 (linia[i + 1] == ' ' || linia[i + 1] == '\t');
            if (c == '>' || ukosnik) {
                nazwa = przytnij(nazwa);
                if (!nazwa.empty()) sciezka.push_back(nazwa);
                nazwa.clear();
            } else {
                nazwa += c;
            }
        }
        nazwa = przytnij(nazwa);
        if (!nazwa.empty()) sciezka.push_back(nazwa);
        if (!sciezka.empty()) wynik.push_back(std::move(sciezka));
    }
    return wynik;
}

bool importujZTekstu(LogikaMagazynu& logika, const std::string& tekst, Wynik& wynik,
                     std::string& blad) {
    wynik = Wynik();
    const auto sciezki = parsuj(tekst);
    wynik.sciezek = static_cast<int>(sciezki.size());
    if (sciezki.empty()) {
        blad = "Plik nie zawiera żadnych kategorii.";
        return false;
    }
    if (!logika.dodajSciezkiKategorii(sciezki, &wynik.dodano)) {
        blad = logika.pobierzBlad();
        return false;
    }
    return true;
}

bool importujZPliku(LogikaMagazynu& logika, const std::string& sciezka, Wynik& wynik,
                    std::string& blad) {
    std::ifstream plik(std::filesystem::u8path(sciezka), std::ios::binary);
    if (!plik) {
        blad = "Nie można otworzyć pliku: " + sciezka;
        return false;
    }
    const std::string tekst((std::istreambuf_iterator<char>(plik)), std::istreambuf_iterator<char>());
    return importujZTekstu(logika, tekst, wynik, blad);
}

} // namespace ImportKategorii
