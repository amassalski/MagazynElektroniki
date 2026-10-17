#ifndef MINIXML_H
#define MINIXML_H

#include <map>
#include <memory>
#include <string>
#include <vector>

/**
 * Mały, tolerancyjny parser XML (bez zależności od bibliotek): elementy, atrybuty,
 * tekst, komentarze, CDATA, instrukcje <? ?> i <!DOCTYPE>, encje (&amp; &lt; &gt;
 * &quot; &apos; &#N; &#xN;). Wystarcza do plików sieciowych (netlist) KiCada i
 * schematów Eagle. Nie sprawdza zgodności z DTD.
 */
namespace MiniXML {

struct Wezel {
    std::string nazwa;
    std::map<std::string, std::string> atrybuty;
    std::string tekst;                        ///< sklejony tekst bezpośrednich dzieci tekstowych
    std::vector<std::unique_ptr<Wezel>> dzieci;

    std::string atrybut(const std::string& klucz, const std::string& domyslny = "") const;
    /// Pierwszy bezpośredni potomek o danej nazwie albo nullptr
    const Wezel* dziecko(const std::string& nazwaDziecka) const;
    /// Wszyscy bezpośredni potomkowie o danej nazwie
    std::vector<const Wezel*> dzieciONazwie(const std::string& nazwaDzieci) const;
};

/// Zwraca korzeń dokumentu albo nullptr (wtedy `blad` opisuje problem)
std::unique_ptr<Wezel> parsuj(const std::string& tekst, std::string& blad);

} // namespace MiniXML

#endif // MINIXML_H
