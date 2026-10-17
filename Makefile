.PHONY: all clean build run

all: build

build:
	mkdir -p build
	cd build && cmake .. && make

clean:
	rm -rf build

run: build
	./build/DomowyMagazyn

db-init:
	psql -U postgres -c "CREATE DATABASE domowy_magazyn;" || true
	psql -U postgres -d domowy_magazyn -f sql/schema.sql
	psql -U postgres -d domowy_magazyn -f sql/dane_startowe.sql

package-deb:
	mkdir -p package_deb
	cp -r package/* package_deb/
	dpkg-deb --build package_deb domowymagazyn_1.0.0_amd64.deb
