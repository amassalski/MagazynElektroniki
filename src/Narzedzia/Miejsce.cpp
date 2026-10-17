#include "Miejsce.h"

#include <cctype>
#include <regex>
#include <vector>

namespace {

std::string przytnij(const std::string& s) {
    const char* sp = " \t\r\n";
    const size_t a = s.find_first_not_of(sp);
    if (a == std::string::npos) return std::string();
    return s.substr(a, s.find_last_not_of(sp) - a + 1);
}

/// Przycina też końcowe przecinki i średniki
std::string przytnijSeparatory(std::string s) {
    s = przytnij(s);
    while (!s.empty() && (s.back() == ',' || s.back() == ';' || s.back() == '-')) {
        s.pop_back();
        s = przytnij(s);
    }
    return s;
}

/// "A1/3" -> szuflada "A1", przegroda "3"
void ustawSzuflade(MiejsceMagazynowe& m, const std::string& tekst) {
    const std::string t = przytnij(tekst);
    const size_t uk = t.find('/');
    if (uk == std::string::npos) {
        m.szuflada = t;
    } else {
        m.szuflada = przytnij(t.substr(0, uk));
        m.przegroda = przytnij(t.substr(uk + 1));
    }
}

} // namespace

std::string MiejsceMagazynowe::naTekst() const {
    std::string szuf = szuflada;
    if (!przegroda.empty()) szuf += "/" + przegroda;
    if (kontener.empty()) return szuf;
    if (szuf.empty()) return kontener;
    return kontener + ", " + szuf;
}

MiejsceMagazynowe MiejsceMagazynowe::parsuj(const std::string& wejscie) {
    MiejsceMagazynowe m;
    const std::string t = przytnij(wejscie);
    if (t.empty()) return m;

    // 1. słowo "szuflada/szufladka/szuf." przed oznaczeniem szuflady
    static const std::regex zeSlowem(
        R"(^(.*?)[\s,;]*\bszuf\w*\.?\s*(\S+(?:\s*/\s*\S+)?)$)", std::regex::icase);
    // 2. ostatni człon wygląda jak oznaczenie szuflady: A1, B12, A1/3, 7/2
    static const std::regex oznaczenie(R"(^[A-Za-z]{1,3}\d{1,3}(/\S+)?$|^\d{1,3}/\S+$)");

    std::smatch sm;
    if (std::regex_match(t, sm, zeSlowem)) {
        m.kontener = przytnijSeparatory(sm[1].str());
        ustawSzuflade(m, sm[2].str());
        return m;
    }

    // 2. przecinek lub średnik: "Kontener 1, A1/1"
    const size_t sep = t.find_last_of(",;");
    if (sep != std::string::npos) {
        m.kontener = przytnijSeparatory(t.substr(0, sep));
        ustawSzuflade(m, t.substr(sep + 1));
        return m;
    }

    // 3. ścieżka z ukośnikami: "K2/B3/4" (kontener/szuflada/przegroda) albo "A1/1"
    if (t.find(' ') == std::string::npos && t.find('/') != std::string::npos) {
        std::vector<std::string> czesci;
        size_t start = 0;
        while (true) {
            const size_t uk = t.find('/', start);
            czesci.push_back(t.substr(start, uk == std::string::npos ? std::string::npos : uk - start));
            if (uk == std::string::npos) break;
            start = uk + 1;
        }
        if (czesci.size() >= 3) {
            m.kontener = czesci[0];
            m.szuflada = czesci[1];
            for (size_t i = 2; i < czesci.size(); ++i) m.przegroda += (i > 2 ? "/" : "") + czesci[i];
            return m;
        }
        ustawSzuflade(m, t);
        return m;
    }

    // 4. jedno słowo: samo oznaczenie szuflady (A1) albo nazwa kontenera
    const size_t ost = t.find_last_of(" \t");
    if (ost == std::string::npos) {
        if (std::regex_match(t, oznaczenie)) m.szuflada = t; else m.kontener = t;
        return m;
    }

    // 5. "Kontener 1 A1": ostatnie słowo jest oznaczeniem szuflady
    const std::string ogon = t.substr(ost + 1);
    if (std::regex_match(ogon, oznaczenie)) {
        m.kontener = przytnij(t.substr(0, ost));
        ustawSzuflade(m, ogon);
        return m;
    }

    m.kontener = t;
    return m;
}
