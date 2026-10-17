#include "ParserCSV.h"

namespace ParserCSV {

char wykryjSeparator(const std::string& tekst) {
    size_t start = tekst.compare(0, 3, "\xEF\xBB\xBF") == 0 ? 3 : 0;
    // pierwsza niepusta linia
    while (start < tekst.size() && (tekst[start] == '\r' || tekst[start] == '\n')) ++start;

    int przecinki = 0, srednik = 0, tabulatory = 0;
    bool wCudzyslowie = false;
    for (size_t i = start; i < tekst.size(); ++i) {
        const char c = tekst[i];
        if (c == '"') wCudzyslowie = !wCudzyslowie;
        else if (!wCudzyslowie) {
            if (c == '\n' || c == '\r') break;
            if (c == ',') ++przecinki;
            else if (c == ';') ++srednik;
            else if (c == '\t') ++tabulatory;
        }
    }
    if (tabulatory > przecinki && tabulatory > srednik) return '\t';
    if (srednik > przecinki) return ';';
    return ',';
}

std::vector<Wiersz> parsuj(const std::string& tekst, char separator) {
    if (separator == 0) separator = wykryjSeparator(tekst);

    std::vector<Wiersz> wiersze;
    Wiersz wiersz;
    std::string pole;
    bool wCudzyslowie = false;
    bool polePodane = false; // czy w bieżącym wierszu było cokolwiek (także puste pole w cudzysłowie)

    auto zakonczPole = [&] {
        wiersz.push_back(pole);
        pole.clear();
    };
    auto zakonczWiersz = [&] {
        zakonczPole();
        bool pusty = true;
        for (const std::string& p : wiersz) {
            if (!p.empty()) { pusty = false; break; }
        }
        if (!pusty || polePodane) wiersze.push_back(wiersz);
        wiersz.clear();
        polePodane = false;
    };

    size_t i = tekst.compare(0, 3, "\xEF\xBB\xBF") == 0 ? 3 : 0;
    for (; i < tekst.size(); ++i) {
        const char c = tekst[i];
        if (wCudzyslowie) {
            if (c == '"') {
                if (i + 1 < tekst.size() && tekst[i + 1] == '"') {
                    pole += '"';
                    ++i;
                } else {
                    wCudzyslowie = false;
                }
            } else {
                pole += c;
            }
            continue;
        }
        if (c == '"' && pole.empty()) {
            wCudzyslowie = true;
            polePodane = true;
        } else if (c == separator) {
            zakonczPole();
        } else if (c == '\r') {
            if (i + 1 < tekst.size() && tekst[i + 1] == '\n') ++i;
            zakonczWiersz();
        } else if (c == '\n') {
            zakonczWiersz();
        } else {
            pole += c;
        }
    }
    if (!pole.empty() || !wiersz.empty() || polePodane) zakonczWiersz();
    return wiersze;
}

} // namespace ParserCSV
