#include "OdczytDatasheetu.h"


#include <algorithm>
#include <regex>
#include <set>
#include <sstream>

namespace OdczytDatasheetu {
namespace {

std::vector<std::string> linie(const std::string& t) {
    std::vector<std::string> w;
    std::string l;
    std::istringstream s(t);
    while (std::getline(s, l)) {
        while (!l.empty() && (l.back() == '\r' || l.back() == ' ' || l.back() == '\t')) l.pop_back();
        size_t p = 0;
        while (p < l.size() && (l[p] == ' ' || l[p] == '\t')) ++p;
        w.push_back(l.substr(p));
    }
    return w;
}

std::string maleLitery(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// ujednolica znaki: minus Unicode, stopnie, "–"
std::string normalizuj(std::string s) {
    auto zamien = [&](const std::string& co, const std::string& naCo) {
        size_t p = 0;
        while ((p = s.find(co, p)) != std::string::npos) { s.replace(p, co.size(), naCo); p += naCo.size(); }
    };
    zamien("\xE2\x88\x92", "-");   // −
    zamien("\xE2\x80\x93", "-");   // –
    zamien("\xE2\x80\x94", "-");   // —
    zamien("\xC2\xB0", " deg");    // °
    zamien("\xE2\x84\x83", " degC"); // ℃
    zamien("\xC2\xA0", " ");
    zamien("\xCE\xBC", "\xC2\xB5");   // μ (grecka mi) -> µ
    zamien("\xCE\xA9", " ohm");        // Ω
    zamien("\xE2\x84\xA6", " ohm");   // Ω (znak om)
    zamien("\xE2\x80\xA2", "- ");     // •
    zamien("\xEF\x82\xB7", "- ");     // punktor z czcionki Symbol
    return s;
}

bool zawiera(const std::string& l, std::initializer_list<const char*> slowa) {
    const std::string m = maleLitery(l);
    for (const char* s : slowa) if (m.find(s) != std::string::npos) return true;
    return false;
}

std::string skroc(const std::string& s, size_t n = 140) { return s.size() <= n ? s : s.substr(0, n) + "..."; }

void dodaj(Wynik& w, std::set<std::string>& mamy, const std::string& nazwa, const std::string& wart, const std::string& frag, bool pewny = true) {
    if (mamy.count(nazwa)) return;
    mamy.insert(nazwa);
    w.parametry.push_back({nazwa, wart, skroc(frag), pewny});
}


// ===================== sekcje tabel parametrów =====================

enum class Grupa { Brak, Maks, Elektr };

struct Wzor {
    const char* klucz;     // wyrażenie regularne (bez rozróżniania wielkości liter)
    const char* nazwa;     // polska nazwa
    const char* symbol;    // symbol z kart katalogowych
    bool bezJednostki;     // true np. dla hFE
};

const Wzor kMaks[] = {
    {R"(collector[- ]emitter (breakdown )?voltage|\bV\s?\(?BR\)?\s?CEO\b|\bV\s?CEO\b)", "Napięcie kolektor-emiter", "VCEO", false},
    {R"(collector[- ]base voltage|\bV\s?CBO\b)", "Napięcie kolektor-baza", "VCBO", false},
    {R"(emitter[- ]base voltage|\bV\s?EBO\b)", "Napięcie emiter-baza", "VEBO", false},
    {R"(peak collector current|\bI\s?CM\b)", "Prąd kolektora szczytowy", "ICM", false},
    {R"(collector current|\bI\s?C\b)", "Prąd kolektora", "IC", false},
    {R"(base current|\bI\s?B\b)", "Prąd bazy", "IB", false},
    {R"(drain[- ]source voltage|\bV\s?DSS?\b)", "Napięcie dren-źródło", "VDS", false},
    {R"(gate[- ]source voltage|\bV\s?GSS?\b)", "Napięcie bramka-źródło", "VGS", false},
    {R"(pulsed drain current|\bI\s?DM\b)", "Prąd drenu impulsowy", "IDM", false},
    {R"(drain current|\bI\s?D\b)", "Prąd drenu", "ID", false},
    {R"(repetitive peak reverse voltage|\bV\s?RRM\b|\bV\s?RM\b)", "Napięcie wsteczne powtarzalne", "VRRM", false},
    {R"(reverse voltage|\bV\s?R\b)", "Napięcie wsteczne", "VR", false},
    {R"(surge|\bI\s?FSM\b)", "Prąd udarowy", "IFSM", false},
    {R"(average (rectified )?(forward )?current|\bI\s?F\s?\(AV\)|\bI\s?O\b|output current)", "Prąd średni / wyjściowy", "IO", false},
    {R"(forward current|\bI\s?F\b)", "Prąd przewodzenia", "IF", false},
    {R"(supply voltage|\bV\s?CC\b|\bV\s?DD\b|\bV\s?S\b)", "Napięcie zasilania", "VCC", false},
    {R"(input voltage|\bV\s?IN\b|\bV\s?I\b)", "Napięcie wejściowe", "VIN", false},
    {R"(power dissipation|total dissipation|device dissipation|\bP\s?tot\b|\bP\s?D\b)", "Moc admisyjna", "Ptot", false},
    {R"(storage temperature|junction temperature|\bT\s?stg\b|\bT\s?J\b)", "Temperatura złącza / przechowywania", "Tj, Tstg", false},
    {R"(operating (free[- ]air |ambient )?temperature|ambient temperature|\bT\s?A\b)", "Temperatura pracy", "TA", false},
    {R"(thermal resistance|\bR\s?(th|θ)\s?J\s?[AC]\b)", "Rezystancja cieplna", "Rth", false},
};

const Wzor kElektr[] = {
    {R"(\bh\s?FE\b|\bh\s?fe\b|dc current gain|current gain)", "Wzmocnienie prądowe", "hFE", true},
    {R"(collector[- ]emitter saturation voltage|\bV\s?CE\s?\(sat\))", "Napięcie nasycenia C-E", "VCE(sat)", false},
    {R"(base[- ]emitter (saturation |on )?voltage|\bV\s?BE\s?\((sat|on)\))", "Napięcie baza-emiter", "VBE", false},
    {R"(collector cut-?off current|\bI\s?CBO\b|\bI\s?CEO\b|\bI\s?CES\b)", "Prąd zerowy kolektora", "ICBO", false},
    {R"(transition frequency|current[- ]gain bandwidth|gain[- ]bandwidth|\bf\s?T\b|\bGBW\b)", "Częstotliwość graniczna", "fT", false},
    {R"(output capacitance|\bC\s?ob[o]?\b)", "Pojemność wyjściowa", "Cob", false},
    {R"(input capacitance|\bC\s?ib[o]?\b|\bC\s?iss\b)", "Pojemność wejściowa", "Cib", false},
    {R"(noise figure|\bNF\b)", "Współczynnik szumów", "NF", false},
    {R"(drain[- ]source on[- ]resistance|on[- ]resistance|\bR\s?DS\s?\(on\))", "Rezystancja w stanie włączenia", "RDS(on)", false},
    {R"(gate threshold voltage|\bV\s?GS\s?\(th\))", "Napięcie progowe bramki", "VGS(th)", false},
    {R"(zero gate voltage drain current|\bI\s?DSS\b)", "Prąd drenu IDSS", "IDSS", false},
    {R"(forward transconductance|\bg\s?fs\b)", "Transkonduktancja", "gfs", false},
    {R"(zener voltage|\bV\s?Z\b)", "Napięcie Zenera", "VZ", false},
    {R"(forward voltage|\bV\s?F\b)", "Napięcie przewodzenia", "VF", false},
    {R"(reverse (leakage )?current|\bI\s?R\b)", "Prąd wsteczny", "IR", false},
    {R"(reverse recovery time|\bt\s?rr\b)", "Czas wstecznego wyłączania", "trr", false},
    {R"(quiescent current|supply current|\bI\s?Q\b|\bI\s?CC\b|\bI\s?DD\b)", "Prąd zasilania", "ICC", false},
    {R"(dropout voltage|\bV\s?DO\b)", "Spadek napięcia (dropout)", "Vdo", false},
    {R"(reference voltage|\bV\s?REF\b)", "Napięcie odniesienia", "VREF", false},
    {R"(output voltage|\bV\s?OUT\b|\bV\s?O\b)", "Napięcie wyjściowe", "VOUT", false},
    {R"(input offset voltage|\bV\s?IO\b|\bV\s?OS\b)", "Napięcie niezrównoważenia", "VIO", false},
    {R"(input bias current|\bI\s?IB\b|\bI\s?B\b)", "Prąd polaryzacji wejścia", "IIB", false},
    {R"(slew rate|\bSR\b)", "Szybkość narastania", "SR", false},
    {R"(common[- ]mode rejection|\bCMRR\b)", "Tłumienie sygnału wspólnego", "CMRR", false},
    {R"(open[- ]loop (voltage )?gain|large[- ]signal voltage gain|\bA\s?VD\b|\bA\s?OL\b)", "Wzmocnienie w otwartej pętli", "AVD", false},
    {R"(propagation delay|\bt\s?PD\b|\bt\s?PLH\b|\bt\s?PHL\b)", "Opóźnienie propagacji", "tpd", false},
    {R"(rise time|\bt\s?r\b)", "Czas narastania", "tr", false},
    {R"(fall time|\bt\s?f\b)", "Czas opadania", "tf", false},
    {R"(input (logic )?high( level)? voltage|\bV\s?IH\b)", "Napięcie wejściowe stanu H", "VIH", false},
    {R"(input (logic )?low( level)? voltage|\bV\s?IL\b)", "Napięcie wejściowe stanu L", "VIL", false},
};

std::string zwin(const std::string& s) {
    std::string w;
    bool spacja = false;
    for (char c : s) {
        if (c == ' ' || c == '\t' || c == '\f') { spacja = true; continue; }
        if (spacja && !w.empty()) w += ' ';
        spacja = false;
        w += c;
    }
    return w;
}

// Jednostka na pozycji (zaczyna się od @p od). Zwraca długość dopasowania (0 = brak jednostki).
size_t jednostkaOd(const std::string& s, size_t od, std::string& jedn) {
    static const std::regex re(
        R"((?:(?:\xC2\xB5|u|m|k|M|n|p|G)?(?:Hz|V|A|W|F|H)(?:dc|DC|rms|RMS|pk)?|(?:k|M)?\s?ohms?|dB|ppm|%|(?:\xC2\xB5|u|m|n)?s|deg\s?C|degC|C)(?![A-Za-z]))");
    std::smatch m;
    auto b = s.begin() + static_cast<std::ptrdiff_t>(od);
    if (!std::regex_search(b, s.end(), m, re, std::regex_constants::match_continuous)) return 0;
    std::string j = m[0].str();
    if (j == "C") return 0;   // pojedyncze "C" bez "deg" to raczej symbol, nie stopnie
    if (j.find("deg") == 0) j = "\xC2\xB0" "C";
    else if (j.find("ohm") != std::string::npos) {
        j = (j[0] == 'k' ? "k" : j[0] == 'M' ? "M" : "") + std::string("\xCE\xA9");
    } else {
        for (const char* suf : {"dc", "DC", "rms", "RMS", "pk"}) {
            const size_t n = std::char_traits<char>::length(suf);
            if (j.size() > n && j.compare(j.size() - n, n, suf) == 0 && j.size() - n >= 1 && (j[j.size() - n - 1] == 'V' || j[j.size() - n - 1] == 'A'))
                j.erase(j.size() - n);
        }
    }
    jedn = j;
    return m[0].length();
}

// Wartość z wiersza tabeli: usuwa warunki pomiaru ("IC = 10 mA"), przypisy i nazwę parametru,
// a z reszty składa "min ... max jednostka" albo "min / typ / max jednostka".
bool wartoscZWiersza(const std::string& wiersz, const std::regex& klucz, bool bezJednostki, std::string& wynik) {
    static const std::regex warunek(R"([A-Za-z][A-Za-z0-9()]{0,9}\s*=\s*[-+]?[0-9]+(?:[.,][0-9]+)?\s*(?:[A-Za-z\xC2\xB5]+(?:\s?deg\s?C)?)?\s*,?)");
    static const std::regex przypis(R"(\(\s*note\s*[0-9]+\s*\)|\bnote\s*[0-9]+\b|\b[0-9]\))", std::regex::icase);
    std::string t = wiersz;
    t = std::regex_replace(t, warunek, " ");
    t = std::regex_replace(t, przypis, " ");
    t = std::regex_replace(t, klucz, " ");

    struct Tok { std::string liczba, jedn; };
    std::vector<Tok> tok;
    static const std::regex num(R"([-+]?[0-9]+(?:[.,][0-9]+)?)");
    for (auto it = std::sregex_iterator(t.begin(), t.end(), num); it != std::sregex_iterator(); ++it) {
        const size_t poz = static_cast<size_t>(it->position());
        const size_t kon = poz + static_cast<size_t>(it->length());
        if (poz > 0 && (std::isalnum(static_cast<unsigned char>(t[poz - 1])) || t[poz - 1] == '_' || t[poz - 1] == '/')) continue;
        if (kon < t.size() && (std::isalpha(static_cast<unsigned char>(t[kon])) && t[kon] != 'm' && t[kon] != 'k' && t[kon] != 'u' && t[kon] != 'M' && t[kon] != 'n' && t[kon] != 'p' && t[kon] != 'V' && t[kon] != 'A' && t[kon] != 'W' && t[kon] != 'H' && t[kon] != 'F' && t[kon] != 'd' && t[kon] != '%')) continue;
        size_t k = kon;
        while (k < t.size() && t[k] == ' ') ++k;
        Tok x{it->str(), ""};
        const size_t dl = jednostkaOd(t, k, x.jedn);
        (void)dl;
        // przecinek dziesiętny -> kropka? zostaw tak jak w dokumencie, ale unikaj tysięcy "1,000"
        tok.push_back(x);
    }
    if (tok.empty()) return false;
    // jednostka dla liczb bez własnej: najbliższa następna, a gdy brak - ostatnia
    std::string ost;
    for (size_t i = tok.size(); i-- > 0;) { if (tok[i].jedn.empty()) tok[i].jedn = ost; else ost = tok[i].jedn; }
    if (!bezJednostki && ost.empty()) return false;
    if (tok.size() > 3) tok.resize(3);
    std::string j = tok.back().jedn;
    bool jednakowa = true;
    for (const auto& x : tok) if (x.jedn != j) jednakowa = false;
    std::string w;
    if (jednakowa) {
        if (tok.size() == 1) w = tok[0].liczba;
        else if (tok.size() == 2) w = tok[0].liczba + " ... " + tok[1].liczba;
        else w = tok[0].liczba + " / " + tok[1].liczba + " / " + tok[2].liczba;
        if (!j.empty()) w += " " + j;
    } else {
        for (size_t i = 0; i < tok.size(); ++i) {
            if (i) w += tok.size() == 2 ? " ... " : " / ";
            w += tok[i].liczba + (tok[i].jedn.empty() ? "" : " " + tok[i].jedn);
        }
    }
    wynik = w;
    return true;
}

bool naglowekSekcji(const std::string& l, Grupa& g, bool& koniec) {
    static const std::regex pocz(R"(^(?:table\s*[0-9]+[.:]?\s*|[0-9]+(?:\.[0-9]+)*\.?\s*)?)", std::regex::icase);
    static const std::regex maks(R"(^(?:absolute )?maximum ratings?\b|^limiting values\b|^absolute maximum\b)", std::regex::icase);
    static const std::regex elektr(R"(^(?:electrical|dc|ac|static|dynamic|operating|recommended operating|switching) characteristics?\b|^characteristics\b|^electrical specifications?\b)", std::regex::icase);
    static const std::regex stop(R"(^(?:typical (?:performance |operating )?(?:characteristics|curves|applications?)|ordering information|package (?:information|outline|dimensions|drawing)|pin (?:configuration|description|assignment|functions)|mechanical data|revision history|marking|tape and reel|application information|block diagram|physical dimensions)\b)", std::regex::icase);
    koniec = false;
    if (l.size() > 100) return false;
    std::smatch m;
    std::string reszta = l;
    if (std::regex_search(l, m, pocz)) reszta = l.substr(static_cast<size_t>(m[0].length()));
    if (std::regex_search(reszta, maks)) { g = Grupa::Maks; return true; }
    if (std::regex_search(reszta, elektr)) { g = Grupa::Elektr; return true; }
    if (std::regex_search(reszta, stop)) { g = Grupa::Brak; koniec = true; return true; }
    return false;
}

void odczytajTabele(const std::vector<std::string>& L, Wynik& w, std::set<std::string>& mamy) {
    struct Skomp { std::regex re; const Wzor* wz; };
    auto skompiluj = [](const Wzor* tab, size_t n) {
        std::vector<Skomp> v;
        for (size_t i = 0; i < n; ++i) v.push_back({std::regex(tab[i].klucz, std::regex::icase), &tab[i]});
        return v;
    };
    static const std::vector<Skomp> wMaks = skompiluj(kMaks, sizeof(kMaks) / sizeof(kMaks[0]));
    static const std::vector<Skomp> wElektr = skompiluj(kElektr, sizeof(kElektr) / sizeof(kElektr[0]));

    Grupa g = Grupa::Brak;
    int wSekcji = 0, ogolnych = 0;
    for (const std::string& surowa : L) {
        Grupa nowa = g;
        bool koniec = false;
        if (naglowekSekcji(surowa, nowa, koniec)) { g = nowa; wSekcji = 0; continue; }
        if (g == Grupa::Brak) continue;
        if (++wSekcji > 220) { g = Grupa::Brak; continue; }
        const std::string l = zwin(surowa);
        if (l.size() < 6) continue;
        const std::string przedrostek = g == Grupa::Maks ? "Maks.: " : "Elektr.: ";
        bool znane = false;
        for (const Skomp& s : (g == Grupa::Maks ? wMaks : wElektr)) {
            if (!std::regex_search(l, s.re)) continue;
            znane = true;
            std::string wart;
            if (!wartoscZWiersza(l, s.re, s.wz->bezJednostki, wart)) break;
            const std::string nazwa = przedrostek + s.wz->nazwa + " (" + s.wz->symbol + ")";
            dodaj(w, mamy, nazwa, wart, l, true);
            break;
        }
        if (znane || ogolnych >= 25) continue;
        // ogólne zgadywanie: tekst z początku wiersza + liczba z jednostką
        std::string nazwa;
        for (size_t i = 0; i < l.size() && nazwa.size() < 42; ++i) {
            if (std::isdigit(static_cast<unsigned char>(l[i])) || l[i] == '=' || l[i] == '@') break;
            if (l[i] == ' ' && i + 1 < l.size() && l[i + 1] == ' ') break;
            nazwa += l[i];
        }
        while (!nazwa.empty() && (nazwa.back() == ' ' || nazwa.back() == ',' || nazwa.back() == '-' || nazwa.back() == '(')) nazwa.pop_back();
        int litery = 0;
        for (unsigned char c : nazwa) if (std::isalpha(c)) ++litery;
        if (litery < 4 || !std::isalpha(static_cast<unsigned char>(nazwa[0]))) continue;
        if (zawiera(nazwa, {"rating", "symbol", "unit", "value", "note", "characteristic", "parameter", "test condition", "min", "typ", "max"}) && litery < 12) continue;
        static const std::regex niczego("^$");
        std::string wart;
        if (!wartoscZWiersza(l, niczego, false, wart)) continue;
        dodaj(w, mamy, przedrostek + nazwa, wart, l, false);
        ++ogolnych;
    }
}

// ===================== uwagi: opis zastosowania =====================

bool tytulInnejSekcji(const std::string& l) {
    static const std::regex re(R"(^(?:[0-9]+\.?\s*)?(features?|key features|description|general description|ordering|order information|pin |absolute|electrical|block diagram|package|typical|specifications?|contents|revision|maximum ratings|benefits|overview|applications? information|functional)\b)", std::regex::icase);
    return std::regex_search(l, re);
}

std::string akapitPoNaglowku(const std::vector<std::string>& L, const std::regex& naglowek) {
    for (size_t i = 0; i < L.size(); ++i) {
        std::smatch m;
        const std::string l = zwin(L[i]);
        if (l.size() > 140 || !std::regex_search(l, m, naglowek)) continue;
        std::string tekst = l.substr(static_cast<size_t>(m[0].length()));
        while (!tekst.empty() && (tekst[0] == ':' || tekst[0] == ' ')) tekst.erase(0, 1);
        int puste = 0, wierszy = 0;
        for (size_t j = i + 1; j < L.size() && wierszy < 10 && tekst.size() < 500; ++j) {
            const std::string x = zwin(L[j]);
            if (x.empty()) { if (++puste >= 2 || (!tekst.empty() && puste >= 1 && wierszy >= 3)) break; continue; }
            puste = 0;
            if (tytulInnejSekcji(x)) break;
            std::string y = x;
            if (y.size() > 2 && y[0] == '-' && y[1] == ' ') y = y.substr(2);
            if (!tekst.empty()) tekst += (tekst.back() == '.' || tekst.back() == ';' || tekst.back() == ':') ? " " : "; ";
            tekst += y;
            ++wierszy;
        }
        tekst = zwin(tekst);
        if (tekst.size() >= 12) return tekst.size() > 500 ? tekst.substr(0, 500) + "..." : tekst;
    }
    return {};
}

void odczytajUwagi(const std::vector<std::string>& L, Wynik& w) {
    static const std::regex zast(R"(^(?:[0-9]+\.?\s*)?(?:typical )?applications?\s*:?\s*)", std::regex::icase);
    static const std::regex opis(R"(^(?:[0-9]+\.?\s*)?(?:general )?description\s*:?\s*)", std::regex::icase);
    std::string t = akapitPoNaglowku(L, zast);
    if (!t.empty()) { w.uwagi = "Zastosowanie: " + t; return; }
    t = akapitPoNaglowku(L, opis);
    if (!t.empty()) w.uwagi = "Opis: " + t;
}

} // namespace

Wynik wyciagnij(const std::string& surowy) {
    Wynik w;
    std::set<std::string> mamy;
    const std::vector<std::string> L = linie(normalizuj(surowy));

    // --- opis: pierwsza linia z początku dokumentu, która wygląda jak zdanie/tytuł
    {
        static const std::regex smiec(R"((copyright|all rights reserved|www\.|http|datasheet$|rev\.? ?[0-9a-z]|page \d|\bdocument\b))", std::regex::icase);
        int sprawdzone = 0;
        for (const std::string& l : L) {
            if (l.empty()) continue;
            if (++sprawdzone > 25) break;
            if (l.size() < 15 || l.size() > 140) continue;
            if (std::regex_search(l, smiec)) continue;
            int litery = 0;
            for (unsigned char c : l) if (std::isalpha(c)) ++litery;
            if (litery < static_cast<int>(l.size()) / 2) continue;
            // dwie lub więcej liter małych/wyrazów - odrzuć same symbole
            if (std::count(l.begin(), l.end(), ' ') < 1) continue;
            w.opis = l;
            break;
        }
    }

    // --- obudowa: pierwszy dopasowany typ w tekście
    {
        static const std::regex re(R"(\b(PDIP|DIP|SOIC|SOP|SSOP|TSSOP|MSOP|SOT|TQFP|LQFP|QFP|QFN|DFN|BGA|LGA|TO)[- ]?([0-9]{1,3}[A-Z]?(?:-[0-9]+)?)\b)");
        for (const std::string& l : L) {
            std::smatch m;
            if (std::regex_search(l, m, re)) {
                w.obudowa = m[1].str() + "-" + m[2].str();
                break;
            }
        }
    }

    static const std::regex liczba(R"(([-+]?[0-9]+(?:[.,][0-9]+)?))");
    // --- napięcie zasilania: "X V to Y V" w linii o zasilaniu
    {
        static const std::regex zakres(R"(([0-9]+(?:[.,][0-9]+)?)\s*V?\s*(?:to|-|~|\.\.\.)\s*([0-9]+(?:[.,][0-9]+)?)\s*V\b)", std::regex::icase);
        for (const std::string& l : L) {
            if (!zawiera(l, {"supply voltage", "operating voltage", "vcc", "vdd", "power supply", "input voltage"})) continue;
            std::smatch m;
            if (std::regex_search(l, m, zakres)) {
                dodaj(w, mamy, "Napięcie zasilania", m[1].str() + " V ... " + m[2].str() + " V", l);
                break;
            }
        }
    }
    // --- temperatura pracy
    {
        static const std::regex zakres(R"(([-+]?[0-9]{1,3})\s*(?:deg\s*C|C)?\s*(?:to|-|\.\.\.|~)\s*\+?([0-9]{1,3})\s*(?:deg\s*)?C\b)", std::regex::icase);
        for (const std::string& l : L) {
            if (!zawiera(l, {"temperature", "temp.", "operating"})) continue;
            std::smatch m;
            if (std::regex_search(l, m, zakres)) {
                int a = std::stoi(m[1].str()), b = std::stoi(m[2].str());
                if (a >= b || a < -100 || b > 300) continue;
                dodaj(w, mamy, "Temperatura pracy", std::to_string(a) + " ... +" + std::to_string(b) + " °C", l);
                break;
            }
        }
    }
    // --- liczba wyprowadzeń
    {
        static const std::regex re(R"(\b([0-9]{1,3})[- ](?:pin|lead|pins|leads)\b)", std::regex::icase);
        for (const std::string& l : L) {
            std::smatch m;
            if (std::regex_search(l, m, re)) {
                const int n = std::stoi(m[1].str());
                if (n >= 2 && n <= 400) { dodaj(w, mamy, "Liczba wyprowadzeń", std::to_string(n), l); break; }
            }
        }
    }
    // --- prąd (wyjściowy / maksymalny)
    {
        static const std::regex re(R"(([0-9]+(?:[.,][0-9]+)?)\s*(mA|A|uA|µA)\b)");
        for (const std::string& l : L) {
            if (!zawiera(l, {"output current", "continuous current", "collector current", "forward current", "max current", "drain current"})) continue;
            std::smatch m;
            if (std::regex_search(l, m, re)) {
                dodaj(w, mamy, "Prąd maksymalny", m[1].str() + " " + m[2].str(), l);
                break;
            }
        }
    }
    // --- napięcie maksymalne
    {
        static const std::regex re(R"(([0-9]+(?:[.,][0-9]+)?)\s*V\b)");
        for (const std::string& l : L) {
            if (!zawiera(l, {"collector-emitter", "drain-source", "breakdown voltage", "reverse voltage", "maximum voltage", "max. voltage"})) continue;
            std::smatch m;
            if (std::regex_search(l, m, re)) {
                dodaj(w, mamy, "Napięcie maksymalne", m[1].str() + " V", l);
                break;
            }
        }
    }
    // --- częstotliwość
    {
        static const std::regex re(R"(([0-9]+(?:[.,][0-9]+)?)\s*(kHz|MHz|GHz)\b)", std::regex::icase);
        for (const std::string& l : L) {
            if (!zawiera(l, {"frequency", "clock", "oscillator", "bandwidth", "speed"})) continue;
            std::smatch m;
            if (std::regex_search(l, m, re)) {
                dodaj(w, mamy, "Częstotliwość", m[1].str() + " " + m[2].str(), l);
                break;
            }
        }
    }
    // --- moc
    {
        static const std::regex re(R"(([0-9]+(?:[.,][0-9]+)?)\s*(mW|W)\b)");
        for (const std::string& l : L) {
            if (!zawiera(l, {"power dissipation", "total power", "power rating"})) continue;
            std::smatch m;
            if (std::regex_search(l, m, re)) {
                dodaj(w, mamy, "Moc maksymalna", m[1].str() + " " + m[2].str(), l);
                break;
            }
        }
    }
    (void)liczba;
    odczytajTabele(L, w, mamy);
    odczytajUwagi(L, w);
    return w;
}

} // namespace OdczytDatasheetu
