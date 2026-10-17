#ifndef DIALOGKAMERY_H
#define DIALOGKAMERY_H

#include "../Narzedzia/Kamera.h"

#include <wx/wx.h>
#include <wx/timer.h>

#include <vector>

/**
 * Podgląd z kamery USB na żywo i robienie zdjęcia elementu.
 * "Zrób zdjęcie" zatrzymuje obraz; wtedy można go zapisać ("Użyj zdjęcia") albo wrócić do podglądu ("Powtórz").
 * Zdjęcie trafia jako JPEG do katalogu zdjęć z ustawień aplikacji; ścieżkę zwraca Sciezka().
 */
class DialogKamery : public wxDialog {
public:
    DialogKamery(wxWindow* parent, const wxString& symbolElementu);
    ~DialogKamery() override;

    const wxString& Sciezka() const { return sciezka_; }
    // do testów
    bool Otwarta() const { return kamera_.otwarta(); }
    int LiczbaKlatek() const { return klatek_; }
    bool Zamrozony() const { return zamrozony_; }
    void Odswiez() { OnTimer(); }     ///< pobiera i rysuje jedną klatkę (jak timer)
    void ZrobZdjecie();
    void Powtorz();
    bool Zatwierdz();                  ///< zapisuje zamrożoną klatkę; false przy błędzie

private:
    class Podglad;
    Kamera kamera_;
    wxString symbol_, sciezka_;
    wxTimer timer_;
    wxChoice* chKamera = nullptr;
    wxButton *bZrob = nullptr, *bPowtorz = nullptr, *bUzyj = nullptr;
    wxStaticText* lbInfo = nullptr;
    Podglad* podglad_ = nullptr;
    std::vector<InfoKamery> kamery_;
    KlatkaRGB klatka_;
    bool zamrozony_ = false;
    int klatek_ = 0;

    void WykryjKamery();
    void OtworzWybrana();
    void OnTimer();
    void Ustaw(bool zamrozony);
};

#endif // DIALOGKAMERY_H
