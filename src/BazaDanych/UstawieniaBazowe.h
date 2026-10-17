#ifndef USTAWIENIABAZOWE_H
#define USTAWIENIABAZOWE_H

#include <string>
#include <map>

/**
 * @struct KolorProgu
 * @brief Struktura dla jednego progu ilości z kolorem
 */
struct KolorProgu {
    int minIlosc = 0;
    std::string kolor = "#FF0000"; // Domyślnie czerwony
};

/**
 * @struct UstawieniaBazowe
 * @brief Wszystkie ustawienia aplikacji (różne od konfiguracji bazy)
 */
struct UstawieniaBazowe {
    // Ścieżki
    std::string sciezkaObrazki = "./assets/zdjecia";
    std::string sciezkaPDFDatasheet = "./dokumenty";
    
    // Kod EAN
    std::string typKoduEAN = "EAN13";
    int prefixEAN = 20;
    
    // Kolory progów dla ilości
    std::map<int, std::string> koloryProgow; // mapa: minIlosc -> kolor
    
    // Czcionka
    std::string nazwaFontu = "Arial";
    int rozmiarFontu = 10;
    bool pogrubienie = false;
    bool kursywa = false;
};

#endif // USTAWIENIABAZOWE_H
