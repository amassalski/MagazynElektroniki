#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Buduje pomoc programu.

Wejście:
  pomoc/pomoc_pl.src.htm   treść pomocy (bez spisu treści i nagłówka), kotwice <a name=...>
  pomoc/pola.txt           opisy pól: klucz <TAB> tytuł <TAB> opis <TAB> kotwica

Wyjście:
  pomoc/pomoc_pl.htm       gotowy plik pomocy (UTF-8) w układzie pomocy Eagle'a 7.6
  sql/pomoc_pola.sql       zasilenie tabeli tbl_pomoc_pol

Sprawdza też, że każdy odnośnik (#kotwica) i każda kotwica z pola.txt istnieje.
Użycie: python3 narzedzia/zbuduj_pomoc.py   (z katalogu projektu)
"""
import html
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ZRODLO = ROOT / "pomoc" / "pomoc_pl.src.htm"
POLA = ROOT / "pomoc" / "pola.txt"
WYJSCIE = ROOT / "pomoc" / "pomoc_pl.htm"
SQL = ROOT / "sql" / "pomoc_pola.sql"
WERSJA = "1.0"
SPRAWDZ = "--sprawdz" in sys.argv   # tylko sprawdza spójność i aktualność plików

NAGLOWEK = """<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<title>Domowy Magazyn - Pomoc, wersja {wersja}</title>
<style type="text/css"><!-- h1 {{ background-color: #CCCCCC; color: #0050B0; }} pre {{ background-color: #EEEEEE; }} mb {{ font-weight: bold; font-family: courier; background-color: #EEEEEE; }}--></style>
</head>
<body>
<table width=100% cellspacing=0 border=0><tr>
<td align=left>Domowy Magazyn - Pomoc, wersja {wersja}</td>
<td align=right><font size=-1>
<i>Magazyn części elektronicznych (wxWidgets, PostgreSQL)</i>
</font></td></tr></table>

"""


def wczytaj_naglowki(tekst):
    """Zwraca listę (poziom, kotwica, tytuł) w kolejności występowania."""
    wynik = []
    wzor = re.compile(
        r"<a name=([\w-]+)>\s*\n<h(1)>(.*?)</h1>|<h(2)><a name=([\w-]+)>(.*?)</a></h2>", re.S)
    for m in wzor.finditer(tekst):
        if m.group(1):
            wynik.append((1, m.group(1), m.group(3).strip()))
        else:
            wynik.append((2, m.group(5), m.group(6).strip()))
    return wynik


def spis_tresci(naglowki):
    linie = ["<h1>Spis treści</h1>", "<ul>"]
    otwarte_h2 = False
    for poziom, kotwica, tytul in naglowki:
        if poziom == 1:
            if otwarte_h2:
                linie.append("</ul>")
                otwarte_h2 = False
            linie.append(f"<li><a href=#{kotwica}>{tytul}</a>")
        else:
            if not otwarte_h2:
                linie.append("<ul>")
                otwarte_h2 = True
            linie.append(f"<li><a href=#{kotwica}>{tytul}</a>")
    if otwarte_h2:
        linie.append("</ul>")
    linie.append("</ul>")
    return "\n".join(linie) + "\n\n"


def sql_tekst(s):
    return "'" + s.replace("'", "''") + "'"


def wczytaj_pola():
    pola = []
    for nr, linia in enumerate(POLA.read_text(encoding="utf-8").splitlines(), 1):
        if not linia.strip() or linia.startswith("#"):
            continue
        czesci = linia.split("\t")
        if len(czesci) != 4:
            sys.exit(f"pola.txt, wiersz {nr}: oczekiwano 4 pól rozdzielonych tabulatorem")
        pola.append([c.strip() for c in czesci])
    return pola


def main():
    tekst = ZRODLO.read_text(encoding="utf-8")
    naglowki = wczytaj_naglowki(tekst)
    kotwice = [k for _, k, _ in naglowki]
    bledy = []

    wszystkie = re.findall(r"<a name=([\w-]+)>", tekst)
    for k in set(wszystkie):
        if wszystkie.count(k) > 1:
            bledy.append(f"powtórzona kotwica: {k}")
    if len(wszystkie) != len(naglowki):
        bledy.append("kotwica bez nagłówka (lub odwrotnie): "
                     f"{len(wszystkie)} kotwic, {len(naglowki)} nagłówków")
    for odn in sorted(set(re.findall(r"href=#([\w-]+)", tekst))):
        if odn not in kotwice:
            bledy.append(f"odnośnik do nieistniejącej kotwicy: #{odn}")

    pola = wczytaj_pola()
    klucze = [p[0] for p in pola]
    for k in set(klucze):
        if klucze.count(k) > 1:
            bledy.append(f"powtórzony klucz pola: {k}")
    for klucz, tytul, opis, kotwica in pola:
        if kotwica not in kotwice:
            bledy.append(f"pole {klucz}: nieistniejąca kotwica #{kotwica}")
        if len(klucz) > 80 or len(tytul) > 200 or len(kotwica) > 80:
            bledy.append(f"pole {klucz}: za długi klucz, tytuł lub kotwica")
    if bledy:
        sys.exit("BŁĘDY:\n  " + "\n  ".join(bledy))

    html_out = (NAGLOWEK.format(wersja=WERSJA) + spis_tresci(naglowki) + "\n" + tekst
                + "\n</body>\n</html>\n")
    if SPRAWDZ:
        if not WYJSCIE.exists() or WYJSCIE.read_text(encoding="utf-8") != html_out:
            sys.exit("pomoc/pomoc_pl.htm jest nieaktualny: uruchom narzedzia/zbuduj_pomoc.py")
    else:
        WYJSCIE.write_text(html_out, encoding="utf-8", newline="\n")

    sql = ["-- Wygenerowane przez narzedzia/zbuduj_pomoc.py z pomoc/pola.txt. Nie edytuj ręcznie.",
           "-- Skrypt jest idempotentny. Opisy zmienione ręcznie w bazie (zmienione = true)",
           "-- nie są nadpisywane.",
           "SELECT set_config('dm.import_pomocy', 'on', false);",
           "INSERT INTO public.tbl_pomoc_pol (klucz, tytul, opis, kotwica) VALUES"]
    wiersze = [f"    ({sql_tekst(k)}, {sql_tekst(t)}, {sql_tekst(o)}, {sql_tekst(a)})"
               for k, t, o, a in pola]
    sql.append(",\n".join(wiersze))
    sql.append("ON CONFLICT (klucz) DO UPDATE SET tytul = EXCLUDED.tytul, opis = EXCLUDED.opis,")
    sql.append("    kotwica = EXCLUDED.kotwica")
    sql.append("WHERE NOT public.tbl_pomoc_pol.zmienione;")
    sql.append("SELECT set_config('dm.import_pomocy', '', false);")
    sql_out = "\n".join(sql) + "\n"
    if SPRAWDZ:
        if not SQL.exists() or SQL.read_text(encoding="utf-8") != sql_out:
            sys.exit("sql/pomoc_pola.sql jest nieaktualny: uruchom narzedzia/zbuduj_pomoc.py")
    else:
        SQL.write_text(sql_out, encoding="utf-8", newline="\n")
    print(f"OK: {len(naglowki)} nagłówków, {len(pola)} opisów pól -> {WYJSCIE.name}, {SQL.name}")


if __name__ == "__main__":
    main()
