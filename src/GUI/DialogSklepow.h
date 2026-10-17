#ifndef DIALOGSKLEPOW_H
#define DIALOGSKLEPOW_H

#include "../Logika/LogikaSklepow.h"

#include <wx/wx.h>
#include <wx/listctrl.h>

#include <vector>

/// Lista sklepów: dodawanie, zmiana, usuwanie, wczytanie listy z tekstu/pliku, kursy walut
class DialogSklepow : public wxDialog {
public:
    DialogSklepow(wxWindow* parent, LogikaSklepow& logika);

    void Odswiez();
    const std::vector<Sklep>& Sklepy() const { return sklepy_; }
    /// Operacje z przycisków (publiczne dla testów); zwracają, czy coś zmieniły
    bool Dodaj();
    bool Edytuj();
    bool Usun();
    bool Importuj();
    bool Kursy();
    void Wybierz(long long idSklepu);

    /// Tekst kursów "USD = 4,00" po jednym w wierszu i jego zapis; false + opis błędu przy złym wierszu
    static std::string KursyNaTekst(const std::map<std::string, double>& kursy);
    static bool TekstNaKursy(const std::string& tekst, std::map<std::string, double>& kursy, std::string& blad);

private:
    LogikaSklepow& logika_;
    std::vector<Sklep> sklepy_;
    wxListCtrl* lista = nullptr;

    const Sklep* Wybrany() const;
};

#endif // DIALOGSKLEPOW_H
