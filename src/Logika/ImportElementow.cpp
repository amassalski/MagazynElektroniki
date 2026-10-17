#include "ImportElementow.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>

namespace {

std::string przytnij(const std::string& s) {
    const char* sp = " \t\r\n";
    const size_t a = s.find_first_not_of(sp);
    if (a == std::string::npos) return std::string();
    return s.substr(a, s.find_last_not_of(sp) - a + 1);
}

std::string male(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

} // namespace

std::vector<std::string> ImportElementow::rozbijSciezke(const std::string& sciezka) {
    std::vector<std::string> czesci;
    std::string biezaca;
    auto zakoncz = [&] {
        const std::string t = przytnij(biezaca);
        if (!t.empty()) czesci.push_back(t);
        biezaca.clear();
    };
    for (size_t i = 0; i < sciezka.size(); ++i) {
        if (sciezka[i] == '>') { zakoncz(); continue; }
        // separator " / " (ze spacjami); ukośnik bez spacji należy do nazwy, np. "ADC/DAC"
        if (sciezka[i] == '/' && i > 0 && i + 1 < sciezka.size() && sciezka[i - 1] == ' ' && sciezka[i + 1] == ' ') {
            zakoncz();
            continue;
        }
        biezaca += sciezka[i];
    }
    zakoncz();
    return czesci;
}

bool ImportElementow::wykonaj(const std::vector<PozycjaImportu>& pozycje, TrybDuplikatow tryb,
                              WynikImportuElementow& wynik) {
    blad_.clear();
    wynik = WynikImportuElementow();

    // 1. kategorie: tworzymy brakujące, potem mapujemy pełną nazwę -> id
    std::vector<std::vector<std::string>> sciezki;
    std::set<std::string> widziane;
    for (const PozycjaImportu& p : pozycje) {
        if (!p.blad.empty() || przytnij(p.kategoria).empty()) continue;
        if (!widziane.insert(male(p.kategoria)).second) continue;
        auto s = rozbijSciezke(p.kategoria);
        if (!s.empty()) sciezki.push_back(s);
    }
    if (!sciezki.empty()) {
        int dodano = 0;
        if (!magazyn_.dodajSciezkiKategorii(sciezki, &dodano)) {
            blad_ = magazyn_.pobierzBlad();
            return false;
        }
        wynik.utworzonoKategorii = dodano;
    }
    if (!magazyn_.odswiezKategorie()) {
        blad_ = magazyn_.pobierzBlad();
        return false;
    }
    std::map<std::string, long long> kategorie;
    for (const Kategoria& k : magazyn_.kategorie()) kategorie[male(k.pelnaNazwa)] = k.id;

    // 2. elementy
    for (const PozycjaImportu& p : pozycje) {
        const std::string opis = (p.nrWiersza ? "wiersz " + std::to_string(p.nrWiersza) + " (" : "(") + p.element.symbol + ")";
        if (!p.blad.empty()) {
            wynik.bledy.push_back(opis + ": " + p.blad);
            continue;
        }
        Element nowy = p.element;
        nowy.symbol = przytnij(nowy.symbol);
        if (nowy.symbol.empty()) {
            wynik.bledy.push_back(opis + ": brak symbolu");
            continue;
        }
        std::optional<long long> idKat;
        if (!przytnij(p.kategoria).empty()) {
            const auto s = rozbijSciezke(p.kategoria);
            std::string pelna;
            for (size_t i = 0; i < s.size(); ++i) pelna += (i ? " / " : "") + s[i];
            auto it = kategorie.find(male(pelna));
            if (it != kategorie.end()) idKat = it->second;
        }

        auto istniejacy = magazyn_.znajdzPoSymbolu(nowy.symbol);
        if (!istniejacy) {
            nowy.id = 0;
            nowy.idKategorii = idKat;
            if (!magazyn_.zapiszElement(nowy)) wynik.bledy.push_back(opis + ": " + magazyn_.pobierzBlad());
            else ++wynik.dodano;
            continue;
        }
        switch (tryb) {
            case TrybDuplikatow::Pomin:
                ++wynik.pominieto;
                break;
            case TrybDuplikatow::DodajIlosc:
                if (nowy.iloscDostepna <= 0) { ++wynik.pominieto; break; }
                if (!magazyn_.zmienIlosc(istniejacy->id, nowy.iloscDostepna, "PRZYJECIE", "Import zestawu"))
                    wynik.bledy.push_back(opis + ": " + magazyn_.pobierzBlad());
                else ++wynik.zsumowano;
                break;
            case TrybDuplikatow::Aktualizuj: {
                Element e = *istniejacy;
                auto uzupelnij = [](std::string& cel, const std::string& zrodlo) { if (!przytnij(zrodlo).empty()) cel = przytnij(zrodlo); };
                uzupelnij(e.symbolProducenta, nowy.symbolProducenta);
                uzupelnij(e.producent, nowy.producent);
                uzupelnij(e.opis, nowy.opis);
                uzupelnij(e.wartosc, nowy.wartosc);
                uzupelnij(e.jednostka, nowy.jednostka);
                uzupelnij(e.obudowa, nowy.obudowa);
                uzupelnij(e.tolerancja, nowy.tolerancja);
                uzupelnij(e.parametry, nowy.parametry);
                uzupelnij(e.datasheetUrl, nowy.datasheetUrl);
                uzupelnij(e.tagi, nowy.tagi);
                uzupelnij(e.kontener, nowy.kontener);
                uzupelnij(e.szuflada, nowy.szuflada);
                uzupelnij(e.przegroda, nowy.przegroda);
                if (nowy.iloscMinimalna > 0) e.iloscMinimalna = nowy.iloscMinimalna;
                if (nowy.cena) { e.cena = nowy.cena; e.waluta = nowy.waluta; }
                if (idKat) e.idKategorii = idKat;
                if (!magazyn_.zapiszElement(e)) wynik.bledy.push_back(opis + ": " + magazyn_.pobierzBlad());
                else ++wynik.zaktualizowano;
                break;
            }
        }
    }
    return true;
}
