# Vorhaben: Doku, Memory und Code beisammenhalten

> **Übergabe für eine neue Session — Stand 2026-09-18.**
> Teil 1 und 2 sind erledigt: Prüfungen für die Tabellen des Repos und ein
> pre-commit-Hook. Offen sind **Teil 3** (Memory verschlanken) und **Teil 4**
> (Abschluss-Skill). Beides ist mit dem Owner abgesprochen, die Einzelheiten
> entscheidet er je Punkt in der neuen Session.
> **Ergebnis: offen.**

## Worum es geht

Dieses Projekt wird vollständig von Claude bearbeitet. Zwischen zwei Sessions
bleibt nur, was im Repo oder im Memory steht — was keine Prüfung erzwingt,
hängt am Daran-Denken. Das ging in Kleinigkeiten immer wieder schief. Die
Durchsicht vom 2026-09-18 hat drei Ursachen gefunden:

1. **Derselbe Sachverhalt steht an mehreren Stellen** — in `src/version.h`,
   README, einem Vorhaben, einer Zuordnungstabelle und im Memory. Jede Kopie
   ist eine Stelle, die man vergessen kann.
2. **Geprüft wurde nur ein Teil**, und ein Teil davon nur auf Zuruf.
3. **Doku-Commits liefen ohne jede Prüfung** direkt auf `main`; aufgefallen
   wäre ein Fehler frühestens in der CI nach dem Push.

## Erledigt am 2026-09-18 (Teil 1 und 2)

Merges `a48e6a6`, `1095408` und `88a8044` auf `main`.

* [`test/doku_zuordnung_test.py`](test/doku_zuordnung_test.py) —
  `Byte-Zuordnung.md` gegen den Code, dazu `set_top_zuordnung.py --pruefen`.
* `set_top_zuordnung.py --pruefen` prüft jetzt auch die Topic-Listen in
  Abschnitt 3 von `SET-TOP-Zuordnung.md` und die Zahlen im Text. Anlass: Die
  sieben Installer-Topics aus 3.19.0 fehlten dort drei Wochen unbemerkt.
* [`test/repo_konsistenz_test.py`](test/repo_konsistenz_test.py) — README
  „Aufbau“, die Werkzeugtabelle in `test/README.md`, `MQTT-Topics.md` gegen den
  Code, die Pfade in `CLAUDE.md` und jeder relative Link. Fand beim Einführen
  14 Lücken, alle berichtigt.
* `test/hosttests.sh --schnell` fährt nur die Python-Prüfungen (< 1 s); die
  Vollständigkeitsprüfung erfasst jetzt auch `*_test.py`.
* [`.githooks/pre-commit`](.githooks/pre-commit) ruft `--schnell` vor jedem
  Commit. **Einmal je Klon aktivieren:** `git config core.hooksPath .githooks`
  — im Arbeitsverzeichnis des Owners ist das seit 2026-09-18 gesetzt.

**Was diese Prüfungen nicht abdecken:** Bedeutungstexte (Befunde,
Begründungen, Fußnoten), das Memory (liegt außerhalb des Repos) und ob der
Kopf eines Vorhabens das aktuelle Ergebnis nennt. Darum geht es in Teil 3
und 4.

## Teil 3 — Memory verschlanken

### Lage (2026-09-18)

* 43 Memory-Dateien, zusammen rund 25 000 Wörter. `MEMORY.md` hat 44 Zeilen und
  wird in jeder Session geladen.
* **`projektstand-heishamon` allein hat rund 10 500 Wörter**, darin acht
  übereinandergestapelte Absätze „AKTUELLER STAND“ von 3.11.0 bis 3.22.0. Das ist
  ein zweiter Changelog neben `src/version.h` — genau die Doppelung aus Ursache 1.
* Acht Memories nennen sich selbst „erledigt“ oder „abgeschlossen“ — Kandidaten
  für ein Archiv.
* Der **zweistufige Index** (`INDEX_*.md` je Thema, `ARCHIV.md` für Erledigtes,
  in `MEMORY.md` nur Grundregeln und Verzeichnis) ist in der globalen
  Arbeitsanweisung des Owners beschrieben und wird von seinem `memory-lint` (V2)
  unterstützt — dieses Projekt nutzt ihn noch nicht.
* Eine Memory verweist auf zwei Logdateien unter `test/`, die es nicht mehr gibt.

### Ziel

* **Im Memory steht nur noch, wie Claude arbeitet** — Messkniffe, Kalibrierung,
  Entscheidungsgewohnheiten des Owners. **Fakten stehen im Repo.** So zieht
  die globale Arbeitsanweisung des Owners die Grenze, sinngemäß: Was ein Mensch
  bei der Übergabe braucht, gehört ins Repo; ins Memory kommt nur, was steuert,
  wie Claude arbeitet — kein Fakt an beiden Stellen.
* **Der Stand der Boards gehört ins Repo**: welche Version auf welchem der vier
  Boards läuft, welcher Rückfall bereitliegt. Das braucht auch die Familie bei
  einer Übergabe. Naheliegender Ort ist `Ablauf-Backup-Boards.md` (dort stehen
  die Adressen der Boards bereits); der Owner entscheidet.
* `MEMORY.md` enthält nur noch Grundregeln, die jede Session braucht, und das
  Verzeichnis der Themen-Indizes.

### Vorgehen

1. **Bestandsaufnahme vorlegen:** eine Tabelle aller Memories — Name, Typ,
   Umfang, Vorschlag (behalten / kürzen / ins Repo / ins Archiv / löschen) mit
   Begründung. **Dem Owner vorlegen und je Memory entscheiden lassen**, nicht als
   Paket. Ein Vorschlag ist kein Entscheid und wird auch nicht so notiert.
2. **`projektstand-heishamon` zuerst:** auf den aktuellen Stand und die offenen
   Punkte kürzen. Alles Geschichtliche steht in `src/version.h`, in den
   Vorhaben-Dateien oder im Changelog der Releases; vor dem Löschen eines Absatzes
   stichprobenartig nachsehen, dass er dort wirklich steht. Was nirgends steht und
   ein Mensch braucht, kommt ins Repo.
3. **Themen-Indizes anlegen** und die Zeiger aus `MEMORY.md` dorthin umziehen;
   Erledigtes nach `ARCHIV.md`. Die Regeln setzt `memory-lint` durch — er blockiert
   bei Schemafehlern, also Zeile für Zeile umziehen, nicht alles auf einmal.
4. **Tote Verweise bereinigen** (die zwei Logdateien).
5. **Vorschlag an den Owner, nicht umsetzen:** `memory-lint` könnte zusätzlich
   melden, wenn eine Memory auf eine Datei im Repo zeigt, die es nicht gibt. Das
   Werkzeug gehört dem Owner und läuft in allen seinen Projekten.

### Vorsicht

* **Das Repo ist öffentlich, das Memory nicht.** Manche Memories tragen den
  ausdrücklichen Vermerk, ihren Inhalt nicht öffentlich zu dokumentieren. Vor
  jedem Umzug ins Repo je Punkt prüfen, ob er öffentlich sein darf; im Zweifel
  den Owner fragen.
* `doku-intern/` gilt weiter: nachschlagen ja, zitieren nie — auch nicht beim
  Umformulieren einer Memory.
* Passwörter und Ports gehören weiterhin nur in `platformio_user_env.ini`.

### Abnahme

* `projektstand-heishamon` enthält nur noch den aktuellen Stand und offene Punkte
  (Richtwert: unter 1 000 Wörtern).
* Stichprobe: Zu fünf gelöschten Fakten findet sich die Stelle im Repo.
* `memory-lint` grün, `MEMORY.md` nur Grundregeln und Verzeichnis.

## Teil 4 — Abschluss-Skill `/abschluss`

### Zweck

Ein fester Handgriff am Ende jeder Session statt „daran denken“. Er lohnt erst
nach Teil 3 — vorher prüft er gegen ein Memory, das selbst nicht stimmt.

### Inhalt (Vorschlag, mit dem Owner abstimmen)

1. **Prüfen:** `./test/hosttests.sh --schnell`, bei Änderungen in `src/` der volle
   Lauf. `git status`, ungepushte Commits, offene Branches melden.
2. **Memory abgleichen:** Welche Memories berühren die Änderungen dieser Session
   (Suchbegriffe aus dem Diff)? Stimmen deren Angaben noch? Neue Erkenntnis: ins
   Repo (braucht ein Mensch) oder ins Memory (steuert Claude)?
3. **Vorhaben-Köpfe:** Nennt jede heute bearbeitete Datei `Vorhaben-*`,
   `Auftrag-*`, `Arbeitsplan-*` oben das aktuelle Ergebnis?
4. **Abschlussmeldung** nach der globalen Regel des Owners: was geändert wurde,
   warum, und was offen ist — drei bis fünf Sätze.

### Form

`.claude/skills/abschluss/SKILL.md`, aufgebaut wie `/pruefen-vor-merge`: nur
lesen, prüfen und melden; pushen nur auf Auftrag. Eintrag in `CLAUDE.md` unter
„Wo was steht“.

### Abnahme

Einmal am Ende der Session laufen lassen, die Teil 3 abschließt; dabei gefundene
Abweichungen berichtigen.

## Nebenbefund: das Ergebnis im Kopf der Vorhaben

`CLAUDE.md` sagt über die Vorhaben- und Auftragsdateien: „Ergebnis steht jeweils
im Kopf“. Zwei weichen ab:

* [`Arbeitsplan-KNX-Vorderhaus.md`](Arbeitsplan-KNX-Vorderhaus.md) führt oben noch
  „Ergebnis: offen“; das tatsächliche Ergebnis („ausgerollt“) steht drei Absätze
  tiefer.
* [`Vorhaben-Nur-ESP32-Pfad.md`](Vorhaben-Nur-ESP32-Pfad.md) nennt im Kopf nur den
  Planungsstand vom 2026-08-25, obwohl 3.16.0 umgesetzt und ausgerollt ist.

**Vorschlag für den Owner:** eine feste Zeile `**Ergebnis:** …` direkt unter der
Überschrift jeder solchen Datei. Dann kann `repo_konsistenz_test.py` prüfen, dass
sie vorhanden ist — ob sie stimmt, prüft der Abschluss-Skill (Teil 4, Schritt 3).

## Regeln für die Session

* Anrede in Du-Form, Deutsch. Optionen nummeriert und mit einer markierten
  Empfehlung vorlegen, Punkte **einzeln** entscheiden lassen.
* Die harten Regeln aus [`CLAUDE.md`](CLAUDE.md) gelten. Doku-Änderungen gehen
  direkt auf `main` (der Hook prüft sie); Änderungen an Tests und Skripten auf
  einem Branch mit Rettungsanker, wie am 2026-09-18.
* Kein Zugriff auf die Anlage nötig.
* Pushen nur auf Auftrag, und ohne `--follow-tags` — die Rettungsanker bleiben
  lokal.
* Zum Schluss den Kopf dieser Datei auf das Ergebnis setzen.
