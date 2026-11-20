# AI-asszisztált Fejlesztési Munkamenet és Optimalizáció

Az AI-asszisztált fejlesztőkörnyezetekben (mint az Antigravity IDE vagy hasonló, LLM-alapú autonóm kódolók) a legnagyobb kihívás a **kontextus (tokenek) szigorú menedzselése**. Az alábbi dokumentum összefoglalja a hatékony munkavégzéshez szükséges struktúrát és legjobb gyakorlatokat.

## 1. Az ideális munkamenet (Workflow)

A fejlesztés egy folyamatos "mikro-ciklusokból" álló iteráció kell, hogy legyen:

1. **Tervezés és Scope:** Emberként meghatározod a konkrét, pici feladatot.
2. **Kontextus betöltése:** Az AI elolvassa a szabályokat és az aktuális feladatot.
3. **Generálás/Módosítás:** Az AI megírja a kódot.
4. **Tesztelés és Validálás:** Rendszer vagy AI által futtatott tesztek.
5. **Állapot frissítése:** Az AI (vagy a fejlesztő) bejegyzi a sikert a nyomkövető fájlba.
6. **Tiszta lap (Context Wipe):** Új chat/session indítása a következő feladathoz, hogy a korábbi próbálkozások ne terheljék a memóriát.

## 2. Rendszer promptok: Mi kerül az `AGENTS.md` fájlba?

Az `AGENTS.md` az ügynök "agyának" alapbeállítása. **Ne ide tedd a dinamikus adatokat (todo, státusz)!** 

**Mi legyen benne? (Statikus, irányadó adatok - max 100-200 sor)**
* **Tech stack és verziók:** Pontosan milyen nyelveket, keretrendszereket használ (pl. "React 18, Tailwind 3, TypeScript - szigorú típusossággal").
* **Kódolási konvenciók:** Architektúra szabályok (pl. "Ne használj class komponenseket", "Minden adatbázis hívást a `services/` mappába tegyél").
* **Viselkedési szabályok:** Pl. "Mielőtt kódot írsz, mindig nézd meg a `TODO.md` fájlt", vagy "Ne írj helykitöltő (placeholder) kommenteket, mindig a teljes kódot generáld le".

## 3. A dokumentáció és a dinamikus állapot struktúrája

A naplózást, tervezést és tesztelést külön fájlokba kell szedni egy dedikált (pl. `docs/`) mappában:

* **`TODO.md` vagy `PROGRESS.md`:** A feladatok listája. Az ügynök munka előtt olvassa el, munka után pipálja ki, amit elvégzett.
* **`ARCHITECTURE.md`:** A rendszerterv, adatbázis sémák, mappastruktúra magyarázata. Csak akkor hivatkozd be, ha új komponenst kell létrehozni.
* **`TEST_STRATEGY.md`:** A tesztelés menete, használandó parancsok és elvárások.

## 4. Tokenfelhasználás és futási idő csökkentése

* **Fájlok feldarabolása (Modularitás):** Törekedj a max 200-300 soros fájlokra (SOLID elvek). A hosszú fájlok módosítása drága és lassú. Akkor bonts szét egy fájlt, ha több elkülöníthető feladatot lát el.
* **Kontextus ürítése (Clear Chat):** Ha egy feladat kész, nyiss új munkamenetet. A hosszú chat history a lassulás fő oka.
* **Célzott fájlhivatkozások (Mentions):** Csak a releváns fájlokat lásd el az AI-val (pl. "A `auth.ts` és `Login.tsx` fájlok alapján javítsd a hibát").
* **Részfeladatok (Chunking):** A feladatokat bontsd a lehető legkisebb, jól definiálható egységekre.
* **AI Ignore (`.gitignore` analógia):** Zárd ki a build mappákat, logokat, nagy JSON fájlokat az AI kontextusából (pl. `.aiignore`).

## 5. Állapot menedzselése: A Check-in / Check-out protokoll

A leghatékonyabb módszer a dinamikus állapot követésére egy dedikált `STATUS.md` vagy `MEMORY.md` fájl karbantartása.

**A folyamat:**
1. **Prompt kiegészítés indításkor:** *"Minden feladat elkezdése előtt olvasd el a `STATUS.md`-t."*
2. **A `STATUS.md` tartalma:**
   * **Jelenlegi cél:** A sprint/nap fő fókusza.
   * **Kész feladatok:** Röviden a már lefejlesztett funkciók.
   * **Következő lépés:** A konkrét "Next Action".
   * **Ismert hibák / Elakadások:** Zsákutcák, amiket el kell kerülni.
3. **Prompt kiegészítés záráskor:** *"Ha végeztél, frissítsd a `STATUS.md` fájlt a tényekkel, mielőtt befejezed a válaszadást."*

Ezzel a módszerrel a "memória" egy rövid szövegfájlban marad, minimalizálva a tokeneket és maximalizálva az áttekinthetőséget.

---

## 6. Munkamenet-optimalizációs Mesterprompt (Repo Audit & Workflow Refactor)

Ha a projekt dokumentációja elburjánzott, megnőtt a tokenfogyasztás, vagy az `AGENTS.md` túl nagyra duzzadt az előzményektől, másold be az alábbi promptot az AI asszisztensnek egy új, tiszta munkamenetben:

````markdown
Te a projekt Vezető Rendszerarchitektje és Munkamenet-Optimalizációs Szakértője vagy.
A feladatod a repó teljes átvizsgálása, a kontextus/token-terhelés optimalizálása, valamint az `IDE.md` dokumentumban rögzített elvek szerinti fejlesztési munkamenet kialakítása és élesítése.

Kérlek, szigorúan kövesd az alábbi 4 fázisú folyamatot:

### 1. FÁZIS: Teljes Körű Kódbázis és Dokumentáció Audit
Vizsgáld át szisztematikusan a teljes repository struktúráját és dokumentációs rendszerét:
1. **Mappaszerkezet:** Térképezd fel az összes mappát (`.github/`, `docs/`, `milestones/`, `src/`, gyökér).
2. **Dokumentációk leltára:** Vizsgáld meg az összes `.md` fájl méretét, sorainak számát és funkcióját (`AGENTS.md`, `IDE.md`, `README.md`, `docs/*.md`, `milestones/*.md`).
3. **Összevetés az `IDE.md` 5 alapelvével:**
   - **AGENTS.md mérete és fókusza:** Mennyire haladja meg az ajánlott 100–200 sort? Tartalmaz-e dinamikus build changelogokat, korábbi verziókat vagy olyan adatokat, amiknek nem ott a helyük?
   - **Állapotkövetés (Check-in/Check-out):** Létezik-e dedikált `STATUS.md` vagy `PROGRESS.md` fájl az aktuális célokhoz és hibákhoz?
   - **Kontextusvédelem:** Létezik-e `.aiignore` a build mappák, logok és felesleges nagy fájlok kiszűrésére?
   - **Moduláris dokumentáció:** A `docs/` mappa anyagai (pl. `docs/BUILD_WORKFLOW.md`) mennyire tiszták és átláthatóak, a korábbi build leírások archiválva vannak-e a `milestones/` alá?
   - **AI Router & Hivatkozások:** A `docs/INDEX.md` naprakész-e, és pontosan leköveti-e a forrásfájlokat?

### 2. FÁZIS: Optimalizációs Terv és Token-Megtakarítási Kalkuláció
Készíts egy részletes, pontokba szedett Implementációs Tervet:
1. **AGENTS.md radikális karcsúsítása:**
   - Mutasd be a tervezett új, tömör (`~100–150 soros`) `AGENTS.md` vázat (kizárólag: tech stack, kódolási konvenciók, szálbiztonsági alapszabályok, viselkedési direktívák).
   - Jelöld meg a 30+ korábbi build naplójának pontos archiválási helyét (pl. `milestones/BUILD_HISTORY.md`).
2. **STATUS.md bevezetése:**
   - Tervezd meg a dinamikus állapotfájl pontos szerkezetét (Aktuális Sprint / Fókusz, Kész funkciók, Következő lépés, Ismert elakadások).
3. **.aiignore létrehozása:**
   - Tervezd meg a kizárási mintákat (build, .vs, bin, *.obj, nagy JSON gyorsítótárak).
4. **docs/ és INDEX.md szinkronizálása:**
   - Biztosítsd, hogy a moduláris leírások és hivatkozások épek maradjanak.
5. **Várható eredmény és token-megtakarítás:**
   - Számítsd ki és mutasd be a várható token-megtakarítást (körönkénti kontextusterhelés csökkenése).

### 3. FÁZIS: Felhasználói Jóváhagyási Kapu (STOP!)
**ÁLLJ MEG!** Semmilyen fájlt ne módosíts és ne hozz létre önhatalmúlag!
Mutasd be a fenti tervet, emeld ki a kulcsfontosságú változtatásokat, és kérj kifejezett jóváhagyást a felhasználótól.

### 4. FÁZIS: Végrehajtás és Ellenőrzés (Kizárólag jóváhagyás után)
A jóváhagyás megérkezése után hajtsd végre az optimalizációt:
1. Hozd létre az archiváló fájlt (`milestones/BUILD_HISTORY.md`) a történeti build naplóknak.
2. Írd át az `AGENTS.md`-t a tiszta, tömör verzióra.
3. Hozd létre a `STATUS.md` fájlt a legutolsó stabil állapot (pl. Build #233) rögzítésével.
4. Hozd létre a `.aiignore` fájlt.
5. Frissítsd a `docs/INDEX.md` térképet az új struktúrához.
6. Végezz ellenőrzést (fájlméretek, linkek épsége), és készíts záró Walkthrough jelentést.
````