#ifndef EKSPORTRAPORT_H
#define EKSPORTRAPORT_H

#include "../Model/Raport.h"

#include <string>

/// Zapis raportu do plików (bez bazy danych i wxWidgets)
namespace EksportRaport {

/// CSV dla polskiego Excela (UTF-8 z BOM, średnik); podsumowanie jako dodatkowe wiersze
bool zapiszCSV(const std::string& sciezka, const Raport& raport, std::string& blad);

/// Samodzielna strona HTML (do wydruku z przeglądarki albo "Zapisz jako PDF")
std::string naHTML(const Raport& raport, const std::string& dataWygenerowania = "");
bool zapiszHTML(const std::string& sciezka, const Raport& raport, const std::string& dataWygenerowania,
                std::string& blad);

/// Znaki specjalne HTML
std::string ucieknijHTML(const std::string& tekst);

} // namespace EksportRaport

#endif // EKSPORTRAPORT_H
