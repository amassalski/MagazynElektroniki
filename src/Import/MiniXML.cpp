#include "MiniXML.h"

#include <cctype>
#include <cstdlib>

namespace MiniXML {

std::string Wezel::atrybut(const std::string& klucz, const std::string& domyslny) const {
    auto it = atrybuty.find(klucz);
    return it == atrybuty.end() ? domyslny : it->second;
}

const Wezel* Wezel::dziecko(const std::string& nazwaDziecka) const {
    for (const auto& d : dzieci) {
        if (d->nazwa == nazwaDziecka) return d.get();
    }
    return nullptr;
}

std::vector<const Wezel*> Wezel::dzieciONazwie(const std::string& nazwaDzieci) const {
    std::vector<const Wezel*> wynik;
    for (const auto& d : dzieci) {
        if (d->nazwa == nazwaDzieci) wynik.push_back(d.get());
    }
    return wynik;
}

namespace {

void dopiszUtf8(std::string& wynik, unsigned long kod) {
    if (kod < 0x80) {
        wynik += static_cast<char>(kod);
    } else if (kod < 0x800) {
        wynik += static_cast<char>(0xC0 | (kod >> 6));
        wynik += static_cast<char>(0x80 | (kod & 0x3F));
    } else if (kod < 0x10000) {
        wynik += static_cast<char>(0xE0 | (kod >> 12));
        wynik += static_cast<char>(0x80 | ((kod >> 6) & 0x3F));
        wynik += static_cast<char>(0x80 | (kod & 0x3F));
    } else {
        wynik += static_cast<char>(0xF0 | (kod >> 18));
        wynik += static_cast<char>(0x80 | ((kod >> 12) & 0x3F));
        wynik += static_cast<char>(0x80 | ((kod >> 6) & 0x3F));
        wynik += static_cast<char>(0x80 | (kod & 0x3F));
    }
}

std::string odkoduj(const std::string& s) {
    if (s.find('&') == std::string::npos) return s;
    std::string wynik;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '&') {
            wynik += s[i];
            continue;
        }
        const size_t koniec = s.find(';', i);
        if (koniec == std::string::npos || koniec - i > 10) {
            wynik += s[i];
            continue;
        }
        const std::string nazwa = s.substr(i + 1, koniec - i - 1);
        if (nazwa == "amp") wynik += '&';
        else if (nazwa == "lt") wynik += '<';
        else if (nazwa == "gt") wynik += '>';
        else if (nazwa == "quot") wynik += '"';
        else if (nazwa == "apos") wynik += '\'';
        else if (!nazwa.empty() && nazwa[0] == '#') {
            const bool hex = nazwa.size() > 1 && (nazwa[1] == 'x' || nazwa[1] == 'X');
            char* k = nullptr;
            const unsigned long kod = std::strtoul(nazwa.c_str() + (hex ? 2 : 1), &k, hex ? 16 : 10);
            if (k && *k == '\0' && kod > 0) dopiszUtf8(wynik, kod);
            else wynik += s.substr(i, koniec - i + 1);
        } else {
            wynik += s.substr(i, koniec - i + 1); // nieznana encja zostaje bez zmian
        }
        i = koniec;
    }
    return wynik;
}

bool znakNazwy(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == ':' || c == '.' ||
           (static_cast<unsigned char>(c) >= 0x80);
}

class Parser {
public:
    Parser(const std::string& t, std::string& b) : t_(t), blad_(b) {}

    std::unique_ptr<Wezel> uruchom() {
        if (t_.compare(0, 3, "\xEF\xBB\xBF") == 0) poz_ = 3;
        std::vector<Wezel*> stos;
        std::unique_ptr<Wezel> korzen;
        std::string tekst;

        while (poz_ < t_.size()) {
            if (t_[poz_] != '<') {
                const size_t nast = t_.find('<', poz_);
                const size_t koniec = nast == std::string::npos ? t_.size() : nast;
                if (!stos.empty()) stos.back()->tekst += odkoduj(t_.substr(poz_, koniec - poz_));
                poz_ = koniec;
                continue;
            }
            if (t_.compare(poz_, 4, "<!--") == 0) {
                if (!pomnijDo("-->")) return blad("niezamknięty komentarz");
            } else if (t_.compare(poz_, 9, "<![CDATA[") == 0) {
                const size_t k = t_.find("]]>", poz_);
                if (k == std::string::npos) return blad("niezamknięty CDATA");
                if (!stos.empty()) stos.back()->tekst += t_.substr(poz_ + 9, k - poz_ - 9);
                poz_ = k + 3;
            } else if (t_.compare(poz_, 2, "<?") == 0) {
                if (!pomnijDo("?>")) return blad("niezamknięta instrukcja <?");
            } else if (t_.compare(poz_, 2, "<!") == 0) {
                if (!pominDoctype()) return blad("niezamknięty DOCTYPE");
            } else if (t_.compare(poz_, 2, "</") == 0) {
                const size_t k = t_.find('>', poz_);
                if (k == std::string::npos) return blad("niezamknięty znacznik zamykający");
                std::string nazwa = t_.substr(poz_ + 2, k - poz_ - 2);
                while (!nazwa.empty() && std::isspace(static_cast<unsigned char>(nazwa.back()))) nazwa.pop_back();
                if (stos.empty() || stos.back()->nazwa != nazwa) {
                    return blad("niezgodny znacznik zamykający </" + nazwa + ">");
                }
                stos.pop_back();
                poz_ = k + 1;
                if (stos.empty() && korzen) return korzen;
            } else {
                auto wezel = std::make_unique<Wezel>();
                bool pusty = false;
                if (!czytajZnacznik(*wezel, pusty)) return nullptr;
                Wezel* surowy = wezel.get();
                if (stos.empty()) {
                    if (korzen) return blad("więcej niż jeden element główny");
                    korzen = std::move(wezel);
                } else {
                    stos.back()->dzieci.push_back(std::move(wezel));
                }
                if (!pusty) stos.push_back(surowy);
                else if (stos.empty() && korzen) return korzen;
            }
        }
        if (!stos.empty()) return blad("niezamknięty element <" + stos.back()->nazwa + ">");
        if (!korzen) return blad("brak elementu głównego");
        return korzen;
    }

private:
    const std::string& t_;
    std::string& blad_;
    size_t poz_ = 0;

    std::unique_ptr<Wezel> blad(const std::string& opis) {
        // numer linii do komunikatu
        int linia = 1;
        for (size_t i = 0; i < poz_ && i < t_.size(); ++i) {
            if (t_[i] == '\n') ++linia;
        }
        blad_ = "Błąd XML (linia " + std::to_string(linia) + "): " + opis;
        return nullptr;
    }

    bool pomnijDo(const char* koniec) {
        const size_t k = t_.find(koniec, poz_);
        if (k == std::string::npos) return false;
        poz_ = k + std::string(koniec).size();
        return true;
    }

    bool pominDoctype() {
        // <!DOCTYPE ... [ ... ]> : uwzględnij nawiasy kwadratowe
        int glebokosc = 0;
        for (; poz_ < t_.size(); ++poz_) {
            if (t_[poz_] == '[') ++glebokosc;
            else if (t_[poz_] == ']') --glebokosc;
            else if (t_[poz_] == '>' && glebokosc <= 0) {
                ++poz_;
                return true;
            }
        }
        return false;
    }

    void pominSpacje() {
        while (poz_ < t_.size() && std::isspace(static_cast<unsigned char>(t_[poz_]))) ++poz_;
    }

    bool czytajZnacznik(Wezel& wezel, bool& pusty) {
        ++poz_; // '<'
        const size_t start = poz_;
        while (poz_ < t_.size() && znakNazwy(t_[poz_])) ++poz_;
        wezel.nazwa = t_.substr(start, poz_ - start);
        if (wezel.nazwa.empty()) {
            blad("pusta nazwa elementu");
            return false;
        }
        for (;;) {
            pominSpacje();
            if (poz_ >= t_.size()) {
                blad("niezamknięty znacznik <" + wezel.nazwa + ">");
                return false;
            }
            if (t_[poz_] == '>') {
                ++poz_;
                pusty = false;
                return true;
            }
            if (t_[poz_] == '/' && poz_ + 1 < t_.size() && t_[poz_ + 1] == '>') {
                poz_ += 2;
                pusty = true;
                return true;
            }
            const size_t a = poz_;
            while (poz_ < t_.size() && znakNazwy(t_[poz_])) ++poz_;
            const std::string klucz = t_.substr(a, poz_ - a);
            if (klucz.empty()) {
                blad("błędny atrybut w <" + wezel.nazwa + ">");
                return false;
            }
            pominSpacje();
            if (poz_ >= t_.size() || t_[poz_] != '=') {
                blad("atrybut „" + klucz + "” bez wartości");
                return false;
            }
            ++poz_;
            pominSpacje();
            if (poz_ >= t_.size() || (t_[poz_] != '"' && t_[poz_] != '\'')) {
                blad("wartość atrybutu „" + klucz + "” bez cudzysłowu");
                return false;
            }
            const char cudzyslow = t_[poz_++];
            const size_t k = t_.find(cudzyslow, poz_);
            if (k == std::string::npos) {
                blad("niezamknięta wartość atrybutu „" + klucz + "”");
                return false;
            }
            wezel.atrybuty[klucz] = odkoduj(t_.substr(poz_, k - poz_));
            poz_ = k + 1;
        }
    }
};

} // namespace

std::unique_ptr<Wezel> parsuj(const std::string& tekst, std::string& blad) {
    blad.clear();
    Parser parser(tekst, blad);
    return parser.uruchom();
}

} // namespace MiniXML
