#ifndef LOGIKAMAGAZYNU_H
#define LOGIKAMAGAZYNU_H

#include "../BazaDanych/ManagerBazyDanych.h"
#include "../Model/Element.h"
#include "../Model/Kategoria.h"
#include "../Narzedzia/Liczby.h"

#include <optional>
#include <set>
#include <string>
#include <vector>

/// Kryteria wyboru elementów do listy
struct FiltrElementow {
    std::optional<long long> idKategorii;  ///< brak = wszystkie kategorie
    bool zPodkategoriami = true;           ///< czy pokazywać też elementy z podkategorii
    std::string tekst;                     ///< szukane słowa (bez rozróżniania wielkości liter i polskich znaków)
    bool tylkoNiskieStany = false;         ///< tylko elementy poniżej stanu minimalnego
    int limit = 20000;                     ///< maksymalna liczba wierszy
};

/// Co się stało z elementem po poleceniu "usuń"
enum class WynikUsuniecia {
    Usuniety,       ///< wiersz skasowany z bazy
    Zarchiwizowany, ///< element jest używany (BOM, zamienniki...), więc tylko ukryty
    Blad
};

/**
 * @class LogikaMagazynu
 * @brief Operacje na kategoriach i elementach magazynu (bez wxWidgets).
 *
 * Wszystkie metody zwracają informację o powodzeniu; przyczyna błędu jest w
 * pobierzBlad() (po polsku, gotowa do pokazania użytkownikowi).
 * Obiekt używa połączenia z ManagerBazyDanych i nie jest bezpieczny wątkowo.
 */
class LogikaMagazynu {
public:
    explicit LogikaMagazynu(ManagerBazyDanych& manager);

    std::string pobierzBlad() const { return blad_; }

    // ============ KATEGORIE ============

    /// Pobierz drzewo kategorii z bazy do pamięci podręcznej
    bool odswiezKategorie();
    /// Kategorie w kolejności drzewa (rodzic przed dziećmi, rodzeństwo alfabetycznie)
    const std::vector<Kategoria>& kategorie() const { return kategorie_; }
    const Kategoria* znajdzKategorie(long long id) const;
    /// "Rodzic / Dziecko" albo "" gdy brak kategorii
    std::string pelnaNazwaKategorii(long long id) const;

    std::optional<long long> dodajKategorie(std::optional<long long> idRodzica,
                                            const std::string& nazwa,
                                            const std::string& opis = "");
    bool zmienNazweKategorii(long long id, const std::string& nazwa);
    /// Przenosi kategorię (z podkategoriami) pod nowego rodzica; brak = na poziom główny
    bool przeniesKategorie(long long id, std::optional<long long> nowyRodzic);
    /// Usuwa pustą kategorię (bez podkategorii i elementów)
    bool usunKategorie(long long id);
    /**
     * @brief Dodaje brakujące kategorie z podanych ścieżek (każda ścieżka to lista nazw od
     *        kategorii głównej). Istniejące pomija. Całość w jednej transakcji.
     * @param dodano (opcjonalnie) liczba nowo utworzonych kategorii
     */
    bool dodajSciezkiKategorii(const std::vector<std::vector<std::string>>& sciezki,
                               int* dodano = nullptr);

    // ============ ELEMENTY ============

    bool pobierzElementy(const FiltrElementow& filtr, std::vector<Element>& wynik);
    std::optional<Element> pobierzElement(long long id);
    /**
     * @brief Dodaje (id == 0) albo zmienia element. Zmiana ilości dostępnej jest
     *        zapisywana w historii transakcji (zmien_ilosc_elementu).
     * @return id elementu
     */
    std::optional<long long> zapiszElement(const Element& element);
    /// Przyjęcie (zmiana > 0) lub wydanie (zmiana < 0). Typ: np. PRZYJECIE, WYDANIE, KOREKTA.
    bool zmienIlosc(long long idElementu, int zmiana, const std::string& typ,
                    const std::string& komentarz = "");
    WynikUsuniecia usunElement(long long id);
    /// Aktywny element o podanym symbolu (bez rozróżniania wielkości liter); nullopt, gdy go nie ma
    std::optional<Element> znajdzPoSymbolu(const std::string& symbol);
    /// Symbole wszystkich aktywnych elementów małymi literami (do wykrywania duplikatów przy imporcie)
    std::set<std::string> pobierzSymbole();

    // ============ MIEJSCA MAGAZYNOWANIA ============

    /// Kontenery użyte dotąd w elementach (alfabetycznie, do podpowiedzi w formularzu)
    std::vector<std::string> pobierzKontenery();
    /// Szuflady użyte w danym kontenerze (pusty kontener = elementy bez kontenera), w formie "A1" albo "A1/2"
    std::vector<std::string> pobierzSzuflady(const std::string& kontener);
    /// Zmienia nazwę kontenera we wszystkich elementach; zwraca liczbę zmienionych elementów
    std::optional<int> zmienNazweKontenera(const std::string& stara, const std::string& nowa);

private:
    ManagerBazyDanych& manager_;
    std::vector<Kategoria> kategorie_;
    std::string blad_;

    PolaczenieBazy* polaczenie();
    bool ustawBlad(const std::string& blad);
    bool ustawBladBazy(PolaczenieBazy* pol, const std::string& zaZnaczeniem = "");
    void wypelnijKategorieElementu(std::vector<Element>& elementy) const;
};

#endif // LOGIKAMAGAZYNU_H
