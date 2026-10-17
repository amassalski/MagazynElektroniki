#include "Liczby.h"

#include <cstdio>

#include <iomanip>
#include <locale>
#include <sstream>

std::string liczbaNaTekstKlasyczny(double wartosc) {
    std::ostringstream strumien;
    strumien.imbue(std::locale::classic());
    strumien << std::setprecision(12) << wartosc;
    return strumien.str();
}

bool tekstNaLiczbe(const std::string& tekst, double& wartosc) {
    std::string t;
    for (size_t i = 0; i < tekst.size(); ++i) {
        const char c = tekst[i];
        if (c == ' ' || c == '\t') continue;
        // spacja niełamliwa (UTF-8: C2 A0)
        if (c == '\xC2' && i + 1 < tekst.size() && tekst[i + 1] == '\xA0') {
            ++i;
            continue;
        }
        t += (c == ',') ? '.' : c;
    }
    if (t.empty()) return false;
    std::istringstream strumien(t);
    strumien.imbue(std::locale::classic());
    double v = 0.0;
    strumien >> v;
    if (strumien.fail() || !strumien.eof()) return false;
    wartosc = v;
    return true;
}

std::string liczbaNaTekstPolski(double wartosc, int maxMiejsc, int minMiejsc) {
    char b[64];
    std::snprintf(b, sizeof b, "%.*f", maxMiejsc, wartosc);
    std::string s = b;
    const size_t k = s.find('.');
    if (k != std::string::npos) {
        size_t koniec = s.size();
        while (koniec > k + 1 + static_cast<size_t>(minMiejsc) && s[koniec - 1] == '0') --koniec;
        s.resize(koniec);
        if (s.back() == '.') s.pop_back();
        const size_t p = s.find('.');
        if (p != std::string::npos) s[p] = ',';
    }
    return s;
}
