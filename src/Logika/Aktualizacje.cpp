#include "Aktualizacje.h"

#include <cctype>
#include <sstream>

namespace Aktualizacje {

std::string adresWersji() {
    // HEAD = gałąź domyślna repozytorium (main albo master)
    return "https://raw.githubusercontent.com/amassalski/Develop/HEAD/DomowyMagazyn/wersja.txt";
}

std::string adresStronyPobierania() { return "https://github.com/amassalski/Develop/tree/HEAD/DomowyMagazyn"; }

static std::string przytnij(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::optional<std::vector<int>> parsujNumer(const std::string& tekst) {
    std::string t = przytnij(tekst);
    if (!t.empty() && (t[0] == 'v' || t[0] == 'V')) t.erase(0, 1);
    if (t.empty()) return std::nullopt;
    std::vector<int> w;
    size_t i = 0;
    while (i < t.size()) {
        if (!std::isdigit(static_cast<unsigned char>(t[i]))) return std::nullopt;
        long n = 0;
        size_t cyfr = 0;
        while (i < t.size() && std::isdigit(static_cast<unsigned char>(t[i]))) {
            n = n * 10 + (t[i] - '0');
            if (++cyfr > 6) return std::nullopt;
            ++i;
        }
        w.push_back(static_cast<int>(n));
        if (i < t.size()) {
            if (t[i] != '.') return std::nullopt;
            ++i;
            if (i >= t.size()) return std::nullopt;   // "1.2." nie jest numerem
        }
    }
    return w.size() > 4 ? std::nullopt : std::optional<std::vector<int>>(w);
}

int porownaj(const std::vector<int>& a, const std::vector<int>& b) {
    const size_t n = a.size() > b.size() ? a.size() : b.size();
    for (size_t i = 0; i < n; ++i) {
        const int x = i < a.size() ? a[i] : 0, y = i < b.size() ? b[i] : 0;
        if (x != y) return x < y ? -1 : 1;
    }
    return 0;
}

bool nowsza(const std::string& dostepna, const std::string& biezaca) {
    const auto a = parsujNumer(dostepna), b = parsujNumer(biezaca);
    return a && b && porownaj(*a, *b) > 0;
}

std::optional<Informacja> parsujPlikWersji(const std::string& tresc) {
    std::string t = tresc;
    if (t.size() >= 3 && static_cast<unsigned char>(t[0]) == 0xEF && static_cast<unsigned char>(t[1]) == 0xBB &&
        static_cast<unsigned char>(t[2]) == 0xBF)
        t.erase(0, 3);
    std::istringstream s(t);
    std::string linia;
    Informacja info;
    bool pierwsza = true;
    while (std::getline(s, linia)) {
        linia = przytnij(linia);
        if (pierwsza) {
            if (linia.empty()) continue;                   // puste linie na początku pomijamy
            const auto n = parsujNumer(linia);
            if (!n) return std::nullopt;
            info.wersja = linia[0] == 'v' || linia[0] == 'V' ? linia.substr(1) : linia;
            pierwsza = false;
            continue;
        }
        if (linia.empty()) { if (!info.opis.empty()) info.opis += "\n"; continue; }
        if (info.adresPobierania.empty() && (linia.rfind("http://", 0) == 0 || linia.rfind("https://", 0) == 0)) {
            info.adresPobierania = linia;
            continue;
        }
        if (!info.opis.empty() && info.opis.back() != '\n') info.opis += "\n";
        info.opis += linia;
    }
    if (pierwsza) return std::nullopt;
    info.opis = przytnij(info.opis);
    return info;
}

} // namespace Aktualizacje
