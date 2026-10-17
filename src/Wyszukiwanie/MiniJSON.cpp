#include "MiniJSON.h"
#include "../Narzedzia/Liczby.h"

#include <cstdio>
#include <cstdlib>

const JsonWartosc* JsonWartosc::pole(const std::string& nazwa) const {
    if (typ != Typ::Obiekt) return nullptr;
    for (const auto& p : pola) if (p.first == nazwa) return &p.second;
    return nullptr;
}

std::string JsonWartosc::jakoTekst() const {
    if (typ == Typ::Tekst) return tekst;
    if (typ == Typ::Liczba) {
        char b[64];
        std::snprintf(b, sizeof b, "%.10g", liczba);
        return b;
    }
    return std::string();
}

namespace {

void dopiszUTF8(std::string& s, unsigned long cp) {
    if (cp < 0x80) s += static_cast<char>(cp);
    else if (cp < 0x800) { s += static_cast<char>(0xC0 | (cp >> 6)); s += static_cast<char>(0x80 | (cp & 0x3F)); }
    else if (cp < 0x10000) {
        s += static_cast<char>(0xE0 | (cp >> 12)); s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        s += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        s += static_cast<char>(0xF0 | (cp >> 18)); s += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); s += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

class Parser {
public:
    explicit Parser(const std::string& t) : t_(t) {}
    std::string blad;

    bool wartosc(JsonWartosc& w, int glebokosc) {
        if (glebokosc > 64) return err("zbyt głęboko zagnieżdżony JSON");
        pomin();
        if (p_ >= t_.size()) return err("nieoczekiwany koniec");
        const char c = t_[p_];
        if (c == '{') return obiekt(w, glebokosc);
        if (c == '[') return tablica(w, glebokosc);
        if (c == '"') { w.typ = JsonWartosc::Typ::Tekst; return napis(w.tekst); }
        if (t_.compare(p_, 4, "true") == 0) { p_ += 4; w.typ = JsonWartosc::Typ::Bool; w.logiczna = true; return true; }
        if (t_.compare(p_, 5, "false") == 0) { p_ += 5; w.typ = JsonWartosc::Typ::Bool; return true; }
        if (t_.compare(p_, 4, "null") == 0) { p_ += 4; w.typ = JsonWartosc::Typ::Null; return true; }
        return liczba(w);
    }
    bool koniec() { pomin(); return p_ >= t_.size(); }

private:
    const std::string& t_;
    size_t p_ = 0;

    bool err(const std::string& co) { if (blad.empty()) blad = co + " (pozycja " + std::to_string(p_) + ")"; return false; }
    void pomin() { while (p_ < t_.size() && (t_[p_] == ' ' || t_[p_] == '\t' || t_[p_] == '\n' || t_[p_] == '\r')) ++p_; }

    bool liczba(JsonWartosc& w) {
        const char* start = t_.c_str() + p_;
        char* koniec = nullptr;
        const double v = strtodKlasyczny(start, &koniec);
        if (koniec == start) return err("błędna wartość");
        p_ += static_cast<size_t>(koniec - start);
        w.typ = JsonWartosc::Typ::Liczba;
        w.liczba = v;
        return true;
    }

    bool hex4(unsigned long& v) {
        if (p_ + 4 > t_.size()) return err("błędna sekwencja \\u");
        v = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = t_[p_++];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= static_cast<unsigned long>(c - '0');
            else if (c >= 'a' && c <= 'f') v |= static_cast<unsigned long>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= static_cast<unsigned long>(c - 'A' + 10);
            else return err("błędna sekwencja \\u");
        }
        return true;
    }

    bool napis(std::string& s) {
        ++p_;  // "
        s.clear();
        while (p_ < t_.size()) {
            const char c = t_[p_++];
            if (c == '"') return true;
            if (c != '\\') { s += c; continue; }
            if (p_ >= t_.size()) break;
            const char e = t_[p_++];
            switch (e) {
                case 'n': s += '\n'; break;
                case 't': s += '\t'; break;
                case 'r': s += '\r'; break;
                case 'b': s += '\b'; break;
                case 'f': s += '\f'; break;
                case 'u': {
                    unsigned long cp = 0;
                    if (!hex4(cp)) return false;
                    if (cp >= 0xD800 && cp < 0xDC00 && t_.compare(p_, 2, "\\u") == 0) {
                        p_ += 2;
                        unsigned long lo = 0;
                        if (!hex4(lo)) return false;
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    }
                    dopiszUTF8(s, cp);
                    break;
                }
                default: s += e;  // \" \\ \/
            }
        }
        return err("niezamknięty napis");
    }

    bool tablica(JsonWartosc& w, int g) {
        ++p_;
        w.typ = JsonWartosc::Typ::Tablica;
        pomin();
        if (p_ < t_.size() && t_[p_] == ']') { ++p_; return true; }
        for (;;) {
            JsonWartosc e;
            if (!wartosc(e, g + 1)) return false;
            w.elementy.push_back(std::move(e));
            pomin();
            if (p_ >= t_.size()) return err("niezamknięta tablica");
            if (t_[p_] == ',') { ++p_; continue; }
            if (t_[p_] == ']') { ++p_; return true; }
            return err("oczekiwano , lub ]");
        }
    }

    bool obiekt(JsonWartosc& w, int g) {
        ++p_;
        w.typ = JsonWartosc::Typ::Obiekt;
        pomin();
        if (p_ < t_.size() && t_[p_] == '}') { ++p_; return true; }
        for (;;) {
            pomin();
            if (p_ >= t_.size() || t_[p_] != '"') return err("oczekiwano nazwy pola");
            std::string nazwa;
            if (!napis(nazwa)) return false;
            pomin();
            if (p_ >= t_.size() || t_[p_] != ':') return err("oczekiwano :");
            ++p_;
            JsonWartosc v;
            if (!wartosc(v, g + 1)) return false;
            w.pola.emplace_back(std::move(nazwa), std::move(v));
            pomin();
            if (p_ >= t_.size()) return err("niezamknięty obiekt");
            if (t_[p_] == ',') { ++p_; continue; }
            if (t_[p_] == '}') { ++p_; return true; }
            return err("oczekiwano , lub }");
        }
    }
};

} // namespace

bool MiniJSON::parsuj(const std::string& tekst, JsonWartosc& wynik, std::string& blad) {
    wynik = JsonWartosc();
    Parser p(tekst);
    if (!p.wartosc(wynik, 0)) { blad = p.blad; return false; }
    if (!p.koniec()) { blad = "dodatkowe znaki po JSON"; return false; }
    return true;
}
