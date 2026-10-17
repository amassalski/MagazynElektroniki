#ifndef PANELPODGLADU_H
#define PANELPODGLADU_H

#include "../Logika/LogikaMagazynu.h"
#include "../Model/Element.h"
#include "../Narzedzia/Tlumaczenia.h"

#include <wx/wx.h>

#include <functional>
#include <optional>

/**
 * @class PanelPodgladu
 * @brief Panel po prawej stronie listy elementów: zdjęcie i najważniejsze dane zaznaczonego elementu,
 *        z przyciskami "Zdjęcie z kamery", "Z pliku" i "Usuń zdjęcie".
 */
class PanelPodgladu : public wxPanel {
public:
    PanelPodgladu(wxWindow* parent, LogikaMagazynu& logika);

    /// Pokazuje element (nullptr = nic nie zaznaczono)
    void Pokaz(const Element* element);
    /// Po zapisie zdjęcia (lista powinna się odświeżyć)
    std::function<void()> naZmiana;
    std::function<void(const wxString&)> naKomunikat;

    /// Zdjęcie z kamery dla pokazywanego elementu (otwiera okno kamery)
    void ZdjecieZKamery();
    void ZdjecieZPliku();
    /// Zapisuje ścieżkę zdjęcia w pokazywanym elemencie (pusta = usuwa powiązanie ze zdjęciem)
    bool UstawZdjecie(const wxString& sciezka);

    // dostęp dla testów
    long long IdElementu() const { return element_ ? element_->id : 0; }
    wxString Sciezka() const { return element_ ? TekstZUTF8(element_->zdjecieSciezka) : wxString(); }
    bool ZdjecieWidoczne() const;
    wxString Opis() const { return lbDane_->GetLabel(); }

private:
    class Zdjecie;
    LogikaMagazynu& logika_;
    std::optional<Element> element_;
    Zdjecie* zdjecie_ = nullptr;
    wxStaticText *lbSymbol_ = nullptr, *lbDane_ = nullptr;
    wxButton *bKamera_ = nullptr, *bPlik_ = nullptr, *bUsun_ = nullptr;

    void Odswiez();
    void Komunikat(const wxString& t) { if (naKomunikat) naKomunikat(t); }
};

#endif // PANELPODGLADU_H
