#ifndef PANELPROJEKTOW_H
#define PANELPROJEKTOW_H

#include "../Logika/LogikaMagazynu.h"
#include "../Logika/LogikaProjektow.h"

#include <wx/wx.h>
#include <wx/listctrl.h>
#include <wx/splitter.h>

#include <functional>
#include <optional>
#include <vector>

/**
 * @class PanelProjektow
 * @brief Zakładka "Projekty": lista projektów po lewej, lista części wybranego projektu po prawej
 *        (z kolorowaniem braków), wydawanie części z magazynu, import BOM i lista zakupów.
 */
class PanelProjektow : public wxPanel {
public:
    PanelProjektow(wxWindow* parent, LogikaMagazynu& magazyn, LogikaProjektow& projekty);

    /// Pobiera projekty i listę części (zachowuje wybór)
    void Odswiez();
    void Wyczysc();

    void NowyProjekt();
    void EdytujProjekt();
    void UsunProjekt();
    void ImportujBOM(const wxString& sciezka = wxEmptyString);
    void DodajCzesc();
    void EdytujPozycje();
    void UsunPozycje();
    void WydajPozycje();
    void WydajWszystko();
    void ListaZakupow();
    /// Zapisuje skrypt Eagle (.scr) z atrybutami części projektu; @p sciezka pusta = zapytaj o plik. Zwraca liczbę części.
    int SkryptEagle(const wxString& sciezka = wxEmptyString);
    /// Zapisuje program ULP do eksportu listy części z Eagle
    bool ZapiszUlp(const wxString& sciezka = wxEmptyString);

    /// Id zaznaczonego projektu (nullopt, gdy nic nie zaznaczono)
    std::optional<long long> WybranyId() const { const Projekt* p = WybranyProjekt(); if (p) return p->id; return std::nullopt; }
    bool MaProjekt() const { return WybranyProjekt() != nullptr; }
    bool MaPozycje() const { return WybranaPozycja() != nullptr; }
    const std::vector<Projekt>& Projekty() const { return projekty_; }
    const std::vector<PozycjaProjektu>& Pozycje() const { return pozycje_; }
    /// Wybiera projekt o danym id (do testów i po utworzeniu)
    void WybierzProjekt(long long id);

    std::function<void(const wxString&)> naStatus;
    std::function<void(const wxString&)> naKomunikat;
    /// Po zmianie stanów magazynowych (wydanie, nowy element z importu)
    std::function<void()> naZmianaMagazynu;

private:
    enum {
        ID_P_NOWY = wxID_HIGHEST + 400, ID_P_EDYTUJ, ID_P_USUN, ID_P_ARCHIWUM,
        ID_B_IMPORT, ID_B_DODAJ, ID_B_EDYTUJ, ID_B_USUN, ID_B_WYDAJ, ID_B_WYDAJ_WSZYSTKO, ID_B_ZAKUPY
    };
    enum Kol { K_OZN, K_SYMBOL, K_PRODUCENT, K_WARTOSC, K_OBUDOWA, K_WYMAGANE, K_ZUZYTE, K_DOSTEPNE, K_BRAKUJE,
               K_ZAMOWIONE, K_LICZBA };

    LogikaMagazynu& magazyn_;
    LogikaProjektow& projekty_logika_;
    std::vector<Projekt> projekty_;
    std::vector<PozycjaProjektu> pozycje_;

    wxCheckBox* cbArchiwum_ = nullptr;
    wxListCtrl* listaProjektow_ = nullptr;
    wxListCtrl* listaPozycji_ = nullptr;
    wxStaticText* naglowek_ = nullptr;
    wxStaticText* podsumowanie_ = nullptr;

    const Projekt* WybranyProjekt() const;
    const PozycjaProjektu* WybranaPozycja() const;
    void WypelnijProjekty(long long zachowajId);
    void OdswiezPozycje();
    void Komunikat(const wxString& t) const { if (naKomunikat) naKomunikat(t); }
    void Blad(const wxString& naglowek, const std::string& tresc);
    void OnAktualizujPrzycisk(wxUpdateUIEvent& event);
};

#endif // PANELPROJEKTOW_H
