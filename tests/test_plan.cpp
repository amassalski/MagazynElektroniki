// Testy najtańszego zestawienia zakupów (bez bazy danych)
#include "Logika/PlanZakupow.h"

#include <cmath>
#include <iostream>

static int sprawdzenia = 0, bledy = 0;
#define CHECK(w) do { ++sprawdzenia; if (!(w)) { ++bledy; std::cerr << "BŁĄD " << __FILE__ << ":" << __LINE__ << ": " #w "\n"; } } while (0)
static bool bliskie(double a, double b) { return std::fabs(a - b) < 0.005; }

static Sklep sklep(long long id, const std::string& n, const std::string& w, double dostawa, std::optional<double> prog = {}) {
    Sklep s; s.id = id; s.nazwa = n; s.waluta = w; s.kosztDostawy = dostawa; s.darmowaDostawaOd = prog; s.priorytet = static_cast<int>(id);
    return s;
}
static LinkSklepu oferta(long long el, long long sk, double cena, int min = 1, const std::string& waluta = "") {
    LinkSklepu l; l.idElementu = el; l.idSklepu = sk; l.cena = cena; l.minimalnaIlosc = min; l.waluta = waluta; l.url = "https://x/" + std::to_string(sk);
    return l;
}

int main() {
    using namespace PlanZakupowLogika;
    const std::map<std::string, double> kursy = {{"PLN", 1.0}, {"USD", 4.0}};
    std::vector<Sklep> sklepy = {sklep(1, "S1", "PLN", 15, 100.0), sklep(2, "S2", "PLN", 10), sklep(3, "S3", "USD", 0)};
    std::vector<PotrzebaZakupu> p = {{1, "A", 10}, {2, "B", 5}, {3, "C", 1}};
    std::vector<LinkSklepu> o = {oferta(1, 1, 1.00), oferta(2, 1, 2.00), oferta(3, 1, 4.50),
                                 oferta(1, 2, 0.95), oferta(2, 2, 1.95)};

    // dostawa
    CHECK(bliskie(kosztDostawyPLN(sklepy[0], 99.99, kursy), 15));
    CHECK(bliskie(kosztDostawyPLN(sklepy[0], 100.0, kursy), 0));
    CHECK(bliskie(kosztDostawyPLN(sklep(9, "U", "USD", 5, 50.0), 199.0, kursy), 20));
    CHECK(bliskie(kosztDostawyPLN(sklep(9, "U", "USD", 5, 50.0), 200.0, kursy), 0));
    CHECK(bliskie(kosztDostawyPLN(sklep(9, "U", "XXX", 5), 10.0, kursy), 0));  // brak kursu: nie zgadujemy

    // najtańsze ceny: A,B w S2, C w S1
    PlanZakupow n = zaplanuj(p, o, sklepy, kursy, TrybPlanu::NajtanszeCeny);
    CHECK(n.kompletny() && n.pozycje.size() == 3);
    CHECK(bliskie(n.towarPLN, 9.5 + 9.75 + 4.5));
    CHECK(bliskie(n.dostawaPLN, 25));
    CHECK(bliskie(n.razemPLN(), 48.75));

    // najtańszy z dostawą: wszystko w S1 (24,5 + 15)
    PlanZakupow t = zaplanuj(p, o, sklepy, kursy, TrybPlanu::Najtanszy);
    CHECK(t.koszyki.size() == 1 && t.koszyki[0].nazwaSklepu == "S1");
    CHECK(bliskie(t.razemPLN(), 39.5));
    CHECK(t.razemPLN() <= n.razemPLN());

    PlanZakupow j = zaplanuj(p, o, sklepy, kursy, TrybPlanu::JedenSklep);
    CHECK(j.kompletny() && j.koszyki.size() == 1 && bliskie(j.razemPLN(), 39.5));

    // próg darmowej dostawy: A x100 w S1 = 100 zł
    std::vector<PotrzebaZakupu> duze = {{1, "A", 100}};
    PlanZakupow d = zaplanuj(duze, o, sklepy, kursy, TrybPlanu::Najtanszy);
    // S1: 100 + 0 dostawy; S2: 95 + 10 = 105
    CHECK(d.koszyki.size() == 1 && d.koszyki[0].nazwaSklepu == "S1" && bliskie(d.razemPLN(), 100));

    // waluta obca i minimalna ilość
    std::vector<LinkSklepu> o2 = {oferta(1, 3, 0.20, 1), oferta(1, 2, 0.95, 100)};
    PlanZakupow w = zaplanuj({{1, "A", 10}}, o2, sklepy, kursy, TrybPlanu::NajtanszeCeny);
    CHECK(w.pozycje.size() == 1 && w.pozycje[0].nazwaSklepu == "S3" && bliskie(w.pozycje[0].cenaPLN, 0.8));
    PlanZakupow w2 = zaplanuj({{1, "A", 10}}, {oferta(1, 2, 0.95, 100)}, sklepy, kursy, TrybPlanu::Najtanszy);
    CHECK(w2.pozycje[0].iloscKupowana == 100 && w2.pozycje[0].iloscPotrzebna == 10 && bliskie(w2.towarPLN, 95));

    // brak oferty, nieaktywny sklep, brak kursu, powtórzony element
    sklepy.push_back(sklep(4, "S4", "CHF", 0));
    sklepy.push_back(sklep(5, "S5", "PLN", 0)); sklepy.back().aktywny = false;
    std::vector<LinkSklepu> o3 = {oferta(1, 4, 1.0), oferta(1, 5, 0.01), oferta(1, 1, 1.0)};
    PlanZakupow b = zaplanuj({{1, "A", 3}, {1, "A", 2}, {7, "D", 1}}, o3, sklepy, kursy, TrybPlanu::Najtanszy);
    CHECK(b.pozycje.size() == 1 && b.pozycje[0].iloscPotrzebna == 5 && b.pozycje[0].nazwaSklepu == "S1");
    CHECK(b.bezOferty.size() == 1 && b.bezOferty[0].symbol == "D" && !b.kompletny());
    CHECK(b.ostrzezenia.size() == 1 && b.ostrzezenia[0].find("CHF") != std::string::npos);
    PlanZakupow nj = zaplanuj(p, {oferta(1, 1, 1.0)}, sklepy, kursy, TrybPlanu::JedenSklep);
    CHECK(nj.pozycje.empty() && nj.bezOferty.size() == 3);

    // brak danych
    CHECK(zaplanuj({}, o, sklepy, kursy, TrybPlanu::Najtanszy).pozycje.empty());

    // raport i lista
    Raport r = naRaport(t, "Zakupy");
    CHECK(r.wiersze.size() == 3 && r.wiersze[0].size() == r.naglowki.size());
    CHECK(r.wiersze[0][4] == "1,00");
    CHECK(r.podsumowanie.back().find("39,50") != std::string::npos);
    const std::string lista = naListeZamowien(t);
    CHECK(lista.find("S1:") == 0 && lista.find("10 x A") != std::string::npos);

    // wiele sklepów (powyżej 12: poprawki lokalne, bez wybuchu)
    std::vector<Sklep> duzo; std::vector<LinkSklepu> oo;
    for (int i = 1; i <= 15; ++i) { duzo.push_back(sklep(i, "M" + std::to_string(i), "PLN", 5)); oo.push_back(oferta(1, i, 1.0 + i * 0.01)); oo.push_back(oferta(2, i, 2.0 - i * 0.01)); }
    PlanZakupow m = zaplanuj({{1, "A", 1}, {2, "B", 1}}, oo, duzo, kursy, TrybPlanu::Najtanszy);
    CHECK(m.kompletny() && m.koszyki.size() == 1);   // jedna dostawa 5 zł jest tańsza niż dwie

    std::cout << sprawdzenia << " sprawdzeń, " << bledy << " błędów\n";
    return bledy == 0 ? 0 : 1;
}
