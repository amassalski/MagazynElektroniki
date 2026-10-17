#include "ParserStrony.h"
#include "../Narzedzia/Liczby.h"

#include "MiniJSON.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <regex>
#include <set>

namespace ParserStrony {

namespace {

std::string malymi(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string przytnij(const std::string& s) {
    const char* sp = " \t\r\n\xC2\xA0";
    const size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return std::string();
    (void)sp;
    return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}

/// Tag otwierający: nazwa małymi literami i atrybuty (wartości po odkodowaniu encji)
struct Tag {
    std::string nazwa;
    std::map<std::string, std::string> atrybuty;
    size_t poczatek = 0, koniec = 0;   ///< pozycje < i po >
};

/// Czyta kolejne tagi otwierające; komentarze są pomijane, a zawartość <script>/<style> pomijana w całości
class Skaner {
public:
    explicit Skaner(const std::string& html) : h_(html) {}

    bool nastepny(Tag& t) {
        while (true) {
            const size_t p = h_.find('<', poz_);
            if (p == std::string::npos) return false;
            if (h_.compare(p, 4, "<!--") == 0) {
                const size_t k = h_.find("-->", p + 4);
                poz_ = k == std::string::npos ? h_.size() : k + 3;
                continue;
            }
            if (p + 1 >= h_.size() || !std::isalpha(static_cast<unsigned char>(h_[p + 1]))) { poz_ = p + 1; continue; }
            if (!czytajTag(p, t)) { poz_ = p + 1; continue; }
            poz_ = t.koniec;
            if (t.nazwa == "script" || t.nazwa == "style") {
                const size_t k = malymi(h_.substr(poz_, std::string::npos)).find("</" + t.nazwa);
                poz_ = k == std::string::npos ? h_.size() : poz_ + k;
            }
            return true;
        }
    }

private:
    const std::string& h_;
    size_t poz_ = 0;

    bool czytajTag(size_t p, Tag& t) {
        t = Tag();
        t.poczatek = p;
        size_t i = p + 1;
        while (i < h_.size() && (std::isalnum(static_cast<unsigned char>(h_[i])) || h_[i] == '-' || h_[i] == ':')) t.nazwa += static_cast<char>(std::tolower(static_cast<unsigned char>(h_[i++])));
        while (i < h_.size()) {
            while (i < h_.size() && std::isspace(static_cast<unsigned char>(h_[i]))) ++i;
            if (i >= h_.size()) return false;
            if (h_[i] == '>') { t.koniec = i + 1; return true; }
            if (h_[i] == '/') { ++i; continue; }
            std::string nazwa;
            while (i < h_.size() && !std::isspace(static_cast<unsigned char>(h_[i])) && h_[i] != '=' && h_[i] != '>' && h_[i] != '/')
                nazwa += static_cast<char>(std::tolower(static_cast<unsigned char>(h_[i++])));
            while (i < h_.size() && std::isspace(static_cast<unsigned char>(h_[i]))) ++i;
            std::string wartosc;
            if (i < h_.size() && h_[i] == '=') {
                ++i;
                while (i < h_.size() && std::isspace(static_cast<unsigned char>(h_[i]))) ++i;
                if (i < h_.size() && (h_[i] == '"' || h_[i] == '\'')) {
                    const char q = h_[i++];
                    const size_t k = h_.find(q, i);
                    if (k == std::string::npos) return false;
                    wartosc = h_.substr(i, k - i);
                    i = k + 1;
                } else {
                    while (i < h_.size() && !std::isspace(static_cast<unsigned char>(h_[i])) && h_[i] != '>') wartosc += h_[i++];
                }
            }
            if (!nazwa.empty()) t.atrybuty[nazwa] = odkodujEncje(wartosc);
            else ++i;
        }
        return false;
    }
};

std::string atr(const Tag& t, const std::string& n) {
    auto it = t.atrybuty.find(n);
    return it == t.atrybuty.end() ? std::string() : it->second;
}

/// Tekst od pozycji do najbliższego '<'
std::string tekstPo(const std::string& h, size_t poz) {
    const size_t k = h.find('<', poz);
    return przytnij(odkodujEncje(h.substr(poz, k == std::string::npos ? std::string::npos : k - poz)));
}

// ---- JSON-LD ----

struct OfertaLD { bool jest = false; double cena = 0; std::string waluta, dostepnosc; };

bool ofertaZObiektu(const JsonWartosc& o, OfertaLD& w) {
    if (!o.czyObiekt()) return false;
    for (const char* klucz : {"price", "lowPrice"}) {
        const JsonWartosc* c = o.pole(klucz);
        double v = 0;
        if (c && liczbaZCeny(c->jakoTekst(), v)) {
            w.jest = true;
            w.cena = v;
            if (const JsonWartosc* wal = o.pole("priceCurrency")) w.waluta = wal->jakoTekst();
            if (const JsonWartosc* d = o.pole("availability")) w.dostepnosc = nazwaDostepnosci(d->jakoTekst());
            return true;
        }
    }
    // cena w specyfikacji ceny (priceSpecification)
    if (const JsonWartosc* ps = o.pole("priceSpecification")) {
        if (ps->czyObiekt()) return ofertaZObiektu(*ps, w);
        if (ps->czyTablica()) for (const JsonWartosc& e : ps->elementy) if (ofertaZObiektu(e, w)) return true;
    }
    return false;
}

/// Szuka "offers" w całym drzewie (Product, @graph, zagnieżdżenia)
bool szukajOfert(const JsonWartosc& w, OfertaLD& wynik, int glebokosc = 0) {
    if (glebokosc > 20) return false;
    if (w.czyTablica()) {
        for (const JsonWartosc& e : w.elementy) if (szukajOfert(e, wynik, glebokosc + 1)) return true;
        return false;
    }
    if (!w.czyObiekt()) return false;
    if (const JsonWartosc* of = w.pole("offers")) {
        if (of->czyObiekt() && ofertaZObiektu(*of, wynik)) return true;
        if (of->czyTablica()) {
            // z kilku ofert bierzemy najniższą
            OfertaLD naj;
            for (const JsonWartosc& e : of->elementy) {
                OfertaLD x;
                if (ofertaZObiektu(e, x) && (!naj.jest || x.cena < naj.cena)) naj = x;
            }
            if (naj.jest) { wynik = naj; return true; }
        }
    }
    for (const auto& p : w.pola) if (p.first != "offers" && szukajOfert(p.second, wynik, glebokosc + 1)) return true;
    return false;
}

} // namespace

std::string odkodujEncje(const std::string& t) {
    if (t.find('&') == std::string::npos) return t;
    static const std::map<std::string, std::string> nazwane = {
        {"amp", "&"}, {"lt", "<"}, {"gt", ">"}, {"quot", "\""}, {"apos", "'"}, {"nbsp", " "},
        {"zl", "zł"}, {"euro", "€"}, {"pound", "£"}};
    std::string w;
    for (size_t i = 0; i < t.size(); ++i) {
        if (t[i] != '&') { w += t[i]; continue; }
        const size_t k = t.find(';', i);
        if (k == std::string::npos || k - i > 10) { w += t[i]; continue; }
        const std::string nazwa = t.substr(i + 1, k - i - 1);
        if (!nazwa.empty() && nazwa[0] == '#') {
            unsigned long cp = 0;
            char* koniec = nullptr;
            const bool hex = nazwa.size() > 1 && (nazwa[1] == 'x' || nazwa[1] == 'X');
            cp = std::strtoul(nazwa.c_str() + (hex ? 2 : 1), &koniec, hex ? 16 : 10);
            if (koniec && *koniec == '\0' && cp > 0 && cp < 0x110000) {
                if (cp == 0xA0) { w += ' '; }
                else if (cp < 0x80) w += static_cast<char>(cp);
                else if (cp < 0x800) { w += static_cast<char>(0xC0 | (cp >> 6)); w += static_cast<char>(0x80 | (cp & 0x3F)); }
                else if (cp < 0x10000) { w += static_cast<char>(0xE0 | (cp >> 12)); w += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); w += static_cast<char>(0x80 | (cp & 0x3F)); }
                else { w += static_cast<char>(0xF0 | (cp >> 18)); w += static_cast<char>(0x80 | ((cp >> 12) & 0x3F)); w += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); w += static_cast<char>(0x80 | (cp & 0x3F)); }
                i = k;
                continue;
            }
        } else {
            auto it = nazwane.find(nazwa);
            if (it != nazwane.end()) { w += it->second; i = k; continue; }
        }
        w += t[i];
    }
    return w;
}

bool liczbaZCeny(const std::string& tekst, double& wynik) {
    // zostawiamy cyfry i separatory; resztę (waluta, spacje, NBSP) pomijamy
    std::string c;
    bool zaczeta = false;
    for (size_t i = 0; i < tekst.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(tekst[i]);
        if (std::isdigit(ch)) { c += static_cast<char>(ch); zaczeta = true; }
        else if ((ch == ',' || ch == '.') && zaczeta) c += static_cast<char>(ch);
        else if (zaczeta && (ch == ' ' || ch == 0xC2 || ch == 0xA0) ) {
            // spacja jako separator tysięcy tylko między cyframi
            size_t j = i;
            while (j < tekst.size() && (tekst[j] == ' ' || static_cast<unsigned char>(tekst[j]) == 0xC2 || static_cast<unsigned char>(tekst[j]) == 0xA0)) ++j;
            if (j < tekst.size() && std::isdigit(static_cast<unsigned char>(tekst[j])) && i > 0 &&
                std::isdigit(static_cast<unsigned char>(tekst[i - 1])) ) { i = j - 1; }
            else break;
        } else if (zaczeta) break;
    }
    while (!c.empty() && (c.back() == ',' || c.back() == '.')) c.pop_back();
    if (c.empty()) return false;
    const size_t przecinki = static_cast<size_t>(std::count(c.begin(), c.end(), ','));
    const size_t kropki = static_cast<size_t>(std::count(c.begin(), c.end(), '.'));
    char dziesietny = 0;
    if (przecinki && kropki) dziesietny = c.rfind(',') > c.rfind('.') ? ',' : '.';
    else if (kropki > 1) dziesietny = 0;
    else if (przecinki > 1) dziesietny = 0;
    else if (kropki == 1) dziesietny = '.';
    else if (przecinki == 1) {
        const size_t p = c.find(',');
        const bool tysiace = c.size() - p - 1 == 3 && p > 0 && c.substr(0, p) != "0" && p <= 3;
        dziesietny = tysiace ? 0 : ',';
    }
    std::string n;
    for (char ch : c) {
        if (ch == dziesietny) n += '.';
        else if (ch != ',' && ch != '.') n += ch;
    }
    if (n.empty() || n == ".") return false;
    try { wynik = strtodKlasyczny(n.c_str()); } catch (...) { return false; }
    return true;
}

std::string nazwaDostepnosci(const std::string& wartosc) {
    std::string k = wartosc;
    const size_t p = k.find_last_of('/');
    if (p != std::string::npos) k = k.substr(p + 1);
    static const std::map<std::string, std::string> mapa = {
        {"instock", "Dostępny"}, {"outofstock", "Niedostępny"}, {"preorder", "Przedsprzedaż"},
        {"backorder", "Na zamówienie"}, {"limitedavailability", "Ograniczona dostępność"},
        {"soldout", "Wyprzedany"}, {"discontinued", "Wycofany"}, {"onlineonly", "Tylko online"},
        {"instoreonly", "Tylko w sklepie"}, {"presale", "Przedsprzedaż"}};
    auto it = mapa.find(malymi(k));
    return it == mapa.end() ? przytnij(wartosc) : it->second;
}

WynikCeny znajdzCene(const std::string& html, const std::string& wzorzecSklepu) {
    WynikCeny w;

    // 1. własny wzorzec sklepu
    if (!przytnij(wzorzecSklepu).empty()) {
        try {
            const std::regex re(wzorzecSklepu, std::regex::ECMAScript | std::regex::icase);
            std::smatch m;
            if (std::regex_search(html, m, re) && m.size() > 1) {
                double v = 0;
                if (liczbaZCeny(odkodujEncje(m[1].str()), v)) {
                    w.znaleziono = true; w.cena = v; w.zrodlo = "wzorzec sklepu";
                    return w;
                }
                w.blad = "Wzorzec sklepu znalazł tekst, który nie jest ceną: " + m[1].str();
            } else {
                w.blad = "Wzorzec sklepu nie pasuje do strony (albo nie ma grupy w nawiasie).";
            }
        } catch (const std::regex_error&) {
            w.blad = "Błędny wzorzec ceny sklepu.";
        }
    }

    // 2-4: dane strukturalne
    Skaner s(html);
    Tag t;
    std::string metaCena, metaWaluta, itemCena, itemWaluta, dostItem;
    std::vector<std::string> jsonLD;
    while (s.nastepny(t)) {
        if (t.nazwa == "script") {
            const std::string typ = malymi(atr(t, "type"));
            if (typ.find("ld+json") != std::string::npos) {
                const std::string malyHtml = malymi(html.substr(t.koniec));
                const size_t k = malyHtml.find("</script");
                jsonLD.push_back(html.substr(t.koniec, k == std::string::npos ? std::string::npos : k));
            }
        } else if (t.nazwa == "meta" && atr(t, "itemprop").empty()) {
            const std::string nazwa = malymi(atr(t, "property").empty() ? atr(t, "name") : atr(t, "property"));
            const std::string tresc = atr(t, "content");
            if (nazwa == "product:price:amount" || nazwa == "og:price:amount" || nazwa == "product:sale_price:amount") {
                if (metaCena.empty()) metaCena = tresc;
            } else if (nazwa == "product:price:currency" || nazwa == "og:price:currency") {
                metaWaluta = tresc;
            }
        } else {
            const std::string ip = malymi(atr(t, "itemprop"));
            if (ip == "price" && itemCena.empty()) {
                itemCena = atr(t, "content");
                if (itemCena.empty()) itemCena = atr(t, "value");
                if (itemCena.empty()) itemCena = tekstPo(html, t.koniec);
            } else if (ip == "pricecurrency" && itemWaluta.empty()) {
                itemWaluta = atr(t, "content");
                if (itemWaluta.empty()) itemWaluta = tekstPo(html, t.koniec);
            } else if (ip == "availability" && dostItem.empty()) {
                dostItem = atr(t, "href");
                if (dostItem.empty()) dostItem = atr(t, "content");
                if (dostItem.empty()) dostItem = tekstPo(html, t.koniec);
            }
        }
    }

    for (const std::string& blok : jsonLD) {
        JsonWartosc j;
        std::string blad;
        if (!MiniJSON::parsuj(blok, j, blad)) continue;
        OfertaLD o;
        if (szukajOfert(j, o)) {
            w.znaleziono = true; w.cena = o.cena; w.waluta = o.waluta; w.dostepnosc = o.dostepnosc; w.zrodlo = "json-ld";
            w.blad.clear();
            return w;
        }
    }
    double v = 0;
    if (!metaCena.empty() && liczbaZCeny(metaCena, v)) {
        w.znaleziono = true; w.cena = v; w.waluta = metaWaluta; w.zrodlo = "meta"; w.blad.clear();
        return w;
    }
    if (!itemCena.empty() && liczbaZCeny(itemCena, v)) {
        w.znaleziono = true; w.cena = v; w.waluta = itemWaluta; w.dostepnosc = nazwaDostepnosci(dostItem);
        w.zrodlo = "itemprop"; w.blad.clear();
        return w;
    }
    return w;
}

std::string rozwiazAdres(const std::string& strona, const std::string& href0) {
    const std::string href = przytnij(href0);
    if (href.empty() || href[0] == '#') return std::string();
    const std::string m = malymi(href);
    if (m.rfind("http://", 0) == 0 || m.rfind("https://", 0) == 0) return href;
    if (m.rfind("javascript:", 0) == 0 || m.rfind("mailto:", 0) == 0 || m.rfind("data:", 0) == 0) return std::string();
    const size_t sch = strona.find("://");
    if (sch == std::string::npos) return std::string();
    if (href.rfind("//", 0) == 0) return strona.substr(0, sch + 1) + href;
    const size_t host_end = strona.find('/', sch + 3);
    const std::string origin = host_end == std::string::npos ? strona : strona.substr(0, host_end);
    if (href[0] == '/') return origin + href;
    // względny: od katalogu strony (bez zapytania i fragmentu)
    std::string sciezka = host_end == std::string::npos ? "/" : strona.substr(host_end);
    sciezka = sciezka.substr(0, sciezka.find_first_of("?#"));
    sciezka = sciezka.substr(0, sciezka.rfind('/') + 1);
    std::string pelna = sciezka + href;
    // zwijanie ./ i ../
    std::vector<std::string> czesci;
    const size_t zap = pelna.find_first_of("?#");
    const std::string ogon = zap == std::string::npos ? "" : pelna.substr(zap);
    std::string tylko = pelna.substr(0, zap);
    size_t i = 0;
    while (i <= tylko.size()) {
        size_t k = tylko.find('/', i);
        if (k == std::string::npos) k = tylko.size();
        const std::string c = tylko.substr(i, k - i);
        if (c == "..") { if (!czesci.empty()) czesci.pop_back(); }
        else if (c != "." ) czesci.push_back(c);
        i = k + 1;
    }
    std::string wynik;
    for (size_t j = 0; j < czesci.size(); ++j) { if (j) wynik += "/"; wynik += czesci[j]; }
    if (wynik.empty() || wynik[0] != '/') wynik = "/" + wynik;
    return origin + wynik + ogon;
}

std::vector<LinkDatasheet> znajdzDatasheety(const std::string& html, const std::string& adresStrony) {
    std::vector<LinkDatasheet> wynik;
    std::set<std::string> widziane;
    Skaner s(html);
    Tag t;
    while (s.nastepny(t)) {
        if (t.nazwa != "a") continue;
        const std::string href = atr(t, "href");
        const std::string url = rozwiazAdres(adresStrony, href);
        if (url.empty()) continue;
        const std::string tekst = tekstPo(html, t.koniec);
        const std::string tytul = atr(t, "title");
        const std::string sciezka = malymi(url.substr(0, url.find_first_of("?#")));
        const bool pdf = sciezka.size() > 4 && sciezka.compare(sciezka.size() - 4, 4, ".pdf") == 0;
        const std::string opis = malymi(tekst + " " + tytul + " " + href);
        const bool slowo = opis.find("datasheet") != std::string::npos || opis.find("data sheet") != std::string::npos ||
                           opis.find("karta katalogowa") != std::string::npos || opis.find("karta_katalogowa") != std::string::npos ||
                           opis.find("karty katalogowej") != std::string::npos || opis.find("katalog-") != std::string::npos;
        if (!pdf && !slowo) continue;
        if (!widziane.insert(url).second) continue;
        LinkDatasheet d;
        d.url = url;
        d.tekst = !tekst.empty() ? tekst : tytul;
        d.ocena = (pdf ? 3 : 0) + (slowo ? 2 : 0);
        wynik.push_back(d);
    }
    std::stable_sort(wynik.begin(), wynik.end(), [](const LinkDatasheet& a, const LinkDatasheet& b) { return a.ocena > b.ocena; });
    return wynik;
}

} // namespace ParserStrony
