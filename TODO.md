# Populous: Migrations-Checkliste

Stand: 2026-11-02, Inventarbasis `dfbe56d`; erste Originalwelt-Etappe im Arbeitsbaum.

Ziel ist die funktional vollstaendige C/SDL2-Portierung des vorhandenen
Amiga-Disassemblats, nicht ein neues Spiel mit aehnlichen Regeln. Launcher,
Spielmodul, A4-Zustand und originale Daten bleiben die Grundlage.

Diese Liste beruht auf einem statischen Abgleich der aktiven C-Pfade mit den
Originalquellen. Sie ist eine Bestandsaufnahme, keine bereits abgeschlossene
Pruefung jeder Assemblerroutine. Es gibt noch keinen belegten Fertigstellungsgrad
in Prozent. Die urspruengliche Bestandsaufnahme war statisch; neu umgesetzte Punkte
werden unten mit ihrer begrenzten Verifikation ergaenzt. Historische Quellzeilen
beziehen sich auf die Inventarbasis, neue Funktionsnamen auf den Arbeitsbaum.

## Legende und Reihenfolge

- `[x]` Vorhandene Grundlage; bedeutet nicht vollstaendige Originalparitaet.
- `[ ] Fehlt`: noch kein funktionaler Laufzeitpfad.
- `[ ] Teilweise`: vorhandenen Pfad vervollstaendigen, nicht neu erfinden.
- `[ ] Pruefen`: Umfang oder Originaltreue noch nicht abschliessend belegt.
- **P0:** Paritaetsinventar und reproduzierbare Arbeitsgrundlage.
- **P1:** Originalwelten, Bewohnerregeln, Aktionssteuerung, Kraefte und KI.
- **P2:** Kampagne, vollstaendige Praesentation, Audio und Speichern/Laden.
- **P3:** Zweispielermodus und Abschlusspruefung der Gesamtmigration.
- **Optional:** historische Diagnosewerkzeuge und zusaetzliche Hardwaretreue.

Prioritaet bezeichnet die empfohlene Reihenfolge, nicht den Aufwand.
Tests gehoeren zu jedem Schritt und werden nicht bis P3 aufgeschoben.

## Bereits vorhandene Grundlage

- [x] Sicherer Launcher-zu-Spiel-Uebergang, SDL-Fenster, Software-Renderer-Fallback und Ressourcenbereinigung.
- [x] A4-Speicher mit gueltigem negativem Bereich, native Grafikpointer und 22-Byte-Bewohnerdatensaetze.
- [x] Original-ADF sowie extrahierte Rohdaten, Grafiken, Masken, Paletten und Herkunfts-/Hashinformationen im Repository.
- [x] Original-LAND-Regeln fuer Wachstum, Mana, Waffen und Kampfwerte; kein Ersatz durch erfundene Tabellen.
- [x] Original-RNG-Algorithmus; dessen Aufrufreihenfolge ist noch nicht originalgetreu nachgewiesen.
- [x] 65x65 Hoehenpunkte, 64x64 Zellen, Terrainableitung und lokale Aktualisierung beim Formen.
- [x] Begrenzte normale Bewegung, Siedlungen, Wachstum, Mana, Abspaltung, Vereinigung, Anfuehreruebergabe, Zweierkampf und Wasserverlust.
- [x] Originaler Spielhintergrund, LAND-Gelaende, Siedlungsaufsaetze, kleine Sprites, Vorderkanten und Minimap im isometrischen Renderer.
- [x] Indizierte AND/OR-Maskenkomposition statt unzutreffender einfacher Transparenz.
- [x] Hoehenabhaengige Auswahl, Heben/Senken, begrenzter Magnet, Tendenzen, Kamera, Pause, Reset und Beenden.
- [x] Acht synthetische Starts und einfaches bevoelkerungsbasiertes Matchende.
- [x] Tests fuer Regeln, Eingaben, Grafik, Extraktion, Fehlerpfade und Speicherbereinigung; ASan/UBSan-Testziele.

Belege: `populous_c_port/README.md:11-46`,
`populous_c_port/populous_prg.c:645-1068`,
`populous_c_port/graphics.c:42-139`, `populous_c_port/land_data.c:23-69`.

## P0 - Umfang und Entwicklungsgrundlage absichern

- [ ] **Pruefen:** Alle relevanten Routinen und Datentabellen aus `src/populous_main.asm` und `src/populous_prg.asm` erfassen und einem aktiven C-Pfad zuordnen: migriert, teilweise, fehlend, durch Hostadapter ersetzt oder im Original unbenutzt.
- [ ] **Pruefen:** Originale Aufrufketten verfolgen; Deklarationen und leere Legacy-Wrapper nicht als Implementierung zaehlen. Umgekehrt sind leere `_sculpt`-, `_mouse`- oder Sprite-Wrapper kein Beweis fuer fehlende Terrainbearbeitung, Eingabe oder Grafik.
- [ ] **Teilweise:** Veraltete Stub-/Demo-Kommentare und widerspruechliche Statuspassagen in den READMEs mit dem aktiven Code abgleichen.
- [ ] **Pruefen:** Referenzversion der bereitgestellten ADF und Abweichungen zum Disassemblat feststellen; erkannte Dateilaengen allein belegen keine genaue historische Release-Zuordnung.
- [ ] **Teilweise:** Reproduzierbaren Build aus frischem Checkout absichern. Die lokale WSL-Entwicklungsumgebung ist jetzt persistent (GNU Make 4.4.1, SDL2 2.32.10), die README ersetzt veraltete `/tmp`-Befehle. Ein Bootstrap fuer frische Rechner und deren Verifikation fehlen noch.
- [ ] **Teilweise:** Asset-Suche fuer verschobene Checkouts/installierte Builds festlegen; Umgebungsvariablen bleiben nutzbar, der einkompilierte absolute Pfad darf nicht die einzige verlaessliche Voreinstellung sein.
- [ ] **Pruefen:** Unterstuetzte Plattformen festhalten. POSIX-Dateizugriffe, GCC-Attribute, GNU Make und `timeout` begrenzen derzeit die Portabilitaet. Native Windows-/MSVC-, macOS-, ARM- und Big-Endian-Unterstuetzung nicht ungeprueft zusagen.

**Abnahme:** Ein frischer Checkout laesst sich auf einer benannten Zielumgebung
bauen, testen und starten; das Routineninventar macht die verbleibende Arbeit
nachvollziehbar. Plattformerweiterungen sind von Original-Spielparitaet getrennt.

Belege: `populous_c_port/Makefile:1-35`,
`populous_c_port/populous_prg.c:79-80`, `populous_c_port/populous_prg.c:322-487`,
`populous_c_port/graphics.c:1-51`.

## P1 - Originalwelten und Leveldaten

- [x] `level.dat` zur Laufzeit laden: 99 Zehn-Byte-Datensaetze, `--level 0..494`, Datensatz `N/5`, interne Nummer `5*N`, vollstaendiger BE16-Seed-Offset. Laenge, Terrain und Dateityp werden validiert; weitere Bytes sind keine Authentizitaetspruefung.
- [x] Levelparameter fuer Gegnerstaerke, Reaktionsgeschwindigkeit, Kraeftefreigaben beider Seiten, Spielmodus, Terrain und Startgruppen installieren. Kraefteausfuehrung und strategische KI bleiben offen.
- [x] Assemblygestuetzte Hoehengenerierung einschliesslich drei `_make_thing`-Durchlaeufen und initialer RNG-Reihenfolge fuer Originalstarts; synthetische Starts bleiben unveraendert.
- [x] Originale Wald-/Felsverteilung mit bedingtem RNG-Verbrauch und bestehenden Terrainregeln verbinden.
- [x] Block 47 bei Kartenupdates gemaess Original erhalten beziehungsweise bei vollstaendiger Ueberflutung entfernen.
- [ ] **Pruefen:** Weitere Karten-Sonderfaelle und Wechselwirkungen mit spaeteren Spezialterrainbloecken migrieren.
- [x] Ueberlappende Conquest-Startbelegung gemaess Original zulassen: alle Bewohnerdatensaetze erhalten, letzter Eintrag repraesentiert die Zelle. Feindlichen aktuellen Bewohner vor Gruendung/Abmarsch behandeln; verdeckte/doppelte Stadtgruendung verhindern. Keine Verlegung der Originalstarts.
- [ ] **Teilweise:** Custom-/Serial-Startmodi sowie originale Kollisionswarte-Erzeugung und phasenweise Bewegung fehlen weiterhin. Conquest-Suchpfade und Blockade-Retry sind implementiert; der historische Zugriff auf Zelle 4096 bleibt sicher ausgelassen.
- [ ] **Pruefen:** Kartenrotation, Kartenbearbeitung und weitere Welt-/Modusfunktionen aus dem Routineninventar nach tatsaechlicher Originalverwendung zuordnen und migrieren.

**Abhaengigkeiten:** LAND-Lader und RNG sind vorhanden. Conquest benoetigt diesen
Schritt; KI und Kraefte muessen auch auf nichtflachen Originalwelten funktionieren.
**Abnahme:** Ausgewaehlte Original-Level erzeugen reproduzierbare Hoehen, Bloecke,
Parameter und Startbewohner; Referenzvergleiche gehen ueber einen gueltigen Start hinaus.

Neue Umsetzung: `populous_c_port/level_data.c` (`level_data_parse`,
`level_data_select`, `level_data_load`) und `populous_c_port/populous_prg.c`
(`original_clear`, `original_make_thing`, `original_woods`,
`original_place_people`, `game_reset_start`).

Unabhaengige assemblygestuetzte Vergleichsberechnung: alle 495 Hoehen-/Block-/
Hoehenstufen-/Overlaykarten, geordneten Besitzer-/Positionsdatensaetze, exakte
Letztschreiber-Belegung, Seeds und Anfuehrer stimmen ueberein. Keine unabhaengige
Amiga-Ausfuehrung. Die vorher abgewiesenen Starts 70, 74, 93, 94, 160, 201, 204,
472, 477, 478, 479 werden jetzt ohne veraenderte Karten oder Platzierungen geladen.

Die Begegnungslogik bleibt an der bisherigen C-Bewegungsschranke. Blockadeflag
0x40 und der Wartezustands-Verbrauch sind inzwischen migriert (siehe unten);
Erzeugung des Kollisions-Warteflags 0x20 und phasenabhaengige vorherige Belegung
fehlen weiterhin. Keine tickgenaue Gesamtparitaet behauptet.
Der exhaustive Originalstart-Test `test-level-local` hat wegen Assetladen/Simulation
ein separates 120s-Zeitlimit. Testdatenisolation und transaktionaler Reset bleiben
abgesichert. Verifikation der Ueberlappungsetappe: Build, `test`, `test-sanitize`
und alle 495 Originalstarts (Laden, Rendern, 100 Ticks mit Belegungs-/Kampfinvarianten,
Reset) normal und unter ASan/UBSan bestanden. Originaldaten-Matchregressionen dieser historischen Ueberlappungsetappe
lagen bei Sieg Tick 8353 und Niederlage Tick 694. Mit Kampfverstaerkung liegt der
erneut gepruefte Sieg bei Tick 7537 (279 Kaempfe, 190 Stadtgruendungen, 39278/0),
Niederlage weiter Tick 694. Nach freundlicher Zustandsvereinigung: Sieg Tick 9058,
271 Kaempfe, 296 Stadtgruendungen, 93085/0; Niederlage Tick 694. Normal-/Sanitizer-
laeufe und alle 495 Starts bestanden ohne veraenderte Siegerassertionen.
Nach originaler Siedlungsaufloesung mit geloeschtem Bewegungsdelta: Sieg Tick 7515,
266 Kaempfe, 136 Stadtgruendungen, 17045/0; Niederlage unveraendert Tick 694.
Nach gemeinsamem originalem Kampfabschluss: Sieg weiter Tick 7515, nun 263 Kaempfe,
163 Stadtgruendungen und 27636/0; Niederlage weiter Tick 694. Siegerassertionen
und Eingabeskript unveraendert. Nach verzoegertem Wassereintritt: Sieg Tick 7516,
263 Kaempfe, 164 Stadtgruendungen und 27250/0; Niederlage Tick 695.
Unabhaengiges Review ohne
blockierende Befunde; kurze Integrationstests sind kein Kampagnen-Paritaetsnachweis.

Historische Belege: `populous_c_port/populous_prg.c:546-600`, `:956-975`, `:1319-1325`;
`src/populous_prg.asm:1189-1270`, `:1475-1517`, `:7652-7859`, `:8535-8626`, `:12340-12362`;
`docs/_posts/2025-06-14-level-format.md:113-131`.

## P1 - Bewohner, Navigation und Kampf vervollstaendigen

- [x] Blockierte Navigation (`999` -> Flag 0x40, Zaehler 7), Warteframes 101/102, Verlust pro Wartetick und Retry bei vorherigem Zaehler >14 migrieren; keine doppelte Bewegungsverlustbuchung beim Retry.
- [ ] **Teilweise:** Gesamten Bewohner-Zustandsautomaten abgleichen: Erzeugung der Kollisions-Warteflags 0x20, weitere Flagkombinationen, phasenweise Bewegung, Anfuehrer-/Folgerzustand und Uebergaenge zwischen Bewegung, Stadt und Kampf.
- [x] Gewoehnliche Magnetnavigation: Original-Richtungstabelle mit Nordschritt am Ziel, Anfuehrer-/Ansichtserwerb, Folgerziel, gegenueberliegende Ausweichhistorie und Historie bei acht Fehlversuchen; direkter Block-47-Schritt im Krieg, keine Felsfreigabe fuer Ausweichschritte.
- [x] Originale `_get_heading`-Zielwahl als isolierte Grundlage: Selbstreferenz ohne passenden Gegner, sonst erster naechster lebender Fremdbesitzer ohne Bit 7; Manhattanabstand, keine Weg-/Belegungsfilter. Sichere vierbytebreite Einserindizes im A4-Hostzustand statt nativer Pointer.
- [x] Bestehende Heading-Referenzen in Navigation aktivieren: gueltiges Ziel beibehalten, einmaliger Zielwechsel bei Tot/Freund/Bit 7/gleicher Zelle; Anfuehrerfreigabe vor Kriegszielreset, keine Stadtgruendung, Sumpfvermeidung und Kriegs-Wasserbefehle. Opaque ungueltige Referenzen bleiben ohne Dereferenz erhalten.
- [ ] **Teilweise:** Heading in Sonderkampf und Darstellung vervollstaendigen; Terrain-Nachrichten ausserhalb des Kriegs anbinden. Hostindizes sind kein originales Speicherdateiformat.
- [x] Gewoehnlichen Beitritt zu laufenden Kaempfen migrieren: richtige Partei verstaerken, Population bis 32000, Waffen/Anfuehrer/Ansicht/Metadaten uebertragen, bestehende Kampfpartner und Frames erhalten. Ungueltige Paare sicher ablehnen; originale phasenweise Begegnungszeitpunkte und Sonderfiguren bleiben offen.
- [x] Zeitverbrauch des exakten Restzustands 0x80: leere Belegung beanspruchen, 16-Bit-Zaehler senken und erst bei vorherigem Nullwert entfernen (40 -> 41 Updates). Normale Begegnungen ignorieren Bit-7-Bewohner, ohne deren Datensatz zu loeschen; belegte Stadtgruendung bleibt gesperrt.
- [x] Exakten Geh-Siegeszustand 6 verbrauchen: Wortframe 85 -> 86 -> 87, danach Flag 0x04 loeschen und normalen Gehframe weiterschalten; waehrenddessen kein Schritt, Bevoelkerungsverlust oder RNG. Vorzeichen-/Ueberlaufgrenzen und Wasser-Vorrang bleiben explizit getestet.
- [x] Exakten Stadt-Siegeszustand 5 verbrauchen: gleiche Feierframes, danach Stadtframe aus aktueller Kapazitaet berechnen; keine Fussabdruckpflege, Mana, Wachstum, Abspaltung oder Aufloesung im Abschluss-Tick. Normaler Stadtpfad erst beim naechsten Bewohnerupdate.
- [x] Heading-gebundenen Einzelgewinner gegen Nicht-Stadt-Verlierer abschliessen: Siegeszustand 5/6, originale Belohnungsprioritaet und Wort-/32-Bit-Manaarithmetik, Besitzerbuchung, geschuetzte Drittbelegung und Ansichtsuebergabe. Kein neuer globaler Kampf-/Todeszeitpunkt.
- [x] Heading-gebundene Stadtniederlage: Belohnung bei exakt null versus negativer Restpopulation unterscheiden, Staedte niederbrennen und Restzustand 0x80 erzeugen; ohne Heading des Gewinners Stadtuebernahme mit originalem Timer. Besitzerbuchung und bestehende Siegesverbraucher verbinden, gleiche-Pass-Reihenfolge erhalten.
- [x] Neuen Wassereintritt ohne Kampfbit verzoegern: Stadtfussabdruck entfernen, exakt 0x12 setzen und nur vorzeichenbehaftete Frames >=8 nullen; keine Verluste/Fatalentfernung, Rettungsnachricht oder Bewegung im Eintrittsupdate. Gemischte Landrueckkehr und Kampf-Wasserausnahme bleiben getrennte Folgeschritte.
- [x] Bereits nasse Nichtkaempfer auf Wasser mit originaler ASL.W-/SUB.W-Arithmetik verarbeiten: erst Wortverlust, dann vorzeichenbehaftete Todesentscheidung; Ueberlebende behalten Flags/Frames/Timer/Fussabdruck. Fatalmodus ohne neue Rettungsnachricht, nichtfataler Kriegsbefehl vor Verlust; bestehende Host-Aufraeumregeln bleiben erhalten.
- [x] Exakten Wasserzustand 0x12 an Land zurueckfuehren: Fatalmodus vor Erholung, sonst doppelten Wortverlust buchen, Wasserbit loeschen und ersten Gehframe auch bei negativer Restpopulation ausfuehren; nur positive Population gelangt zum zweiten Frame und normaler Bewegung. Gemischte Wasserflags und Kampfpfade bleiben getrennt.
- [x] Trockene Wasser-Siegeszustaende 0x15/0x16 mit Feierzustand erhalten: erster Frame ignoriert, zweiter entscheidet Abschluss, dritter berechnet Stadt-/Gehframe; negative Restpopulation erst danach entfernen, exakt null ohne Frameaenderung. Kein normaler Geh-/Stadtlauf im selben Update.
- [x] Trockene Wasser-Wartezustaende 0x32/0x52/0x72 erhalten: doppelter Wort-Wasserverlust, zwei Warteframes, separater Warteverlust und originaler unsigned Retry-Zaehler. Bei Ablauf auch null/negative Population zum bestehenden aktuellen-Zellen-Begegnungspfad lassen, danach sicher aufraeumen; keine zusaetzliche Gehattrition oder Sumpf-Abschlusspruefung.
- [x] Trockenen Wasser-Brandrest 0x90 als 0x80 fortsetzen: Wortverlust vor Belegungs-/Timerverbrauch, alten unsigned Timer auch bei null/negativer Restpopulation senken, danach Ablauf oder Tod aufraeumen. Fatalmodus unveraenderter Timer/Frame, Drittbelegung schuetzen; keine normale Bewegung oder Terrainfolge.
- [ ] **Teilweise:** Gemischte Sonderflags, Ertrinkensphasen, Darstellung und originale Wortueberlaeufe der Kampfverluste migrieren. Der Host behaelt bisherige Verluste/Entscheidungen; extreme Waffenwerte koennen vom originalen SUB.W abweichen.
- [ ] **Teilweise:** Weitere Sonderfiguren und `_do_funny`-/`_do_place_funny`-Ereignisse, auch bei erschoepften 208 normalen Bewohnerplaetzen; Gameplay-Ritter sind separat angebunden.
- [x] Sumpf-Folgen fuer normale Bewohner am abgeschlossenen Gehframe: gesamte Population entfernen, Effekt 66, flachen Sumpf zu Block 15 zuruecksetzen; bodenlosen Sumpf erhalten.
- [x] Verbrauch des gesetzten Kriegszustands und Siedlungsaufloesung: vier Ziele auf 0x0820; exakte friedliche Stadt bei Kapazitaet <=0, Metadaten oder Krieg aufloesen, Frame/Delta leeren und Tick beenden. Bewohnerdaten/Belegung erhalten, vorhandenen Stadtfussabdruck entfernen; kein vorgezogener Mana-/Wachstums-/Bewegungsschritt.
- [x] Nachgelagerter Terrain-Befehlsempfaenger: je Besitzer drei Bytes, Besitzerreihenfolge 0/1, unabhaengiger Controller-/Divisor-Latch, Opcode 1 mit originalem rekursivem Heben, Wortkosten und lokalen Kartenupdates; terminale Partie verwirft alte Befehle.
- [x] Kriegsgebundene Wasserproduzenten: primarer Magnet-Wasserschritt schreibt aktuelle Zelle vor Ausweichsuche; vorhandenes Wasserflag erzeugt vor Verlust/Rueckkehr einen Befehl ausser bei Fatalwasser/Kampf. Je Besitzer letzter Schreiber, keine Sofort-Terrainmutation; neuer Wasserzustand erzeugt erst im Folgepass.
- [ ] **Teilweise:** Nichtkriegs-KI-Produzenten, originale Ertrinkensphasen und weitere Terrain-/Aktionsopcodes anbinden. Ungueltiger Controller-1-Null-Divisor wird sicher abgewiesen, nicht durch einen erfundenen Takt ersetzt.
- [x] Zustandsuebertragung bei freundlicher Zielzellen-Vereinigung nach `_join_forces`: Metadaten-/Stadtsperre, Anfuehrer/Ansicht, Bevoelkerung/Waffen und Zuruecksetzen der Wartebits/Frames. Nicht mit unveraenderten Kampfframes bei Verstaerkung vermischen.
- [x] Gemeinsamen originalen Abschluss fuer alle gueltigen Einzelgewinner verwenden; gewoehnliche Kaempfe erhalten ebenfalls Siegeszustand 5/6, Ansichts-/Besitzerbuchung, geschuetzte Drittbelegung und originale Manaarithmetik. Alte Ersatz-Abschlusslogik entfernt; Kampfverluste und Doppel-Tod bleiben separate bestehende Pfade.
- [x] Gewoehnliche Stadtwirtschaft mit originalen Breiten: signiertes Mana-Tabellenwort modulo32 addieren, Wachstum nach Abspaltungsversuch modulo16 speichern und am normalen Stadtende nichtpositive Population aufraeumen. Acht-Tick-Takt und Waffenbyte erhalten; Siedlungsaufloesung umgeht weiterhin diese Todespruefung.
- [x] Siedlungsfussabdruck nach Wirtschaft/Abspaltung und vor Todespruefung pflegen: nur bei Framewechsel, flachem Block oder unvollstaendiger Stadt; alten Frame vor Neuberechnung merken. Normale und uebersprungene Wirtschaft erreichen dieselbe Endpflege, Aufloesung bleibt separater Rueckweg.
- [x] Abgespaltene Bewohner selektiv initialisieren: wiederverwendete Bytes 6/7/21 erhalten, Delta/Frame/Heading/Marker20 leeren und Ansicht unabhaengig vom Anfuehrer uebertragen. Erste freie Normalstelle, Radius vor Elterninkrement und geschuetzte Drittbelegung erhalten; spaetere Slots werden noch im selben Pass verarbeitet.
- [ ] **Pruefen:** Weitere Sonderfaelle bei Stadtuebernahme, Kampfende, Belohnungen, gleichem-Zellen-Vereinigungszeitpunkt, Tod und Belegungswechseln gegen die Originalpfade testen; gewoehnlicher Zweierkampf existiert bereits.
- [ ] **Pruefen:** Bewusste Host-Sicherheitsgrenzen dokumentieren und gegen originale Wortarithmetik abgrenzen: Vereinigungshelfer bis 32000, allgemeine Manahelfer -250..1999999; rohe Terrain-Befehle, Einzelgewinner-Kampfabschluesse, Stadtwirtschaft und Verluste bereits untergetauchter Nichtkaempfer nutzen dagegen ihre originalen Wort-/32-Bit-Regeln. Speicher-/Undefiniertes-Verhalten-Sicherheit nicht zugunsten vermeintlicher Treue aufgeben.
- [ ] **Pruefen:** RNG-Aufrufreihenfolge, Tick-Reihenfolge und Zustandshashes mit Originalreferenzen vergleichen; gleicher RNG allein ergibt keinen gleichen Spielverlauf.

**Abnahme:** Jeder migrierte Zustand und Uebergang hat reproduzierbare Tests;
keine lebenden unsichtbaren Siedlungen, verlorenen Anfuehrer oder widerspruechlichen
Belegungen. Spezialkraefte werden mit betroffenen Bewohnerzustaenden zusammen getestet.

Belege: `populous_c_port/populous_prg.c:803-915`, `:1509-1549`;
`src/populous_prg.asm:3425-3432`, `:3613-3633`, `:3998-4006`, `:4068-4079`,
`:4425-4431`, `:6663-6765`.

## P1 - Aktionssteuerung und goettliche Kraefte

- [ ] **Teilweise:** Originalen Aktionspfad mit Werkzeugwahl, Zielvalidierung, Freigabemasken, Modusbedingungen und Rueckmeldung herstellen. Bestehende Tastaturaktionen erhalten, aber nicht als vollstaendige Toolbar-Migration behandeln.
- [ ] **Teilweise:** Heben/Senken und Magnet gegen alle Originalzulassungen, Kosten, Ziele und Zustandsbedingungen abgleichen.
- [x] Gameplay-Erdbeben auf aktuellem Viewport: 2500 Mana, aktive Freigabe 0x08, zwei spaltenweise 9x9-Durchlaeufe mit bedingtem RNG, rekursivem Heben/Senken und lokaler Kartenableitung; Port-Taste Q.
- [ ] **Teilweise:** Originales Erdbeben-Zittern (20 reine Terrainbilder, -4/-1 Pixel), Sound, Toolbar und Editor-Freimodus migrieren. Keine Ganzbildverschiebung als Originaleffekt ausgeben.
- [x] Gameplay-Vulkan: 10000 Mana, Freigabe 0x40, 100 Spielerpunkte, 165 verschachtelte Hoehenziehungen plus 64 Lavaziehungen, Originalschutz fuer zweimal denselben blauen Anfuehrer und beide Magnetziele; Port-Taste V auf Viewportursprung.
- [ ] **Teilweise:** Vulkan-Toolbar, Audio, Editor und Original-Nachrichtenzeitpunkt migrieren; Original-Lava erweitert Dirty-Bounds nicht und wird nicht pauschal als Krater/Bewohnerschaden umgedeutet.
- [x] Gameplay-Sumpfkraft: aktive Freigabe, 5000 Mana, 30 Versuche/60 RNG-Ziehungen, Punkte, Block 53 und einmalige Zielwahl ueber Port-Taste S. Originale Vertex-minus-eins-Bytekoordinaten erhalten; Bounds vor Kartenlesezugriff pruefen.
- [ ] **Teilweise:** Sumpf in Originaltoolbar, Editor-Freimodus und Computer-KI integrieren; Audio fehlt. Diagnose-P darf keine kostenfreien Kraefte freischalten; War-Abbruch behaelt die bewaffnete Zielwahl wie im Original.
- [x] Gameplay-Ritter: aktiver Anfuehrer, Freigabe 0x20, 7500 Mana, 150 Spielerpunkte, originale Heading-Zielwahl und Markierung; Port-Taste K. Navigation, Sonderkampfabschluss und kanonische Sprites sind angebunden.
- [ ] **Teilweise:** Ritter-Toolbar, Audio, Editor-/KI-Ausloesung und vollstaendige Bewegungs-/Kampfparitaet vervollstaendigen.
- [x] Gameplay-Flut: aktive Freigabe 0x80, 40000 Mana, 250 Spielerpunkte, globale einmalige Absenkung positiver Hoehen und vollstaendige Kartenableitung ohne RNG; Port-Taste F.
- [ ] **Teilweise:** Originale Folge-Tick-/Ertrinkensparitaet (insbesondere Kaempfe), Flut-Audio, Toolbar, KI und Editor-Freimodus migrieren.
- [x] Gameplay-Armageddon: Freigabe 0x0100, 80000 Mana, 5000 Spielerpunkte, Metadaten aller belegten Datensatzplaetze leeren und Krieg aktivieren; Port-Taste A. Erneute bezahlte Ausloesung im Krieg erlaubt; Folgen erst in Bewohnerticks, manuelle Terrain-/Magneteingabe im Krieg gesperrt.
- [ ] **Teilweise:** Armageddon-Toolbar, Audio, Editor-/KI-Ausloesung und originale Sonderfiguren-/Ertrinkens-/Endabrechnungsparitaet vervollstaendigen.
- [ ] **Fehlt:** Fuer jede Kraft die gesamte Wirkungskette integrieren: Auswahl, Freigabe, Mana/Punkte, Weltveraenderung, Bewohnerfolgen, Animation und spaeter Audio. Ein grafischer Effekt allein erfuellt den Punkt nicht.
- [ ] **Pruefen:** Originale Tendenz-/Befehlsmodi und Aktionsnachrichten vollstaendig zuordnen; dies bildet auch die spaetere Multiplayer-Grundlage.

**Abnahme:** Jede Kraft besitzt positive und abgewiesene Eingabetests, exakte
Kosten-/Zustandspruefungen sowie Tests an Kartenraendern und belegten Feldern.

Belege: `populous_c_port/populous_prg.c:917-953`, `:1446`, `:1659-1667`;
`src/populous_prg.asm:6769-6819`, `:7567`, `:7863`, `:8046`, `:8271`, `:8295`,
`:8392`, `:8482`, `:17707`, `:18376`.

## P1 - Originale Computer-KI

- [ ] **Fehlt:** Gegnerische Magnet-/Anfuehrerstrategie (`_set_devil_magnet`).
- [ ] **Fehlt:** Computer-Kraftwahl und -ausfuehrung (`_devil_effect`, `_do_computer_effect`).
- [ ] **Fehlt:** Automatische Gelaendenivellierung (`_make_level`, `_one_block_flat`). `_make_level` bezeichnet hier KI-Gelaendearbeit, nicht den Leveldatei-Loader.
- [ ] **Fehlt:** Bewertung, Reaktionsgeschwindigkeit, erlaubte Kraefte und weitere Levelparameter in KI-Entscheidungen einbeziehen.
- [ ] **Pruefen:** KI-Aufrufreihenfolge und RNG-Verbrauch unter unterschiedlichen Welten, Manareserven und Spielmodi.

**Abhaengigkeiten:** Original-Levelparameter, Aktionspfad und Kraefte.
**Abnahme:** Gegner handeln reproduzierbar nach den Originalentscheidungen statt
nur dieselben normalen Siedlungs-/Explorationsregeln wie der Spieler auszufuehren.

Belege: `populous_c_port/populous_prg.c:1030-1062`, `:1509-1543`;
`src/populous_prg.asm:3252-3269`, `:9017`, `:9148`, `:9526`.

## P2 - Originalbedienung und Grafik vervollstaendigen

- [ ] **Fehlt:** Toolbar anklickbar machen, Werkzeug-/Tendenz-/Modusicons und deren aktive/gesperrte Zustaende originalgetreu darstellen.
- [ ] **Fehlt:** Originale Manaanzeige, Schild, Bewohner-/Siedlungsabfrage und Informationsanzeigen. `_move_mana` ist Praesentation, nicht die bereits vorhandene Manaerzeugung.
- [ ] **Fehlt:** Requester, Optionen, Namens-/Texteingabe und originale Dialog-/Rueckkehrpfade.
- [ ] **Fehlt:** Originalschrift dekodieren und verwenden; eingebettetes `_font_data` und separates Launcher-`font.dat` vergleichen, nicht ungeprueft gleichsetzen.
- [ ] **Fehlt:** Originalen Mauszeiger und Auswahl-/Zielanzeigen statt Ersatzkreuz integrieren.
- [ ] **Fehlt:** Bereits extrahierte 32x32-Grosssprites in Laufzeit-Assets, Zeichnen und passende Spielzustaende integrieren.
- [x] Originalsprites fuer exakte Feierflags 5/6, Brandreste 0x80 und kanonische Heading-Geh-/Warte-/Wasserzustaende anbinden; originale Frameoffsets und Anker, volle Vierbyte-Praesenz statt Pointer-Halbwort, unveraenderter A4-Zustand beim Zeichnen.
- [x] Kampfframes nach beiden Verlustberechnungen und vor dem Abschluss weiterschalten: originale Dreierzyklen 70/130/134/138 nach roher Heading-Praesenz; exakt null ueberspringen, negative Vor-Clamp-Population beachten. Zeichnen nutzt gespeicherte gueltige Kampfframes ohne Zustandsmutation.
- [x] Beim aktuellen-Zellen- und Zielzellen-Kampfeintritt beide Frames nach den Kampfverknuepfungen initialisieren; keine vorgezogenen Verluste oder RNG. Exakt-null-Stadtniederlage im Heading-Pfad verwendet dadurch den originalen Kampf-Frame-Fallback fuer die Belohnung.
- [ ] **Teilweise:** Bewegungsinterpolation, weitere Sonderfiguren-/Effektanimationen und gemischte Darstellungsflags vervollstaendigen. Fuer ungueltige/Legacy-Kampfframes bleibt eine reine Ersatzdarstellung.
- [ ] **Fehlt:** Mundanimation aus `mouths.pic`, inklusive Frames, Palette und Aufruf-/Anzeigesequenzen.
- [ ] **Pruefen:** Ueberdeckung, Clipping, Vorderkanten, Sonderterrain, Minimap und Auswahl in allen Hoehen-/Kameralagen anhand von Originalbildern vergleichen.
- [ ] **Pruefen:** Alle tatsaechlich sichtbaren Palettenwechsel und Sprite-/Bildschirmsonderpfade erfassen und im Renderer abbilden; Hardwareimplementierung siehe separater Abschnitt.

**Abhaengigkeiten:** UI-Anzeigen koennen vorgezogen werden; Kraeftebuttons und
Sonderanimationen benoetigen die passenden Spielregeln.
**Abnahme:** Keine dekorativen Bedienelemente fuer implementierte Aktionen;
Referenzbilder und Eingabetests decken aktive, gesperrte und modale Zustaende ab.

Belege: `populous_c_port/populous_prg.c:1342-1446`, `:1545-1595`;
`populous_c_port/graphics.h:15-20`, `populous_c_port/extract_assets.py:230-234`;
`src/populous_prg.asm:16668-16684`, `:19894-19920`, `:19973-20025`;
`src/populous_main.asm:1815-1841`.

## P2 - Launcher, Intro und Bildschirmablaeufe

- [ ] **Fehlt:** Originale Startauswahl und Mausregionen statt konstanter Auswahl.
- [ ] **Fehlt:** Lade-/Intro-/Lord-Bilder in die originalen Ablaeufe integrieren; die vier Bilder sind bereits extrahiert.
- [ ] **Teilweise:** Bildloader/-darstellung fuer den Fuenf-Ebenen-/32-Farben-Fall von `load.pic` erweitern, ohne die Vier-Bit-Spielgrafik stillschweigend falsch zu interpretieren.
- [ ] **Fehlt:** Introablauf, zugehoerige Sprites/Texte und Uebergaenge gemaess Launcher-Quellcode.
- [ ] **Pruefen:** Bildschirmwechsel, Paletten, Laden, Abbruch und Rueckkehr zwischen Intro, Auswahl, Spiel und Ergebnis komplett verfolgen.

**Abnahme:** Start bis Spiel und Rueckkehr funktionieren ohne Testmuster oder
konstante Auswahl; Abbruch und Ladefehler hinterlassen keine Ressourcen.

Belege: `populous_c_port/populous_init.c:265-290`, `:363-378`;
`src/populous_main.asm:383-443`; `populous_c_port/graphics.c:57`.

## P2 - Spielende, Punkte und Conquest

- [ ] **Teilweise:** Originale Endbedingungen einschliesslich Aufgabe und gleichzeitiger Ausloeschung abgleichen. C zeigt derzeit DRAW; das Original prueft zunaechst die eigene Ausloeschung als Niederlage.
- [ ] **Fehlt:** Ergebnisstatistik, originale Punkteberechnung und Endbildschirm.
- [ ] **Fehlt:** Conquest-Welt-/Levelauswahl, Levelcodes und Kodieren/Dekodieren.
- [ ] **Fehlt:** Fortschrittsentscheidung nach Sieg, naechster Level, Wiederholung und zugehoerige Sequenzen.
- [ ] **Pruefen:** Grenzen der Kampagnentabelle, ungueltige Codes und alle originalen Spiel-/Optionsmodi.

**Abhaengigkeiten:** Level-Loader, Regeln/KI, Original-UI und Statistikzaehler.
**Abnahme:** Mehrere aufeinanderfolgende Original-Level lassen sich ohne
Demo-Reset spielen; Codes, Ergebnisse und Fortschritt stimmen mit Referenzen ueberein.

Belege: `populous_c_port/populous_prg.c:1055-1062`, `:1590`;
`src/populous_prg.asm:4323-4350`, `:12190`, `:14019`, `:14204-14272`,
`:14460-14501`, `:14793`, `:14900`.

## P2 - Audio

- [ ] **Pruefen:** Originale Audioquellen, Sample-/Instrumentformate, Sequenzen, Frequenzen und Ereigniszuordnung aus ADF und Assembly bestimmen. Der bisherige Export schliesst Audio aus.
- [ ] **Fehlt:** Benoetigte Audiodaten nachvollziehbar extrahieren und validieren.
- [ ] **Fehlt:** SDL-Audiopfad mit Kanalverwaltung, Mixing, Prioritaeten und sauberem Ressourcenlebenszyklus.
- [ ] **Fehlt:** Soundeffekte und originale Sequenz-/Measure-Wiedergabe an die richtigen Spiel-/Introereignisse anbinden.
- [ ] **Pruefen:** Timing, Lautstaerken, Unterbrechung/Priorisierung, Pause und Verhalten ohne Audiogeraet gegen Referenzen pruefen.

**Abnahme:** Verifizierte Originaldaten werden ereignis- und zeitgerecht abgespielt;
kein erfundener Ersatzsound gilt als abgeschlossene Audiomigration.

Belege: `populous_c_port/populous_init.c:302`,
`populous_c_port/populous_prg.c:433-439`, `:1675-1676`;
`populous_c_port/extract_assets.py:241`; `src/populous_prg.asm:20307-20821`.

## P2 - Speichern und Laden

- [ ] **Fehlt:** Originalformat mit Signatur, Byteordnung, Feldreihenfolge und Checksumme implementieren; A4-Hostspeicher nicht einfach als Datei schreiben.
- [ ] **Fehlt:** Hoehen, Karten, Bewohner, Magnet, RNG, Optionen und weitere im Original gespeicherte Felder vollstaendig serialisieren/deserialisieren.
- [ ] **Fehlt:** Save-/Load-Dialoge sowie sichere Dateipfade und atomaren Speicheraustausch auf dem Host integrieren.
- [ ] **Fehlt:** Laden vor Zustandswechsel validieren; fehlerhafte, verkuerzte oder inkompatible Dateien duerfen das laufende Spiel nicht beschaedigen.
- [ ] **Pruefen:** Originaldatei-Kompatibilitaet und Roundtrip testen; Fortsetzen muss bei identischen Aktionen denselben Zustand erreichen.

**Abhaengigkeiten:** Vollstaendig erfasster Spielzustand, Formatforschung und UI.
**Abnahme:** Speichern/Laden stellt eine laufende Partie wieder her, nicht bloss
einen Startlevel; fehlerhafte Dateien werden nachvollziehbar abgewiesen.

Belege: `populous_c_port/populous_prg.c:385-399`;
`src/populous_prg.asm:17123-17227`, `:17412-17501`.

## P3 - Zweispielermodus

- [ ] **Fehlt:** Originalen Verbindungs-/Nachrichtenablauf, Aktionspakete, Kompression und Seed-/Gleichlaufpruefungen erfassen und migrieren.
- [ ] **Pruefen:** Hosttransport festlegen und von der Spiellogik trennen. Moderne Netzwerkverbindung und Kompatibilitaet mit originaler serieller Hardware sind verschiedene Ziele.
- [ ] **Fehlt:** Verbindungsaufbau, Spielerrollen, Datenaustausch und synchronen deterministischen Simulationsablauf implementieren.
- [ ] **Fehlt:** Abbruch, Gegenstellenverlust, fehlerhafte Nachrichten und erkannte Desynchronisation sauber behandeln.
- [ ] **Fehlt:** Zwei Instanzen mit denselben Aktionsfolgen durch komplette Partien testen; RNG- und Zustandshashes vergleichen.

**Abhaengigkeiten:** Gemeinsamer Aktionspfad, deterministische Regeln, vollstaendige
Zustandsdefinition und Optionen. Funktionaler Zweispielerbetrieb gehoert zur
Spielparitaet; ein bestimmter physischer Serialadapter ist eine separate Entscheidung.

Belege: `populous_c_port/populous_prg.c:417-487`, `:1562`;
`src/populous_prg.asm:17734-17833`.

## Querschnitt - Nachweis der Originaltreue und Abschluss

- [ ] **Teilweise:** Bestehende Tests fortlaufend um jeden oben migrierten Originalpfad ergaenzen; gezielte Regression zuerst, dann Gesamtlaeufe und ASan/UBSan.
- [ ] **Fehlt:** Reproduzierbare Referenztraces aus einer passenden Originalausfuehrung fuer Kartengenerierung, RNG, Regeln, Kraefte, KI und Ergebnisse etablieren.
- [ ] **Teilweise:** Echte Original-Asset-Integrationstests und Bildvergleiche erweitern; synthetische Pixeltests allein belegen keine visuelle Paritaet.
- [ ] **Pruefen:** 50-ms-Simulation, Nachholen verspaeteter Ticks, Eingabereihenfolge, Renderinterpolation und Audiotiming gegen den Originalablauf abgleichen.
- [ ] **Teilweise:** Lange Partien, alle Terrainarten, ungueltige Eingaben, Ressourcenfehler, wiederholtes Laden/Reset und sauberes Beenden testen.
- [ ] **Pruefen:** Menschliche Spielbarkeit und Gewinnbarkeit bestaetigen. Automatisierte Siege und eine sichtbare Partie bis zur Niederlage ersetzen keinen manuellen Gewinnbarkeitstest.
- [ ] **Pruefen:** Fensterdarstellung auf der unterstuetzten Desktopumgebung tatsaechlich bestaetigen; ein laufender Prozess oder X11-Screenshot allein beweist keine Sichtbarkeit auf Windows.
- [ ] **Teilweise:** Warnungen und veraltete, unbenutzte Geruestpfade nach geklaerter Zuordnung bereinigen; keine funktionalen Adapter versehentlich entfernen.
- [ ] **Pruefen:** Abschliessendes Routinen-/Featureinventar ohne unbegruendete Luecken; verbleibende bewusste Abweichungen und nicht unterstuetzte Varianten dokumentieren.

**Gesamtabnahme:** Originalwelten, Regeln/Kraefte/KI, Kampagne, UI/Praesentation,
Audio, Save/Load und Zweispielerbetrieb sind auf dem benannten Zielsystem spielbar
und gegen Referenzen geprueft. Ungepruefte oder bewusst abweichende Bereiche bleiben
sichtbar, statt die Version vorzeitig als vollstaendig portiert zu bezeichnen.

Belege: `populous_c_port/Makefile:31-105`,
`populous_c_port/tests/test_game.c:563-665`,
`populous_c_port/tests/test_graphics.c:12-90`,
`populous_c_port/tests/test_runtime.c:86-179`,
`populous_c_port/populous_prg.c:977-992`.

## Separat - Erforderliche Anzeigeeffekte und optionale Hardwaretreue

- [ ] **Pruefen, fuer Funktionsparitaet relevant:** Copper-/Bitplane-/Spritepfade darauf untersuchen, welche sichtbaren Effekte noch fehlen. Den erforderlichen Effekt migrieren, nicht automatisch das gesamte Amiga-Chipset.
- [ ] **Teilweise, bei Bedarf:** Mehrere Bitplanes korrekt kombinieren, falls der bislang unbenutzte Low-Level-Helfer fuer neue Bildschirmablaeufe eingesetzt wird; alternativ den vorhandenen indizierten Renderer erweitern.
- [ ] **Optional:** Copper als deterministische virtuelle Rasterzustandsmaschine mit geordneten Registeraenderungen abbilden. Ein frei laufender OS-Nebenthread stellt Beam-Timing nicht her.
- [ ] **Optional:** Zyklusgenaue Copper-/DMA-/Blitter-Konkurrenz, PAL/NTSC-Spezifika und CRT-Effekte.
- [ ] **Optional:** Originales unaufgerufenes IFF-Screenshot-Testwerkzeug oder funktional gleichwertiger Host-Screenshotexport.

Der aktuelle Spielrenderer umgeht die Copper-/Bitplane-Stubs. Sein vorhandener
AND/OR-Compositor bildet relevante CPU-Spriteoperationen bereits ab. Ein kompletter
Amiga-Emulator ist deshalb keine belegte Voraussetzung fuer die funktionale Migration.
Weitere dynamische Hardwarepfade sind dennoch zu pruefen.

Belege: `populous_c_port/sdl_lowlevel.c:114-137`,
`populous_c_port/populous_prg.c:400`, `:1065-1068`, `:1438-1439`;
`src/populous_prg.asm:15291-15297`, `:19321-19354`, `:19699-19796`.

## Empfohlene naechste Etappen

1. P0-Inventar/Bootstrap vervollstaendigen; nach Originalwelt- und Ueberlappungsetappe die originalen Warte-/Bewegungszustaende systematisch migrieren.
2. Bewohner-Sonderzustaende, Aktionssteuerung und Kraefte gemeinsam vervollstaendigen; strategische KI anbinden.
3. Original-UI, Kampagne/Ergebnisse, Animationen/Intro, Audio und Save/Load abschliessen.
4. Zweispielerbetrieb und Gesamtparitaet pruefen; optionale Hardwaretreue getrennt verfolgen.
