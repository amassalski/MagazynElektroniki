#ifndef IMPORTKICAD_H
#define IMPORTKICAD_H

#include "ImportBOM.h"

/// Lista sieciowa XML z KiCada (Plik -> Eksport -> Netlista, format XML)
namespace ImportKiCad {
bool parsujNetliste(const std::string& xml, WynikImportuBOM& wynik, std::string& blad);
} // namespace ImportKiCad

#endif // IMPORTKICAD_H
