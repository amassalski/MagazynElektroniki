#ifndef PANELELEMENTOW_H
#define PANELELEMENTOW_H

#include "../Logika/LogikaMagazynu.h"
#include "../Model/Element.h"

#include <wx/wx.h>
#include <wx/listctrl.h>
#include <wx/srchctrl.h>
#include <wx/timer.h>

#include <functional>
#include <optional>
#include <vector>

/**
 * @class PanelElementow
 * @brief Lista elementów z wyszukiwaniem, filtrem kategorii i akcjami (dodaj, edytuj,
 *        usuń, zmień stan).
 *
 * Lista jest wirtualna (szybka także przy tysiącach elementów). Kliknięcie nagłówka
 * kolumny sortuje, drugie kliknięcie odwraca kolejność.
 */
class PanelElementow : public wxPanel {
public:
    PanelElementow(wxWindow* parent, LogikaMagazynu& logika);

    /// Ustawia kategorię filtra (nullopt = wszystkie) i odświeża listę
    void UstawKategorie(std::optional<long long> idKategorii);
    /// Ponownie pobiera elementy wg bieżącego filtra (zachowuje wybór)
    void Odswiez();
    void Wyczysc();

    void NowyElement();
    void EdytujWybrany();
    void UsunWybrany();
    void ZmienStanWybranego();
    void FokusSzukania();
    /// Zaznacza pierwszy wiersz listy (o ile jest)
    void WybierzPierwszy();

    bool MaWybor() const;
    /// Zaznaczony element albo nullptr
    const Element* Wybrany() const { return WybranyElement(); }
    /// Elementy w kolejności widocznej na liście (do eksportu)
    const std::vector<Element>& Elementy() const { return elementy_; }
    const std::optional<long long>& Kategoria() const { return kategoria_; }
    /// Ustawia tekst wyszukiwania i odświeża od razu (bez opóźnienia)
    void Szukaj(const wxString& tekst);

    /// Zmiana zaznaczenia (nullptr = nic nie zaznaczono); wskaźnik ważny tylko w czasie wywołania
    std::function<void(const Element*)> naWybor;
    /// Pasek stanu okna głównego
    std::function<void(const wxString&)> naStatus;
    /// Dziennik okna głównego
    std::function<void(const wxString&)> naKomunikat;
    /// Po zmianie danych (liczby elementów w kategoriach się zmieniły)
    std::function<void()> naZmianaDanych;

private:
    class Lista;
    friend class Lista;

    enum {
        ID_DODAJ = wxID_HIGHEST + 200,
        ID_EDYTUJ,
        ID_USUN,
        ID_STAN,
        ID_TIMER,
        ID_PODKATEGORIE,
        ID_NISKIE
    };
    enum Kolumna {
        K_SYMBOL, K_PRODUCENT, K_OPIS, K_WARTOSC, K_TOL, K_OBUDOWA, K_KATEGORIA, K_MIEJSCE, K_ILOSC, K_MIN, K_CENA,
        K_LICZBA
    };

    LogikaMagazynu& logika_;
    std::vector<Element> elementy_;
    std::optional<long long> kategoria_;
    int sortKolumna_ = -1;
    bool sortRosnaco_ = true;
    bool limitOsiagniety_ = false;

    wxSearchCtrl* szukaj_ = nullptr;
    wxCheckBox* cbPodkategorie_ = nullptr;
    wxCheckBox* cbNiskie_ = nullptr;
    Lista* lista_ = nullptr;
    wxTimer timer_;
    wxListItemAttr atrybutNiski_;

    long WybranyIndeks() const;
    const Element* WybranyElement() const;
    void WybierzElement(long long id);
    void Posortuj();
    void Komunikat(const wxString& tekst);
    void Blad(const wxString& naglowek);
    void PokazStatus();
    void ZglosWybor() { if (naWybor) naWybor(WybranyElement()); }
    wxString TekstKomorki(const Element& e, int kolumna) const;

    void OnZmianaTekstu(wxCommandEvent& event);
    void OnTimer(wxTimerEvent& event);
    void OnZmianaFiltra(wxCommandEvent& event);
    void OnKlikniecieNaglowka(wxListEvent& event);
    void OnAktywacja(wxListEvent& event);
    void OnKlawisz(wxListEvent& event);
    void OnPrawyPrzycisk(wxListEvent& event);
    void OnAktualizujPrzycisk(wxUpdateUIEvent& event);
};

#endif // PANELELEMENTOW_H
