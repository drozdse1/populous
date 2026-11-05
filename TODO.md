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
- **P2:** Kampagne, vollstaendige Praesentation, Audio; Speichern/Laden im Originalformat erledigt.
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
- [ ] **Teilweise:** Custom-Karten sind portiert (`_setup_display` mit `_in_conquest` -1, asm:430-500: Seed, Bodenwechsel, Custom-Leutezahlen); Serial-Startmodi fehlen weiterhin; phasenweise Bewegung ist migriert. Conquest-Suchpfade und Blockade-Retry sind implementiert; der historische Zugriff auf Zelle 4096 bleibt sicher ausgelassen.
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
Erzeugung des Kollisions-Warteflags 0x20 ist migriert; phasenabhaengige vorherige Belegung
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
Nach lokalen Kapazitaetskorrekturen: isometrischer Sieg Tick 11938 (125 Kaempfe,
392 Stadtgruendungen, 192496/0), Diagnoseansicht Tick 11953 (125/402/212387/0).
Niederlage weiter Tick 695; Eingaben, Siegerassertionen und 12000-Tick-Budget in dieser Etappe unveraendert.
Nach Nichtkriegs-Wasserrettung gewinnt die alte Acht-Tick-Versenkung nicht mehr;
separate Originaldaten-Regression bestaetigt beide laufenden Partien bei Tick12000.
Mit Senkeingaben in jedem Simulationsschritt statt im Acht-Tick-Abstand
ab6000 gewinnen beide Ansichten bei Tick11928: isometrisch125/394/205816/0,
Diagnose125/401/219245/0 (historisch). Mit phasenweiser Bewegung verlieren diese Skripte bei Zug4801
(Regressionen --rescue-legacy-every-tick/--rescue-legacy-match); dieselben Eingaben ab Tick3000
gewannen bei Zug4185 (historisch). Mit Computer-KI: Sieg bei Zug4697 in beiden Ansichten (32/78/1434/0),
alte Skripte Niederlage bei Zug3104 (33/79/0/30639), Niederlagephase Zug831 (0/6323) (mit Original-`good_pop`). Eingaben unveraendert, strikte Siegerassertionen und Limits erhalten.
Fokussierte/vollstaendige Normal-/Sanitizerlaeufe, historische Strategien und alle495
Starts bestanden.
Unabhaengiges Review ohne
blockierende Befunde; kurze Integrationstests sind kein Kampagnen-Paritaetsnachweis.

Historische Belege: `populous_c_port/populous_prg.c:546-600`, `:956-975`, `:1319-1325`;
`src/populous_prg.asm:1189-1270`, `:1475-1517`, `:7652-7859`, `:8535-8626`, `:12340-12362`;
`docs/_posts/2025-06-14-level-format.md:113-131`.

## P1 - Bewohner, Navigation und Kampf vervollstaendigen

- [x] Blockierte Navigation (`999` -> Flag 0x40, Zaehler 7), Warteframes 101/102, Verlust pro Wartetick und Retry bei vorherigem Zaehler >14 migrieren; keine doppelte Bewegungsverlustbuchung beim Retry.
- [ ] **Teilweise:** Gesamten Bewohner-Zustandsautomaten abgleichen: Kollisions-Warteflag 0x20 und phasenweise Bewegung implementiert; weitere Flagkombinationen, Anfuehrer-/Folgerzustand und Uebergaenge zwischen Bewegung, Stadt und Kampf.
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
- [x] Kampfverluste modulo16 auswerten: signiertes SUB.W-Ergebnis entscheidet Ueberleben und Null-/Negativ-Framing vor gemeinsamem Kampfabschluss. Ueberlauf kann wieder positive Population ergeben; Host normalisiert gespeicherte nichtpositive Verluste weiterhin auf null und behaelt seine Doppel-Tod-Aufraeumreihenfolge.
- [ ] **Teilweise:** Gemischte Sonderflags, Ertrinkensphasen, Darstellung und originalen Kampf-/Todeszeitpunkt migrieren; transiente negative Kampfpopulation und vollstaendige originale Aufraeumregeln bleiben getrennte Arbeit.
- [x] `_do_funny`/`_do_place_funny` (asm:8630-9013): zwei Funny-Objekte in den `_peeps`-Plaetzen $D1/$D2, Typen 0..2 aus `_funny` (asm:24161-24171), Platzierung bei Spielzug 4096 (`_start_seed&3`, asm:752-760) und einmal bei erschoepften 208 Bewohnerplaetzen (`_funny_done`, asm:3996-4006); Frame-Pendel, Schritt alle 8 Zuege, Spuren (bk2 50..52, Block 53, Bloecke 47..49) und Entfernen von Bewohnern. Abweichung: das nicht uebergebene zweite Argument (Startkante, `($A,A5)`) ist im Port 0; Zellen ausserhalb der Karte werden nicht geschrieben.
- [ ] **Teilweise:** Weitere Sonderfiguren; Gameplay-Ritter sind separat angebunden.
- [x] Sumpf-Folgen fuer normale Bewohner am abgeschlossenen Gehframe: gesamte Population entfernen, Effekt 66, flachen Sumpf zu Block 15 zuruecksetzen; bodenlosen Sumpf erhalten.
- [x] Verbrauch des gesetzten Kriegszustands und Siedlungsaufloesung: vier Ziele auf 0x0820; exakte friedliche Stadt bei Kapazitaet <=0, Metadaten oder Krieg aufloesen, Frame/Delta leeren und Tick beenden. Bewohnerdaten/Belegung erhalten, vorhandenen Stadtfussabdruck entfernen; kein vorgezogener Mana-/Wachstums-/Bewegungsschritt.
- [x] Nachgelagerter Terrain-Befehlsempfaenger: je Besitzer drei Bytes, Besitzerreihenfolge 0/1, unabhaengiger Controller-/Divisor-Latch, Opcode 1 mit originalem rekursivem Heben, Wortkosten und lokalen Kartenupdates; terminale Partie verwirft alte Befehle.
- [x] Kriegsgebundene Wasserproduzenten: primarer Magnet-Wasserschritt schreibt aktuelle Zelle vor Ausweichsuche; vorhandenes Wasserflag erzeugt vor Verlust/Rueckkehr einen Befehl ausser bei Fatalwasser/Kampf. Je Besitzer letzter Schreiber, keine Sofort-Terrainmutation; neuer Wasserzustand erzeugt erst im Folgepass.
- [x] Bereits nasse Computerbewohner ausserhalb des Kriegs retten: Controllerwort exakt1 und Bauen erlaubt erzeugt denselben bezahlten Terrainbefehl vor Wasserverlust/Rueckkehr. Wortgrenzen, Wiederherstellung, Reihenfolge und Stadt-Latch getestet. Alte Acht-Tick-Versenkung als separate laufende Partie bei Tick12000 erhalten; Siegskripte senken nach6000 jetzt jeden Tick ueber dieselben legalen SDL-Eingaben, ohne geaenderte Siegerassertion oder Limits.
- [x] Primaeren Magnet-Wasserschritt ausserhalb des Kriegs anbinden: Controllerwort exakt1 und Bauen erlaubt erzeugt vor Ausweichsuche den Befehl fuer die aktuelle Zelle. Fatalwasser sperrt nur den bereits nassen Aufrufer; Opaque-Heading-Sperre und Felsregeln erhalten. Gemeinsamer gepruefter Mailboxhelfer, bezahltes Heben mit spaeterem Ueberqueren und gemischte Schreiber getestet; alle Normal-/Sanitizer-/Originaldaten-/495-Start-Pruefungen bestanden.
- [x] Primaeres Sumpfziel vor zielgerichtetem Computerbewohner heben: nur ausserhalb des Kriegs, Controllerwort exakt1 und weder Bauverbot noch Nur-Heben-Modus; Zielzelle statt aktueller Zelle in den gemeinsamen Mailboxpfad schreiben, danach normale Ausweichsuche. Geometrie-/Opaque-Sicherheitsgrenzen explizit erhalten; bezahltes Entfernen und spaeteres Ueberqueren getestet. Vollstaendige Normal-/Sanitizer-/Originaldaten-/495-Start-Pruefungen bestanden.
- [ ] **Teilweise:** Weitere Nichtkriegs-KI-Produzenten, originale Ertrinkensphasen und weitere Terrain-/Aktionsopcodes anbinden. Ungueltiger Controller-1-Null-Divisor wird sicher abgewiesen, nicht durch einen erfundenen Takt ersetzt.
- [x] Zustandsuebertragung bei freundlicher Zielzellen-Vereinigung nach `_join_forces`: Metadaten-/Stadtsperre, Anfuehrer/Ansicht, Bevoelkerung/Waffen und Zuruecksetzen der Wartebits/Frames. Nicht mit unveraenderten Kampfframes bei Verstaerkung vermischen.
- [x] Gemeinsamen originalen Abschluss fuer alle gueltigen Einzelgewinner verwenden; gewoehnliche Kaempfe erhalten ebenfalls Siegeszustand 5/6, Ansichts-/Besitzerbuchung, geschuetzte Drittbelegung und originale Manaarithmetik. Alte Ersatz-Abschlusslogik entfernt; Kampfverluste und Doppel-Tod bleiben separate bestehende Pfade.
- [x] Gewoehnliche Stadtwirtschaft mit originalen Breiten: signiertes Mana-Tabellenwort modulo32 addieren, Wachstum nach Abspaltungsversuch modulo16 speichern und am normalen Stadtende nichtpositive Population aufraeumen. Acht-Tick-Takt und Waffenbyte erhalten; Siedlungsaufloesung umgeht weiterhin diese Todespruefung.
- [x] Siedlungsfussabdruck nach Wirtschaft/Abspaltung und vor Todespruefung pflegen: nur bei Framewechsel, flachem Block oder unvollstaendiger Stadt; alten Frame vor Neuberechnung merken. Normale und uebersprungene Wirtschaft erreichen dieselbe Endpflege, Aufloesung bleibt separater Rueckweg.
- [x] Abgespaltene Bewohner selektiv initialisieren: wiederverwendete Bytes 6/7/21 erhalten, Delta/Frame/Heading/Marker20 leeren und Ansicht unabhaengig vom Anfuehrer uebertragen. Erste freie Normalstelle, Radius vor Elterninkrement und geschuetzte Drittbelegung erhalten; spaetere Slots werden noch im selben Pass verarbeitet.
- [x] Lokale Stadt-Kapazitaetskorrekturen vor Abspaltung: Frame42/Controller1/Population>305/Latch0 setzt Kapazitaet305 und Latch1, danach vorhandenes internes Cheatwort gegebenenfalls Kapazitaet50. Reales Stadtframe/Tabellenindex erhalten, keine neue Eingabefreigabe oder Terrainnachricht; geteilter Latch und voller Normalpool getestet.
- [x] Trockene Wasser-Stadt 0x11 an den normalen Stadtverbraucher anbinden: Wortverlust, erster Stadtframe nur bei nichtnull Population (auch negativ), danach Wachstum oder Aufloesung ohne vorgezogene Todespruefung. Aufloesung behaelt nichtpositive Datensaetze bis zum uebersprungenen Folgepass; gueltige Staedte nutzen vorhandene Kapazitaetskorrekturen, Abspaltung und bedingte Fussabdruckpflege.
- [ ] **Pruefen:** Weitere Sonderfaelle bei Stadtuebernahme, Kampfende, Belohnungen, gleichem-Zellen-Vereinigungszeitpunkt, Tod und Belegungswechseln gegen die Originalpfade testen; gewoehnlicher Zweierkampf existiert bereits.
- [ ] **Pruefen:** Bewusste Host-Sicherheitsgrenzen dokumentieren und gegen originale Wortarithmetik abgrenzen: Vereinigungshelfer bis 32000, allgemeine Manahelfer -250..1999999; rohe Terrain-Befehle, Kampfverluste und Einzelgewinner-Abschluesse, Stadtwirtschaft und Verluste bereits untergetauchter Nichtkaempfer nutzen dagegen ihre originalen Wort-/32-Bit-Regeln. Speicher-/Undefiniertes-Verhalten-Sicherheit nicht zugunsten vermeintlicher Treue aufgeben.
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
- [x] Originales Erdbeben-Zittern: 20 synchrone reine Terrainbilder (-4/-1 Zeilen, ohne Figuren, vor der Terrainaenderung), asm:7895-7925.
- [ ] **Teilweise:** Erdbeben-Sound migrieren (Toolbar-Icon, Zittern und Editor-Freimodus erledigt).
- [x] Gameplay-Vulkan: 10000 Mana, Freigabe 0x40, 100 Spielerpunkte, 165 verschachtelte Hoehenziehungen plus 64 Lavaziehungen, Originalschutz fuer zweimal denselben blauen Anfuehrer und beide Magnetziele; Port-Taste V auf Viewportursprung.
- [ ] **Teilweise:** Vulkan-Audio und Original-Nachrichtenzeitpunkt migrieren (Editor-Freimodus erledigt); Original-Lava erweitert Dirty-Bounds nicht und wird nicht pauschal als Krater/Bewohnerschaden umgedeutet.
- [x] Gameplay-Sumpfkraft: aktive Freigabe, 5000 Mana, 30 Versuche/60 RNG-Ziehungen, Punkte, Block 53 und einmalige Zielwahl ueber Port-Taste S. Originale Vertex-minus-eins-Bytekoordinaten erhalten; Bounds vor Kartenlesezugriff pruefen.
- [ ] **Teilweise:** Sumpf in Computer-KI integrieren (Toolbar-Icon und Editor-Freimodus erledigt); Audio fehlt. War-Abbruch behaelt die bewaffnete Zielwahl wie im Original.
- [x] Gameplay-Ritter: aktiver Anfuehrer, Freigabe 0x20, 7500 Mana, 150 Spielerpunkte, originale Heading-Zielwahl und Markierung; Port-Taste K. Navigation, Sonderkampfabschluss und kanonische Sprites sind angebunden.
- [ ] **Teilweise:** Ritter-Audio, KI-Ausloesung und vollstaendige Bewegungs-/Kampfparitaet vervollstaendigen.
- [x] Gameplay-Flut: aktive Freigabe 0x80, 40000 Mana, 250 Spielerpunkte, globale einmalige Absenkung positiver Hoehen und vollstaendige Kartenableitung ohne RNG; Port-Taste F.
- [ ] **Teilweise:** Originale Folge-Tick-/Ertrinkensparitaet (insbesondere Kaempfe), Flut-Audio und KI migrieren (Editor-Freimodus erledigt).
- [x] Gameplay-Armageddon: Freigabe 0x0100, 80000 Mana, 5000 Spielerpunkte, Metadaten aller belegten Datensatzplaetze leeren und Krieg aktivieren; Port-Taste A. Erneute bezahlte Ausloesung im Krieg erlaubt; Folgen erst in Bewohnerticks, manuelle Terrain-/Magneteingabe im Krieg gesperrt.
- [ ] **Teilweise:** Armageddon-Audio, KI-Ausloesung und originale Sonderfiguren-/Ertrinkens-/Endabrechnungsparitaet vervollstaendigen.
- [x] Editor-/Freimodus `_paint_map` wie im Original: Ein/Aus ueber PAINT MAP / PLAY GAME im Game-Setup (asm:13044-13056, CANCEL stellt wieder her, START GAME loescht); alle Kraft-Prologe (Heben/Senken, Flut, Erdbeben, Vulkan, Sumpf, Ritter nach Anfuehrertest, Armageddon) ueberspringen Mana-, Krieg-, Pause- und Freigabepruefung sowie die Kosten (Punkte bleiben, ausser Armageddon); Magnet ohne Bypass. `_animate` friert Bewohner, `_game_turn` und Spielende ein; Kartenklicks formen ohne Baubeschraenkung und kostenlos; `_paint_the_map`-Tasten F1-F8, Entf und 1-9 (Figuren/Anfuehrer setzen, Baum/Fels, Loeschen, Mana, Spiegeln, Land loeschen, Landschaft) ueber `_get_message` 7..13 und `_do_action` y=9..13. Diagnose-Taste P entfernt.
- [ ] **Teilweise:** Editor: Landschaften 4..8 (Tasten 5-9, eigene Landschaften) fehlen im Port; Kartenpunkte im Editor nur in der Diagnoseansicht.
- [ ] **Fehlt:** Fuer jede Kraft die gesamte Wirkungskette integrieren: Auswahl, Freigabe, Mana/Punkte, Weltveraenderung, Bewohnerfolgen, Animation und spaeter Audio. Ein grafischer Effekt allein erfuellt den Punkt nicht.
- [ ] **Pruefen:** Originale Tendenz-/Befehlsmodi und Aktionsnachrichten vollstaendig zuordnen; dies bildet auch die spaetere Multiplayer-Grundlage.

**Abnahme:** Jede Kraft besitzt positive und abgewiesene Eingabetests, exakte
Kosten-/Zustandspruefungen sowie Tests an Kartenraendern und belegten Feldern.

Belege: `populous_c_port/populous_prg.c:917-953`, `:1446`, `:1659-1667`;
`src/populous_prg.asm:6769-6819`, `:7567`, `:7863`, `:8046`, `:8271`, `:8295`,
`:8392`, `:8482`, `:17707`, `:18376`.

## P1 - Originale Computer-KI

- [x] Gegnerische Magnet-/Anfuehrerstrategie (`_set_devil_magnet`, asm 9601-9898), Aufruf vor `_devil_effect` bei freiem Latch (asm 3254-3270).
- [x] Computer-Kraftwahl und -ausfuehrung (`_devil_effect`, `_do_computer_effect`) mit Opcode-Verbrauchern 3 Beben, 4 Sumpf, 5 Magnet, 6 Vulkan, 14 Tendenz/Krieg/Flut/Ritter. `good_pop` und Spielende-Pruefung wie im Original (kein Unentschieden). Start/`r`-Reset behaelt wie `_clear_map` (asm 1023-1188) 0x99fc, good_pop/bad_pop und `_stats`+0x14/+0x16/+0x22/+0x26/+0x2a (`--start-reset`). Offen: Opcodes 7-13, `_toggle`-Mana, Kartenrand-Ziele (Host lehnt x/y>56 ab).
- [x] Automatische Gelaendenivellierung (`_make_level`, `_one_block_flat`). **Annahme:** das fruehe Exit-Byte in peep+20 ist das Low-Byte der `_stats`-Adresse (0xa4/0xd2 bei Disassembly-Basis, `MAKE_LEVEL_EXIT_BYTE_OWNER*`), ladeadressabhaengig und nicht gemessen. `_make_level` bezeichnet hier KI-Gelaendearbeit, nicht den Leveldatei-Loader.
- [x] Bewertung (`_stats`+$c: passiver Tendenzpfad solange Staedte < 2*Bewertung+15 oder `game_turn`%90 < Bewertung+10, asm 9612-9627), Reaktionsgeschwindigkeit (`_stats`+$10: Latch eines Controller-1-Spielers nur bei `game_turn`%Tempo==0 frei, `_get_message` asm 17932-17950), erlaubte Kraefte (`_stats`+$e, BTST vor jeder Wahl in `_devil_effect`/`_set_devil_magnet`/`_make_level` und erneut im Verbraucher) und Levelinstallation (asm 406-427) in KI-Entscheidungen; Test `--ai-level-parameters` (alle 512 Kraeftemasken, Tempo 1..6, Bewertung 0..10 an beiden Grenzen, Determinismus bei festem Seed). Korrigiert: `_do_computer_effect` zielt nur OHNE eigenen Anfuehrer auf die eigene Magnetzelle (asm 9530-9536), der Port hatte die Bedingung invertiert.
- [ ] **Pruefen:** KI-Aufrufreihenfolge und RNG-Verbrauch unter unterschiedlichen Welten, Manareserven und Spielmodi.

**Abhaengigkeiten:** Original-Levelparameter, Aktionspfad und Kraefte.
**Abnahme:** Gegner handeln reproduzierbar nach den Originalentscheidungen statt
nur dieselben normalen Siedlungs-/Explorationsregeln wie der Spieler auszufuehren.

Belege: `populous_c_port/populous_prg.c:1030-1062`, `:1509-1543`;
`src/populous_prg.asm:3252-3269`, `:9017`, `:9148`, `:9526`.

## P2 - Originalbedienung und Grafik vervollstaendigen

- [ ] **Teilweise:** Toolbar anklickbar (alle `_zoom_map`-Icons, asm:1700-2451). Erledigt: Abfragemodus (`_interogate`) mit Schild (`_show_the_shield`, ohne Flaggen-/Waffen-Icons mangels `_icon_data`) und dauerhafte Icon-Hervorhebung (`_toggle_icon`) fuer Tendenz, Modus, Musik, FX und Pause. Options- und Spieloptionen-Requester (`_do_action` 7/8) oeffnen die originalen Dialoge. Offen: Nachrichten-Requester (`_do_action` 2), kurze Tastendruck-Blitze der Power-/View-Icons.
- [ ] **Teilweise:** Originale Manaanzeige erledigt (`_move_mana` asm:3164-3244: Sprite 69 auf der Manaskala nach `_mana_values`, reine Praesentation). Offen: weitere Bewohner-/Siedlungs-Informationsanzeigen.
- [ ] **Teilweise:** Requester und Optionen. Erledigt: `_requester` (Rahmen aus `_icon_data` 12..23, OK/CANCEL), `_check_cancel`, `_options` (Machtbits, Aggression, Rate), `_game_options` (Spiel/Karte malen, Gut/Boese, Mensch/Amiga, Custom Game, naechste Karte/Neustart fuer Eroberungswelten und Custom-Karten, Aufgeben), `_set_options` (`_game_mode`-Bits); Simulation pausiert, Eingabe geht an den Requester. Eroberung oeffnet die Weltauswahl `_show_world` mit `_get_name`. Zahl in der Weltauswahl startet diese Custom-Karte. Speichern/Laden ueber `_save_load` (siehe P2). Offen (angezeigt, aber wirkungslos): Zwei Spieler/Seriell, Nachrichten-Requester, Requester in der Diagnoseansicht.
- [x] **Erledigt:** Originalschrift dekodiert und genutzt (`amiga_text`, `_text` asm:19973-20052); `raw/font.dat` ist byteweise identisch mit dem eingebetteten `_font_data` (asm:21949-22372, 4400 Bytes geprueft).
- [ ] **Teilweise:** Originaler Mauszeiger erledigt (`_pointer` A4 0x84b6 wie im Original gesetzt, `_a_sprite`-Frames aus `_sprite_to_amiga` asm:16783-16830, Hotspot oben links nach `_move_pointer` asm:19928-19972; Ersatzkreuz entfernt). Abweichung: nach Sumpfziel bleibt der Port-Magnet samt Zeiger. Offen: weitere Auswahl-/Zielanzeigen.
- [x] Port-HUD (keine Originalfunktion, y 200..255) nutzt die Originalschrift ueber `amiga_text`, 40 Spalten x 7 Zeilen; Ersatzschrift `hud_text` entfernt.
- [x] 32x32-Grosssprites integriert: optionales `spr_32N.dat` (asm:16426-16449) geladen, `_draw_s_32` (asm:19365-19504, keine Zeilen bei y>=199, Clip bei x 320) nur fuer Funny-Eintraege der Sprite-Liste (+6 >= $D1, asm:630-641) bei (x-8,y-16); Position/Interpolation/Randausblendung nach `_move_sprite` (asm:16061-16197). Der view_who-Marker mit Frame $44 liegt ausserhalb der 13 Frames und wird nicht gezeichnet. Andere Bildschirme nutzen das Blatt im Original nicht.
- [x] Originalsprites fuer exakte Feierflags 5/6, Brandreste 0x80 und kanonische Heading-Geh-/Warte-/Wasserzustaende anbinden; originale Frameoffsets und Anker, volle Vierbyte-Praesenz statt Pointer-Halbwort, unveraenderter A4-Zustand beim Zeichnen.
- [x] Kampfframes nach beiden Verlustberechnungen und vor dem Abschluss weiterschalten: originale Dreierzyklen 70/130/134/138 nach roher Heading-Praesenz; exakt null ueberspringen, negative Vor-Clamp-Population beachten. Zeichnen nutzt gespeicherte gueltige Kampfframes ohne Zustandsmutation.
- [x] Beim aktuellen-Zellen- und Zielzellen-Kampfeintritt beide Frames nach den Kampfverknuepfungen initialisieren; keine vorgezogenen Verluste oder RNG. Exakt-null-Stadtniederlage im Heading-Pfad verwendet dadurch den originalen Kampf-Frame-Fallback fuer die Belohnung.
- [ ] **Teilweise:** Bewegungsinterpolation, weitere Sonderfiguren-/Effektanimationen und gemischte Darstellungsflags vervollstaendigen. Fuer ungueltige/Legacy-Kampfframes bleibt eine reine Ersatzdarstellung.
- [x] **Erledigt:** Mundanimation aus `mouths.pic` (6 Frames 48x35, `_read_mouth`/`_draw_mouth` asm:16668-16690, 19894-19922; Exporter verifiziert, Frame 0 = `lord.pic` an Byte 0x12/y 0x87) mit `lord.pic`-Palette und der `_won_conquest`-Sequenz (`core_lord_*`).
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

- [x] **Erledigt:** Originale Startauswahl `SUB_41C`/`SUB_456` (asm:330-442): `load.pic`, Klick-Latch des Vbi (asm:3743-3800), Regionen 3 Tutorial / 2 Conquest / 1 Spiel (Custom-Karte 0 wie das Original), sonst bzw. nach Zeitablauf Intro; Wahl als `_start_of_game` (`$42`, prg asm:94-96) an das PRG. Tutorial nach `_start_game` asm:13975-13999 (Seed `0x69bc`, `_tutorial_game_mode`, `_pause`, Stats, 10000 Mana, 15/3 Leute). CLI-Argumente umgehen den Launcher; `--tutorial` neu. Escape/Fenster schliessen beendet (Port).
- [x] **Erledigt:** `lord.pic` im Lord-Bildschirm nach einem Eroberungssieg (`_read_lord` asm:16631-16663, Fehlerpfad asm:14762-14765).
- [x] **Erledigt:** `load.pic` (Auswahl) und `demo.pic` (Intro) in die originalen Ablaeufe integriert.
- [x] **Erledigt:** `load.pic` mit fuenf Ebenen und eigener 32-Farben-Palette im Launcher; die Spielgrafik bleibt vierbittig.
- [x] **Teilweise:** Intro `SUB_1186` (asm:1517-1566): fallende/abprallende land0-Bloecke `SUB_10A2`/`SUB_110A`/`SUB_1C12` mit Launcher-RNG `SUB_6538`; Titel `SUB_E40` mit den Glyphen `SUB_1802` aus `LAB_5A0C` (asm:4270-4662, nach `intro_data.c` uebernommen; Banner 5/6/7, Buchstaben `$6e98` P O P U L O U S, Fallen $a8-drop, Steigen drop+8); `SUB_536`-Vorlauf (16 Drops, ohne Klicktest); Credits von `SUB_714` als zwei Textstreifen (`SUB_1D1A`, `SUB_5026` mit Ausblendmasken `LAB_1F62`, Zaehler 75/50, alle 51 Frames, Liste `LAB_1DCE` endlos); nur ein Klick beendet das Intro, danach wieder `load.pic`; Titelmusik `popsam`.
- [ ] **Fehlt:** Blockskript in `SUB_714` (Bytecode `LAB_1272`..`LAB_15F6`, obere Blockebene `SUB_50B2`/`SUB_1CBC`/`SUB_3F92`, fallende Bloecke in `SUB_536`); Tastatur-Abbruch ueber `SUB_5792`/`LAB_7158`; Paletten-Fades.
- [ ] **Pruefen:** Bildschirmwechsel, Paletten, Laden, Abbruch und Rueckkehr zwischen Intro, Auswahl, Spiel und Ergebnis komplett verfolgen (Auswahl -> Intro -> Auswahl -> Spiel ist getestet; Zeitablauf 0x55730 Durchlaeufe als 250 Frames geschaetzt; Beschriftung der drei Knoepfe nicht am Bild geprueft).

**Abnahme:** Start bis Spiel und Rueckkehr funktionieren ohne Testmuster oder
konstante Auswahl; Abbruch und Ladefehler hinterlassen keine Ressourcen.

Belege: `populous_c_port/populous_init.c` (`sub_456`, `launch_step`),
`populous_c_port/title_music.c`, `populous_c_port/tests/test_runtime.c` (`launcher`, `intro`, `cli`);
`src/populous_main.asm:330-442`, `:1234-1566`, `:2200-2339`, `:2418-3089`, `:3743-3800`.

## P2 - Spielende, Punkte und Conquest

- [x] **Erledigt:** Endbedingungen wie `_move_peeps` (eigene Ausloeschung/Aufgabe zuerst als Niederlage, dann Sieg); es gibt kein Unentschieden, die tote DRAW-Anzeige ist entfernt.
- [x] **Erledigt:** Ergebnisstatistik (`_end_text`-Zeilen), originale Punkteberechnung inkl. Serial-Bonus, Endbildschirm mit Originalschrift (`font.dat` = `_font_data`).
- [x] **Erledigt:** Levelcodes `_code`/`_decode`, `GENESIS`, CLI `--code NAME`.
- [x] **Erledigt:** Fortschritt nach Sieg (`_won_conquest`: naechster Level, Obergrenze 2470, Umlauf auf 0), Wiederholung nach Niederlage, Neustart bei synthetischen Szenarien.
- [x] **Erledigt:** Weltauswahl `_show_world` mit Namenseingabe `_get_name`, `level.dat`-Parameterzeilen, GENESIS, "NO SUCH WORLD", Seed-Eingabe; geoeffnet per `--conquest`, Spieloptionen "Eroberung" und nach einem Eroberungssieg (naechste Welt vorbelegt).
- [x] **Erledigt:** Lord-Bildschirm von `_won_conquest` (asm:14503-14765): Bild, `_con_text`-Zeilen (aus dem Endbildschirm dorthin verschoben, Endbildschirm wieder original), Mundanimation, Sprache `gwords`, Klick zur Weltauswahl.
- [ ] **Fehlt:** Endbildschirm in der Diagnoseansicht, "Disk einlegen"-Meldung bei fehlender `level.dat`.
- [ ] **Pruefen:** Gleichzeitige Ausloeschung und Aufgabe gegen Referenzlaeufe; Punktestand gegen einen echten Amiga-Lauf; alle originalen Spiel-/Optionsmodi.

**Abhaengigkeiten:** Original-UI (Requester), Audio, Lord-Bilder.
**Abnahme:** Mehrere aufeinanderfolgende Original-Level lassen sich ohne
Demo-Reset spielen (Testabdeckung: `test_end_transition`); Codes, Ergebnisse
und Fortschritt stimmen mit Referenzen ueberein.

Belege: `populous_c_port/populous_prg.c` (`core_end_stats`, `core_code`,
`core_decode`, `core_next_level`, `core_enter_end`, `core_leave_end`,
`amiga_text`, `core_draw_end_screen`);
`src/populous_prg.asm:4323-4350`, `:12278-12300`, `:14019-14455`,
`:14460-14790`, `:14793-15118`, `:19973-20052`, `:24726-24843`, `:25075-25173`.

## P2 - Audio

- [x] **Erledigt:** Quellen bestimmt: `_load_sound`/`_PlaySound`/`_setsound`/`_PlayMeas`/`_kill_effect`/`_check_effect` (asm:20307-20908), Vbi-Teil (asm:19620-19672), `_effect`-Setzer (asm:3434-3454, 4075, 6786, 7605-7647, 7900, 8088, 8266, 8477, 8529), Herzschlag-`_tempo` (asm:564-605). Spiel-Bank `gmusic1`, Sprach-Bank `gwords` (`_won_conquest`); `popsam` ist Intro-Musik von `populous_main`, `awords` wird von `populous.prg` nicht geladen.
- [x] **Erledigt:** `extract_assets.py` exportiert `raw/gmusic1` (90534 B) und `raw/gwords` (79076 B) mit strenger `_load_sound`-Validierung (exakter Verbrauch, Pufferindizes) und Tabellen in `values.json`; PCM bleibt roh. Beide Rohdateien liegen wie die uebrigen `raw/`-Dateien im Repo.
- [x] **Erledigt:** `audio.c`: Paula-Mixer (4 Kanaele, 8-Bit signed, 3546895/Periode, Lautstaerke 0..64, Zyklen/Schleifen, Warteschlange, Stereo 0/3 links, 1/2 rechts); ohne Audiogeraet stumm und lauffaehig.
- [x] **Erledigt:** Effekte 0x42..0x4d auf Kanal 2+3, Kampf 0x43/0x44 und Feuer 0x46 ueber `_move_sprite`-Flags, Herzschlag 0x47/0x48 auf Kanal 0, Prioritaeten (`_Soundpri`), Beben-Abbruch `_kill_effect(2,4)`, Musik `_PlayMeas` (`gmusic1`), Toolbar-Schalter (`_music_off`/`_effect_off` != 0 = an, Einschalten stoppt alle Kanaele).
- [ ] **Abweichung:** Zyklenzahl der zweiten `_PlaySound`-Anforderung liegt im Original ausserhalb der Argumente (undefiniertes Stackwort); der Port nutzt das Wiederholwort (+6) des Drumkit-Eintrags.
- [x] **Erledigt:** Lord-Sprache `gwords` (asm:14503-14765): Bank statt `gmusic1` geladen, `_words`/`_end_words` auf Kanal 2 per `_check_effect(2)` synchronisiert, danach `gmusic1` und die Schalter zurueck.
- [x] **Erledigt:** Intro-Musik `popsam` (`populous_main`): `extract_assets.py` exportiert `raw/popsam` (114718 B = 0x1c01e, `SUB_2C36` asm:2418-2471) mit strenger Pruefung der 8 Samples (BE32 Laenge, BE16 Rate, PCM; `SUB_2F9C` asm:2645-2662) und Tabelle in `values.json`; `title_music.c` portiert den Hubbard-Replayer `LAB_3060` (asm:2706-2896) mit der aus dem Listing uebernommenen Partitur `LAB_3532`..`LAB_3F91`; Paula-Latch in `audio.c`. Spielt nur im Intro.
- [ ] **Pruefen:** Timing, Lautstaerken und Klang gegen einen echten Amiga bzw. Emulator hoeren; Vbi wird im Port per `SDL_GetTicks` getaktet.

**Abnahme:** Verifizierte Originaldaten werden ereignis- und zeitgerecht abgespielt;
kein erfundener Ersatzsound gilt als abgeschlossene Audiomigration.

Belege: `populous_c_port/audio.c`, `populous_c_port/populous_prg.c`
(`core_PlaySound`, `core_PlayMeas`, `core_load_sound`, `core_vbi_audio`,
`core_tempo_pass`), `populous_c_port/extract_assets.py` (`sound_values`),
`populous_c_port/tests/test_audio.c`, `tests/test_game.c` (`test_sound`, `test_lord_screen`);
`src/populous_prg.asm:564-614`, `:1744-1781`, `:3434-3454`, `:13966-13972`,
`:19620-19672`, `:20307-20908`, `:24129-24134`.

## P2 - Speichern und Laden

- [x] Originalformat implementiert (`_save_game`/`_checksum`/`_load_game`): Signatur `$FFFFB615`, Big-Endian, Feldreihenfolge wie `_Write`, 29646 Bytes, Wortsummen-Checksumme ueber `_alt` und vier Karten plus `good_pop+bad_pop`; feldweise Umwandlung statt Hostspeicher-Dump, Bewohnerzeiger als `_peeps`-Adressen.
- [x] Alle Originalfelder: `_alt`, `_map_alt/_blk/_bk2/_who`, `_magnet`, `_peeps`, beide `_stats`, `_mode`, `_seed`, `_start_seed`, `_player`, `_ground_in`, `_war`, `_game_turn`, `_conquest`, `_in_conquest`, `_paint_map`, `_battle_won`, `_game_mode`, `_level_number`, `_score`; Nachlauf wie Original (`___load_ground`, `_not_player`, Magnete, `no_peeps`, Mailboxen, `_get_heading`, Zeiger).
- [x] `_save_load`-Requester in SAVE A GAME / LOAD A GAME (Verzeichnisliste `.GAM`, Blaettern, `_get_name` fuer Verzeichnis/Dateiname, ERROR IN SAVING). Speicherort `$POPULOUS_SAVE_DIR`, sonst `$XDG_DATA_HOME/populous/saves`, sonst `~/.local/share/populous/saves`; Speichern ueber Temporaerdatei + `rename`.
- [x] Laden validiert vor jeder Aenderung (Groesse, Signatur, Checksumme, `_player`, `_ground_in`, LAND/Grafik) und uebernimmt den Zustand gestaffelt; Abweichung vom Original (prueft nur die Signatur) dokumentiert.
- [x] Tests: Roundtrip (Speichern -> Laden -> erneut Speichern bitgleich bis auf die vom Original neu berechneten Felder), Byte-Layout gegen das asm-Format, defekte/verkuerzte/ueberlange/inkompatible Dateien ohne Zustandsaenderung, Requester-Bedienung.
- [ ] **Pruefen:** Kompatibilitaet mit echten Amiga-`.GAM`-Dateien (keine Testdatei vorhanden); Zeiger anderer Ladeadressen werden ueber `_get_heading` neu bestimmt.

**Abhaengigkeiten:** Vollstaendig erfasster Spielzustand, Formatforschung und UI.
**Abnahme:** Speichern/Laden stellt eine laufende Partie wieder her, nicht bloss
einen Startlevel; fehlerhafte Dateien werden nachvollziehbar abgewiesen.

Belege: `populous_c_port/populous_prg.c` (`core_save_game`, `core_load_game`, `core_file_*`);
`src/populous_prg.asm:10307-10972`, `:11016-11079`, `:16841-16915`, `:17123-17703`.

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
