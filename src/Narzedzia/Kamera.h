#ifndef KAMERA_H
#define KAMERA_H

#include <memory>
#include <string>
#include <vector>

/// Jedna klatka obrazu: piksele RGB, 3 bajty na piksel, wiersz po wierszu od lewej-górnej
struct KlatkaRGB {
    int szer = 0;
    int wys = 0;
    std::vector<unsigned char> rgb;
    bool pusta() const { return szer <= 0 || wys <= 0 || rgb.size() < static_cast<size_t>(szer) * wys * 3; }
};

struct InfoKamery {
    int indeks = 0;
    std::string nazwa;
};

/**
 * @class Kamera
 * @brief Kamera USB (UVC) z podglądem na żywo i zdjęciami.
 *
 * Obsługa urządzeń korzysta z OpenCV (videoio), jeśli program zbudowano z opcją DM_KAMERA. Bez niej
 * obslugiwana() zwraca false, a reszta programu działa normalnie (zdjęcie można wskazać z pliku).
 * Ustawienie zmiennej środowiskowej DM_KAMERA_TEST=1 włącza kamerę testową (generowany obraz): do testów i
 * sprawdzania okna bez urządzenia.
 */
class Kamera {
public:
    Kamera();
    ~Kamera();
    Kamera(const Kamera&) = delete;
    Kamera& operator=(const Kamera&) = delete;

    /// Czy program potrafi w ogóle obsługiwać kamery (zbudowany z OpenCV albo włączona kamera testowa)
    static bool obslugiwana();
    /// Sprawdza kolejne indeksy urządzeń 0...maks-1. Może chwilę potrwać.
    static std::vector<InfoKamery> wykryj(int maks = 4);

    bool otworz(int indeks, int szerZadana = 1280, int wysZadana = 720);
    void zamknij();
    bool otwarta() const;
    /// Pobiera bieżącą klatkę. false, gdy kamera nie jest otwarta albo nie dała obrazu.
    bool pobierz(KlatkaRGB& klatka);
    const std::string& blad() const { return blad_; }

private:
    struct Impl;
    std::unique_ptr<Impl> p_;
    std::string blad_;
};

#endif // KAMERA_H
