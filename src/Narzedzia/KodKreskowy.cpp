#include "KodKreskowy.h"

#include <algorithm>
#include <cstdlib>

namespace KodKreskowy {

namespace {

const char* const L_KOD[10] = {"0001101", "0011001", "0010011", "0111101", "0100011",
                               "0110001", "0101111", "0111011", "0110111", "0001011"};
const char* const G_KOD[10] = {"0100111", "0110011", "0011011", "0100001", "0011101",
                               "0111001", "0000101", "0010001", "0001001", "0010111"};
const char* const R_KOD[10] = {"1110010", "1100110", "1101100", "1000010", "1011100",
                               "1001110", "1010000", "1000100", "1001000", "1110100"};
const char* const PARZYSTOSC[10] = {"LLLLLL", "LLGLGG", "LLGGLG", "LLGGGL", "LGLLGG",
                                    "LGGLLG", "LGGGLL", "LGLGLG", "LGLGGL", "LGGLGL"};

// Szerokości pasków i przerw dla wartości 0..106 (106 = znak stop, 7 elementów)
const char* const CODE128[107] = {
    "212222", "222122", "222221", "121223", "121322", "131222", "122213", "122312", "132212", "221213",
    "221312", "231212", "112232", "122132", "122231", "113222", "123122", "123221", "223211", "221132",
    "221231", "213212", "223112", "312131", "311222", "321122", "321221", "312212", "322112", "322211",
    "212123", "212321", "232121", "111323", "131123", "131321", "112313", "132113", "132311", "211313",
    "231113", "231311", "112133", "112331", "132131", "113123", "113321", "133121", "313121", "211331",
    "231131", "213113", "213311", "213131", "311123", "311321", "331121", "312113", "312311", "332111",
    "314111", "221411", "431111", "111224", "111422", "121124", "121421", "141122", "141221", "112214",
    "112412", "122114", "122411", "142112", "142211", "241211", "221114", "413111", "241112", "134111",
    "111242", "121142", "121241", "114212", "124112", "124211", "411212", "421112", "421211", "212141",
    "214121", "412121", "111143", "111341", "131141", "114113", "114311", "411113", "411311", "113141",
    "114131", "311141", "411131", "211412", "211214", "211232", "2331112"};

void dodaj(std::vector<bool>& m, const char* wzor) {
    for (const char* p = wzor; *p; ++p) m.push_back(*p == '1');
}

bool same_cyfry(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
    }
    return true;
}

} // namespace

Typ typZNazwy(const std::string& nazwa) {
    std::string n;
    for (char c : nazwa) n += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (n == "ean8") return Typ::EAN8;
    if (n == "code128") return Typ::Code128;
    return Typ::EAN13;
}

char cyfraKontrolna(const std::string& cyfry) {
    // wagi 3,1,3,1... liczone od prawej strony (cyfry bez sumy kontrolnej)
    int suma = 0;
    bool trzy = true;
    for (size_t i = cyfry.size(); i-- > 0;) {
        suma += (cyfry[i] - '0') * (trzy ? 3 : 1);
        trzy = !trzy;
    }
    return static_cast<char>('0' + (10 - suma % 10) % 10);
}

bool poprawnyEAN(const std::string& kod) {
    if ((kod.size() != 13 && kod.size() != 8) || !same_cyfry(kod)) return false;
    return cyfraKontrolna(kod.substr(0, kod.size() - 1)) == kod.back();
}

std::string tekstKoduElementu(Typ typ, long long id, int prefiks) {
    if (id <= 0) return std::string();
    std::string cyfry = std::to_string(id);
    if (typ == Typ::EAN8) {
        if (cyfry.size() > 7) return std::string();
        cyfry = std::string(7 - cyfry.size(), '0') + cyfry;
        return cyfry + cyfraKontrolna(cyfry);
    }
    if (cyfry.size() > 10) return std::string();
    prefiks = std::max(0, std::min(99, prefiks));
    std::string p = std::to_string(prefiks);
    if (p.size() < 2) p = "0" + p;
    const std::string body = p + std::string(10 - cyfry.size(), '0') + cyfry;
    return body + cyfraKontrolna(body);
}

std::optional<long long> idZKodu(const std::string& tekst, int prefiks) {
    if (!poprawnyEAN(tekst)) return std::nullopt;
    long long id = 0;
    if (tekst.size() == 8) {
        id = std::atoll(tekst.substr(0, 7).c_str());
    } else {
        prefiks = std::max(0, std::min(99, prefiks));
        const int p = std::atoi(tekst.substr(0, 2).c_str());
        if (p != prefiks) return std::nullopt;
        id = std::atoll(tekst.substr(2, 10).c_str());
    }
    if (id <= 0) return std::nullopt;
    return id;
}

std::vector<bool> modulyEAN(const std::string& kod) {
    std::vector<bool> m;
    if (!poprawnyEAN(kod)) return m;
    dodaj(m, "101");
    if (kod.size() == 13) {
        const char* parz = PARZYSTOSC[kod[0] - '0'];
        for (int i = 0; i < 6; ++i) {
            const int cyfra = kod[static_cast<size_t>(i) + 1] - '0';
            dodaj(m, parz[i] == 'L' ? L_KOD[cyfra] : G_KOD[cyfra]);
        }
        dodaj(m, "01010");
        for (int i = 7; i < 13; ++i) dodaj(m, R_KOD[kod[static_cast<size_t>(i)] - '0']);
    } else {
        for (int i = 0; i < 4; ++i) dodaj(m, L_KOD[kod[static_cast<size_t>(i)] - '0']);
        dodaj(m, "01010");
        for (int i = 4; i < 8; ++i) dodaj(m, R_KOD[kod[static_cast<size_t>(i)] - '0']);
    }
    dodaj(m, "101");
    return m;
}

std::vector<bool> modulyCode128(const std::string& tekst) {
    std::vector<bool> m;
    if (tekst.empty()) return m;
    std::vector<int> wartosci = {104};  // Start B
    int suma = 104;
    for (size_t i = 0; i < tekst.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(tekst[i]);
        if (c < 32 || c > 126) return std::vector<bool>();  // zestaw B: tylko ASCII do wypisania
        const int w = c - 32;
        wartosci.push_back(w);
        suma += w * static_cast<int>(i + 1);
    }
    wartosci.push_back(suma % 103);
    wartosci.push_back(106);  // Stop
    for (int w : wartosci) {
        bool kreska = true;
        for (const char* p = CODE128[w]; *p; ++p) {
            for (int i = 0; i < *p - '0'; ++i) m.push_back(kreska);
            kreska = !kreska;
        }
    }
    return m;
}

std::vector<bool> moduly(Typ typ, const std::string& tekst) {
    if (typ == Typ::Code128) return modulyCode128(tekst);
    return modulyEAN(tekst);
}

// ---------------------------------------------------------------------------
// Odczyt
// ---------------------------------------------------------------------------

namespace {

/// Dopasowuje 4 szerokości (w modułach) wzorca cyfry do zmierzonych długości
int rozpoznajCyfre(const double* dl, const char* const* tabela, int ile = 10) {
    int najlepsza = -1;
    double blad = 1e9;
    for (int c = 0; c < ile; ++c) {
        // z kodu 7-modułowego liczymy długości 4 kolejnych serii
        const char* w = tabela[c];
        double seria[4];
        int n = 0, i = 0;
        while (i < 7 && n < 4) {
            int j = i;
            while (j < 7 && w[j] == w[i]) ++j;
            seria[n++] = j - i;
            i = j;
        }
        if (n != 4 || i != 7) continue;
        double b = 0;
        for (int k = 0; k < 4; ++k) b += (dl[k] - seria[k]) * (dl[k] - seria[k]);
        if (b < blad) {
            blad = b;
            najlepsza = c;
        }
    }
    return blad < 1.2 ? najlepsza : -1;
}

/// Czy ciąg długości serii (zaczynający się od kreski) jest EAN-om; zwraca kod
std::optional<std::string> dekodujSerie(const std::vector<double>& seria) {
    // EAN-13: 3 + 24 + 5 + 24 + 3 = 59 serii, EAN-8: 3 + 16 + 5 + 16 + 3 = 43
    for (size_t pelna : {size_t(59), size_t(43)}) {
        if (seria.size() < pelna) continue;
        for (size_t start = 0; start + pelna <= seria.size(); ++start) {
            const double u = (seria[start] + seria[start + 1] + seria[start + 2]) / 3.0;  // szerokość modułu
            if (u < 1.0) continue;
            std::vector<double> s(pelna);
            for (size_t i = 0; i < pelna; ++i) s[i] = seria[start + i] / u;
            if (std::abs(s[0] - 1) > 0.6 || std::abs(s[1] - 1) > 0.6 || std::abs(s[2] - 1) > 0.6) continue;
            const size_t polowa = pelna == 59 ? 6 : 4;
            std::string cyfry;
            std::string parzystosc;
            bool ok = true;
            size_t poz = 3;
            for (size_t k = 0; k < polowa && ok; ++k, poz += 4) {
                int c = rozpoznajCyfre(&s[poz], L_KOD);
                if (c >= 0) {
                    cyfry += static_cast<char>('0' + c);
                    parzystosc += 'L';
                    continue;
                }
                c = rozpoznajCyfre(&s[poz], G_KOD);
                if (c >= 0 && pelna == 59) {
                    cyfry += static_cast<char>('0' + c);
                    parzystosc += 'G';
                } else {
                    ok = false;
                }
            }
            if (!ok) continue;
            for (size_t i = 0; i < 5; ++i) {
                if (std::abs(s[poz + i] - 1) > 0.6) ok = false;
            }
            if (!ok) continue;
            poz += 5;
            for (size_t k = 0; k < polowa && ok; ++k, poz += 4) {
                const int c = rozpoznajCyfre(&s[poz], R_KOD);
                if (c < 0) ok = false;
                else cyfry += static_cast<char>('0' + c);
            }
            if (!ok) continue;
            if (pelna == 59) {
                int pierwsza = -1;
                for (int p = 0; p < 10; ++p) {
                    if (parzystosc == PARZYSTOSC[p]) pierwsza = p;
                }
                if (pierwsza < 0) continue;
                cyfry = std::string(1, static_cast<char>('0' + pierwsza)) + cyfry;
            }
            if (poprawnyEAN(cyfry)) return cyfry;
        }
    }
    return std::nullopt;
}

} // namespace

std::optional<std::string> odczytajLinie(const std::vector<unsigned char>& jasnosc) {
    if (jasnosc.size() < 40) return std::nullopt;
    // próg: środek między najciemniejszym a najjaśniejszym (z lekkim wygładzeniem szumu)
    unsigned char mn = 255, mx = 0;
    for (unsigned char v : jasnosc) {
        mn = std::min(mn, v);
        mx = std::max(mx, v);
    }
    if (mx - mn < 60) return std::nullopt;
    const int prog = (mn + mx) / 2;
    std::vector<double> serie;
    bool czarny = jasnosc[0] < prog;
    bool pierwszaKreska = czarny;
    double dl = 0;
    for (unsigned char v : jasnosc) {
        const bool c = v < prog;
        if (c == czarny) {
            dl += 1;
        } else {
            serie.push_back(dl);
            dl = 1;
            czarny = c;
        }
    }
    serie.push_back(dl);
    // serie mają naprzemiennie kreskę i przerwę; wzorzec zaczyna się od kreski
    if (!pierwszaKreska) serie.erase(serie.begin());
    if (auto w = dekodujSerie(serie)) return w;
    // odwrócony o 180 stopni
    std::vector<double> odwr(serie.rbegin(), serie.rend());
    if (serie.size() % 2 == 0) {
        // po odwróceniu pierwsza seria jest przerwą: odrzucamy ją
        odwr.erase(odwr.begin());
    }
    return dekodujSerie(odwr);
}

std::optional<std::string> odczytajObraz(const std::vector<unsigned char>& szarosc, int szerokosc, int wysokosc) {
    if (szerokosc <= 0 || wysokosc <= 0 || szarosc.size() < static_cast<size_t>(szerokosc) * static_cast<size_t>(wysokosc))
        return std::nullopt;
    // linie rozłożone od środka na zewnątrz
    const int krok = std::max(1, wysokosc / 40);
    for (int d = 0; d <= wysokosc / 2; d += krok) {
        for (int znak : {1, -1}) {
            const int y = wysokosc / 2 + znak * d;
            if (y < 0 || y >= wysokosc) continue;
            const auto poczatek = szarosc.begin() + static_cast<std::ptrdiff_t>(y) * szerokosc;
            if (auto w = odczytajLinie(std::vector<unsigned char>(poczatek, poczatek + szerokosc))) return w;
            if (d == 0) break;
        }
    }
    return std::nullopt;
}

} // namespace KodKreskowy
