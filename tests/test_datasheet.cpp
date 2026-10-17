#include "../src/Logika/OdczytDatasheetu.h"
#include <cstdio>
#include <string>

static int bledy = 0;
#define SPR(w) do { if (!(w)) { std::printf("BLAD: %s (linia %d)\n", #w, __LINE__); ++bledy; } } while (0)

static std::string param(const OdczytDatasheetu::Wynik& w, const std::string& n) {
    for (const auto& p : w.parametry) if (p.nazwa == n) return p.wartosc;
    return "";
}

int main() {
    const std::string t =
        "LM7805 Positive Voltage Regulator\n"
        "Copyright 2020 Example Corp. All rights reserved\n"
        "Features\n"
        "  Output current up to 1.5 A\n"
        "  Input voltage range 7 V to 25 V\n"
        "  Operating temperature range: \xE2\x88\x92" "40\xC2\xB0" "C to +125\xC2\xB0" "C\n"
        "  Available in TO-220 package, 3-pin\n"
        "  Oscillator frequency 100 kHz\n"
        "  Power dissipation 15 W\n";
    auto w = OdczytDatasheetu::wyciagnij(t);
    SPR(w.opis == "LM7805 Positive Voltage Regulator");
    SPR(w.obudowa == "TO-220");
    SPR(param(w, "Prąd maksymalny") == "1.5 A");
    SPR(param(w, "Napięcie zasilania") == "7 V ... 25 V");
    SPR(param(w, "Temperatura pracy") == "-40 ... +125 °C");
    SPR(param(w, "Liczba wyprowadzeń") == "3");
    SPR(param(w, "Częstotliwość") == "100 kHz");
    SPR(param(w, "Moc maksymalna") == "15 W");
    // tranzystor: sekcje Absolute Maximum Ratings i Electrical Characteristics, hFE, zastosowania
    const std::string tr =
        "BC547 NPN General Purpose Transistor\n"
        "\n"
        "Applications\n"
        "  \xE2\x80\xA2 Audio preamplifiers\n"
        "  \xE2\x80\xA2 General purpose switching\n"
        "\n"
        "Absolute Maximum Ratings (TA = 25\xC2\xB0" "C unless otherwise noted)\n"
        "Rating                              Symbol     Value     Unit\n"
        "Collector-Emitter Voltage           VCEO        45       Vdc\n"
        "Collector-Base Voltage              VCBO        50       Vdc\n"
        "Emitter-Base Voltage                VEBO        6.0      Vdc\n"
        "Collector Current - Continuous      IC         100       mAdc\n"
        "Total Device Dissipation @ TA = 25\xC2\xB0" "C   PD   625   mW\n"
        "Operating and Storage Junction Temperature Range  TJ, Tstg  \xE2\x88\x92" "55 to +150  \xC2\xB0" "C\n"
        "\n"
        "Electrical Characteristics\n"
        "DC Current Gain  hFE   (IC = 2.0 mA, VCE = 5.0 V)    110    -    800\n"
        "Collector-Emitter Saturation Voltage  VCE(sat)  (IC = 10 mA, IB = 0.5 mA)   0.09   0.25   V\n"
        "Current-Gain - Bandwidth Product  fT   (IC = 10 mA, VCE = 5 V, f = 100 MHz)   300   MHz\n"
        "Noise Figure  NF   (IC = 0.2 mA)   2.0   10   dB\n"
        "\n"
        "Ordering Information\n"
        "BC547B  TO-92\n";
    auto t2 = OdczytDatasheetu::wyciagnij(tr);
    SPR(param(t2, "Maks.: Napięcie kolektor-emiter (VCEO)") == "45 V");
    SPR(param(t2, "Maks.: Napięcie kolektor-baza (VCBO)") == "50 V");
    SPR(param(t2, "Maks.: Napięcie emiter-baza (VEBO)") == "6.0 V");
    SPR(param(t2, "Maks.: Prąd kolektora (IC)") == "100 mA");
    SPR(param(t2, "Maks.: Moc admisyjna (Ptot)") == "625 mW");
    SPR(param(t2, "Maks.: Temperatura złącza / przechowywania (Tj, Tstg)") == "-55 ... +150 \xC2\xB0" "C" ||
        param(t2, "Maks.: Temperatura złącza / przechowywania (Tj, Tstg)") == "-55 ... 150 \xC2\xB0" "C");
    SPR(param(t2, "Elektr.: Wzmocnienie prądowe (hFE)") == "110 ... 800");
    SPR(param(t2, "Elektr.: Napięcie nasycenia C-E (VCE(sat))") == "0.09 ... 0.25 V");
    SPR(param(t2, "Elektr.: Częstotliwość graniczna (fT)") == "300 MHz");
    SPR(param(t2, "Elektr.: Współczynnik szumów (NF)") == "2.0 ... 10 dB");
    SPR(t2.uwagi.find("Audio preamplifiers") != std::string::npos && t2.uwagi.find("Zastosowanie") == 0);
    for (const auto& x : t2.parametry) if (x.nazwa.find("Maks.: ") == 0 || x.nazwa.find("Elektr.: ") == 0) std::printf("  %s = %s\n", x.nazwa.c_str(), x.wartosc.c_str());

    auto p = OdczytDatasheetu::wyciagnij("");
    SPR(p.parametry.empty() && p.opis.empty() && p.obudowa.empty());
    std::printf(bledy ? "BLEDY: %d\n" : "test_datasheet OK\n", bledy);
    return bledy ? 1 : 0;
}
