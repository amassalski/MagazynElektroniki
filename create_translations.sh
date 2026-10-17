# create_translations.sh
#!/bin/bash

# Utwórz katalogi
mkdir -p lang

# Znajdź wszystkie stringi w kodzie
find src -name "*.cpp" -o -name "*.h" | xgettext \
    --keyword=_ \
    --keyword=wxT \
    --keyword=_T \
    --keyword=_translate \
    --from-code=UTF-8 \
    --output=lang/DomowyMagazyn.pot \
    --files-from=-

# Utwórz/aktualizuj plik polski
if [ -f lang/pl.po ]; then
    msgmerge --update lang/pl.po lang/DomowyMagazyn.pot
else
    msginit --locale=pl --input=lang/DomowyMagazyn.pot --output=lang/pl.po
fi

# Kompiluj tłumaczenia
msgfmt lang/pl.po -o lang/pl.mo

echo "Tłumaczenia gotowe!"