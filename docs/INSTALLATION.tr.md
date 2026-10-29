# SCC — Streamer Daily kurulumu

## Desteklenen sürüm

SCC şu anda aşağıdaki EXE SHA-256 değerine sahip Steam sürümünü destekler:

```text
DAC4C516BF382A4FBE809EA4940F830D54CEF92CC035DB55DF5871E4A694666C
```

Eşleşen `Assembly-CSharp.dll` SHA-256 değeri:

```text
D237B41BF9DD4835F2C56E980A19F1AAA523036275A0F780286B67DB7A0EB2C7
```

Başlatıcı iki değeri de kontrol eder. Oyun güncellendiyse uyumlu SCC sürümünü bekle.

## Hazır sürümü kurma

1. GitHub Releases sayfasından son `SCC-StreamerDaily-<sürüm>.zip` dosyasını indir.
2. ZIP'e sağ tıklayıp **Tümünü ayıkla** seçeneğini kullan ve dosyaları birlikte tut.
3. Oyunu Steam üzerinden açıp kayıtlı oyuna gir; Mono çalışma ortamı başlamış olmalı.
4. Çıkartılan klasörde `SCC-StreamerDaily-Aktif-Et.cmd` dosyasını çalıştır.
5. Başlatıcıda `AKTİF: Mono oyun verisi ve ImGui menüsü doğrulandı` mesajını bekle.
6. Oyunda `Insert` tuşuyla menüyü açıp kapat.

Yalnız DLL dosyasını taşıma; aktivasyon betiği ve başlatıcı kurulumun parçasıdır.

## SCC'yi güncelleme

1. Menüde **Sistem → Hileyi tamamen kaldır** düğmesine bas veya şunu çalıştır:

   ```powershell
   .\Activate-StreamerDaily.ps1 -Unload
   ```

2. Başlatıcının kopyanın kaldırıldığını bildirdiğini doğrula.
3. Dosyaları yeni sürümle değiştir.
4. Başlatıcıyı yeniden çalıştır.

## Kaynak koddan derleme

Visual Studio 2022 **C++ ile masaüstü geliştirme** iş yükünü veya MSVC v143 ile Windows 10/11 SDK içeren Build Tools'u kur. Ardından:

```powershell
git clone https://github.com/VallaBenyemedim/SCC-StreamerDaily.git
cd SCC-StreamerDaily
.\Build-StreamerDaily.ps1
.\Activate-StreamerDaily.ps1 -CheckOnly
```

`-CheckOnly` yalnız okuma uyumluluk kontrolleri yapar. Başarılı kontrolden sonra etkinleştirmek için:

```powershell
.\Activate-StreamerDaily.ps1
```

## Sorun giderme

- **DLL bulunamadı:** `Build-StreamerDaily.ps1` çalıştır veya hazır sürüm arşivinin tamamını kullan.
- **Sürüm uyuşmuyor:** oyun güncellenmiştir; eski DLL'yi zorla yükleme.
- **Mono yüklenmedi:** oyunun ana menüsüne/kayıtlı oyuna girip yeniden dene.
- **Menü görünmüyor:** `Insert` tuşuna bir kez bas ve `SCC-StreamerDaily.log` içinde `Menu rendering active` satırını kontrol et.
- **Zaten aktif:** var olan menüyü kullan; başlatıcı ikinci kopyayı özellikle engeller.
- **Dosya değiştirilemiyor:** önce SCC'yi temiz biçimde kaldır. Yüklü DLL'yi değiştirmek için oyun sürecini zorla kapatma.

Proje Windows güvenlik yazılımını kapatmanı istemez. İndirilen binary'ye güvenmiyorsan yayımlanan kaynak koddan kendin derle.
