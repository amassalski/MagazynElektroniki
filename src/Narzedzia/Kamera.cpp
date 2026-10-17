#include "Kamera.h"

#include <cmath>
#include <cstdlib>

#ifdef DM_KAMERA
#include <opencv2/videoio.hpp>
#endif

namespace {
bool kameraTestowa() {
    const char* v = std::getenv("DM_KAMERA_TEST");
    return v && *v && std::string(v) != "0";
}
} // namespace

struct Kamera::Impl {
    bool testowa = false;
    bool otwarta = false;
    long licznik = 0;
#ifdef DM_KAMERA
    cv::VideoCapture cap;
#endif
};

Kamera::Kamera() : p_(new Impl) {}
Kamera::~Kamera() { zamknij(); }

bool Kamera::obslugiwana() {
#ifdef DM_KAMERA
    return true;
#else
    return kameraTestowa();
#endif
}

std::vector<InfoKamery> Kamera::wykryj(int maks) {
    std::vector<InfoKamery> wynik;
    if (kameraTestowa()) {
        wynik.push_back({0, "Kamera testowa"});
        return wynik;
    }
#ifdef DM_KAMERA
    for (int i = 0; i < maks; ++i) {
        cv::VideoCapture c(i);
        if (c.isOpened()) wynik.push_back({i, "Kamera " + std::to_string(i + 1)});
    }
#else
    (void)maks;
#endif
    return wynik;
}

bool Kamera::otworz(int indeks, int szer, int wys) {
    zamknij();
    blad_.clear();
    if (kameraTestowa()) {
        p_->testowa = true;
        p_->otwarta = true;
        return true;
    }
#ifdef DM_KAMERA
    p_->cap.open(indeks);
    if (!p_->cap.isOpened()) {
        blad_ = "Nie udało się otworzyć kamery. Sprawdź, czy inny program jej nie używa i czy system pozwala "
                "na dostęp do kamery.";
        return false;
    }
    p_->cap.set(cv::CAP_PROP_FRAME_WIDTH, szer);
    p_->cap.set(cv::CAP_PROP_FRAME_HEIGHT, wys);
    p_->otwarta = true;
    return true;
#else
    (void)indeks; (void)szer; (void)wys;
    blad_ = "Program zbudowano bez obsługi kamery (OpenCV).";
    return false;
#endif
}

void Kamera::zamknij() {
    if (!p_) return;
#ifdef DM_KAMERA
    if (p_->cap.isOpened()) p_->cap.release();
#endif
    p_->otwarta = false;
    p_->testowa = false;
}

bool Kamera::otwarta() const { return p_->otwarta; }

bool Kamera::pobierz(KlatkaRGB& k) {
    if (!p_->otwarta) return false;
    if (p_->testowa) {
        // ruchomy gradient z ramką: widać, że podgląd żyje
        k.szer = 320; k.wys = 240;
        k.rgb.assign(static_cast<size_t>(k.szer) * k.wys * 3, 0);
        const long t = p_->licznik++;
        for (int y = 0; y < k.wys; ++y)
            for (int x = 0; x < k.szer; ++x) {
                unsigned char* px = &k.rgb[(static_cast<size_t>(y) * k.szer + x) * 3];
                px[0] = static_cast<unsigned char>((x + t * 3) % 256);
                px[1] = static_cast<unsigned char>(y * 255 / k.wys);
                px[2] = static_cast<unsigned char>(128 + 100 * std::sin((x + y + t * 4) / 25.0));
                if (x < 4 || y < 4 || x >= k.szer - 4 || y >= k.wys - 4) px[0] = px[1] = px[2] = 255;
            }
        return true;
    }
#ifdef DM_KAMERA
    cv::Mat m;
    if (!p_->cap.read(m) || m.empty() || m.type() != CV_8UC3) {
        if (!m.empty() && m.channels() == 1) {
            // czarno-biała kamera: powiel kanał
            k.szer = m.cols; k.wys = m.rows;
            k.rgb.resize(static_cast<size_t>(k.szer) * k.wys * 3);
            for (int y = 0; y < m.rows; ++y)
                for (int x = 0; x < m.cols; ++x)
                    for (int c = 0; c < 3; ++c) k.rgb[(static_cast<size_t>(y) * m.cols + x) * 3 + c] = m.at<uchar>(y, x);
            return true;
        }
        return false;
    }
    k.szer = m.cols; k.wys = m.rows;
    k.rgb.resize(static_cast<size_t>(k.szer) * k.wys * 3);
    for (int y = 0; y < m.rows; ++y) {
        const unsigned char* src = m.ptr<unsigned char>(y);
        unsigned char* dst = &k.rgb[static_cast<size_t>(y) * m.cols * 3];
        for (int x = 0; x < m.cols; ++x) {      // OpenCV daje BGR
            dst[x * 3] = src[x * 3 + 2];
            dst[x * 3 + 1] = src[x * 3 + 1];
            dst[x * 3 + 2] = src[x * 3];
        }
    }
    return true;
#else
    return false;
#endif
}
