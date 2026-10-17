#include "PomocKontekstowa.h"

#include <fstream>
#include <sstream>

namespace {

std::string przytnij(const std::string& s) {
    const char* sp = " \t\r\n";
    const size_t a = s.find_first_not_of(sp);
    if (a == std::string::npos) return std::string();
    return s.substr(a, s.find_last_not_of(sp) - a + 1);
}

} // namespace

PomocKontekstowa& PomocKontekstowa::pobierz() {
    static PomocKontekstowa instancja;
    return instancja;
}

void PomocKontekstowa::wyczysc() {
    wpisy_.clear();
}

int PomocKontekstowa::wczytajZTekstu(const std::string& tekst, std::vector<std::string>* bledy) {
    std::istringstream in(tekst);
    std::string linia;
    int nr = 0, wczytano = 0;
    while (std::getline(in, linia)) {
        ++nr;
        if (nr == 1 && linia.compare(0, 3, "\xEF\xBB\xBF") == 0) linia.erase(0, 3);  // BOM
        if (!linia.empty() && linia.back() == '\r') linia.pop_back();
        if (przytnij(linia).empty() || linia[0] == '#') continue;

        std::vector<std::string> pola;
        size_t start = 0;
        while (true) {
            const size_t tab = linia.find('\t', start);
            if (tab == std::string::npos) { pola.push_back(linia.substr(start)); break; }
            pola.push_back(linia.substr(start, tab - start));
            start = tab + 1;
        }
        const std::string klucz = pola.empty() ? std::string() : przytnij(pola[0]);
        if (pola.size() < 3 || klucz.empty() || przytnij(pola[2]).empty()) {
            if (bledy) bledy->push_back("wiersz " + std::to_string(nr) + ": oczekiwano klucza, tytułu i opisu");
            continue;
        }
        WpisPomocy w;
        w.tytul = przytnij(pola[1]);
        w.opis = przytnij(pola[2]);
        if (pola.size() > 3) w.kotwica = przytnij(pola[3]);
        wpisy_[klucz] = w;
        ++wczytano;
    }
    return wczytano;
}

int PomocKontekstowa::wczytajZPliku(const std::string& sciezka, std::vector<std::string>* bledy) {
    std::ifstream plik(sciezka, std::ios::binary);
    if (!plik) return -1;
    std::stringstream bufor;
    bufor << plik.rdbuf();
    return wczytajZTekstu(bufor.str(), bledy);
}

int PomocKontekstowa::wczytajZBazy(ManagerBazyDanych& manager) {
    PolaczenieBazy* pol = manager.pobierzPolaczenie();
    if (!pol || !pol->czyPolaczony()) return -1;
    auto odp = pol->zapytanie("SELECT klucz, tytul, opis, COALESCE(kotwica, '') FROM tbl_pomoc_pol");
    if (!odp) return -1;   // tabela jeszcze nie istnieje (stara struktura bazy)
    for (int i = 0; i < odp->liczbaWierszy(); ++i) {
        WpisPomocy w;
        w.tytul = odp->tekst(i, 1);
        w.opis = odp->tekst(i, 2);
        w.kotwica = odp->tekst(i, 3);
        wpisy_[odp->tekst(i, 0)] = w;
    }
    return odp->liczbaWierszy();
}

const WpisPomocy* PomocKontekstowa::znajdz(const std::string& klucz) const {
    auto it = wpisy_.find(klucz);
    return it == wpisy_.end() ? nullptr : &it->second;
}
