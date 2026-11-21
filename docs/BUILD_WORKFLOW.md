# Építés, Telepítés & Munkafolyamat Specifikáció (BUILD_WORKFLOW)

Ez a dokumentum a fordítási környezetet, a GitHub Actions CI folyamatot, a helyi gyorsfordítási lépéseket és a kiemelt referencia mérföldköveket tartalmazza.
A teljes történeti build napló a [milestones/BUILD_HISTORY.md](file:///c:/gitrepo/obs_vj-dj_plugin/milestones/BUILD_HISTORY.md) fájlban található.

---

## 1. FORRÁSFAJLOK ÉS KONFIGURÁCIÓK
* **`CMakeLists.txt`:** C++17, Qt6 (Core, Gui, Widgets), libobs, obs-frontend-api, Windows Media Foundation és WinMM könyvtárak.
* **`.github/workflows/build.yml`:** Automatizált felhős fordítás Windows 2022 virtuális gépen, OBS 33 SDK-val és Qt6-tal.

---

## 2. KÖVETELMÉNYEK ÉS FÜGGŐSÉGEK
* **Fordító:** Microsoft Visual C++ (MSVC v143 - Visual Studio 2022 x64).
* **Nyelvi szabvány:** C++17 (`set(CMAKE_CXX_STANDARD 17)`).
* **Keretrendszerek:**
  * **OBS Studio SDK:** Fejlécek és könyvtárak (`libobs.lib`, `obs-frontend-api.lib`).
  * **Qt 6.6+:** `Qt6Core.lib`, `Qt6Gui.lib`, `Qt6Widgets.lib`.
  * **Windows API:** `winmm.lib` (MIDI), `mfplat.lib`, `mfreadwrite.lib`, `mfuuid.lib`, `propsys.lib` (WMF audió dekódolás).
* **Generált Bináris:** Szigorúan **1 darab `obs_dj-vj_plugin.dll`**.

---

## 3. TELEPÍTÉSI ÚTVONALAK ÉS JOGOSULTSÁGOK
A kész binárist a Windows OBS Studio 64 bites bővítmény mappájába kell másolni:
* **Elsődleges célhely:** `C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll`
* **Biztonsági másolat:** `C:\Users\B166ER\Desktop\obs_dj-vj_plugin.dll`

*Telepítés az `install_plugin.bat` szkripttel:*
A repó gyökerében lévő `install_plugin.bat` rendszergazdaként futtatva automatikusan bezárja az OBS-t és átmásolja a friss binárist.

---

## 4. GITHUB ACTIONS FELHŐS FORDÍTÁS (CI PIPELINE)
Amikor a kódot felküldjük a GitHubra (`git push origin main`), lefut a `.github/workflows/build.yml`:
1. Elindul egy tiszta `windows-2022` futtató környezet.
2. Letölti a Qt6 csomagokat és az előre lefordított OBS Studio 33 SDK-t.
3. CMake-kel és MSVC-vel lefordítja a plugint Release módban.
4. Létrehoz egy `obs-vj-mixer.zip` artifactot.
5. Az artifact a GitHub API-n vagy a nightly.link-en keresztül letölthető.

---

## 5. HELYI FORDÍTÁS (WINDOWS 10/11 x64)
```powershell
# Környezeti változó beállítása az OBS SDK helyére:
$env:OBS_SDK_DIR = "C:\path\to\obs-studio-sdk"

# CMake konfigurálás
cmake -S . -B build -A x64 "-DOBS_SDK_DIR=$env:OBS_SDK_DIR"

# Fordítás Release módban
cmake --build build --config Release --parallel
```

---

## 6. KIEMELT REFERENCIA MÉRFÖLDKÖVEK

### Kiemelt Referencia Támpont: Build #222 / GitHub Run #231
* **Git Tag:** `milestone-run231-build222`
* **Commit:** `c680bd5f72af90d939729dd54952c5ed2d60c59f`
* **Kész bináris mentés:** `milestones/obs_dj-vj_plugin_run231_build222.dll`
* **SHA256:** `5CA2942365846B944C89AD3347F84D5AA141DBBF4082D7D12464214BE80C401D`
* **Visszaállítás parancsa:**
  `Copy-Item "c:\gitrepo\obs_vj-dj_plugin\milestones\obs_dj-vj_plugin_run231_build222.dll" "C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll" -Force`

### Aktuális Legfrissebb Állapot: Build #317 / GitHub Run #44891655313 (Milestone #33)
* **Commit:** `e4523dc`
* **SHA256:** `D2C887DC7BF5D8CECB4A729017BD16901984E80A197FC2DA904352C65C0F655B`
* **Méret:** 1 267 200 bájt
* **Főbb funkciók:**
  - Beállítások (Settings) ablak azonnali megnyílása (Akai Network MIDI fagyás és timeout megszüntetése `activated` és `QSignalBlocker` használatával).
  - Korg KAOSS DJ véletlenszerű lecsatlakozásának és Standby-ba esésének megelőzése.
  - Szálbiztos MIDI OUT (`std::mutex g_midi_out_mutex`) és belső LED feedback memóriakezelés.
  - Korg KAOSS DJ Jog kerék `BIN_OFFSET` irány és 50 ms / 2 ms Shift finomítás, folytonos Touch Strip Beatgrid húzás.

> [!NOTE]
> A korábbi 31 mérföldkő (Build #222-től Build #312-ig) és a korábbi fejlesztési backlog teljes részletes leírása megtalálható a **[milestones/BUILD_HISTORY.md](file:///c:/gitrepo/obs_vj-dj_plugin/milestones/BUILD_HISTORY.md)** archívumban.
