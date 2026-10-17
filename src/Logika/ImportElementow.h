#ifndef IMPORTELEMENTOW_H
#define IMPORTELEMENTOW_H

#include "LogikaMagazynu.h"
#include "PozycjaImportu.h"

#include <string>
#include <vector>

/// Co zrobić z elementem, którego symbol już jest w magazynie
enum class TrybDuplikatow {
    Pomin,        ///< zostaw istniejący bez zmian
    DodajIlosc,   ///< przyjmij podaną ilość na istniejący element (zapis w historii)
    Aktualizuj    ///< uzupełnij dane istniejącego elementu niepustymi polami z importu (stan bez zmian)
};

struct WynikImportuElementow {
    int dodano = 0;               ///< nowe elementy
    int zsumowano = 0;            ///< istniejące, do których dodano ilość
    int zaktualizowano = 0;       ///< istniejące ze zmienionymi danymi
    int pominieto = 0;            ///< istniejące bez zmian
    int utworzonoKategorii = 0;
    std::vector<std::string> bledy;   ///< "wiersz 5 (R-1K): powód"
};

/**
 * @class ImportElementow
 * @brief Zapisuje pozycje z generatora szeregów lub z CSV w magazynie. Brakujące kategorie są tworzone.
 *        Import można powtarzać: symbole, które już są, obsługuje wybrany tryb.
 */
class ImportElementow {
public:
    explicit ImportElementow(LogikaMagazynu& magazyn) : magazyn_(magazyn) {}

    bool wykonaj(const std::vector<PozycjaImportu>& pozycje, TrybDuplikatow tryb, WynikImportuElementow& wynik);
    std::string pobierzBlad() const { return blad_; }

    /// Ścieżka "A / B / C" albo "A > B > C" -> nazwy
    static std::vector<std::string> rozbijSciezke(const std::string& sciezka);

private:
    LogikaMagazynu& magazyn_;
    std::string blad_;
};

#endif // IMPORTELEMENTOW_H
