#ifndef SPRAWDZANIECEN_H
#define SPRAWDZANIECEN_H

#include "../Logika/LogikaSklepow.h"
#include "ParserStrony.h"

#include <wx/event.h>
#include <wx/timer.h>

#include <functional>
#include <string>
#include <vector>

/// Jedna strona do sprawdzenia
struct ZadanieCeny {
    long long idLinku = 0;
    std::string url;
    std::string wzorzec;          ///< własny wzorzec ceny sklepu (może być pusty)
    std::string walutaSklepu;     ///< gdy strona nie podaje waluty
    std::string opis;             ///< np. "BC547 / TME" (do okna postępu)
};

/// Co wyszło ze sprawdzenia jednej strony
struct WynikSprawdzenia {
    ZadanieCeny zadanie;
    bool zapisano = false;        ///< cena znaleziona i zapisana w bazie
    double cena = 0;
    std::string waluta;
    std::string komunikat;        ///< po polsku: co się stało
};

/**
 * @class SprawdzaczCen
 * @brief Sprawdza ceny kolejno, jedna strona naraz, z przerwą między zapytaniami (nie obciążamy sklepów).
 *        Uruchamiany na życzenie użytkownika. Wymaga pętli zdarzeń wx.
 */
class SprawdzaczCen : public wxEvtHandler {
public:
    using Postep = std::function<void(int numer, int razem, const WynikSprawdzenia&)>;
    using Koniec = std::function<void(bool anulowano)>;

    explicit SprawdzaczCen(LogikaSklepow& logika);
    ~SprawdzaczCen() override;

    /// Zaczyna (zadania nie mogą już być w toku); przerwa w milisekundach
    bool Start(std::vector<ZadanieCeny> zadania, Postep postep, Koniec koniec, int przerwaMs = 1500);
    void Anuluj();
    bool wToku() const { return wToku_; }

    /// Zadania dla linków elementu (pomija linki bez adresu); wzorzec i waluta z sklepu
    static std::vector<ZadanieCeny> ZadaniaDlaLinkow(const std::vector<LinkSklepu>& linki, const std::vector<Sklep>& sklepy,
                                                      const std::string& symbol);

private:
    LogikaSklepow& logika_;
    std::vector<ZadanieCeny> zadania_;
    Postep postep_;
    Koniec koniec_;
    int przerwaMs_ = 1500;
    size_t nr_ = 0;
    bool wToku_ = false, anulowano_ = false, wLocie_ = false;
    wxTimer zegar_;
    long id_ = 0;                 ///< numer uruchomienia (spóźnione odpowiedzi poprzedniego są ignorowane)

    void Nastepne();
    void Zakoncz();
    void NaCzas(wxTimerEvent&);
};

#endif // SPRAWDZANIECEN_H
