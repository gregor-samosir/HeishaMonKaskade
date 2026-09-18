---
name: abschluss
description: Fester Handgriff am Ende einer Session in der HeishaMon-Kaskade — prüft Hosttests und Git-Stand, gleicht die Memory gegen die Änderungen dieser Session ab, prüft die Köpfe bearbeiteter Vorhaben-Dateien und schließt mit der Abschlussmeldung. Nur lesen und prüfen, kein Gerätezugriff, pushen nur auf Auftrag. Verwenden am Ende einer Session, die Code, Doku oder Memory in diesem Repo verändert hat.
---

# Abschluss einer Session

Ersetzt „am Ende dran denken" durch einen festen Handgriff. Läuft **nach**
inhaltlicher Arbeit, **vor** der Abschlussmeldung an den Owner — ändert
nichts von selbst, sondern prüft und meldet; Berichtigungen macht Claude nur
als eigene, genannte Änderung, nie stillschweigend im Hintergrund.

Voraussetzung: Teil 3 aus `Vorhaben-Doku-Konsistenz.md` ist erledigt (Memory
zweistufig, `MEMORY.md` stimmt). Vorher prüft Schritt 2 gegen Angaben, die
selbst schon falsch sein können.

## 1. Hosttests und Git-Stand

```bash
git status --short
git branch --show-current
git log origin/main..HEAD --oneline    # ungepusht auf diesem Branch
git branch --list | grep -v -E '^\*|\bmain\b'   # andere offene Branches
```

```bash
./test/hosttests.sh --schnell
```

Hat diese Session `src/` verändert (uncommittete Reste oder Commits
gegenüber dem Rettungsanker-Tag des Branches betreffen `src/`), zusätzlich
den vollen Lauf:

```bash
./test/hosttests.sh
```

ROT, uncommittete Reste, ungepushte Commits oder offene Fremd-Branches:
**melden**, nicht stillschweigend übergehen.

## 2. Memory abgleichen

Suchbegriffe aus der Session bilden (Dateinamen, Topics, Befunde, die diese
Session angefasst hat). Dann:

1. `MEMORY.md` lesen — Grundregeln und das Verzeichnis der Themen-Indizes.
2. Die zum Thema passenden `INDEX_*.md` öffnen (der Dateiname beschreibt den
   Bereich) und prüfen, ob eine dort verlinkte Memory jetzt eine überholte
   Angabe trägt (Versionsnummer, Pfad, Zustand, offener Punkt, der sich
   erledigt hat).
3. Bei `projektstand-heishamon.md` speziell: **überschreiben, nicht
   ergänzen** — nur der aktuelle Zustand gehört hinein, keine neue
   Historienzeile obendrauf. Das ist der Fehler, der die Datei am
   2026-09-18 auf 10.556 Wörter gebracht hat.
4. Neue Erkenntnis dieser Session einordnen: Braucht sie ein Mensch bei der
   Übergabe (Fakt, Pfad, Befehl, Regel) → ins Repo. Steuert sie nur, *wie*
   Claude arbeitet (Messkniff, Kalibrierung, Entscheidungsgewohnheit) → ins
   Memory, in den passenden Themen-Index. Kein Fakt an beiden Stellen
   (`CLAUDE.md`, Abschnitt „Memory: zweistufiger Index").
5. Vor jedem Umzug ins Repo prüfen, ob der Inhalt öffentlich sein darf
   (`doku-intern/`, Passwörter, die Heizstab-Spezialmenü-Regel bleiben
   privat) — im Zweifel den Owner fragen.

## 3. Köpfe der Vorhaben-Dateien

Erfasst beides: uncommittete Änderungen (Doku-Sessions laufen oft direkt auf
`main`, dort ist ein Diff gegen `main` immer leer) und committete Änderungen
auf einem Arbeits-Branch.

```bash
{ git diff --name-only HEAD -- 'Vorhaben-*.md' 'Auftrag-*.md' 'Arbeitsplan-*.md'
  git diff --name-only main...HEAD -- 'Vorhaben-*.md' 'Auftrag-*.md' 'Arbeitsplan-*.md'
} | sort -u
```

Für jede betroffene Datei: Steht oben das **aktuelle** Ergebnis, oder
widerspricht es dem, was weiter unten in derselben Datei steht (Lehrstück:
`Arbeitsplan-KNX-Vorderhaus.md` und `Vorhaben-Nur-ESP32-Pfad.md`, beide am
2026-09-18 mit veraltetem Kopf gefunden)? Abweichung melden und, wenn es
reine Doku ist, mit dieser Session gleich richtigstellen.

## 4. Abschlussmeldung

Nach der globalen Regel: was geändert wurde, warum (Problem/Motivation), ob
offene Punkte oder Folgeaufgaben bestehen — max. 3–5 Sätze. Pushen nur auf
ausdrücklichen Auftrag.
