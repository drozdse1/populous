# KI und Zustandsautomaten im C-Port von Populous

Überblick für Entwickler. Alle Angaben beziehen sich auf `populous_c_port/populous_prg.c`
(C-Funktionsnamen) und `src/populous_prg.asm` (Zeilennummern „asm:NNNN“, so wie sie in den
Kommentaren des C-Codes stehen). Adressen sind A4-Displacements (`A4_WORD(0x…)`/`A4_LONG(0x…)`).
Stand: Snapshot des Codes zum Zeitpunkt der Erstellung – die Datei wird parallel weiterentwickelt,
Zeilennummern im C-Code können sich verschieben, Funktionsnamen sind stabiler.

Kennzeichnung: **[nicht portiert]** = im C-Port leer/fehlend, **[unsicher]** = nicht vollständig
verifiziert.

---

## 1. Spielschleife

### 1.1 Äußere Schleife: `_animate()`
- Startet über `game_reset_start()`, optional zuerst den Welt-Auswahlbildschirm (`core_world_open(CORE_WORLD_START)`).
- Pro Host-Durchlauf: `___keyboard()` (SDL-Events) → 50-Hz-VBI-Emulation (`core_vbi_audio()`,
  `core_lord_vblank()`, alle 20 ms; asm:19620-19672) → Spiel-Ticks alle 50 ms (max. 5 Nachhol-Ticks):
  `core_tempo_pass()` (nur ohne Requester/Ergebnis), dann `game_tick()` → nach dem Tick ggf.
  `core_enter_end()` (Endbildschirm; im Original `___end_game` aus `_move_peeps`, asm:4323-4347)
  → `game_render()` (`___draw_it(yoff,xoff)` bzw. Lord-Bildschirm).

### 1.2 Ein Spielzug: `game_tick()`
Reihenfolge (Original `_animate` asm:676-750):

1. Abbruch, wenn `pause`, `prg_quit`, `core_result` (Spiel entschieden) oder ein Requester offen ist (`core_req.kind`).
2. **Paint-Map-Modus** (`paint_map`, Landschaftseditor): kein `_move_peeps`, kein `game_turn++`, nur
   `view_timer`-Countdown und `core_consume_terrain_mailboxes()`.
3. `game_turn++` (16 Bit, läuft über).
4. `view_timer`-Countdown (temporäre Ansicht via `core_set_temp_view`, endet nach 10 Zügen).
5. `_move_peeps()` – KI beider Seiten, dann alle Figuren (Abschnitt 2/5).
6. Siegprüfung (asm:4323-4347): `good_pop(player)==0` oder Kapitulation des Spielers (`_surender`,
   `A4 0x8258 == player`) → `core_result=2` (verloren); sonst dasselbe für `not_player` → `core_result=1`
   (gewonnen). Es gibt kein Unentschieden.
7. Mailboxen: bei Ergebnis nur `core_clear_terrain_mailboxes()`, sonst `core_consume_terrain_mailboxes()`
   (Abschnitt 4). Wichtig: **Befehle werden im Zug gesammelt und erst danach ausgeführt** –
   Gelände ändert sich nie mitten in der Figurenbewegung.
8. „Effekte“: Sound-Effekte werden über das Wort `effect` (`A4 0x2fac`) angestoßen, das die Mächte
   setzen (z. B. 76 Erdbeben, 75 Vulkan, 74 Flut, 73 Krieg, 69 Ritter, 77 Magnet, 66 Sumpftod);
   abgespielt in der Audio-/Tempo-Schicht. `_do_funny()` ist **[nicht portiert]** (leere Funktion).
9. Zeichnen geschieht außerhalb von `game_tick()` in `game_render()`.

---

## 2. Zustandsautomat der Spielfiguren (Peeps)

### 2.1 Tabelle `_peeps`
- Basis `A4 0x9c26` (`peeps`), Datensatz 22 Bytes (`peep_word(i,off)` / `peeps[i*22+off]`).
- Speicher: der Bereich bis `_no_peeps` (`A4 0xae5e`) umfasst 212×22 Bytes; das Original löscht beim
  Start **211** Einträge (`CMP.W #$00d3`, asm:1036-1046), erlaubt aber nur **208** belegte Slots
  (`CMPI.W #$00d0,_no_peeps`, asm:6984; Kind-Suche asm:3996). Der C-Port nutzt durchgehend 208
  (`MAX_DEMO_PEEPS`, `core_ai_slot()`), löscht 208×22 Bytes (`original_clear`).
- `_no_peeps` = höchster benutzter Index+1; tote Slots (Population ≤ 0) werden wiederverwendet.
- `map_who[cell]` (`A4 0x1f88`, 64×64) enthält Slot+1 der Figur auf einer Zelle (0 = leer).

| Offset | Größe | Bedeutung (verifiziert an den genannten Stellen) |
|---|---|---|
| +0 | Byte | Zustands-Flags (siehe 2.2) |
| +1 | Byte | Besitzer 0/1 (`peep_owner`) |
| +2 | Byte | Erkundungsreichweite für `core_explore` (1..4, wächst beim Teilen, asm:3984ff.) |
| +3 | Byte | Waffen/Stärke-Stufe, aus Tabelle `weapons_add` (`A4 0x2fcc`), Kampfmultiplikator |
| +4 | Word | Population (Lebenspunkte); ≤ 0 → `core_remove` |
| +6 | Word | Mehrfachnutzung: Kampfpartner-Slot (Flag 8), Gründungs-Zug (Stadt), Warte-Zähler (0x20/0x40), Ruinen-Timer (0x80) |
| +8 | Word | Position (Zelle 0..4095 = y*64+x) |
| +10 | Word | Letzter Schritt (Delta), für „Zelle verlassen“ |
| +12 | Word | Animations-Frame bzw. Stadtstufe: 32..41 Siedlung, 42 Burg; 70/130/134/138 Kampf; 85..87 Sieg; 101..102 Warten |
| +14..17 | Long | „Heading“/Ziel-Referenz (Ritter). Original: Amiga-Zeiger auf Peep; Port: Slot+1 (`peep_reference`) |
| +18 | Word | Geburtszelle eines abgespaltenen Kindes |
| +20 | Byte | Ritter: Rest-Kills (`core_knight` setzt 1, Sieg dekrementiert); bei Städten Rückgabe von `core_make_level` (Original-Kuriosum, siehe Kommentar bei `MAKE_LEVEL_EXIT_BYTE_OWNER0`) |
| +21 | Byte | Letzte Ausweichrichtung des Magnet-Pfadfinders |

### 2.2 Flags / Zustände

| Wert (+0) | Bedeutung | Übergänge / Auslöser | C / asm |
|---|---|---|---|
| `0x02` | **Läufer** | → `0x01` Siedeln (Richtung 0, keine Heading); → `0x08` Kampf (betritt Zelle eines Gegners); Merge mit eigener Figur (`core_join_forces`); → `0x12` Wasser; → `0x40` blockiert; stirbt durch Laufverlust `walk_death` (`A4 0x2fb0`) pro Schritt oder Sumpf (Block 53) | `core_move`, `_move_peeps` asm:4040-4122 |
| `0x01` | **Siedlung/Stadt** | jede Runde `core_settle_tick`; → `0x02` wenn kein Platz mehr (`core_check_life ≤ 0`), Heading gesetzt oder Krieg (`0xae80`); erzeugt Kinder-Läufer; → `0x11` Wasser | `core_settle_tick` asm:3600-4055 |
| `0x08` (+Bit 0/1) | **Kampf** (Partner in +6) | gegenseitig gesetzt beim Zusammentreffen; Auflösung in `core_battle` → Sieger `0x05`/`0x06`, Verlierer entfernt bzw. Ruine `0x80` | `core_move` asm:4510-4559, `_set_battle` asm:6604-6656, `_do_battle` asm:6082-6158 |
| `0x05` / `0x06` | **Siegesfeier** (Stadt/Läufer + 0x04), Frames 85..87 | nach Ablauf Bit 0x04 gelöscht → wieder `0x01`/`0x02` | `core_frame_walker`, `_move_peeps` asm:3584-3599, 5833-5845 |
| `0x10`-Kombinationen (`0x11`,`0x12`,`0x15`,`0x16`,`0x32`,`0x52`,`0x72`,`0x90`) | **Im Wasser / Ertrinken** | Bei `map_blk==0`: Stadt wird geräumt, Flag `0x12`. Solange nass: Verlust `2*walk_death` pro Zug, oder sofortiger Tod bei `GAME_MODE_WATER_FATAL`. Land wieder trocken → Bit 0x10 gelöscht. Zusätzlich bittet die Figur ihren Besitzer per Mailbox um Landhebung (Opcode 1) | `_move_peeps` asm:3481-3580, `core_queue_terrain_raise` |
| `0x20` | **Warten/Kollision** (Frame 101) | gesetzt von `core_collision_wait` auf die Figur, die die Zelle belegt; Verlust pro Zug, nach >14 Zügen Bit gelöscht und neuer Bewegungsversuch | asm:4058-4067, 4145-4184, 4213-4268 |
| `0x40` | **Blockiert** (kein Weg, `delta==999`) | Zähler +6 auf 7 → nach 8 Zügen (Zähler >14) erneut `core_move` | `core_move` asm:4425-4437 |
| `0x80` | **Ruine/Rest** einer vom Ritter zerstörten Stadt | Pop 1, Timer 40 in +6, zählt runter → `core_remove` | `core_battle_over` asm:6207-6564; `_move_peeps` asm:4283-4315 |
| `0x00` | frei/tot | `core_remove` setzt Flags 0 und Pop 0 | `core_remove` (`_zero_population` asm:5671-5716) |

Zusatzzustände, die **keine eigenen Flag-Werte** sind:
- **Anführer (Leader)**: `_magnet`-Wort `core_leader(owner)` = `A4 0x99f6+owner*16` (Slot+1). Ein
  Läufer wird Anführer, wenn es noch keinen gibt und er das Magnetziel `core_target(owner)`
  (`A4 0x99f8+owner*16`) erreicht (`core_magnet_direction`). Beim Tod des Anführers wird das Magnetziel
  auf seinen Sterbeort gesetzt. Beim Teilen einer Stadt geht die Anführerschaft an das Kind.
- **Ritter**: Anführer mit Heading ≠ 0 (+14) und +20 = 1 (`core_knight`). `core_get_heading` wählt die
  nächste gegnerische Figur (Manhattan-Distanz). Zu Beginn jedes `_move_peeps` wird ein Anführer mit
  Heading „freigegeben“ (`core_leader=0`, asm:3357-3424) – der Ritter ist damit kein Anführer mehr.
  Ritter laufen immer per `core_magnet_direction` auf ihr Heading-Ziel. Ein Ritter-Sieg über eine Stadt
  macht sie zur Ruine (Blöcke +21, Flag `0x80`).
- **Magnet-Folgen**: Tendenz 0 („zum päpstlichen Magneten“), Krieg (`0xae80`) oder vorhandenes Heading →
  `core_magnet_direction` statt `core_explore` (`core_move`).
- **Vereinigen**: `core_join_forces` (eigene Figur, kein Kampf; asm:5540-5640) und
  `core_join_battle` (Läufer stößt zu laufendem Kampf und verstärkt den eigenen Kämpfer, asm:6663-6765).
  Population wird addiert (max. 32000), Waffenstufe = Maximum.

### 2.3 Bewegung und Wegwahl
- `core_move` (asm:4425-4617): Richtung aus `core_magnet_direction` (Magnet/Heading) oder
  `core_explore` (`_where_do_i_go`, asm:4622-4901). `core_explore` testet 9 Strahlen bis zur Reichweite (Byte +2)
  und bewertet: [0] baubarer flacher Platz (Block 15), [1] Kampf, [2] Gegner, [3] eigener Läufer,
  [4] am wenigsten begangene Zelle (`map_steps`). Tendenz 3 bevorzugt Gegner, Tendenz 2 eigene Läufer
  (sammeln), sonst Priorität 0..4.
- `core_valid_move` (asm:20912-20954): 1 = außerhalb, 2 = Fels (Block 47), 3 = Wasser (Block 0), 0 = ok.
- Steht ein Läufer der Computerseite (`_stats+6 == 1`) auf Block `0x42` (Ruinen-/Schutt), legt er
  Opcode 2 (Absenken) in die Mailbox (asm:4450-4509).
- Vor jedem Schritt: `core_one_block_flat` (asm:9148-9310) legt evtl. einen Raise/Lower-Befehl ab, um die
  eigene Zelle zu ebnen (nur bei Mana ≥ 20, <51 Nicht-Burg-Städten, Machtbit 0x01, Latch frei).

### 2.4 Kampf
`core_battle` (asm:6082-6158): beide würfeln `(rand%3+1)*Population`; der kleinere Wert `lesser` gilt.
Jede Seite verliert `lesser/100 * Waffen_des_Gegners + 10`. Beide ≤ 0 → beide entfernt. Einer ≤ 0 →
`core_battle_over`: Mana-Belohnung aus `battle_add2` (`A4 0x3010`, Ritter `0x3012`, Anführer `0x3014`)
bzw. für Städte `battle_add1[stufe]` (`0x2ffa`); wird dem Sieger gut- und dem Verlierer abgeschrieben
(nicht unter `mana_values[0]=-250`). Erobert ein normaler Läufer eine Stadt und ist dort Platz, wird er
selbst zur Stadt (`0x01`). Siegzähler `A4 0xae7a+owner*2`.

### 2.5 Beispielsequenzen
- **Siedeln**: Läufer `0x02` → `core_explore` findet flachen Platz → Schritt dorthin → Richtung 0 →
  `peeps[+0]=1`, `+6=game_turn`, `core_frame_town`, `core_set_town` (Gebäude auf `map_bk2`).
- **Kampf bis zum Tod**: Läufer A (Spieler) läuft auf die Zelle von B (Computer) → beide `0x08`, Partner
  gekreuzt → in den Folgezügen `core_battle` (nur vom kleineren Slot ausgelöst, `other > person`) →
  A fällt auf 0 → `core_battle_over(B,A)` → B wird `0x06` (Feier, Frame 85) → A `core_remove` → B wieder
  `0x02`.
- **Ertrinken**: Flut senkt Land (Opcode 14/y=4) → Zelle wird Block 0 → Stadt geräumt, Flag `0x12` →
  Mailbox-Opcode 1 „hebe Land unter mir“ → pro Zug `2*walk_death` Verlust → Tod, falls kein Land kommt.
- **Sumpf**: Läufer beendet einen Schritt-Frame auf Block 53 → `core_remove`, Effekt 66; ohne
  `GAME_MODE_SWAMP_BOTTOMLESS` wird der Sumpf wieder Block 15.

---

## 3. Städte, Wachstum, Population, Mana

- **Bauplatz** `core_check_life` (asm:20958-21042): prüft 17 Nachbarzellen; jede eigene/flache Zelle
  (Block 15 oder 31+owner) gibt +15 (Start 50); Fels −15; Nachbarsiedlungen verhindern Bau. < 35 → 0.
  Volle 305 → 3050 (Burg). `core_frame_town`: Stufe = 32 + capacity*10/305, Burg = 42.
- **`core_set_town`** (asm:5865-6062) zeichnet Gebäude (`map_bk2`, Burg 3×3 aus `core_city`) und färbt Land
  (`map_blk` 31+owner).
- **`core_settle_tick`** pro Zug: neu bewerten; Statistiken für die KI sammeln (beste/älteste/jüngste Stadt
  → `_stats+0x22/+0x26/+0x2a`); ggf. `core_make_level` (asm:9017-9147, Gelände um die Stadt ebnen, 9×9
  `core_a_flat`). **Alle 8 Züge** (`game_turn&7==0`, asm:3818-4014):
  - Mana += `mana_add[stufe]` (`A4 0x2fb6`),
  - Waffen = `weapons_add[stufe]` (`0x2fcc`),
  - Population > Kapazität → Kind-Läufer (Population − Kapazität/2), Stadt behält Kapazität/2,
    die Reichweite (Byte +2) der Stadt steigt bis 4; Anführerschaft geht aufs Kind,
  - Population += `population_add[stufe]` (`0x2fe2`).
  Die Tabellen stammen aus der LAND-Datei des Terrains (`core_install_land`).
- **Mana** pro Seite: `A4 0x9a02+owner*16` (Long). `core_add_mana` begrenzt auf −250..1999999.
  Kostentabelle `mana_values` (`A4 0x8486`, `core_mana_values`):
  `[0]-250 [1]10 [2]200 [3]2500 [4]5000 [5]7500 [6]10000 [7]40000 [8]80000`.
  Die Anzeige (`_move_mana`, asm:3164-3244) ist rein grafisch.
- **good_pop** (`A4 0x99fe+owner*16`): Summe der Population vor der Bewegung, pro `_move_peeps` neu
  berechnet; 0 → Niederlage dieser Seite.

---

## 4. Mailbox-Befehle (`_stats`, `A4 0x82b6 + owner*46`)

Jede Seite hat einen 46-Byte-Datensatz. Byte +0 = Opcode, +1 = x, +2 = y. Produzenten: Maus/Toolbar
(`core_toolbar`, `game_handle_event`), Figuren (`core_queue_terrain_raise`, `core_move`,
`core_one_block_flat`, `core_make_level`) und die KI. Konsument: `core_consume_terrain_mailboxes`
(`_get_message` asm:17929-17995, Sprungtabelle asm:18200-18216), danach Löschen von Byte 0..2
(`_clear_send` asm:18234-18254). Es wird **ein** Befehl pro Seite und Zug ausgeführt; späterer
Schreiber überschreibt.

| Opcode | Wirkung | Kosten / Gates | Wirkung auf Figuren |
|---|---|---|---|
| 1 | Punkt (x,y) **anheben** (`core_raw_raise`, `original_raise_point`) | ≥ 10 Mana; Kosten 10+4×geänderte Punkte; im Krieg gratis | rettet Ertrinkende |
| 2 | Punkt **absenken** (`core_do_lower_point`, asm:7091-7180) | wie 1, Krieg umgeht das Mana-Gate nicht | kann Land zu Wasser machen → Ertrinken |
| 3 | **Erdbeben** bei (x,y) (`core_quake`) | `mana_values[3]`=2500, Machtbit 0x08 | indirekt (Wasser/Stadtzerstörung) |
| 4 | **Sumpf** um (x,y) (`core_swamp`) | 5000, Bit 0x10 | tötet Läufer, die drauf treten |
| 5 | **Papst-Magnet** nach (x,y) (`core_do_magnet`, asm:6769-6820) | 200, braucht Anführer, nicht im Krieg | lenkt Anführer/Magnet-Folger |
| 6 | **Vulkan** bei (x,y) (`core_volcano`) | 10000, Bit 0x40 | Felsen, Städte weg |
| 7/8 | Figur für Seite 0/1 setzen (Editor, `core_place_people`) | – | erzeugt Figur (Pop 45) |
| 9/10 | Anführer für Seite 0/1 setzen | – | ersetzt Anführer |
| 11/12/13 | Editor: Baum/Fels-Zyklus, Löschen (`core_paint_cycle/_delete`) | – | – |
| 14, y=1 | **Tendenz** setzen: x=0 zum Magneten, 1 siedeln, 2 sammeln, 3 kämpfen | – | ändert Wegwahl aller eigenen Läufer |
| 14, y=2 | im Original Nachricht (`_get_a_message`, asm ~18406ff.) **[nicht portiert / unsicher]** | | |
| 14, y=3 | **Armageddon** (`core_war`) | 80000, Bit 0x100 | alle Headings gelöscht, `0xae80=1`: alle laufen zur Kartenmitte 0x820 und kämpfen |
| 14, y=4 | **Flut** (`core_flood`) | 40000, Bit 0x80 | alle Höhen −1 → Ertrinken |
| 14, y=5 | **Ritter** (`core_knight`) | 7500, Bit 0x20, braucht Anführer | Anführer wird Ritter |
| 14, y=6 | Pause – im Port direkt in `core_toolbar` umgesetzt, nicht über die Mailbox | | |
| 14, y=7 / y=8 | Options- bzw. Spieloptionen-Requester (nur für `player`) | – | – |
| 14, y=9/10 | Editor: Mana Seite 0/1 halbieren/verdoppeln (`core_paint_mana`) | | |
| 14, y=11/12/13 | Editor: Karte spiegeln / leeren / Terrain laden | | |

Weitere `_stats`-Felder: +6 „Mailbox per Takt gesteuert“ (=1 für die Computerseite, im Einzelspiel
`0x82bc`=0, `0x82bc+46`=1), **+8 Latch** (Befehl anhängig, KI wartet), +$c Rating, +$e Machtmaske,
+$10 Reaktionsgeschwindigkeit, +$12 Zähler Beben/Sümpfe, +$14 Burgen, +$16 Städte, +$18/+$1a
Zufallsgrenzen (`original_clear`: +$1a = rand%3, +$18 = +$1a + rand%5 + 1), +$1c/+$1e
Magnet-Haltezähler/-zelle, +$22 beste Stadt des Gegners, +$26 älteste Stadt bzw. erster Läufer des
Gegners, +$2a eigene jüngste Stadt (Referenzen als Slot+1).

**Latch-Freigabe / Reaktionsgeschwindigkeit**: Für Seiten mit `+6 == 1` setzt der Konsument den Latch
nur dann zurück, wenn `game_turn % speed == 0` (`speed = _stats+$10`, `0x82c6`). Speed 1 = jeder Zug,
Speed 10 = höchstens alle 10 Züge ein neuer KI-Befehl. Im Einzelspiel: Spieler speed 1, Computer
`selected_speed` bzw. `conq_01_speed` aus dem Level (1 = sehr schnell … 10 = sehr langsam).

---

## 5. Computer-KI

Aufruf in `_move_peeps` (asm:3254-3270), für **beide** Seiten, jeweils nur bei freiem Latch (+8):
`core_set_devil_magnet(owner)`, dann (Latch erneut geprüft) `core_devil_effect(owner)`, das
`core_do_computer_effect` für zielgerichtete Mächte nutzt. Die Entscheidungen basieren auf den Zeigern des
**vorigen** Durchlaufs (+$22/+$26/+$2a werden danach neu gesammelt). In der Praxis wirkt die KI nur auf der
Computerseite, weil beim Spieler der Latch nicht per Takt freigegeben wird (+6=0) – **[unsicher]** für
Zwei-Spieler-Varianten, die serielle Kopplung ist **[nicht portiert]**.

Machtmaske +$e = `(level_powers << 3) | 7` (`original_generate`): Bits 0x01 Gelände formen, 0x02 Magnet
auf Gegnerstadt, 0x04 Magnet auf Gegner-Magnet, 0x08 Beben, 0x10 Sumpf, 0x20 Ritter, 0x40 Vulkan, 0x80 Flut,
0x100 Armageddon. (Hinweis: die Makros `POWER_*` im C-Code nummerieren die Level-Bits ab Bit 0, nicht die
geschobene Maske.)

### 5.1 `core_set_devil_magnet` (asm:9601-9800) – Magnet und Tendenz
1. Ruhephase: `Städte+Burgen < rating*2+15` **oder** `game_turn % 90 < rating+10` → nur falls Tendenz 0:
   Opcode 14/y=1 mit zufälliger Tendenz 1..3. Ende.
2. Kein eigener Anführer → Tendenz auf 0 zurücksetzen (zum Magneten), +$1c=0.
3. Anführer < 6000 Pop: Magnet auf gemerkte Zelle (+$1e, solange +$1c) bzw. auf die eigene jüngste Stadt
   (+$2a) → Opcode 5, +$1c=2 – sonst Tendenz 0.
4. Anführer > gegnerischer Anführer + 500 und Gegnertendenz 0, Bit 0x04 → Magnet auf das Magnetziel des
   Gegners (Angriff).
5. Sonst Bit 0x02 → Magnet auf die beste Gegnerstadt (+$22).

### 5.2 `core_devil_effect` (asm:9300-9525) – Mächte, in dieser Priorität

| Prüfung | Bedingung (Mana strikt größer) | Befehl |
|---|---|---|
| Armageddon | Bit 0x100, Mana > 80000+999, eigene good_pop > gegnerische | 14/y=3 |
| Flut | Bit 0x80, Mana > 40000+1999 | 14/y=4 |
| Ritter | Bit 0x20, Anführer-Pop > 3000 (Abbruch auch ohne Mana), Mana > 7500+500 | 14/y=5 |
| (ohne Ziel +$26 Ende) | | |
| Vulkan | Bit 0x40, Mana > 10000+500 | 6 auf Ziel, +$12=0 |
| Sumpf | Bit 0x10, Mana > 5000+500, Zähler-Grenzen +$18/+$1a, gegnerischer Anführer stärker als eigener | 4 auf Gegner-Anführer (ohne Latch) |
| Erdbeben | Bit 0x08, Mana > 2500+500, Ziel ist Stadt (Flag 1) | 3 auf Ziel, +$12++ |

`core_do_computer_effect` (asm:9526-9600): ohne eigenen Anführer und mit Gegner auf der eigenen
Magnetzelle → dort; sonst Zielpunkt = (+$26-Figur) − 3 in x und y (Mitte des 9×9-Effektbereichs),
auf ≥ 0 begrenzt.

### 5.3 Beispiel einer KI-Entscheidung
Annahmen: Computer (owner 1), rating 5, speed 3, Maske `(0x0f<<3)|7 = 0x7f`, 12 Städte, 2 Burgen,
`game_turn = 1234`, Mana 3100, Anführer mit 1200 Pop, älteste Spielerstadt in +$26.
1. Zug 1234: Latch frei? `1234 % 3 = 1` ≠ 0 – der Latch wurde nur in einem früheren Zug mit `turn%3==0`
   gelöst; angenommen frei.
2. `core_set_devil_magnet`: Städte 14 ≥ 5*2+15=25? Nein → Ruhephase; Tendenz ≠ 0 → nichts.
3. `core_devil_effect`: kein Armageddon/Flut-Bit; Ritter: Anführer 1200 ≤ 3000 → weiter; Vulkan: 3100 >
   10500? nein; Sumpf: 3100 > 5500? nein; Beben: 3100 > 3000 und Ziel ist Stadt, Bit 0x08 → Opcode 3,
   x/y = Stadtposition −3, Latch=1, +$12=1.
4. Nach `_move_peeps` führt `core_consume_terrain_mailboxes` das Beben aus (Mana −2500).
5. Neue KI-Aktion frühestens im nächsten Zug mit `game_turn % 3 == 0` (1236).

---

## 6. Mächte und ihre Wirkung

| Macht | C-Funktion / asm | Gelände | Menschen |
|---|---|---|---|
| Heben/Senken | `core_raw_raise`/`original_raise_point`, `core_do_lower_point`/`original_lower_point` (asm:2571-2713, 7091-7180) | rekursiv, Nachbarn max. 1 Höhenstufe Differenz; `core_make_map_region` | flaches Land → neue Bauplätze; Wasser → Ertrinken |
| Erdbeben | `core_quake` asm:7863-8042 | 9×9 Punkte, 2 Durchläufe, je rand%5: 1 heben, ≥2 senken; Bildschirmwackeln | Städte verlieren Kapazität, Wasser entsteht |
| Sumpf | `core_swamp` asm:8295-8387 | 30 Versuche ±3, flache leere Zellen (15/31/32/66) → Block 53 | Läufer sterben beim Betreten |
| Ritter | `core_knight` asm:8392-8478 | – | Anführer bekommt Heading, Magnet an seine Stelle, Anführer frei |
| Vulkan | `core_volcano` asm:8046-8267 | 5 konzentrische Hebe-Durchläufe, 1/5 der 8×8-Zellen Fels (47), Gebäude weg (außer Anführer-/Magnetzellen) | Städte zerstört |
| Flut | `core_flood` asm:7567-7648 | alle Höhen > 0 um 1 senken, `_make_map` | Ertrinken folgt im nächsten Zug |
| Armageddon | `core_war` asm:8482-8530 | – | Headings gelöscht, `0xae80=1`; Magnete 0x820; Städte räumen (`core_settle_tick`), alle laufen zur Mitte |
| Magnet | `core_do_magnet` asm:6769-6820 | – | Ziel für Anführer/Tendenz-0 |

Punkte für den Spieler (`A4 0xae72`): Beben +25, Sumpf +50, Vulkan +100, Ritter +150, Flut +250,
Armageddon +5000. Im Paint-Map-Modus entfallen Mana-/Pause-/Machtprüfungen und Kosten.

---

## 7. Weitere Zustandsautomaten

### 7.1 Spielmodus / Werkzeuge
- `game_mode` (`A4 0x84b8`, Bits): 0 Wasser tödlich, 1 Sumpf bodenlos, 2 kein Bauen, 3 nur aufwärts,
  4 nur nahe Städten. Aus Level-Byte 4.
- `mode` (`A4 0x8256`): wird in `original_clear` auf 2 gesetzt; die genaue Bedeutung ist im Port nicht
  weiter ausgewertet **[unsicher]**.
- Werkzeug `core_tool` (0 Gelände, 1 Magnet, 2 Abfragen) + `core_swamp_selected`; `pointer` (`0x84b6`)
  wählt das Zeigersprite (`core_set_pointer`).
- `pause`, `paint_map` (Editor), `0xae80` (Krieg) wirken als globale Gates.

### 7.2 Requester (`core_req.kind`)
`CORE_REQ_NONE → OPTIONS | GAME | SET | WORLD | FILE`. Im Original blockierende Schleifen in
`_do_action`; im Port ein Zustand, der `game_tick` anhält und Klicks umleitet
(`core_requester_click`). `SET` läuft innerhalb von `GAME` und kehrt dorthin zurück (`saved_mode`).
Kapitulation (`case 16` in `core_game_options_click`) setzt `_surender = player`.

### 7.3 Weltauswahl (`core_world_*`, `_show_world` asm:12262-12378)
Ursprung `CORE_WORLD_START / OPTIONS / WON`. Eingabe (`core_world_entry`) → `core_world_evaluate`:
Zahl → Landschafts-Seed (Custom-Spiel); Name → `core_decode` (Silben-Code, „GENESIS“=0; Fehler →
„NO SUCH WORLD“, zurück zur Eingabe) → `core_world_read` (level.dat) → Anzeige → `core_world_return`
startet das Level (`game_reset_start(START_ORIGINAL, level/5)`).

### 7.4 Spielende
`game_tick` setzt `core_result` → `_animate` ruft `core_enter_end` (Statistik + Score `core_end_stats`,
asm:14022-14316; bei Sieg ×10, Grenzen 500/515090). Klick → `core_leave_end`: gewonnene Eroberung →
`core_next_level` (Level + score/5000 + 1, auf Vielfache von 5 gerundet, max 2470, danach „fertig“) →
Lord-Bildschirm (`core_lord_open`) bzw. Weltauswahl `CORE_WORLD_WON`; verlorene Eroberung → gleiche Welt;
Custom → nächste Landschaft aus `seed`.

### 7.5 Nicht portiert / Einschränkungen
- `_do_funny`, `_do_place_funny`, `_sculpt`, `_zoom_map` sind leere Stubs.
- Serielle Zwei-Spieler-Kopplung (Mailbox-Austausch, Opcode 14/y=2 Nachricht) fehlt.
- Heading-Referenzen sind Slot+1 statt Amiga-Zeiger; ungültige Zeiger, die das Original
  dereferenzieren würde, werden durch Host-Guards übersprungen (`core_ai_slot`, `core_ai_town_cell`).
- Gehaltene Maustasten verkürzen `view_timer` im Port nicht.
