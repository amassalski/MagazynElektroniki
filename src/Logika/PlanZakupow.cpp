#include "PlanZakupow.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>

namespace PlanZakupowLogika {

namespace {

struct Oferta {
    const LinkSklepu* link;
    const Sklep* sklep;
    double cenaPLN;
};

std::string pln(double v) {
    char b[64];
    std::snprintf(b, sizeof b, "%.2f", v);
    std::string s = b;
    const size_t k = s.find('.');
    if (k != std::string::npos) s[k] = ',';
    return s;
}

double kurs(const std::map<std::string, double>& kursy, const std::string& waluta, bool& ok) {
    auto it = kursy.find(waluta);
    if (it == kursy.end() || !(it->second > 0)) { ok = false; return 0; }
    ok = true;
    return it->second;
}

/// Wynik przypisania: indeks wybranej oferty dla każdej potrzeby (-1 = brak)
struct Przydzial {
    std::vector<int> oferta;
    double razem = 0;
    bool kompletny = true;
};

class Planista {
public:
    Planista(const std::vector<PotrzebaZakupu>& potrzeby, const std::vector<Oferta>& oferty,
             const std::map<std::string, double>& kursy)
        : potrzeby_(potrzeby), oferty_(oferty), kursy_(kursy) {
        dlaPotrzeby_.resize(potrzeby.size());
        for (size_t i = 0; i < oferty_.size(); ++i)
            for (size_t p = 0; p < potrzeby_.size(); ++p)
                if (oferty_[i].link->idElementu == potrzeby_[p].idElementu) dlaPotrzeby_[p].push_back(static_cast<int>(i));
    }

    int ilosc(size_t potrzeba, int oferta) const {
        const int min = std::max(1, oferty_[oferta].link->minimalnaIlosc);
        return std::max(potrzeby_[potrzeba].ilosc, min);
    }
    double koszt(size_t potrzeba, int oferta) const { return oferty_[oferta].cenaPLN * ilosc(potrzeba, oferta); }

    bool maOferty(size_t p) const { return !dlaPotrzeby_[p].empty(); }

    /// Suma z dostawą dla przydziału
    double razem(const std::vector<int>& wybor) const {
        std::map<long long, double> towar;
        for (size_t p = 0; p < wybor.size(); ++p)
            if (wybor[p] >= 0) towar[oferty_[wybor[p]].sklep->id] += koszt(p, wybor[p]);
        double suma = 0;
        for (const auto& kv : towar) {
            const Sklep* s = nullptr;
            for (const Oferta& o : oferty_) if (o.sklep->id == kv.first) { s = o.sklep; break; }
            suma += kv.second + kosztDostawyPLN(*s, kv.second, kursy_);
        }
        return suma;
    }

    /// Każda część w najtańszym sklepie z podanego zbioru (pusty zbiór = wszystkie)
    bool najtanszeW(const std::set<long long>& sklepy, std::vector<int>& wybor) const {
        wybor.assign(potrzeby_.size(), -1);
        for (size_t p = 0; p < potrzeby_.size(); ++p) {
            if (!maOferty(p)) continue;
            double naj = 0;
            for (int o : dlaPotrzeby_[p]) {
                if (!sklepy.empty() && !sklepy.count(oferty_[o].sklep->id)) continue;
                const double k = koszt(p, o);
                if (wybor[p] < 0 || k < naj - 1e-12 ||
                    (std::abs(k - naj) < 1e-12 && oferty_[o].sklep->priorytet < oferty_[wybor[p]].sklep->priorytet)) {
                    wybor[p] = o;
                    naj = k;
                }
            }
            if (wybor[p] < 0) return false;
        }
        return true;
    }

    std::vector<long long> sklepyZOfertami() const {
        std::set<long long> s;
        for (const Oferta& o : oferty_) s.insert(o.sklep->id);
        return std::vector<long long>(s.begin(), s.end());
    }

    /// Najtańszy układ z dostawą: dokładnie po podzbiorach sklepów (do 12), potem poprawki lokalne
    std::vector<int> najtanszyZDostawa() const {
        std::vector<int> najlepszy;
        najtanszeW({}, najlepszy);
        double naj = razem(najlepszy);

        const std::vector<long long> sk = sklepyZOfertami();
        if (sk.size() <= 12) {
            for (unsigned maska = 1; maska < (1u << sk.size()); ++maska) {
                std::set<long long> zb;
                for (size_t i = 0; i < sk.size(); ++i) if (maska & (1u << i)) zb.insert(sk[i]);
                std::vector<int> w;
                if (!najtanszeW(zb, w)) continue;
                const double r = razem(w);
                if (r < naj - 1e-9) { naj = r; najlepszy = w; }
            }
        }
        // poprawki: przenoszenie pojedynczych części (też przy ponad 12 sklepach)
        bool poprawa = true;
        while (poprawa) {
            poprawa = false;
            for (size_t p = 0; p < najlepszy.size(); ++p) {
                for (int o : dlaPotrzeby_[p]) {
                    if (o == najlepszy[p]) continue;
                    std::vector<int> w = najlepszy;
                    w[p] = o;
                    const double r = razem(w);
                    if (r < naj - 1e-9) { naj = r; najlepszy = w; poprawa = true; }
                }
            }
        }
        return najlepszy;
    }

    std::optional<std::vector<int>> jedenSklep() const {
        std::optional<std::vector<int>> wynik;
        double naj = 0;
        for (long long s : sklepyZOfertami()) {
            std::vector<int> w;
            bool wszystko = true;
            for (size_t p = 0; p < potrzeby_.size() && wszystko; ++p) {
                w.push_back(-1);
                for (int o : dlaPotrzeby_[p]) if (oferty_[o].sklep->id == s) { w[p] = o; break; }
                if (w[p] < 0) wszystko = false;
            }
            if (!wszystko) continue;
            const double r = razem(w);
            if (!wynik || r < naj) { wynik = w; naj = r; }
        }
        return wynik;
    }

private:
    const std::vector<PotrzebaZakupu>& potrzeby_;
    const std::vector<Oferta>& oferty_;
    const std::map<std::string, double>& kursy_;
    std::vector<std::vector<int>> dlaPotrzeby_;
};

} // namespace

double kosztDostawyPLN(const Sklep& sklep, double towarPLN, const std::map<std::string, double>& kursy) {
    bool ok = false;
    const double k = kurs(kursy, sklep.waluta, ok);
    if (!ok) return 0;
    if (sklep.darmowaDostawaOd && towarPLN + 1e-9 >= *sklep.darmowaDostawaOd * k) return 0;
    return sklep.kosztDostawy * k;
}

PlanZakupow zaplanuj(const std::vector<PotrzebaZakupu>& potrzebyWej, const std::vector<LinkSklepu>& linki,
                     const std::vector<Sklep>& sklepy, const std::map<std::string, double>& kursy, TrybPlanu tryb) {
    PlanZakupow plan;
    // ta sama część dwa razy (z kilku projektów): sumujemy
    std::vector<PotrzebaZakupu> potrzeby;
    for (const PotrzebaZakupu& p : potrzebyWej) {
        if (p.ilosc <= 0) continue;
        auto it = std::find_if(potrzeby.begin(), potrzeby.end(),
                               [&](const PotrzebaZakupu& x) { return x.idElementu == p.idElementu; });
        if (it == potrzeby.end()) potrzeby.push_back(p);
        else it->ilosc += p.ilosc;
    }

    std::vector<Oferta> oferty;
    std::set<std::string> bezKursu;
    for (const LinkSklepu& l : linki) {
        if (!l.cena) continue;
        auto s = std::find_if(sklepy.begin(), sklepy.end(), [&](const Sklep& x) { return x.id == l.idSklepu; });
        if (s == sklepy.end() || !s->aktywny) continue;
        bool ok = false;
        const std::string waluta = l.waluta.empty() ? s->waluta : l.waluta;
        const double k = kurs(kursy, waluta, ok);
        if (!ok) { bezKursu.insert(waluta); continue; }
        oferty.push_back({&l, &*s, *l.cena * k});
    }
    for (const std::string& w : bezKursu)
        plan.ostrzezenia.push_back("Brak kursu waluty " + w + ": oferty w tej walucie pominięto.");

    Planista planista(potrzeby, oferty, kursy);
    std::vector<int> wybor;
    if (tryb == TrybPlanu::NajtanszeCeny) {
        planista.najtanszeW({}, wybor);
    } else if (tryb == TrybPlanu::Najtanszy) {
        wybor = planista.najtanszyZDostawa();
    } else {
        auto j = planista.jedenSklep();
        if (j) wybor = *j;
        else {
            plan.bezOferty = potrzeby;
            plan.ostrzezenia.push_back("Żaden sklep nie ma wszystkich części z ceną.");
            return plan;
        }
    }
    if (wybor.size() != potrzeby.size()) wybor.assign(potrzeby.size(), -1);

    std::map<long long, KoszykSklepu> koszyki;
    for (size_t p = 0; p < potrzeby.size(); ++p) {
        if (wybor[p] < 0) { plan.bezOferty.push_back(potrzeby[p]); continue; }
        const Oferta& o = oferty[wybor[p]];
        PozycjaPlanu pp;
        pp.idElementu = potrzeby[p].idElementu;
        pp.symbol = potrzeby[p].symbol;
        pp.idSklepu = o.sklep->id;
        pp.nazwaSklepu = o.sklep->nazwa;
        pp.iloscPotrzebna = potrzeby[p].ilosc;
        pp.iloscKupowana = planista.ilosc(p, wybor[p]);
        pp.cenaPLN = o.cenaPLN;
        pp.wartoscPLN = o.cenaPLN * pp.iloscKupowana;
        pp.url = o.link->url;
        plan.pozycje.push_back(pp);
        KoszykSklepu& k = koszyki[pp.idSklepu];
        k.idSklepu = pp.idSklepu;
        k.nazwaSklepu = pp.nazwaSklepu;
        ++k.liczbaPozycji;
        k.towarPLN += pp.wartoscPLN;
    }
    for (auto& kv : koszyki) {
        const Sklep* s = nullptr;
        for (const Sklep& x : sklepy) if (x.id == kv.first) s = &x;
        kv.second.dostawaPLN = s ? kosztDostawyPLN(*s, kv.second.towarPLN, kursy) : 0;
        plan.towarPLN += kv.second.towarPLN;
        plan.dostawaPLN += kv.second.dostawaPLN;
        plan.koszyki.push_back(kv.second);
    }
    std::sort(plan.koszyki.begin(), plan.koszyki.end(), [](const KoszykSklepu& a, const KoszykSklepu& b) {
        return a.razemPLN() > b.razemPLN();
    });
    std::stable_sort(plan.pozycje.begin(), plan.pozycje.end(), [](const PozycjaPlanu& a, const PozycjaPlanu& b) {
        if (a.nazwaSklepu != b.nazwaSklepu) return a.nazwaSklepu < b.nazwaSklepu;
        return a.symbol < b.symbol;
    });
    return plan;
}

Raport naRaport(const PlanZakupow& plan, const std::string& tytul) {
    Raport r;
    r.tytul = tytul;
    r.naglowki = {"Sklep", "Symbol", "Potrzeba", "Kupić", "Cena (PLN)", "Wartość (PLN)"};
    r.wyrownajDoPrawej = {false, false, true, true, true, true};
    for (const PozycjaPlanu& p : plan.pozycje)
        r.wiersze.push_back({p.nazwaSklepu, p.symbol, std::to_string(p.iloscPotrzebna), std::to_string(p.iloscKupowana),
                             pln(p.cenaPLN), pln(p.wartoscPLN)});
    for (const KoszykSklepu& k : plan.koszyki)
        r.podsumowanie.push_back(k.nazwaSklepu + ": towar " + pln(k.towarPLN) + " + dostawa " + pln(k.dostawaPLN) +
                                 " = " + pln(k.razemPLN()) + " PLN");
    r.podsumowanie.push_back("Razem: towar " + pln(plan.towarPLN) + " + dostawa " + pln(plan.dostawaPLN) + " = " +
                             pln(plan.razemPLN()) + " PLN");
    for (const PotrzebaZakupu& b : plan.bezOferty)
        r.podsumowanie.push_back("Brak oferty: " + b.symbol + " (" + std::to_string(b.ilosc) + " szt.)");
    for (const std::string& o : plan.ostrzezenia) r.podsumowanie.push_back("Uwaga: " + o);
    return r;
}

std::string naListeZamowien(const PlanZakupow& plan) {
    std::string w;
    std::string biezacy;
    for (const PozycjaPlanu& p : plan.pozycje) {
        if (p.nazwaSklepu != biezacy) {
            if (!w.empty()) w += "\n";
            w += p.nazwaSklepu + ":\n";
            biezacy = p.nazwaSklepu;
        }
        w += "  " + std::to_string(p.iloscKupowana) + " x " + p.symbol + (p.url.empty() ? "" : "  " + p.url) + "\n";
    }
    return w;
}

} // namespace PlanZakupowLogika
