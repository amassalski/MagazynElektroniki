#ifndef LOGIKARAPORTOW_H
#define LOGIKARAPORTOW_H

#include "../Model/Raport.h"
#include "LogikaMagazynu.h"
#include "LogikaProjektow.h"

#include <string>

/// Parametry raportów (nie każdy raport używa wszystkich)
struct ParametryRaportu {
    std::string dataOd;     ///< "RRRR-MM-DD" (historia zmian); puste = bez ograniczenia
    std::string dataDo;     ///< włącznie
    int limitWierszy = 5000;
};

/**
 * @class LogikaRaportow
 * @brief Buduje raporty z danych magazynu i projektów. Liczby w raportach mają przecinek
 *        dziesiętny i nie zależą od ustawień regionalnych.
 */
class LogikaRaportow {
public:
    LogikaRaportow(ManagerBazyDanych& manager, LogikaMagazynu& magazyn, LogikaProjektow& projekty);

    std::string pobierzBlad() const { return blad_; }
    bool zbuduj(RodzajRaportu rodzaj, const ParametryRaportu& parametry, Raport& raport);

    static std::string nazwaRaportu(RodzajRaportu rodzaj);
    /// Czy raport używa zakresu dat
    static bool uzywaDat(RodzajRaportu rodzaj) { return rodzaj == RodzajRaportu::HistoriaZmian; }
    /// Kwota z dwoma miejscami i przecinkiem: 1234.5 -> "1234,50"
    static std::string formatKwoty(double kwota);

private:
    ManagerBazyDanych& manager_;
    LogikaMagazynu& magazyn_;
    LogikaProjektow& projekty_;
    std::string blad_;

    bool stanMagazynu(const ParametryRaportu& p, Raport& r);
    bool niskieStany(const ParametryRaportu& p, Raport& r);
    bool wartoscWgKategorii(const ParametryRaportu& p, Raport& r);
    bool brakiWProjektach(const ParametryRaportu& p, Raport& r);
    bool historiaZmian(const ParametryRaportu& p, Raport& r);
    bool ustawBlad(const std::string& t) { blad_ = t; return false; }
};

#endif // LOGIKARAPORTOW_H
