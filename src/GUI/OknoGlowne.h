#ifndef OKNOGLOWNE_H
#define OKNOGLOWNE_H

#include "../Logika/LogikaMagazynu.h"
#include "../Logika/LogikaProjektow.h"
#include "../Logika/LogikaKatalogu.h"
#include "../Logika/LogikaRaportow.h"
#include "../Logika/LogikaSklepow.h"

#include <wx/wx.h>
#include <wx/frame.h>
#include <wx/splitter.h>
#include <wx/notebook.h>

#include <string>
#include <thread>

class PanelKategorii;
class PanelElementow;
class PanelPodgladu;
class PanelProjektow;
class PanelKatalogu;

/**
 * @class OknoGlowne
 * @brief Główne okno programu: drzewo kategorii, lista elementów, menu, pasek stanu
 *        i dziennik zdarzeń.
 *
 * Łączenie z bazą odbywa się w osobnym wątku, więc okno nie zawiesza się, gdy
 * serwer jest niedostępny. Dane (kategorie, elementy) są ładowane dopiero, gdy
 * baza jest połączona i ma strukturę programu.
 */
class OknoGlowne : public wxFrame {
public:
    explicit OknoGlowne(const wxString& title = wxEmptyString);
    ~OknoGlowne() override;

private:
    enum {
        ID_POLACZ = wxID_HIGHEST + 1,
        ID_ROZLACZ,
        ID_USTAWIENIA_POLACZENIA,
        ID_INICJALIZUJ_BAZE,
        ID_INFO_O_BAZIE,
        ID_USTAWIENIA_APLIKACJI,
        ID_ELEMENT_NOWY,
        ID_ELEMENT_EDYTUJ,
        ID_ELEMENT_STAN,
        ID_ELEMENT_USUN,
        ID_KAT_NOWA,
        ID_KAT_PODKATEGORIA,
        ID_KAT_NAZWA,
        ID_KAT_USUN,
        ID_IMPORT_STARTOWE,
        ID_IMPORT_KATEGORII,
        ID_EKSPORT_CSV,
        ID_PROJ_NOWY,
        ID_PROJ_EDYTUJ,
        ID_PROJ_USUN,
        ID_PROJ_IMPORT,
        ID_PROJ_DODAJ_CZESC,
        ID_PROJ_WYDAJ_WSZYSTKO,
        ID_PROJ_ZAKUPY,
        ID_PROJ_EAGLE_SKRYPT,
        ID_PROJ_EAGLE_ULP,
        ID_KATALOG_NOWA,
        ID_KATALOG_EDYTUJ,
        ID_KATALOG_USUN,
        ID_KATALOG_DO_MAGAZYNU,
        ID_KATALOG_IMPORT,
        ID_KATALOG_EKSPORT,
        ID_KATALOG_SZABLON,
        ID_KATALOG_DS_OTWORZ,
        ID_KATALOG_DS_ODCZYT,
        ID_RAPORTY,
        ID_SKLEPY,
        ID_SKLEPY_ELEMENTU,
        ID_ZESTAWIENIE,
        ID_POMOC_PRZEGLADARKA,
        ID_POMOC_AKTUALIZACJE,
        ID_POMOC_AUTOAKT,
        ID_ETYKIETY,
        ID_ODCZYT_KODU,
        ID_GENERATOR_SZEREGOW,
        ID_NARZEDZIA_KODY,
        ID_NARZEDZIA_KALKULATORY,
        ID_IMPORT_ELEMENTOW,
        ID_ODSWIEZ,
        ID_SZUKAJ,
        ID_DZIENNIK
    };

    wxTextCtrl* dziennik = nullptr;
    wxBoxSizer* uklad = nullptr;
    wxNotebook* zakladki = nullptr;
    wxSplitterWindow* podzial = nullptr;
    PanelProjektow* panelProjektow = nullptr;
    PanelKatalogu* panelKatalogu = nullptr;
    PanelKategorii* panelKategorii = nullptr;
    PanelElementow* panelElementow = nullptr;
    PanelPodgladu* panelPodgladu = nullptr;
    wxSplitterWindow* podzialListy = nullptr;
    LogikaMagazynu logika;
    LogikaProjektow logikaProjektow;
    LogikaKatalogu logikaKatalogu;
    LogikaRaportow logikaRaportow;
    LogikaSklepow logikaSklepow;
    std::thread watekLaczenia;
    bool laczenie = false;
    bool bazaGotowa = false;  ///< połączono i baza ma strukturę programu

    void UtworzMenu();
public:
    /// Przełącza na zakładkę Katalog (do testów i skrótów)
    void PokazKatalog();
private:
    void OnKatalog(wxCommandEvent& event);

    // Menu
    void OnZakoncz(wxCommandEvent& event);
    void OnPolacz(wxCommandEvent& event);
    void OnRozlacz(wxCommandEvent& event);
    void OnUstawieniaPolaczenia(wxCommandEvent& event);
    void OnInicjalizujBaze(wxCommandEvent& event);
    void OnInfoOBazie(wxCommandEvent& event);
    void OnUstawieniaAplikacji(wxCommandEvent& event);
    void OnOProgramie(wxCommandEvent& event);
    /// Pobiera plik wersja.txt z GitHuba i porównuje z DM_VERSION; @p cicho = bez komunikatów, gdy nie ma nowszej
    /// wersji albo nie ma sieci (sprawdzanie przy uruchomieniu)
    void SprawdzAktualizacje(bool cicho);
    void SprawdzAktualizacjeNaStarcie();
    void OnElementNowy(wxCommandEvent& event);
    void OnElementEdytuj(wxCommandEvent& event);
    void OnElementStan(wxCommandEvent& event);
    void OnElementUsun(wxCommandEvent& event);
    void OnKategoriaNowa(wxCommandEvent& event);
    void OnKategoriaPodkategoria(wxCommandEvent& event);
    void OnKategoriaNazwa(wxCommandEvent& event);
    void OnKategoriaUsun(wxCommandEvent& event);
    void OnImportKategoriiStartowych(wxCommandEvent& event);
    void OnImportKategoriiZPliku(wxCommandEvent& event);
    void OnEksportCSV(wxCommandEvent& event);
    void OnProjekt(wxCommandEvent& event);
    void OnRaporty(wxCommandEvent& event);
    void OnSklepy(wxCommandEvent& event);
    void OnSklepyElementu(wxCommandEvent& event);
    void OnZestawienie(wxCommandEvent& event);
    void OnEtykiety(wxCommandEvent& event);
    void OnOdczytKodu(wxCommandEvent& event);
    void OnGeneratorSzeregow(wxCommandEvent& event);
    void OnNarzedziaKody(wxCommandEvent& event);
    void OnNarzedziaKalkulatory(wxCommandEvent& event);
    void OnImportElementow(wxCommandEvent& event);
    void OnOdswiez(wxCommandEvent& event);
    void OnSzukaj(wxCommandEvent& event);
    void OnPokazDziennik(wxCommandEvent& event);
    void OnAktualizujMenu(wxUpdateUIEvent& event);

    // Logika
    void Polacz();
    void NaZakonczeniuLaczenia(bool ok, const std::string& blad);
    void OdswiezStatus();
    void SprawdzStrukture();
    void InicjalizujBaze();
    void Zapisz(const wxString& tekst);
    /// Ładuje kategorie i elementy (gdy baza gotowa) albo czyści panele
    void OdswiezDane();
    void ImportujKategorie(const std::string& sciezka);
};

#endif // OKNOGLOWNE_H
