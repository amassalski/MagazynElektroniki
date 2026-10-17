# Skrypt budowy wxWidgets 3.2 z najnowszych źródeł

$WorkDir = "G:\Biblioteki"
$wxVersion = "3.2.6"  # Ostatnia stabilna wersja

Write-Host "=== Pobieranie wxWidgets $wxVersion ===" -ForegroundColor Green

# Pobierz źródła
cd $WorkDir
$DownloadUrl = "https://github.com/wxWidgets/wxWidgets/releases/download/v${wxVersion}/wxWidgets-${wxVersion}.tar.bz2"

Write-Host "URL: $DownloadUrl"

# Pobierz plikiem curl (bo PowerShell może mieć problemy)
curl.exe -L -o "wxWidgets-${wxVersion}.tar.bz2" $DownloadUrl

if (-not (Test-Path "wxWidgets-${wxVersion}.tar.bz2")) {
    Write-Host "Błąd: Nie udało się pobrać wxWidgets" -ForegroundColor Red
    exit 1
}

Write-Host "✓ Pobrano wxWidgets" -ForegroundColor Green

# Rozpakuj
Write-Host "=== Rozpakowywanie ===" -ForegroundColor Green
tar -xf "wxWidgets-${wxVersion}.tar.bz2"
Remove-Item "wxWidgets-${wxVersion}.tar.bz2"

cd "wxWidgets-${wxVersion}"

# Stwórz katalog build
Write-Host "=== Kompilacja ===" -ForegroundColor Green
if (Test-Path "build-release") {
    Remove-Item -Recurse -Force "build-release"
}
mkdir "build-release"
cd "build-release"

# Konfiguruj CMake
Write-Host "CMake configure..."
cmake .. `
    -G Ninja `
    -DCMAKE_BUILD_TYPE=Release `
    -DwxBUILD_SHARED=ON `
    -DwxUSE_WEBVIEW=OFF `
    -DCMAKE_CXX_FLAGS="-std=gnu++17"

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configure failed!" -ForegroundColor Red
    exit 1
}

# Kompiluj
Write-Host "Building..."
cmake --build . --config Release

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed!" -ForegroundColor Red
    exit 1
}

Write-Host "✓ wxWidgets skompilowane pomyślnie!" -ForegroundColor Green
Write-Host "Lokalizacja: $WorkDir\wxWidgets-${wxVersion}" -ForegroundColor Cyan

# Informacja o bibliotekach
$DllDir = "$WorkDir\wxWidgets-${wxVersion}\build-release\lib\gcc_x64_dll"
Write-Host "DLL-ki: $DllDir" -ForegroundColor Cyan
Get-ChildItem $DllDir -Filter "*.dll" | Select-Object Name
