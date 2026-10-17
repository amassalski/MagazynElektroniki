#include "PanelKategorii.h"
#include "Ikony.h"
#include "../Logika/LogikaMagazynu.h"
#include "../Narzedzia/Tlumaczenia.h"
#include "../Pomoc/PomocGUI.h"

#include <wx/wrapsizer.h>

#include <algorithm>
#include <set>

namespace {
class DaneKategorii : public wxTreeItemData {
public:
    explicit DaneKategorii(long long idKategorii) : id(idKategorii) {}
    long long id;
};
} // namespace

PanelKategorii::PanelKategorii(wxWindow* parent, LogikaMagazynu& logika)
    : wxPanel(parent, wxID_ANY), logika_(logika) {
    wxBoxSizer* glowny = new wxBoxSizer(wxVERTICAL);

    wxWrapSizer* przyciski = new wxWrapSizer(wxHORIZONTAL);
    auto dodajPrzycisk = [&](int id, const wxString& etykieta) {
        wxButton* b = new wxButton(this, id, etykieta, wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
        Ikony::DlaPrzycisku(b, id == ID_NOWA || id == ID_PODKATEGORIA ? "nowy" : id == ID_NAZWA ? "edytuj" : id == ID_USUN ? "usun" : "");
        przyciski->Add(b, 0, wxRIGHT | wxBOTTOM, 4);
        Bind(wxEVT_UPDATE_UI, &PanelKategorii::OnAktualizujPrzycisk, this, id);
        return b;
    };
    dodajPrzycisk(ID_NOWA, TR("Nowa"));
    dodajPrzycisk(ID_PODKATEGORIA, TR("Podkategoria"));
    dodajPrzycisk(ID_NAZWA, TR("Zmień nazwę"));
    dodajPrzycisk(ID_USUN, TR("Usuń"));
    glowny->Add(przyciski, 0, wxEXPAND | wxALL, 4);

    drzewo_ = new wxTreeCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(260, -1),
                             wxTR_DEFAULT_STYLE | wxTR_SINGLE);
    glowny->Add(drzewo_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 4);
    PomocGUI::Dodaj(drzewo_, "lista.kategorie");
    SetSizer(glowny);

    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { DodajKategorieGlowna(); }, ID_NOWA);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { DodajPodkategorie(); }, ID_PODKATEGORIA);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ZmienNazwe(); }, ID_NAZWA);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { UsunKategorie(); }, ID_USUN);

    drzewo_->Bind(wxEVT_TREE_SEL_CHANGED, &PanelKategorii::OnWybor, this);
    drzewo_->Bind(wxEVT_TREE_ITEM_RIGHT_CLICK, &PanelKategorii::OnPrawyPrzycisk, this);
    drzewo_->Bind(wxEVT_KEY_DOWN, &PanelKategorii::OnKlawisz, this);
    drzewo_->Bind(wxEVT_TREE_BEGIN_DRAG, &PanelKategorii::OnPoczatekPrzeciagania, this);
    drzewo_->Bind(wxEVT_TREE_END_DRAG, &PanelKategorii::OnKoniecPrzeciagania, this);

    Wyczysc();
}

long long PanelKategorii::IdElementu(const wxTreeItemId& element) const {
    if (!element.IsOk()) return 0;
    auto* dane = dynamic_cast<DaneKategorii*>(drzewo_->GetItemData(element));
    return dane ? dane->id : 0;
}

void PanelKategorii::Komunikat(const wxString& tekst) {
    if (naKomunikat) naKomunikat(tekst);
}

void PanelKategorii::Blad(const wxString& naglowek) {
    const wxString tresc = TekstZUTF8(logika_.pobierzBlad());
    Komunikat(TR("✗ ") + naglowek + ": " + tresc);
    wxMessageBox(tresc, naglowek, wxOK | wxICON_WARNING, this);
}

void PanelKategorii::Wyczysc() {
    przebudowa_ = true;
    drzewo_->DeleteAllItems();
    korzen_ = drzewo_->AddRoot(TR("Wszystkie elementy"), -1, -1, new DaneKategorii(0));
    drzewo_->SelectItem(korzen_);
    przebudowa_ = false;
    pierwszeWczytanie_ = true;
}

void PanelKategorii::Odswiez() {
    // Zapamiętaj rozwinięcia i wybór
    std::set<long long> rozwiniete;
    const std::optional<long long> wybrana = WybranaKategoria();
    std::function<void(const wxTreeItemId&)> zbierz = [&](const wxTreeItemId& wezel) {
        wxTreeItemIdValue cookie;
        for (wxTreeItemId d = drzewo_->GetFirstChild(wezel, cookie); d.IsOk();
             d = drzewo_->GetNextChild(wezel, cookie)) {
            if (drzewo_->IsExpanded(d)) rozwiniete.insert(IdElementu(d));
            zbierz(d);
        }
    };
    if (korzen_.IsOk()) zbierz(korzen_);

    if (!logika_.odswiezKategorie()) {
        Blad(TR("Nie udało się odczytać kategorii"));
        return;
    }

    przebudowa_ = true;
    drzewo_->Freeze();
    drzewo_->DeleteAllItems();
    korzen_ = drzewo_->AddRoot(TR("Wszystkie elementy"), -1, -1, new DaneKategorii(0));

    // Kategorie przychodzą w kolejności drzewa z polem poziom
    std::vector<wxTreeItemId> rodzice{korzen_};
    wxTreeItemId doWyboru = korzen_;
    for (const Kategoria& k : logika_.kategorie()) {
        const size_t poziom = static_cast<size_t>(k.poziom);
        const wxTreeItemId rodzic = rodzice[std::min(poziom, rodzice.size() - 1)];
        wxString etykieta = TekstZUTF8(k.nazwa);
        if (k.liczbaElementowRazem > 0) etykieta << " (" << k.liczbaElementowRazem << ")";
        const wxTreeItemId wezel = drzewo_->AppendItem(rodzic, etykieta, -1, -1, new DaneKategorii(k.id));
        rodzice.resize(std::min(poziom + 1, rodzice.size()));
        rodzice.push_back(wezel);
        if (wybrana && *wybrana == k.id) doWyboru = wezel;
    }

    std::function<void(const wxTreeItemId&)> przywroc = [&](const wxTreeItemId& wezel) {
        wxTreeItemIdValue cookie;
        for (wxTreeItemId d = drzewo_->GetFirstChild(wezel, cookie); d.IsOk();
             d = drzewo_->GetNextChild(wezel, cookie)) {
            if (rozwiniete.count(IdElementu(d))) drzewo_->Expand(d);
            przywroc(d);
        }
    };
    przywroc(korzen_);

    drzewo_->Expand(korzen_);
    drzewo_->SelectItem(doWyboru);
    drzewo_->EnsureVisible(doWyboru);
    drzewo_->Thaw();
    przebudowa_ = false;
    pierwszeWczytanie_ = false;
}

std::optional<long long> PanelKategorii::WybranaKategoria() const {
    const long long id = IdElementu(drzewo_->GetSelection());
    if (id == 0) return std::nullopt;
    return id;
}

bool PanelKategorii::CzyWybranaPrawdziwaKategoria() const {
    return WybranaKategoria().has_value();
}

void PanelKategorii::WybierzKategorie(std::optional<long long> id) {
    wxTreeItemId wezel = korzen_;
    if (id) {
        wezel = wxTreeItemId();
        std::function<void(const wxTreeItemId&)> znajdz = [&](const wxTreeItemId& w) {
            wxTreeItemIdValue cookie;
            for (wxTreeItemId d = drzewo_->GetFirstChild(w, cookie); d.IsOk() && !wezel.IsOk();
                 d = drzewo_->GetNextChild(w, cookie)) {
                if (IdElementu(d) == *id) wezel = d;
                else znajdz(d);
            }
        };
        znajdz(korzen_);
    }
    if (!wezel.IsOk()) return;
    drzewo_->SelectItem(wezel);
    drzewo_->EnsureVisible(wezel);
}

void PanelKategorii::DodajKategorieGlowna() {
    wxTextEntryDialog dlg(this, TR("Nazwa nowej kategorii głównej:"), TR("Nowa kategoria"));
    if (dlg.ShowModal() != wxID_OK) return;
    const std::string nazwa = dlg.GetValue().utf8_string();
    auto id = logika_.dodajKategorie(std::nullopt, nazwa);
    if (!id) return Blad(TR("Nie udało się dodać kategorii"));
    Komunikat(TR("Dodano kategorię: ") + dlg.GetValue());
    Odswiez();
    if (naZmianaStruktury) naZmianaStruktury();
}

void PanelKategorii::DodajPodkategorie() {
    const std::optional<long long> rodzic = WybranaKategoria();
    if (!rodzic) return DodajKategorieGlowna();

    const wxString rodzicNazwa = TekstZUTF8(logika_.pelnaNazwaKategorii(*rodzic));
    wxTextEntryDialog dlg(this, TR("Nazwa nowej podkategorii w „") + rodzicNazwa + TR("”:"),
                          TR("Nowa podkategoria"));
    if (dlg.ShowModal() != wxID_OK) return;
    auto id = logika_.dodajKategorie(rodzic, dlg.GetValue().utf8_string());
    if (!id) return Blad(TR("Nie udało się dodać podkategorii"));
    Komunikat(TR("Dodano podkategorię: ") + dlg.GetValue());

    // rozwiń rodzica, żeby nowa pozycja była widoczna
    Odswiez();
    wxTreeItemId wezel;
    std::function<void(const wxTreeItemId&)> znajdz = [&](const wxTreeItemId& w) {
        wxTreeItemIdValue cookie;
        for (wxTreeItemId d = drzewo_->GetFirstChild(w, cookie); d.IsOk() && !wezel.IsOk();
             d = drzewo_->GetNextChild(w, cookie)) {
            if (IdElementu(d) == *rodzic) wezel = d;
            else znajdz(d);
        }
    };
    znajdz(korzen_);
    if (wezel.IsOk()) drzewo_->Expand(wezel);
    if (naZmianaStruktury) naZmianaStruktury();
}

void PanelKategorii::ZmienNazwe() {
    const std::optional<long long> id = WybranaKategoria();
    if (!id) return;
    const Kategoria* k = logika_.znajdzKategorie(*id);
    if (!k) return;

    wxTextEntryDialog dlg(this, TR("Nowa nazwa kategorii:"), TR("Zmiana nazwy"), TekstZUTF8(k->nazwa));
    if (dlg.ShowModal() != wxID_OK) return;
    if (!logika_.zmienNazweKategorii(*id, dlg.GetValue().utf8_string())) {
        return Blad(TR("Nie udało się zmienić nazwy"));
    }
    Komunikat(TR("Zmieniono nazwę kategorii na: ") + dlg.GetValue());
    Odswiez();
    if (naZmianaStruktury) naZmianaStruktury();
}

void PanelKategorii::UsunKategorie() {
    const std::optional<long long> id = WybranaKategoria();
    if (!id) return;
    const wxString nazwa = TekstZUTF8(logika_.pelnaNazwaKategorii(*id));
    if (wxMessageBox(TR("Usunąć kategorię „") + nazwa + TR("”?"), TR("Usuwanie kategorii"),
                     wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION, this) != wxYES) {
        return;
    }
    if (!logika_.usunKategorie(*id)) return Blad(TR("Nie udało się usunąć kategorii"));
    Komunikat(TR("Usunięto kategorię: ") + nazwa);
    Odswiez();
    if (naZmianaStruktury) naZmianaStruktury();
    if (naWybor) naWybor(WybranaKategoria());
}

void PanelKategorii::OnWybor(wxTreeEvent& event) {
    event.Skip();
    if (przebudowa_) return;
    if (naWybor) naWybor(WybranaKategoria());
}

void PanelKategorii::OnPrawyPrzycisk(wxTreeEvent& event) {
    drzewo_->SelectItem(event.GetItem());
    PokazMenu();
}

void PanelKategorii::PokazMenu() {
    wxMenu menu;
    menu.Append(ID_NOWA, TR("Nowa kategoria główna"));
    menu.Append(ID_PODKATEGORIA, WybranaKategoria() ? TR("Nowa podkategoria...") : TR("Nowa kategoria..."));
    menu.AppendSeparator();
    wxMenuItem* nazwa = menu.Append(ID_NAZWA, TR("Zmień nazwę\tF2"));
    wxMenuItem* usun = menu.Append(ID_USUN, TR("Usuń"));
    nazwa->Enable(CzyWybranaPrawdziwaKategoria());
    usun->Enable(CzyWybranaPrawdziwaKategoria());
    switch (GetPopupMenuSelectionFromUser(menu)) {
        case ID_NOWA: DodajKategorieGlowna(); break;
        case ID_PODKATEGORIA: DodajPodkategorie(); break;
        case ID_NAZWA: ZmienNazwe(); break;
        case ID_USUN: UsunKategorie(); break;
        default: break;
    }
}

void PanelKategorii::OnKlawisz(wxKeyEvent& event) {
    switch (event.GetKeyCode()) {
        case WXK_F2: ZmienNazwe(); return;
        case WXK_DELETE: UsunKategorie(); return;
        default: event.Skip();
    }
}

void PanelKategorii::OnAktualizujPrzycisk(wxUpdateUIEvent& event) {
    switch (event.GetId()) {
        case ID_NAZWA:
        case ID_USUN:
            event.Enable(CzyWybranaPrawdziwaKategoria());
            break;
        default:
            event.Enable(true);
    }
}

void PanelKategorii::OnPoczatekPrzeciagania(wxTreeEvent& event) {
    if (IdElementu(event.GetItem()) == 0) return; // korzenia nie przenosimy
    przeciaganyElement_ = event.GetItem();
    event.Allow();
}

void PanelKategorii::OnKoniecPrzeciagania(wxTreeEvent& event) {
    const wxTreeItemId zrodlo = przeciaganyElement_;
    przeciaganyElement_ = wxTreeItemId();
    const wxTreeItemId cel = event.GetItem();
    if (!zrodlo.IsOk() || !cel.IsOk() || zrodlo == cel) return;

    const long long idZrodla = IdElementu(zrodlo);
    const long long idCelu = IdElementu(cel);
    const Kategoria* k = logika_.znajdzKategorie(idZrodla);
    if (!k) return;
    // już tam jest
    if ((k->idRodzica.value_or(0)) == idCelu) return;

    const wxString zrodloNazwa = TekstZUTF8(k->pelnaNazwa);
    const wxString celNazwa = idCelu == 0 ? TR("poziom główny") : TekstZUTF8(logika_.pelnaNazwaKategorii(idCelu));
    if (wxMessageBox(TR("Przenieść kategorię „") + zrodloNazwa + TR("” do: ") + celNazwa + "?",
                     TR("Przenoszenie kategorii"), wxYES_NO | wxICON_QUESTION, this) != wxYES) {
        return;
    }
    std::optional<long long> nowyRodzic;
    if (idCelu != 0) nowyRodzic = idCelu;
    if (!logika_.przeniesKategorie(idZrodla, nowyRodzic)) return Blad(TR("Nie udało się przenieść kategorii"));
    Komunikat(TR("Przeniesiono kategorię „") + zrodloNazwa + TR("” do: ") + celNazwa);
    Odswiez();
    if (naZmianaStruktury) naZmianaStruktury();
}
