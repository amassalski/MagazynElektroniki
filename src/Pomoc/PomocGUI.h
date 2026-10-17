#ifndef POMOCGUI_H
#define POMOCGUI_H

#include <wx/wx.h>
#include <wx/listctrl.h>

#include <functional>
#include <string>

/// Pomoc w oknach programu: przeglądarka pliku pomocy, F1 w polach formularzy, dymki.
namespace PomocGUI {

/// Katalog z plikiem pomoc_pl.htm i pola.txt (obok programu, wyżej albo w bieżącym katalogu); pusty, gdy nie ma
std::string ZnajdzKatalogPomocy();

/// Wczytuje opisy pól z pliku pola.txt (działa bez bazy). Wywołać raz przy starcie.
void WczytajOpisyZPliku();
/// Wczytuje opisy z tabeli tbl_pomoc_pol (po połączeniu z bazą); brak tabeli nie jest błędem.
void WczytajOpisyZBazy();

/// Przypisuje kontrolce opis pola: dymek (po zatrzymaniu myszy) i klucz dla klawisza F1.
/// Nieznany klucz jest ignorowany, więc można wołać także przed załadowaniem opisów.
void Dodaj(wxWindow* kontrolka, const std::string& klucz);

/// Dymek wiersza listy: @p tekstWiersza dostaje numer wiersza pod myszą i zwraca tekst (pusty = brak dymku)
void DymekWiersza(wxListCtrl* lista, std::function<wxString(long)> tekstWiersza);

/// Dymek wiersza zwykłej (niewirtualnej) listy: wszystkie niepuste komórki wiersza jako "Kolumna: wartość"
void DymekZKolumn(wxListCtrl* lista);

/// Otwiera okno pomocy (wbudowane) na kotwicy (pusta = początek). Zwraca false, gdy nie ma pliku pomocy.
bool PokazPomoc(const std::string& kotwica = std::string());
/// Otwiera plik pomocy w przeglądarce systemowej
bool OtworzWPrzegladarce(const std::string& kotwica = std::string());

/// F1: opis pola, które ma fokus (lub najbliższego rodzaca z opisem); bez opisu otwiera spis treści pomocy
void PokazPomocDlaFokusu();

/// Włącza obsługę klawisza F1 we wszystkich oknach programu (w tym oknach dialogowych). Wołać raz.
void WlaczKlawiszF1();

/// Klucz opisu przypisany kontrolce przez Dodaj() (pusty, gdy brak)
std::string KluczKontrolki(const wxWindow* kontrolka);

} // namespace PomocGUI

#endif
