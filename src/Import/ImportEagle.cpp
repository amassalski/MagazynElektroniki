#include "ImportEagle.h"

#include "MiniXML.h"

#include <algorithm>
#include <cctype>
#include <map>

namespace ImportEagle {

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

const MiniXML::Wezel* idz(const MiniXML::Wezel* w, std::initializer_list<const char*> sciezka) {
    for (const char* nazwa : sciezka) {
        if (!w) return nullptr;
        w = w->dziecko(nazwa);
    }
    return w;
}

/// Przypisuje atrybut do pola pozycji, jeśli jego nazwa jest znana
void zastosujAtrybut(PozycjaBOM& p, const std::string& nazwa, const std::string& wartosc, bool& dnp) {
    const std::string v = przytnij(wartosc);
    if (v.empty() || v == "!") return;
    if (jednaZ(nazwa, {"mpn", "manufacturer_part_number", "manufacturer part number", "mfr_part",
                       "partnumber", "part_number", "pn"})) {
        if (p.symbol.empty()) p.symbol = v;
    } else if (jednaZ(nazwa, {"manufacturer", "mfr", "mfg", "manufacturer_name", "producent"})) {
        if (p.producent.empty()) p.producent = v;
    } else if (jednaZ(nazwa, {"description", "opis"})) {
        if (p.opis.empty()) p.opis = v;
    } else if (jednaZ(nazwa, {"dnp", "do_not_populate"})) {
        const std::string t = male(v);
        if (t == "1" || t == "yes" || t == "true" || t == "dnp" || t == "x") dnp = true;
    }
}

} // namespace

bool parsujSchemat(const std::string& xml, WynikImportuBOM& wynik, std::string& blad) {
    auto korzen = MiniXML::parsuj(xml, blad);
    if (!korzen) return false;
    if (korzen->nazwa != "eagle") {
        blad = "To nie jest plik Eagle (brak elementu <eagle>).";
        return false;
    }
    const MiniXML::Wezel* schemat = idz(korzen.get(), {"drawing", "schematic"});
    if (!schemat) {
        blad = "Plik Eagle nie zawiera schematu (to może być płytka .brd, a potrzebny jest .sch).";
        return false;
    }
    const MiniXML::Wezel* czesci = schemat->dziecko("parts");
    if (!czesci) {
        blad = "W schemacie nie ma sekcji <parts>.";
        return false;
    }

    // Biblioteki: biblioteka -> deviceset -> device
    std::map<std::string, const MiniXML::Wezel*> biblioteki;
    if (const MiniXML::Wezel* lib = schemat->dziecko("libraries")) {
        for (const MiniXML::Wezel* l : lib->dzieciONazwie("library")) biblioteki[l->atrybut("name")] = l;
    }

    auto znajdzUrzadzenie = [&](const MiniXML::Wezel& czesc) -> const MiniXML::Wezel* {
        auto it = biblioteki.find(czesc.atrybut("library"));
        if (it == biblioteki.end()) return nullptr;
        const MiniXML::Wezel* zestawy = it->second->dziecko("devicesets");
        if (!zestawy) return nullptr;
        for (const MiniXML::Wezel* zestaw : zestawy->dzieciONazwie("deviceset")) {
            if (zestaw->atrybut("name") != czesc.atrybut("deviceset")) continue;
            const MiniXML::Wezel* urzadzenia = zestaw->dziecko("devices");
            if (!urzadzenia) return nullptr;
            for (const MiniXML::Wezel* u : urzadzenia->dzieciONazwie("device")) {
                if (u->atrybut("name") == czesc.atrybut("device")) return u;
            }
        }
        return nullptr;
    };
    auto znajdzZestaw = [&](const MiniXML::Wezel& czesc) -> const MiniXML::Wezel* {
        auto it = biblioteki.find(czesc.atrybut("library"));
        if (it == biblioteki.end()) return nullptr;
        const MiniXML::Wezel* zestawy = it->second->dziecko("devicesets");
        if (!zestawy) return nullptr;
        for (const MiniXML::Wezel* zestaw : zestawy->dzieciONazwie("deviceset")) {
            if (zestaw->atrybut("name") == czesc.atrybut("deviceset")) return zestaw;
        }
        return nullptr;
    };

    std::vector<PozycjaBOM> surowe;
    int bezObudowy = 0, pominiete = 0;
    for (const MiniXML::Wezel* czesc : czesci->dzieciONazwie("part")) {
        const MiniXML::Wezel* urzadzenie = znajdzUrzadzenie(*czesc);
        // części bez obudowy (masa, zasilanie, symbole) nie są elementami fizycznymi
        if (!urzadzenie || urzadzenie->atrybut("package").empty()) {
            ++bezObudowy;
            continue;
        }
        PozycjaBOM p;
        p.oznaczenia = czesc->atrybut("name");
        p.obudowa = urzadzenie->atrybut("package");
        p.wartosc = przytnij(czesc->atrybut("value"));
        if (p.wartosc.empty()) {
            // brak wartości: nazwa zestawu bez końcówek "-EU_", "_" itp.
            std::string zestaw = czesc->atrybut("deviceset");
            while (!zestaw.empty() && (zestaw.back() == '_' || zestaw.back() == '-')) zestaw.pop_back();
            p.wartosc = zestaw + (czesc->atrybut("device").empty() ? "" : " " + czesc->atrybut("device"));
        }

        bool dnp = false;
        // atrybuty na poziomie technologii urządzenia, potem atrybuty samej części (ważniejsze)
        if (const MiniXML::Wezel* tech = urzadzenie->dziecko("technologies")) {
            for (const MiniXML::Wezel* t : tech->dzieciONazwie("technology")) {
                if (t->atrybut("name") != czesc->atrybut("technology")) continue;
                for (const MiniXML::Wezel* a : t->dzieciONazwie("attribute")) {
                    zastosujAtrybut(p, a->atrybut("name"), a->atrybut("value"), dnp);
                }
            }
        }
        for (const MiniXML::Wezel* a : czesc->dzieciONazwie("attribute")) {
            const std::string nazwa = a->atrybut("name");
            const std::string wartosc = przytnij(a->atrybut("value"));
            if (jednaZ(nazwa, {"mpn", "manufacturer_part_number", "manufacturer part number", "mfr_part",
                               "partnumber", "part_number", "pn"})) {
                if (!wartosc.empty()) p.symbol = wartosc;
            } else if (jednaZ(nazwa, {"manufacturer", "mfr", "mfg", "manufacturer_name", "producent"})) {
                if (!wartosc.empty()) p.producent = wartosc;
            } else {
                zastosujAtrybut(p, nazwa, wartosc, dnp);
            }
        }
        (void)znajdzZestaw;
        if (dnp) {
            ++pominiete;
            continue;
        }
        p.ilosc = 1;
        surowe.push_back(p);
    }
    if (pominiete > 0) {
        wynik.ostrzezenia.push_back("Pominięto " + std::to_string(pominiete) + " części oznaczonych jako niemontowane (DNP).");
    }
    (void)bezObudowy;

    wynik.pozycje = ImportBOM::zgrupuj(surowe, &wynik.ostrzezenia);
    if (wynik.format.empty()) wynik.format = "Eagle (schemat)";
    if (wynik.pozycje.empty()) {
        blad = "W schemacie nie znaleziono żadnych części z obudową.";
        return false;
    }
    return true;
}

} // namespace ImportEagle
