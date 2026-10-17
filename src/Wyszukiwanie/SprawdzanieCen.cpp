#include "SprawdzanieCen.h"

#include "KlientHTTP.h"

#include <algorithm>

SprawdzaczCen::SprawdzaczCen(LogikaSklepow& logika) : logika_(logika), zegar_(this) {
    Bind(wxEVT_TIMER, &SprawdzaczCen::NaCzas, this);
}

SprawdzaczCen::~SprawdzaczCen() {
    ++id_;   // ewentualna odpowiedź w drodze nie odwoła się do zniszczonego obiektu
    zegar_.Stop();
}

std::vector<ZadanieCeny> SprawdzaczCen::ZadaniaDlaLinkow(const std::vector<LinkSklepu>& linki, const std::vector<Sklep>& sklepy,
                                                          const std::string& symbol) {
    std::vector<ZadanieCeny> z;
    for (const LinkSklepu& l : linki) {
        if (l.url.empty()) continue;
        ZadanieCeny zc;
        zc.idLinku = l.id;
        zc.url = l.url;
        zc.opis = symbol + " / " + l.nazwaSklepu;
        auto s = std::find_if(sklepy.begin(), sklepy.end(), [&](const Sklep& x) { return x.id == l.idSklepu; });
        if (s != sklepy.end()) {
            zc.wzorzec = s->regexCeny;
            zc.walutaSklepu = s->waluta;
        }
        if (zc.walutaSklepu.empty()) zc.walutaSklepu = l.waluta;
        z.push_back(zc);
    }
    return z;
}

bool SprawdzaczCen::Start(std::vector<ZadanieCeny> zadania, Postep postep, Koniec koniec, int przerwaMs) {
    if (wToku_ || zadania.empty()) return false;
    zadania_ = std::move(zadania);
    postep_ = std::move(postep);
    koniec_ = std::move(koniec);
    przerwaMs_ = std::max(0, przerwaMs);
    nr_ = 0;
    anulowano_ = false;
    wToku_ = true;
    wLocie_ = false;
    ++id_;
    Nastepne();
    return true;
}

void SprawdzaczCen::Anuluj() {
    if (!wToku_) return;
    anulowano_ = true;
    zegar_.Stop();
    // trwające zapytanie dokończy się samo (i zakończy całość); nowych nie zaczynamy
    if (!wLocie_) Zakoncz();
}

void SprawdzaczCen::NaCzas(wxTimerEvent&) {
    if (wToku_ && !anulowano_) Nastepne();
}

void SprawdzaczCen::Nastepne() {
    if (anulowano_ || nr_ >= zadania_.size()) { Zakoncz(); return; }
    const size_t numer = nr_;
    const long uruchomienie = id_;
    const ZadanieCeny zad = zadania_[numer];
    wLocie_ = true;
    KlientHTTP::pobierzStrone(zad.url, [this, numer, uruchomienie, zad](const WynikHTTP& odp) {
        if (uruchomienie != id_ || !wToku_) return;
        wLocie_ = false;
        WynikSprawdzenia w;
        w.zadanie = zad;
        if (!odp.ok) {
            w.komunikat = odp.blad.empty() ? "Nie udało się pobrać strony." : odp.blad;
        } else {
            const WynikCeny c = ParserStrony::znajdzCene(odp.tresc, zad.wzorzec);
            if (!c.znaleziono) {
                w.komunikat = c.blad.empty() ? "Nie znaleziono ceny na stronie. Jeśli to strona wyników wyszukiwania, "
                                               "wklej adres strony samego produktu; inaczej ustaw wzorzec ceny sklepu."
                                             : c.blad;
            } else {
                w.cena = c.cena;
                w.waluta = c.waluta.empty() ? zad.walutaSklepu : c.waluta;
                if (logika_.ustawCene(zad.idLinku, c.cena, w.waluta, c.dostepnosc, "strona")) {
                    w.zapisano = true;
                    w.komunikat = "Cena zapisana (" + c.zrodlo + ").";
                } else {
                    w.komunikat = logika_.pobierzBlad();
                }
            }
        }
        nr_ = numer + 1;
        if (postep_) postep_(static_cast<int>(numer) + 1, static_cast<int>(zadania_.size()), w);
        if (uruchomienie != id_ || !wToku_) return;   // postęp mógł zamknąć okno
        if (anulowano_ || nr_ >= zadania_.size()) Zakoncz();
        else zegar_.StartOnce(std::max(1, przerwaMs_));
    });
}

void SprawdzaczCen::Zakoncz() {
    if (!wToku_) return;
    wToku_ = false;
    zegar_.Stop();
    Koniec k = std::move(koniec_);
    const bool anul = anulowano_;
    if (k) k(anul);
}
