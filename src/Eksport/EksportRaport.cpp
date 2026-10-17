#include "EksportRaport.h"

#include "EksportCSV.h"

#include <filesystem>
#include <fstream>

namespace EksportRaport {

std::string ucieknijHTML(const std::string& tekst) {
    std::string w;
    for (char c : tekst) {
        switch (c) {
            case '&': w += "&amp;"; break;
            case '<': w += "&lt;"; break;
            case '>': w += "&gt;"; break;
            case '"': w += "&quot;"; break;
            default: w += c;
        }
    }
    return w;
}

bool zapiszCSV(const std::string& sciezka, const Raport& raport, std::string& blad) {
    auto wiersze = raport.wiersze;
    if (!raport.podsumowanie.empty()) {
        wiersze.push_back(std::vector<std::string>(raport.naglowki.size()));
        for (const std::string& linia : raport.podsumowanie) {
            std::vector<std::string> w(raport.naglowki.size());
            if (!w.empty()) w[0] = linia;
            wiersze.push_back(w);
        }
    }
    return EksportCSV::zapisz(sciezka, raport.naglowki, wiersze, blad);
}

std::string naHTML(const Raport& r, const std::string& data) {
    std::string h;
    h += "<!DOCTYPE html>\n<html lang=\"pl\"><head><meta charset=\"utf-8\">\n<title>" + ucieknijHTML(r.tytul) +
         "</title>\n<style>\n"
         "body{font-family:Arial,Helvetica,sans-serif;font-size:11pt;margin:18mm 14mm;color:#111}\n"
         "h1{font-size:18pt;margin:0 0 4px}.opis{color:#555;margin-bottom:12px}\n"
         "table{border-collapse:collapse;width:100%}\n"
         "th{background:#e8ecf0;text-align:left;border-bottom:2px solid #889}\n"
         "th,td{padding:3px 6px;border-bottom:1px solid #ccd}\n"
         "td.p,th.p{text-align:right}tr:nth-child(even) td{background:#f7f8fa}\n"
         ".sum{margin-top:12px;font-weight:bold}.stopka{margin-top:16px;color:#777;font-size:9pt}\n"
         "@media print{body{margin:0}thead{display:table-header-group}tr{page-break-inside:avoid}}\n"
         "</style></head><body>\n";
    h += "<h1>" + ucieknijHTML(r.tytul) + "</h1>\n";
    if (!r.opis.empty()) h += "<div class=\"opis\">" + ucieknijHTML(r.opis) + "</div>\n";
    h += "<table><thead><tr>";
    for (size_t i = 0; i < r.naglowki.size(); ++i) {
        const bool p = i < r.wyrownajDoPrawej.size() && r.wyrownajDoPrawej[i];
        h += std::string("<th") + (p ? " class=\"p\"" : "") + ">" + ucieknijHTML(r.naglowki[i]) + "</th>";
    }
    h += "</tr></thead><tbody>\n";
    for (const auto& w : r.wiersze) {
        h += "<tr>";
        for (size_t i = 0; i < w.size(); ++i) {
            const bool p = i < r.wyrownajDoPrawej.size() && r.wyrownajDoPrawej[i];
            h += std::string("<td") + (p ? " class=\"p\"" : "") + ">" + ucieknijHTML(w[i]) + "</td>";
        }
        h += "</tr>\n";
    }
    h += "</tbody></table>\n";
    if (r.ucieto) h += "<div class=\"opis\">Pokazano tylko część wierszy (limit raportu).</div>\n";
    for (const std::string& s : r.podsumowanie) h += "<div class=\"sum\">" + ucieknijHTML(s) + "</div>\n";
    h += "<div class=\"stopka\">Domowy Magazyn" + (data.empty() ? std::string() : ", " + ucieknijHTML(data)) +
         "</div>\n</body></html>\n";
    return h;
}

bool zapiszHTML(const std::string& sciezka, const Raport& raport, const std::string& data, std::string& blad) {
    std::ofstream f(std::filesystem::u8path(sciezka), std::ios::binary);
    if (!f) {
        blad = "Nie można zapisać pliku: " + sciezka;
        return false;
    }
    f << naHTML(raport, data);
    if (!f) {
        blad = "Błąd zapisu pliku: " + sciezka;
        return false;
    }
    return true;
}

} // namespace EksportRaport
