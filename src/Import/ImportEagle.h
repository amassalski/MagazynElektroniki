#ifndef IMPORTEAGLE_H
#define IMPORTEAGLE_H

#include "ImportBOM.h"

/// Schemat Eagle (.sch, XML): części z obudowami z bibliotek schematu
namespace ImportEagle {
bool parsujSchemat(const std::string& xml, WynikImportuBOM& wynik, std::string& blad);
} // namespace ImportEagle

#endif // IMPORTEAGLE_H
