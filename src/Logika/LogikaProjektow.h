#ifndef LOGIKAPROJEKTOW_H
#define LOGIKAPROJEKTOW_H

#include "../Import/ImportBOM.h"
#include "../Model/BOM.h"
#include "../Model/Projekt.h"
#include "LogikaMagazynu.h"

#include <optional>
#include <string>
#include <vector>

/// Co zrobić z jedną pozycją wczytaną z pliku podczas importu do projektu
struct WierszImportuBOM {
    enum class Akcja {
        Pomin,            ///< nie dodawaj do projektu
        UtworzNowy,       ///< utwórz nowy element w magazynie (stan 0) i dodaj do projektu
        UzyjIstniejacego  ///< dodaj do projektu element idElementu
    };
    PozycjaBOM pozycja;
    Akcja akcja = Akcja::Pomin;
    long long idElementu = 0;                 ///< dla UzyjIstniejacego
    std::optional<long long> idKategorii;     ///< dla UtworzNowy
};

struct WynikImportuDoProjektu {
    int dodanoPozycji = 0;     ///< nowe wiersze listy części
    int zsumowano = 0;         ///< wiersze, które już były w projekcie (ilość zwiększona)
    int utworzonoElementow = 0;
    int pominieto = 0;
    int usunieto = 0;          ///< przy zastępowaniu listy
};

/**
 * @class LogikaProjektow
 * @brief Projekty i ich listy części (BOM), wydawanie części z magazynu do projektu.
 *
 * Zasady jak w LogikaMagazynu: metody zwracają powodzenie, a przyczyna błędu (po polsku)
 * jest w pobierzBlad().
 */
class LogikaProjektow {
public:
    LogikaProjektow(ManagerBazyDanych& manager, LogikaMagazynu& magazyn);

    std::string pobierzBlad() const { return blad_; }

    // ============ PROJEKTY ============

    bool pobierzProjekty(bool tylkoAktywne, std::vector<Projekt>& wynik);
    std::optional<Projekt> pobierzProjekt(long long id);
    /// Dodaje (id == 0) albo zmienia projekt; zwraca id
    std::optional<long long> zapiszProjekt(const Projekt& projekt);
    /// Projekt bez historii wydań jest kasowany razem z listą części; z historią tylko archiwizowany
    WynikUsuniecia usunProjekt(long long id);

    // ============ LISTA CZĘŚCI ============

    /// Pozycje w kolejności oznaczeń (R2 przed R10)
    bool pobierzPozycje(long long idProjektu, std::vector<PozycjaProjektu>& wynik);
    /// Gdy element jest już w projekcie, zwiększa jego ilość (i dopisuje oznaczenia)
    std::optional<long long> dodajPozycje(const PozycjaProjektu& pozycja);
    /// Zmienia oznaczenia, ilości (wymagana, zamówiona), priorytet i uwagi
    bool zapiszPozycje(const PozycjaProjektu& pozycja);
    /// Odmawia, gdy z pozycji wydano już części
    bool usunPozycje(long long idPozycji);

    // ============ WYDAWANIE ============

    /// Wydaje `ilosc` sztuk do projektu: zmniejsza stan magazynu (historia WYDANIE) i zwiększa "zużyte"
    bool wydajDoProjektu(long long idPozycji, int ilosc);
    /// Wydaje z magazynu tyle, ile jest dostępne i potrzebne dla każdej pozycji
    bool wydajWszystkoDostepne(long long idProjektu, int* wydanoSztuk = nullptr);

    // ============ IMPORT I BRAKI ============

    /**
     * Dodaje wczytane pozycje do listy części. Nowe elementy są zakładane najpierw (każdy
     * w osobnej transakcji), potem lista części zmieniana jest w jednej transakcji: albo
     * cała, albo wcale.
     * @param zastap  usuń najpierw pozycje, z których nic jeszcze nie wydano
     * @param mnoznik liczba płytek (ilości z pliku są mnożone)
     */
    bool importujBOM(long long idProjektu, const std::vector<WierszImportuBOM>& wiersze, bool zastap,
                     int mnoznik, WynikImportuDoProjektu* wynik = nullptr);

    /// Pozycje, których brakuje w magazynie (jeden projekt albo wszystkie aktywne)
    bool pobierzBraki(std::optional<long long> idProjektu, std::vector<PozycjaProjektu>& wynik);

    /// Lista zakupów do CSV (symbol, producent, ile brakuje, projekt...)
    bool eksportujListeZakupow(const std::string& sciezka, std::optional<long long> idProjektu);

private:
    ManagerBazyDanych& manager_;
    LogikaMagazynu& magazyn_;
    std::string blad_;

    PolaczenieBazy* polaczenie();
    bool ustawBlad(const std::string& blad);
    bool ustawBladBazy(PolaczenieBazy* pol, const std::string& zaZnaczeniem = "");
    std::optional<long long> dodajPozycjeWTransakcji(PolaczenieBazy* pol, const PozycjaProjektu& p,
                                                     bool* zsumowano);
};

#endif // LOGIKAPROJEKTOW_H
