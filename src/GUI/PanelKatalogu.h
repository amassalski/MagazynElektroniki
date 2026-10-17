#ifndef PANELKATALOGU_H
#define PANELKATALOGU_H

#include "../Logika/LogikaKatalogu.h"
#include "../Logika/LogikaMagazynu.h"

#include <wx/wx.h>
#include <wx/listctrl.h>
#include <wx/splitter.h>
#include <wx/srchctrl.h>

#include <functional>
#include <optional>
#include <vector>

/**
 * @class PanelKatalogu
 * @brief Zakładka "Katalog": katalog elementów elektronicznych (niezależny od stanów magazynowych),
 *        powiązany z magazynem przez symbol. Lista z wyszukiwaniem po lewej, szczegóły i parametry
 *        po prawej, import/eksport CSV, dodawanie do magazynu i odczyt parametrów z datasheetu.
 */
class PanelKatalogu : public wxPanel {
public:
    PanelKatalogu(wxWindow* parent, LogikaMagazynu& magazyn, LogikaKatalogu& katalog);

    void Odswiez();
    void Wyczysc();

    void Nowa();
    void Edytuj();
    void Usun();
    void DodajDoMagazynu();
    /// Importuje CSV; @p sciezka pusta = zapytaj o plik. Zwraca liczbę dodanych + zaktualizowanych (-1 = błąd)
    int ImportujCSV(const wxString& sciezka = wxEmptyString, TrybKatalogu tryb = TrybKatalogu::Pomin);
    bool EksportujCSV(const wxString& sciezka = wxEmptyString);
    bool ZapiszSzablon(const wxString& sciezka = wxEmptyString);
    void OtworzDatasheet();
    void PobierzDatasheet();
    /// Czyta parametry z PDF-a (pdftotext) i proponuje je do zapisu. @p sciezka pusta = plik pobrany lub zapytaj.
    /// @p bezPytania: zapisuje wszystkie propozycje bez okna (testy). Zwraca liczbę zapisanych parametrów (-1 = błąd).
    int OdczytajDatasheet(const wxString& sciezka = wxEmptyString, bool bezPytania = false);

    void Szukaj(const std::string& tekst);
    bool MaWybor() const { return Wybrana() != nullptr; }
    const PozycjaKatalogu* Wybrana() const;
    void Wybierz(long long id);
    const std::vector<PozycjaKatalogu>& Lista() const { return lista_; }

    std::function<void(const wxString&)> naStatus;
    std::function<void(const wxString&)> naKomunikat;
    std::function<void()> naZmianaMagazynu;

private:
    enum { ID_K_NOWA = wxID_HIGHEST + 500, ID_K_EDYTUJ, ID_K_USUN, ID_K_DO_MAGAZYNU, ID_K_IMPORT, ID_K_EKSPORT,
           ID_K_SZABLON, ID_K_DS_OTWORZ, ID_K_DS_POBIERZ, ID_K_DS_ODCZYT };
    enum Kol { K_SYMBOL, K_PRODUCENT, K_OPIS, K_OBUDOWA, K_RODZINA, K_MAGAZYN, K_LICZBA };

    LogikaMagazynu& magazyn_;
    LogikaKatalogu& katalog_;
    std::vector<PozycjaKatalogu> lista_;
    wxSearchCtrl* szukaj_ = nullptr;
    wxChoice* chRodzina_ = nullptr;
    wxCheckBox* cbMagazyn_ = nullptr;
    wxListCtrl* widok_ = nullptr;
    wxListCtrl* parametry_ = nullptr;
    wxStaticText* naglowek_ = nullptr;
    wxStaticText* szczegoly_ = nullptr;
    wxStaticText* licznik_ = nullptr;
    std::vector<std::string> rodziny_;
    bool odswiezanie_ = false;

    void WypelnijRodziny();
    void PokazSzczegoly();
    wxString KatalogDokumentow() const;
    void Komunikat(const wxString& t) const { if (naKomunikat) naKomunikat(t); }
    void Status(const wxString& t) const { if (naStatus) naStatus(t); }
    void Blad(const wxString& t);
};

#endif // PANELKATALOGU_H
