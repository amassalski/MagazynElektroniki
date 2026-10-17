#ifndef POZYCJAIMPORTU_H
#define POZYCJAIMPORTU_H

#include "../Model/Element.h"

#include <string>
#include <vector>

/// Jeden element do wprowadzenia do magazynu: z generatora szeregów albo z pliku CSV
struct PozycjaImportu {
    Element element;            ///< iloscDostepna = ilość do przyjęcia
    std::string kategoria;      ///< ścieżka "Elementy bierne / Rezystory / Rezystory SMD" (pusta = bez kategorii)
    int nrWiersza = 0;          ///< numer wiersza w pliku (0 = z generatora)
    std::string blad;           ///< niepusty = pozycji nie da się zaimportować (powód)
};

#endif // POZYCJAIMPORTU_H
