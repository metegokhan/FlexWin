# FlexWin — Hafif ve Portatif Kurumsal Pencere Yöneticisi

[English](README.md) | **Türkçe**

**FlexWin**, Windows 10/11'de boyutlandırılması (resize) engellenmiş kurumsal uygulamaları (ERP, Java/Swing, Delphi, eski Win32/WinForms, Electron/WebView2 vb.) otomatik olarak hedef konum ve boyuta taşıyan, resize kilidini kıran, küçülten/kapatan veya şeffaflaştıran kurulumsuz native C++20/Win32 pencere yöneticisidir.

## Öne Çıkan Özellikler

- **Maksimum Hafiflik & Tam Portatiflik:** Statik CRT (`/MT`) ile derlenir; harici runtime (.NET, VC++ Redist vb.) gerektirmez. Tüm ikili dosyalar toplam ~880 KB yer kaplar ve ayarlar `FlexWin.exe` yanındaki `rules.json` dosyasında tutulur.
- **Çift Katmanlı Resize Kilidi Kırma (x64 & x86):**
  - *Dış Katman (Out-of-Process):* `WS_THICKFRAME | WS_MAXIMIZEBOX` stil açma, başlık kaldırma (`Borderless`), DWM görünmez kenar telafisi (`DWMWA_EXTENDED_FRAME_BOUNDS`), `SWP_NOSENDCHANGING` zorlaması ve Geri Zıplama Koruması (Anti-Snapback).
  - *Hook DLL Katmanı (`FlexWinHook64.dll` / `FlexWinHook32.dll` + `FlexWinHelper32.exe`):* İnatçı pencerelerin `WM_GETMINMAXINFO`, `WM_WINDOWPOSCHANGING`, `WM_STYLECHANGING` ve isteğe bağlı `WM_NCHITTEST` kilitlerini hem **64-bit** hem **32-bit** kurumsal uygulamalarda kırar.
- **Otomatik Kapatma, Küçültme ve Dereceli Şeffaflık:**
  - İstenmeyen pop-up/uyarı pencereleri için gecikmeli **Otomatik Küçült (Minimize)** ve **Otomatik Kapat (Close)** eylemleri.
  - `%10 – %100` arası **Dereceli Şeffaflık** kaydırıcısı ve isteğe bağlı **Yumuşak Geçiş (Smooth Fade)** animasyonu.
- **Profil Yedekleme & Çift Dil (TR / EN):**
  - Profilleri tek tıkla JSON olarak **Dışa Aktarma (Yedekleme)** ve **İçe Aktarma**.
  - Araç çubuğundan veya tepsi menüsünden anlık **Türkçe / İngilizce** dil değişimi.
- **Kolay Kullanım:**
  - **Sürükle-Bırak Nişangah Seçici (Crosshair Picker)**, **Açık Pencereler Tarayıcısı** (`x64`/`x86` ve Yönetici/UIPI bilgisi), canlı **Monitör Önizleme** (`PerMonitorV2` DPI-duyarlı), **Sistem Tepsisi**, **Yönetici Olarak Yeniden Başlatma** ve deneysel Win32 alt kontrol ölçekleme.

## Varsayılan Global Kısayollar

- `Ctrl + Alt + R`: Odaktaki aktif pencerenin resize kilidini anında açar
- `Ctrl + Alt + B`: Başlık çubuğunu aç/kapat (Borderless)
- `Ctrl + Alt + T`: Her zaman üstte (Always on Top) aç/kapat
- `Ctrl + Alt + A`: Aktif profildeki tüm kuralları hemen uygular

## Derleme ve Test (Visual Studio 2022 C++ Build Tools)

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1 -Config Release -RunTests
```

## Lisans

[MIT Lisansı](LICENSE) ile lisanslanmıştır.
