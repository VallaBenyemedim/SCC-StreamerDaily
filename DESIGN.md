# Arayüz sistemi

Menü, oyun üstünde hızlı okunan premium bir masaüstü kontrol merkezi olarak tasarlandı.

- Düzen: yeniden boyutlandırılabilir pencere, sabit sol gezinme, durum başlığı ve kaydırılabilir bento içerik alanı
- Stil: kömür siyahı katmanlar, yarı saydam yüzey hissi, ince çerçeveler ve 12–16 px köşe yarıçapı
- Temalar: varsayılan Neon Violet; alternatif Digital Cyan ve Premium Gold. Bağlantı yeşili ve tehlike kırmızısı semantik olarak sabit kalır
- Yazı: Segoe UI / Segoe UI Semibold ile 16 px gövde ve 25 px başlık hiyerarşisi; oyunla ek font dağıtılmaz
- İkonlar: aynı çizgi kalınlığında doğrudan ImGui çizim listesiyle üretilen vektör semboller; emoji kullanılmaz
- Etkileşim: kartın tamamından çalışan özel anahtarlar, görünür odak çerçevesi, hover/press geri bildirimi ve metinle desteklenen durum göstergeleri
- Hareket: yaklaşık 150–250 ms yumuşak anahtar, gezinme ve sayfa geçişleri; Sistem sayfasındaki azaltılmış hareket seçeneği bunları anında tamamlar
- Erişilebilirlik: koyu zeminde yüksek metin kontrastı, renk dışında durum metni, klavye gezinmesi ve en az 30 px kontrol yüksekliği
- Klavye: Dear ImGui klavye gezintisi açık, `Insert` menüyü açıp kapatır
- Performans: animasyonlar yalnız arayüz kare durumunu günceller; Mono güncellemesi grafik iş parçacığından ayrıdır ve yaklaşık 30 Hz ile sınırlandırılır

