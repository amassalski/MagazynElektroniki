#!/usr/bin/env python3
"""Rysuje ikony programu (własny, płaski zestaw, bez cudzych licencji) i zapisuje:
  assets/ikony/<nazwa>_<rozmiar>.png   oraz   src/GUI/IkonyDane.h (PNG-i wkompilowane w program).
Uruchomienie: python3 narzedzia/zbuduj_ikony.py"""
import io, os, sys
from PIL import Image, ImageDraw

KAT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
S = 400                       # płótno robocze; współrzędne w skali 0..100
def k(v): return v * S / 100

NIEB, NIEB_J, NIEB_C = "#2f6fb3", "#6fa8e0", "#1d4a7c"
POM, ZIEL, CZERW, ZOLT = "#f08a24", "#3aa655", "#d64545", "#f2c94c"
SZ, SZ_J, BIAL = "#5b6770", "#c9d1d8", "#ffffff"

def nowy(): return Image.new("RGBA", (S, S), (0, 0, 0, 0))

def rr(d, x0, y0, x1, y1, r, fill, outline=None, w=0):
    d.rounded_rectangle([k(x0), k(y0), k(x1), k(y1)], radius=k(r), fill=fill, outline=outline, width=int(k(w)))

def linia(d, pts, kol, w):
    d.line([(k(x), k(y)) for x, y in pts], fill=kol, width=int(k(w)), joint="curve")

def wielokat(d, pts, kol): d.polygon([(k(x), k(y)) for x, y in pts], fill=kol)
def elipsa(d, x0, y0, x1, y1, fill=None, outline=None, w=0):
    d.ellipse([k(x0), k(y0), k(x1), k(y1)], fill=fill, outline=outline, width=int(k(w)))

def ikona_app():
    im = nowy(); d = ImageDraw.Draw(im)
    rr(d, 6, 8, 94, 94, 10, NIEB, NIEB_C, 2)                      # szafka
    for i, y in enumerate((14, 39, 64)):                          # szuflady
        rr(d, 12, y, 88, y + 21, 5, NIEB_J, NIEB_C, 1.5)
        rr(d, 38, y + 7, 62, y + 14, 3.5, BIAL, SZ, 1)            # uchwyt
    # rezystor w środkowej szufladzie jako znak firmowy
    rr(d, 18, 45, 34, 52, 2, "#e2c896", "#8a6a3a", 0.8)
    for x, c in ((21, ZOLT), (25, "#8e44ad"), (29, CZERW)): d.rectangle([k(x), k(45.5), k(x + 2), k(51.5)], fill=c)
    return im

def i_nowy():
    im = nowy(); d = ImageDraw.Draw(im)
    elipsa(d, 8, 8, 92, 92, ZIEL); linia(d, [(50, 26), (50, 74)], BIAL, 13); linia(d, [(26, 50), (74, 50)], BIAL, 13); return im
def i_edytuj():
    im = nowy(); d = ImageDraw.Draw(im)
    wielokat(d, [(66, 12), (88, 34), (38, 84), (12, 90), (16, 62)], POM)
    wielokat(d, [(16, 62), (38, 84), (12, 90)], "#f2d3a0"); linia(d, [(60, 18), (82, 40)], "#b5610f", 4); return im
def i_stan():
    im = nowy(); d = ImageDraw.Draw(im)
    wielokat(d, [(30, 10), (52, 40), (8, 40)], ZIEL); rr(d, 22, 38, 38, 88, 3, ZIEL)
    wielokat(d, [(70, 90), (92, 60), (48, 60)], CZERW); rr(d, 62, 14, 78, 62, 3, CZERW); return im
def i_usun():
    im = nowy(); d = ImageDraw.Draw(im)
    elipsa(d, 8, 8, 92, 92, CZERW)
    linia(d, [(30, 30), (70, 70)], BIAL, 13); linia(d, [(70, 30), (30, 70)], BIAL, 13); return im
def i_kategoria():
    im = nowy(); d = ImageDraw.Draw(im)
    rr(d, 6, 18, 42, 38, 5, "#e0a82e"); rr(d, 6, 28, 94, 84, 7, ZOLT, "#b8891b", 1.5); return im
def i_import():
    im = nowy(); d = ImageDraw.Draw(im)
    rr(d, 10, 64, 90, 90, 6, SZ); wielokat(d, [(50, 62), (24, 34), (76, 34)], NIEB); rr(d, 42, 8, 58, 36, 3, NIEB); return im
def i_eksport():
    im = nowy(); d = ImageDraw.Draw(im)
    rr(d, 10, 64, 90, 90, 6, SZ); wielokat(d, [(50, 8), (24, 38), (76, 38)], POM); rr(d, 42, 36, 58, 64, 3, POM); return im
def i_etykieta():
    im = nowy(); d = ImageDraw.Draw(im)
    rr(d, 6, 18, 94, 82, 7, BIAL, SZ, 2.5)
    x = 16
    for w in (3, 6, 3, 4, 7, 3, 5, 3, 6, 4, 3):
        d.rectangle([k(x), k(28), k(x + w * 0.9), k(72)], fill="#222"); x += w + 2.2
    return im
def i_szukaj():
    im = nowy(); d = ImageDraw.Draw(im)
    elipsa(d, 8, 8, 64, 64, fill="#dcecfb", outline=NIEB, w=9); linia(d, [(56, 56), (88, 88)], NIEB_C, 14); return im
def i_odswiez():
    im = nowy(); d = ImageDraw.Draw(im)
    d.arc([k(14), k(14), k(86), k(86)], 40, 340, fill=NIEB, width=int(k(12)))
    wielokat(d, [(86, 14), (88, 46), (58, 36)], NIEB); return im
def i_projekt():
    im = nowy(); d = ImageDraw.Draw(im)
    rr(d, 8, 14, 92, 86, 6, "#2e8b57", "#1e5c39", 2)
    linia(d, [(20, 34), (46, 34), (56, 48), (80, 48)], "#bfe8cf", 3.5); linia(d, [(20, 64), (40, 64), (50, 54)], "#bfe8cf", 3.5)
    rr(d, 52, 58, 82, 76, 2.5, "#222"); [d.rectangle([k(54 + i * 8), k(78), k(56.5 + i * 8), k(83)], fill=SZ_J) for i in range(4)]
    return im
def i_raport():
    im = nowy(); d = ImageDraw.Draw(im)
    rr(d, 8, 8, 92, 92, 6, BIAL, SZ, 2.5)
    for x, h, c in ((20, 28, NIEB), (40, 48, POM), (60, 36, ZIEL), (76, 56, CZERW)): rr(d, x, 82 - h, x + 11, 82, 2, c)
    return im
def i_sklep():
    im = nowy(); d = ImageDraw.Draw(im)
    linia(d, [(6, 14), (22, 14), (34, 62), (80, 62)], SZ, 7); wielokat(d, [(26, 26), (90, 26), (80, 54), (32, 54)], POM)
    elipsa(d, 32, 72, 46, 86, fill=SZ); elipsa(d, 66, 72, 80, 86, fill=SZ); return im
def i_polacz():
    im = nowy(); d = ImageDraw.Draw(im)
    rr(d, 30, 8, 42, 34, 3, SZ); rr(d, 58, 8, 70, 34, 3, SZ); rr(d, 22, 32, 78, 62, 8, SZ_J, SZ, 2)
    linia(d, [(50, 62), (50, 92)], SZ, 9); return im
def i_pomoc():
    im = nowy(); d = ImageDraw.Draw(im)
    elipsa(d, 6, 6, 94, 94, NIEB)
    d.arc([k(32), k(18), k(68), k(52)], 180, 410, fill=BIAL, width=int(k(10)))
    linia(d, [(50, 52), (50, 62)], BIAL, 10); elipsa(d, 44, 70, 56, 82, fill=BIAL); return im
def i_narzedzia():
    im = nowy(); d = ImageDraw.Draw(im)
    linia(d, [(4, 50), (22, 50)], SZ, 5); linia(d, [(78, 50), (96, 50)], SZ, 5)
    rr(d, 18, 28, 82, 72, 18, "#e2c896", "#8a6a3a", 2)
    for x, c in ((30, ZOLT), (42, "#8e44ad"), (54, CZERW), (68, "#d4af37")): d.rectangle([k(x), k(29), k(x + 6), k(71)], fill=c)
    return im
def i_generator():
    im = nowy(); d = ImageDraw.Draw(im)
    for r in range(3):
        for c in range(3):
            rr(d, 10 + c * 29, 10 + r * 29, 31 + c * 29, 31 + r * 29, 4, (NIEB, NIEB_J, POM)[(r + c) % 3])
    return im
def i_zamknij():
    im = nowy(); d = ImageDraw.Draw(im)
    linia(d, [(20, 20), (80, 80)], SZ, 12); linia(d, [(80, 20), (20, 80)], SZ, 12); return im

IKONY = {"app": ikona_app, "nowy": i_nowy, "edytuj": i_edytuj, "stan": i_stan, "usun": i_usun, "kategoria": i_kategoria,
         "import": i_import, "eksport": i_eksport, "etykieta": i_etykieta, "szukaj": i_szukaj, "odswiez": i_odswiez,
         "projekt": i_projekt, "raport": i_raport, "sklep": i_sklep, "polacz": i_polacz, "pomoc": i_pomoc,
         "narzedzia": i_narzedzia, "generator": i_generator, "zamknij": i_zamknij}
ROZMIARY = {"app": (16, 24, 32, 48, 64, 128, 256)}
DOMYSLNE = (16, 24, 32)

def png(im, r):
    b = io.BytesIO(); im.resize((r, r), Image.LANCZOS).save(b, "PNG", optimize=True); return b.getvalue()

def main():
    kat = os.path.join(KAT, "assets", "ikony"); os.makedirs(kat, exist_ok=True)
    wpisy = []
    for nazwa, f in IKONY.items():
        im = f()
        for r in ROZMIARY.get(nazwa, DOMYSLNE):
            dane = png(im, r)
            open(os.path.join(kat, f"{nazwa}_{r}.png"), "wb").write(dane)
            wpisy.append((nazwa, r, dane))
    # arkusz poglądowy
    arkusz = Image.new("RGBA", (len(IKONY) * 72, 72), "#f4f4f4")
    for i, (n, f) in enumerate(IKONY.items()): arkusz.alpha_composite(f().resize((64, 64), Image.LANCZOS), (i * 72 + 4, 4))
    arkusz.save(os.path.join(kat, "przeglad.png"))
    open(os.path.join(kat, "domowymagazyn.png"), "wb").write(png(ikona_app(), 256))
    with open(os.path.join(KAT, "src", "GUI", "IkonyDane.h"), "w") as h:
        h.write("// Wygenerowane przez narzedzia/zbuduj_ikony.py: nie edytuj ręcznie.\n#ifndef IKONYDANE_H\n#define IKONYDANE_H\n\n")
        h.write("struct DaneIkony { const char* nazwa; int rozmiar; const unsigned char* dane; unsigned dlugosc; };\n\n")
        for i, (n, r, d) in enumerate(wpisy):
            h.write(f"static const unsigned char ikona_{i}[] = {{" + ",".join(str(b) for b in d) + "};\n")
        h.write("\nstatic const DaneIkony kIkony[] = {\n")
        for i, (n, r, d) in enumerate(wpisy): h.write(f'    {{"{n}", {r}, ikona_{i}, {len(d)}}},\n')
        h.write("};\n\n#endif // IKONYDANE_H\n")
    print("OK:", len(IKONY), "ikon,", len(wpisy), "plików PNG")

if __name__ == "__main__": main()
