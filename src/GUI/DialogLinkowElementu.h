#ifndef DIALOGLINKOWELEMENTU_H
#define DIALOGLINKOWELEMENTU_H

#include "../Logika/LogikaMagazynu.h"
#include "../Logika/LogikaSklepow.h"
#include "../Wyszukiwanie/SprawdzanieCen.h"

#include <wx/wx.h>
#include <wx/listctrl.h>
#include <wx/spinctrl.h>

#include <memory>
#include <vector>

/// Dodawanie i zmiana jednego linku element-sklep
class DialogLinku : public wxDialog {
public:
    DialogLinku(wxWindow* parent, LogikaSklepow& logika, const std::vector<Sklep>& sklepy, const LinkSklepu& link,
                const std::string& frazaWyszukiwania);

    long long IdZapisanego() const { return idZapisanego_; }
    LinkSklepu Pobierz() const;
    bool Zapisz();

    wxChoice* chSklep = nullptr;
    wxTextCtrl* tcAdres = nullptr;
    wxTextCtrl* tcCena = nullptr;

private:
    LogikaSklepow& logika_;
    std::vector<Sklep> sklepy_;
    LinkSklepu link_;
    std::string fraza_;
    long long idZapisanego_ = 0;
    wxTextCtrl* tcKod = nullptr;
    wxTextCtrl* tcWaluta = nullptr;
    wxSpinCtrl* spMin = nullptr;
    wxCheckBox* cbDomyslny = nullptr;
    wxTextCtrl* tcUwagi = nullptr;
};

/**
 * Sklepy i ceny jednego elementu: linki do stron w sklepach, sprawdzanie cen, historia cen
 * oraz karta katalogowa (datasheet): otwarcie, pobranie, szukanie na stronach sklepów i w internecie.
 */
class DialogLinkowElementu : public wxDialog {
public:
    DialogLinkowElementu(wxWindow* parent, LogikaSklepow& sklepy, LogikaMagazynu& magazyn, long long idElementu);
    ~DialogLinkowElementu() override;

    void Odswiez();
    const std::vector<LinkSklepu>& Linki() const { return linki_; }
    /// Uruchamia sprawdzanie cen wszystkich linków; `koniec` wywołane po zakończeniu (do testów)
    bool SprawdzCeny(std::function<void(bool anulowano)> koniec = nullptr);
    bool Sprawdzanie() const { return spr_.wToku(); }
    void WybierzLink(long long id);
    wxString StatusDatasheetu() const;
    /// Pobiera datasheet z podanego adresu do katalogu dokumentów i zapisuje adres w elemencie
    void PobierzDatasheet(const std::string& url, std::function<void(bool ok)> po = nullptr);

private:
    LogikaSklepow& sklepy_logika_;
    LogikaMagazynu& magazyn_;
    long long idElementu_;
    Element element_;
    std::vector<Sklep> sklepy_;
    std::vector<LinkSklepu> linki_;
    SprawdzaczCen spr_;
    std::shared_ptr<bool> zywy_ = std::make_shared<bool>(true);
    wxListCtrl* lista = nullptr;
    wxStaticText* lbStatus = nullptr;
    wxStaticText* lbDatasheet = nullptr;
    wxButton* bSprawdz = nullptr;

    const LinkSklepu* Wybrany() const;
    wxString KatalogDokumentow() const;
    std::string Fraza() const;
    void Dodaj();
    void Edytuj();
    void Usun();
    void Otworz();
    void SzukajWSklepie();
    void Historia();
    void SprawdzWybrany();
    void ZnajdzDatasheetNaStronach();
    void SzukajDatasheetuWInternecie();
    void OtworzDatasheet();
    void PobierzDatasheetZAdresu();
    void OdswiezDatasheet();
    void Status(const wxString& t);
};

#endif // DIALOGLINKOWELEMENTU_H
