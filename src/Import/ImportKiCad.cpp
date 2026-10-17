#include "ImportKiCad.h"

#include "MiniXML.h"

#include <algorithm>
#include <cctype>

namespace ImportKiCad {

namespace {

std::string male(std::string s) {
    for (char& c : s) {
        if (static_cast<unsigned char>(c) < 128) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

std::string przytnij(const std::string& s) {
    const char* sp = " \t\r\n";
    const size_t a = s.find_first_not_of(sp);
    if (a == std::string::npos) return std::string();
    return s.substr(a, s.find_last_not_of(sp) - a + 1);
}

bool jednaZ(const std::string& nazwa, std::initializer_list<const char*> lista) {
    const std::string k = male(przytnij(nazwa));
    for (const char* n : lista) {
        if (k == n) return true;
    }
    return false;
}

bool prawda(const std::string& s) {
    const std::string t = male(przytnij(s));
    return t == "1" || t == "yes" || t == "true" || t == "tak" || t == "x";
}

} // namespace

bool parsujNetliste(const std::string& xml, WynikImportuBOM& wynik, std::string& blad) {
    auto korzen = MiniXML::parsuj(xml, blad);
    if (!korzen) return false;
    if (korzen->nazwa != "export") {
        blad = "To nie jest netlista KiCada (brak elementu <export>).";
        return false;
    }
    const MiniXML::Wezel* komponenty = korzen->dziecko("components");
    if (!komponenty) {
        blad = "W netliście nie ma sekcji <components>.";
        return false;
    }

    std::vector<PozycjaBOM> surowe;
    int pominiete = 0;
    for (const MiniXML::Wezel* comp : komponenty->dzieciONazwie("comp")) {
        PozycjaBOM p;
        p.oznaczenia = comp->atrybut("ref");
        if (p.oznaczenia.empty() || p.oznaczenia[0] == '#') continue; // symbole zasilania itp.

        auto tekstDziecka = [&](const char* nazwa) {
            const MiniXML::Wezel* d = comp->dziecko(nazwa);
            return d ? przytnij(d->tekst) : std::string();
        };
        p.wartosc = tekstDziecka("value");
        p.obudowa = tekstDziecka("footprint");
        if (const MiniXML::Wezel* lib = comp->dziecko("libsource")) p.opis = przytnij(lib->atrybut("description"));

        bool pomin = false;
        if (const MiniXML::Wezel* pola = comp->dziecko("fields")) {
            for (const MiniXML::Wezel* pole : pola->dzieciONazwie("field")) {
                const std::string nazwa = pole->atrybut("name");
                const std::string wartosc = przytnij(pole->tekst);
                if (jednaZ(nazwa, {"mpn", "manufacturer part number", "manufacturer_part_number",
                                   "manufacturer part", "mfr part", "mfr_part", "partnumber", "part number", "pn"})) {
                    if (p.symbol.empty()) p.symbol = wartosc;
                } else if (jednaZ(nazwa, {"manufacturer", "manufacturer_name", "mfr", "mfg", "producent"})) {
                    if (p.producent.empty()) p.producent = wartosc;
                } else if (jednaZ(nazwa, {"description", "opis"}) && p.opis.empty()) {
                    p.opis = wartosc;
                } else if (jednaZ(nazwa, {"dnp", "do not populate", "do_not_populate"}) && prawda(wartosc)) {
                    pomin = true;
                }
            }
        }
        for (const MiniXML::Wezel* wlasciwosc : comp->dzieciONazwie("property")) {
            const std::string nazwa = male(wlasciwosc->atrybut("name"));
            if (nazwa == "dnp" || nazwa == "exclude_from_bom" || nazwa == "exclude_from_board") {
                if (nazwa != "exclude_from_board") pomin = true;
            }
        }
        if (pomin) {
            ++pominiete;
            continue;
        }
        p.ilosc = 1;
        surowe.push_back(p);
    }
    if (pominiete > 0) {
        wynik.ostrzezenia.push_back("Pominięto " + std::to_string(pominiete) +
                                    " elementów oznaczonych jako niemontowane lub wyłączone z BOM.");
    }
    wynik.pozycje = ImportBOM::zgrupuj(surowe, &wynik.ostrzezenia);
    if (wynik.format.empty()) wynik.format = "KiCad (netlista XML)";
    if (wynik.pozycje.empty()) {
        blad = "W netliście nie znaleziono żadnych elementów.";
        return false;
    }
    return true;
}

} // namespace ImportKiCad
