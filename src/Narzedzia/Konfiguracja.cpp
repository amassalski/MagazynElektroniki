#include "Konfiguracja.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

namespace {

// Ścieżki w programie są w UTF-8; na Windows zwykły std::string oznaczałby
// stronę kodową ANSI, więc zawsze konwertujemy przez u8path.
fs::path P(const std::string& s) { return fs::u8path(s); }

std::string przytnij(const std::string& s) {
    const char* spacje = " \t\r\n";
    const auto pocz = s.find_first_not_of(spacje);
    if (pocz == std::string::npos) return "";
    const auto kon = s.find_last_not_of(spacje);
    return s.substr(pocz, kon - pocz + 1);
}

std::string bezZnakowNowejLinii(std::string s) {
    std::replace(s.begin(), s.end(), '\n', ' ');
    std::replace(s.begin(), s.end(), '\r', ' ');
    return s;
}

} // namespace

Konfiguracja& Konfiguracja::pobierz() {
    static Konfiguracja instancja;
    return instancja;
}

void Konfiguracja::ustawSciezke(const std::string& sciezka) {
    plik = sciezka;
}

std::string Konfiguracja::znajdzPierwszyIstniejacy(const std::vector<std::string>& kandydaci) {
    for (const auto& k : kandydaci) {
        std::error_code ec;
        if (!k.empty() && fs::is_regular_file(P(k), ec)) return k;
    }
    return "";
}

Konfiguracja::Sekcja* Konfiguracja::znajdzSekcje(const std::string& nazwa) {
    for (auto& s : sekcje) if (s.nazwa == nazwa) return &s;
    return nullptr;
}

const Konfiguracja::Sekcja* Konfiguracja::znajdzSekcje(const std::string& nazwa) const {
    for (const auto& s : sekcje) if (s.nazwa == nazwa) return &s;
    return nullptr;
}

bool Konfiguracja::wczytaj() {
    sekcje.clear();
    blad.clear();

    std::ifstream strumien(P(plik), std::ios::binary);
    if (!strumien.is_open()) {
        return true; // brak pliku = domyślne wartości
    }

    std::string linia;
    std::string biezaca;
    bool pierwsza = true;
    while (std::getline(strumien, linia)) {
        if (pierwsza) {
            pierwsza = false;
            if (linia.compare(0, 3, "\xEF\xBB\xBF") == 0) linia.erase(0, 3); // BOM
        }
        linia = przytnij(linia);
        if (linia.empty() || linia[0] == '#' || linia[0] == ';') continue;

        if (linia.front() == '[' && linia.back() == ']') {
            biezaca = przytnij(linia.substr(1, linia.size() - 2));
            if (!znajdzSekcje(biezaca)) sekcje.push_back({biezaca, {}});
            continue;
        }

        const auto poz = linia.find('=');
        if (poz == std::string::npos) continue;
        const std::string klucz = przytnij(linia.substr(0, poz));
        const std::string wartosc = przytnij(linia.substr(poz + 1));
        if (klucz.empty()) continue;
        ustaw(biezaca, klucz, wartosc);
    }
    return true;
}

bool Konfiguracja::zapisz() {
    blad.clear();

    std::ostringstream wyj;
    wyj << "# Konfiguracja programu Domowy Magazyn\n"
        << "# Plik jest zapisywany automatycznie. Komentarze dodane ręcznie nie są zachowywane.\n";

    // Sekcja bez nazwy (stary format) idzie na początek
    for (const auto& s : sekcje) {
        if (!s.nazwa.empty() || s.wpisy.empty()) continue;
        wyj << "\n";
        for (const auto& w : s.wpisy) wyj << w.first << "=" << w.second << "\n";
    }
    for (const auto& s : sekcje) {
        if (s.nazwa.empty() || s.wpisy.empty()) continue;
        wyj << "\n[" << s.nazwa << "]\n";
        for (const auto& w : s.wpisy) wyj << w.first << "=" << w.second << "\n";
    }

    const fs::path docelowa = P(plik);
    const fs::path tymczasowa = P(plik + ".tmp");
    std::error_code ec;

    if (docelowa.has_parent_path()) {
        fs::create_directories(docelowa.parent_path(), ec);
    }

    {
        std::ofstream strumien(tymczasowa, std::ios::binary | std::ios::trunc);
        if (!strumien.is_open()) {
            blad = "Nie można zapisać pliku konfiguracji: " + plik;
            return false;
        }
        strumien << wyj.str();
        strumien.flush();
        if (!strumien.good()) {
            blad = "Błąd zapisu pliku konfiguracji: " + plik;
            strumien.close();
            fs::remove(tymczasowa, ec);
            return false;
        }
    }

    fs::rename(tymczasowa, docelowa, ec);
    if (ec) {
        // Zapasowo: zwykłe nadpisanie
        fs::remove(docelowa, ec);
        ec.clear();
        fs::rename(tymczasowa, docelowa, ec);
        if (ec) {
            blad = "Nie można zastąpić pliku konfiguracji: " + plik + " (" + ec.message() + ")";
            fs::remove(tymczasowa, ec);
            return false;
        }
    }
    return true;
}

bool Konfiguracja::czyIstnieje(const std::string& sekcja, const std::string& klucz) const {
    if (const Sekcja* s = znajdzSekcje(sekcja)) {
        for (const auto& w : s->wpisy) if (w.first == klucz) return true;
    }
    return false;
}

std::string Konfiguracja::pobierz(const std::string& sekcja, const std::string& klucz,
                                  const std::string& domyslna) const {
    if (const Sekcja* s = znajdzSekcje(sekcja)) {
        for (const auto& w : s->wpisy) if (w.first == klucz) return w.second;
    }
    return domyslna;
}

int Konfiguracja::pobierzInt(const std::string& sekcja, const std::string& klucz, int domyslna) const {
    if (!czyIstnieje(sekcja, klucz)) return domyslna;
    const std::string tekst = pobierz(sekcja, klucz);
    char* koniec = nullptr;
    const long wartosc = std::strtol(tekst.c_str(), &koniec, 10);
    if (koniec == tekst.c_str() || *koniec != '\0') return domyslna; // nie jest liczbą
    return static_cast<int>(wartosc);
}

bool Konfiguracja::pobierzBool(const std::string& sekcja, const std::string& klucz, bool domyslna) const {
    if (!czyIstnieje(sekcja, klucz)) return domyslna;
    std::string t = pobierz(sekcja, klucz);
    std::transform(t.begin(), t.end(), t.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (t == "1" || t == "true" || t == "tak" || t == "yes" || t == "on") return true;
    if (t == "0" || t == "false" || t == "nie" || t == "no" || t == "off") return false;
    return domyslna;
}

void Konfiguracja::ustaw(const std::string& sekcja, const std::string& klucz, const std::string& wartosc) {
    Sekcja* s = znajdzSekcje(sekcja);
    if (!s) {
        sekcje.push_back({sekcja, {}});
        s = &sekcje.back();
    }
    const std::string czysta = przytnij(bezZnakowNowejLinii(wartosc));
    for (auto& w : s->wpisy) {
        if (w.first == klucz) { w.second = czysta; return; }
    }
    s->wpisy.emplace_back(klucz, czysta);
}

void Konfiguracja::ustawInt(const std::string& sekcja, const std::string& klucz, int wartosc) {
    ustaw(sekcja, klucz, std::to_string(wartosc));
}

void Konfiguracja::ustawBool(const std::string& sekcja, const std::string& klucz, bool wartosc) {
    ustaw(sekcja, klucz, wartosc ? "1" : "0");
}

void Konfiguracja::usun(const std::string& sekcja, const std::string& klucz) {
    if (Sekcja* s = znajdzSekcje(sekcja)) {
        s->wpisy.erase(std::remove_if(s->wpisy.begin(), s->wpisy.end(),
                           [&](const auto& w) { return w.first == klucz; }),
                       s->wpisy.end());
    }
}

void Konfiguracja::usunSekcje(const std::string& sekcja) {
    sekcje.erase(std::remove_if(sekcje.begin(), sekcje.end(),
                     [&](const Sekcja& s) { return s.nazwa == sekcja; }),
                 sekcje.end());
}

std::vector<std::pair<std::string, std::string>> Konfiguracja::pobierzSekcje(const std::string& sekcja) const {
    if (const Sekcja* s = znajdzSekcje(sekcja)) return s->wpisy;
    return {};
}

static std::string g_katalogDanych;

void Konfiguracja::ustawKatalogDanych(const std::string& katalog) { g_katalogDanych = katalog; }

UstawieniaBazowe Konfiguracja::pobierzUstawieniaAplikacji() const {
    UstawieniaBazowe us;
    if (!g_katalogDanych.empty()) {
        us.sciezkaObrazki = g_katalogDanych + "/zdjecia";
        us.sciezkaPDFDatasheet = g_katalogDanych + "/dokumenty";
    }
    us.sciezkaObrazki = pobierz("sciezki", "obrazki", us.sciezkaObrazki);
    us.sciezkaPDFDatasheet = pobierz("sciezki", "pdf", us.sciezkaPDFDatasheet);
    us.typKoduEAN = pobierz("ean", "typ", us.typKoduEAN);
    us.prefixEAN = pobierzInt("ean", "prefix", us.prefixEAN);
    us.nazwaFontu = pobierz("czcionka", "nazwa", us.nazwaFontu);
    us.rozmiarFontu = std::clamp(pobierzInt("czcionka", "rozmiar", us.rozmiarFontu), 6, 96);
    us.pogrubienie = pobierzBool("czcionka", "pogrubienie", us.pogrubienie);
    us.kursywa = pobierzBool("czcionka", "kursywa", us.kursywa);

    for (const auto& w : pobierzSekcje("kolory_progow")) {
        char* koniec = nullptr;
        const long prog = std::strtol(w.first.c_str(), &koniec, 10);
        if (koniec != w.first.c_str() && *koniec == '\0') {
            us.koloryProgow[static_cast<int>(prog)] = w.second;
        }
    }
    return us;
}

void Konfiguracja::ustawUstawieniaAplikacji(const UstawieniaBazowe& us) {
    ustaw("sciezki", "obrazki", us.sciezkaObrazki);
    ustaw("sciezki", "pdf", us.sciezkaPDFDatasheet);
    ustaw("ean", "typ", us.typKoduEAN);
    ustawInt("ean", "prefix", us.prefixEAN);
    ustaw("czcionka", "nazwa", us.nazwaFontu);
    ustawInt("czcionka", "rozmiar", us.rozmiarFontu);
    ustawBool("czcionka", "pogrubienie", us.pogrubienie);
    ustawBool("czcionka", "kursywa", us.kursywa);

    usunSekcje("kolory_progow");
    for (const auto& kv : us.koloryProgow) {
        ustaw("kolory_progow", std::to_string(kv.first), kv.second);
    }
}
