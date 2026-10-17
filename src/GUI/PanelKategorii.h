#ifndef PANELKATEGORII_H
#define PANELKATEGORII_H

#include <wx/wx.h>
#include <wx/treectrl.h>

#include <functional>
#include <optional>

class LogikaMagazynu;

/**
 * @class PanelKategorii
 * @brief Drzewo kategorii: wybór kategorii, dodawanie, zmiana nazwy, przenoszenie
 *        (przeciągnij i upuść) i usuwanie.
 *
 * Panel nie ładuje danych sam z siebie: okno główne woła Odswiez() po połączeniu z bazą.
 */
class PanelKategorii : public wxPanel {
public:
    PanelKategorii(wxWindow* parent, LogikaMagazynu& logika);

    /// Pobiera kategorie z bazy i odbudowuje drzewo (zachowuje rozwinięcia i wybór)
    void Odswiez();
    /// Opróżnia drzewo (np. po rozłączeniu)
    void Wyczysc();

    /// Wybrana kategoria; std::nullopt = "Wszystkie elementy"
    std::optional<long long> WybranaKategoria() const;
    bool CzyWybranaPrawdziwaKategoria() const;
    /// Zaznacza kategorię w drzewie (nullopt = "Wszystkie elementy"); wywołuje naWybor
    void WybierzKategorie(std::optional<long long> id);

    void DodajKategorieGlowna();
    void DodajPodkategorie();
    void ZmienNazwe();
    void UsunKategorie();

    /// Wywoływane po zmianie wyboru w drzewie (przez użytkownika)
    std::function<void(std::optional<long long>)> naWybor;
    /// Wywoływane po zmianie struktury kategorii (przeniesienie, usunięcie)
    std::function<void()> naZmianaStruktury;
    /// Komunikaty dla dziennika okna głównego
    std::function<void(const wxString&)> naKomunikat;

private:
    enum {
        ID_NOWA = wxID_HIGHEST + 100,
        ID_PODKATEGORIA,
        ID_NAZWA,
        ID_USUN
    };

    LogikaMagazynu& logika_;
    wxTreeCtrl* drzewo_ = nullptr;
    wxTreeItemId korzen_;
    wxTreeItemId przeciaganyElement_;
    bool przebudowa_ = false;
    bool pierwszeWczytanie_ = true;

    long long IdElementu(const wxTreeItemId& element) const;
    void Komunikat(const wxString& tekst);
    void Blad(const wxString& naglowek);

    void OnWybor(wxTreeEvent& event);
    void OnPrawyPrzycisk(wxTreeEvent& event);
    void OnKlawisz(wxKeyEvent& event);
    void OnPoczatekPrzeciagania(wxTreeEvent& event);
    void OnKoniecPrzeciagania(wxTreeEvent& event);
    void OnAktualizujPrzycisk(wxUpdateUIEvent& event);
    void PokazMenu();
};

#endif // PANELKATEGORII_H
