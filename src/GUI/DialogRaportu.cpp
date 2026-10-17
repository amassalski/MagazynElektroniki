#include "DialogRaportu.h"
#include "../Eksport/EksportRaport.h"
#include "../Narzedzia/Tlumaczenia.h"
#include "../Pomoc/PomocGUI.h"

#include <wx/datetime.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/utils.h>

namespace {
const RodzajRaportu RODZAJE[] = {RodzajRaportu::StanMagazynu, RodzajRaportu::NiskieStany,
                                 RodzajRaportu::WartoscWgKategorii, RodzajRaportu::BrakiWProjektach,
                                 RodzajRaportu::HistoriaZmian};
}

DialogRaportu::DialogRaportu(wxWindow* parent, LogikaRaportow& logika)
    : wxDialog(parent, wxID_ANY, TR("Raporty"), wxDefaultPosition, wxSize(1000, 620),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      logika_(logika) {
    wxBoxSizer* glowny = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* gora = new wxBoxSizer(wxHORIZONTAL);
    gora->Add(new wxStaticText(this, wxID_ANY, TR("Raport:")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    chRaport = new wxChoice(this, wxID_ANY);
    for (RodzajRaportu r : RODZAJE) chRaport->Append(TR(LogikaRaportow::nazwaRaportu(r).c_str()));
    chRaport->SetSelection(0);
    gora->Add(chRaport, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 16);
    lbDaty = new wxStaticText(this, wxID_ANY, TR("Od (RRRR-MM-DD):"));
    gora->Add(lbDaty, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    tcOd = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(110, -1));
    gora->Add(tcOd, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    lbDo = new wxStaticText(this, wxID_ANY, TR("Do:"));
    gora->Add(lbDo, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    tcDo = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(110, -1));
    gora->Add(tcDo, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 16);
    wxButton* bGeneruj = new wxButton(this, wxID_ANY, TR("Generuj"));
    gora->Add(bGeneruj, 0, wxALIGN_CENTER_VERTICAL);
    glowny->Add(gora, 0, wxEXPAND | wxALL, 10);

    lbOpis = new wxStaticText(this, wxID_ANY, wxEmptyString);
    glowny->Add(lbOpis, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);
    lista = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxBORDER_THEME);
    glowny->Add(lista, 1, wxEXPAND | wxLEFT | wxRIGHT, 10);
    PomocGUI::DymekZKolumn(lista);
    lbPodsumowanie = new wxStaticText(this, wxID_ANY, wxEmptyString);
    wxFont f = lbPodsumowanie->GetFont();
    f.SetWeight(wxFONTWEIGHT_BOLD);
    lbPodsumowanie->SetFont(f);
    glowny->Add(lbPodsumowanie, 0, wxALL, 10);

    wxBoxSizer* dol = new wxBoxSizer(wxHORIZONTAL);
    wxButton* bCsv = new wxButton(this, wxID_ANY, TR("Zapisz CSV..."));
    wxButton* bHtml = new wxButton(this, wxID_ANY, TR("Zapisz HTML..."));
    wxButton* bDruk = new wxButton(this, wxID_ANY, TR("Otwórz do wydruku / PDF"));
    dol->Add(bCsv, 0, wxRIGHT, 6);
    dol->Add(bHtml, 0, wxRIGHT, 6);
    dol->Add(bDruk, 0, wxRIGHT, 6);
    dol->AddStretchSpacer();
    dol->Add(new wxButton(this, wxID_CANCEL, TR("Zamknij")), 0);
    glowny->Add(dol, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    SetSizer(glowny);
    SetEscapeId(wxID_CANCEL);

    bGeneruj->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Generuj(); });
    chRaport->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { AktualizujPola(); Generuj(); });
    bCsv->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ZapiszCSV(); });
    bHtml->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ZapiszHTML(false); });
    bDruk->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ZapiszHTML(true); });
    for (wxButton* b : {bCsv, bHtml, bDruk}) {
        b->Bind(wxEVT_UPDATE_UI, [this](wxUpdateUIEvent& e) { e.Enable(wygenerowany_); });
    }
    AktualizujPola();
    CallAfter([this] { Generuj(); });
}

void DialogRaportu::AktualizujPola() {
    const int s = chRaport->GetSelection();
    const bool daty = s >= 0 && LogikaRaportow::uzywaDat(RODZAJE[s]);
    for (wxWindow* w : std::initializer_list<wxWindow*>{lbDaty, tcOd, lbDo, tcDo}) w->Show(daty);
    if (daty && tcOd->IsEmpty()) {
        tcOd->SetValue((wxDateTime::Today() - wxDateSpan::Days(30)).FormatISODate());
        tcDo->SetValue(wxDateTime::Today().FormatISODate());
    }
    Layout();
}

void DialogRaportu::WybierzRodzaj(RodzajRaportu rodzaj) {
    for (size_t i = 0; i < sizeof(RODZAJE) / sizeof(RODZAJE[0]); ++i) {
        if (RODZAJE[i] == rodzaj) chRaport->SetSelection(static_cast<int>(i));
    }
    AktualizujPola();
}

void DialogRaportu::UstawDaty(const wxString& od, const wxString& doo) {
    tcOd->SetValue(od);
    tcDo->SetValue(doo);
}

bool DialogRaportu::Generuj() {
    const int s = chRaport->GetSelection();
    if (s < 0) return false;
    ParametryRaportu p;
    if (LogikaRaportow::uzywaDat(RODZAJE[s])) {
        p.dataOd = tcOd->GetValue().Strip(wxString::both).utf8_string();
        p.dataDo = tcDo->GetValue().Strip(wxString::both).utf8_string();
    }
    bool ok;
    {
        wxBusyCursor zajety;
        ok = logika_.zbuduj(RODZAJE[s], p, raport_);
    }
    wygenerowany_ = ok;
    if (!ok) {
        lista->ClearAll();
        lbOpis->SetLabel(wxEmptyString);
        lbPodsumowanie->SetLabel(wxEmptyString);
        wxMessageBox(TekstZUTF8(logika_.pobierzBlad()), TR("Raport"), wxOK | wxICON_ERROR, this);
        return false;
    }
    Pokaz();
    return true;
}

void DialogRaportu::Pokaz() {
    lista->Freeze();
    lista->ClearAll();
    for (size_t i = 0; i < raport_.naglowki.size(); ++i) {
        const bool p = i < raport_.wyrownajDoPrawej.size() && raport_.wyrownajDoPrawej[i];
        lista->InsertColumn(static_cast<long>(i), TekstZUTF8(raport_.naglowki[i]),
                            p ? wxLIST_FORMAT_RIGHT : wxLIST_FORMAT_LEFT, p ? 80 : 130);
    }
    long r = 0;
    for (const auto& w : raport_.wiersze) {
        lista->InsertItem(r, w.empty() ? wxString() : TekstZUTF8(w[0]));
        for (size_t c = 1; c < w.size(); ++c) lista->SetItem(r, static_cast<long>(c), TekstZUTF8(w[c]));
        ++r;
    }
    lista->Thaw();
    wxString opis = TekstZUTF8(raport_.opis);
    if (raport_.ucieto) opis += TR("  (pokazano tylko część wierszy)");
    lbOpis->SetLabel(opis);
    wxString pod;
    for (const std::string& l : raport_.podsumowanie) pod += (pod.empty() ? "" : "   |   ") + TekstZUTF8(l);
    lbPodsumowanie->SetLabel(pod);
    Layout();
}

void DialogRaportu::ZapiszCSV() {
    wxFileDialog dlg(this, TR("Zapisz raport jako CSV"), wxEmptyString,
                     "raport_" + wxDateTime::Now().Format("%Y%m%d") + ".csv", TR("Pliki CSV (*.csv)|*.csv"),
                     wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() != wxID_OK) return;
    std::string blad;
    if (!EksportRaport::zapiszCSV(dlg.GetPath().utf8_string(), raport_, blad))
        wxMessageBox(TekstZUTF8(blad), TR("Raport"), wxOK | wxICON_ERROR, this);
}

void DialogRaportu::ZapiszHTML(bool otworz) {
    wxString sciezka;
    if (otworz) {
        sciezka = wxFileName::CreateTempFileName("raport");
        sciezka += ".html";
    } else {
        wxFileDialog dlg(this, TR("Zapisz raport jako HTML"), wxEmptyString,
                         "raport_" + wxDateTime::Now().Format("%Y%m%d") + ".html", TR("Pliki HTML (*.html)|*.html"),
                         wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (dlg.ShowModal() != wxID_OK) return;
        sciezka = dlg.GetPath();
    }
    std::string blad;
    if (!EksportRaport::zapiszHTML(sciezka.utf8_string(), raport_, wxDateTime::Now().Format("%Y-%m-%d %H:%M").utf8_string(),
                                   blad)) {
        wxMessageBox(TekstZUTF8(blad), TR("Raport"), wxOK | wxICON_ERROR, this);
        return;
    }
    if (otworz) wxLaunchDefaultBrowser(wxFileName::FileNameToURL(wxFileName(sciezka)));
}
