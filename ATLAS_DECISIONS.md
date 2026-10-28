# Atlas — pomysły do późniejszej decyzji

Zasada użytkownika z 2026-10-26: nowe pomysły, których nie ustalono na początku, zapisujemy tutaj.
Ich opis w audycie lub propozycja agenta nie stanowią zgody na implementację. Najpierw kończymy
istotne elementy uzgodnionego atlasu MM7/MM8 i usuwamy błędy. Nowe funkcje wymagają osobnej decyzji użytkownika.

| Pomysł | Status | Uwagi |
| --- | --- | --- |
| Ocena, kto z drużyny może nauczyć się czaru z księgi i czego mu brakuje | Oczekuje na decyzję | Zbadano istniejącą obsługę ksiąg. Nie rozpoczęto implementacji. |
| Prognozowanie przyszłej dostępności transportu i sklepów | Oczekuje na decyzję | Oddzielne od już działających tras, rozkładów i zapamiętanych ofert. |
| Dodatkowe style własnych markerów i rozbudowany edytor notatek | Oczekuje na decyzję | Podstawowe markery oraz dodawanie, edycja i usuwanie notatek już działają. |
| Nowa ilustracja kontynentu Jadame | Oczekuje na decyzję | Działa natywny schemat regionów; nowa grafika nie blokuje poprawności atlasu. |
| Automatyczne obserwacje przy eksperymentalnej podmianie potworów na modele 3D | Oczekuje na decyzję | Audyt 2026-10-27: aktywne paczki MM7/MM8 nie zawierają manifestów `models/actors.yml`, a podmiany wymagają opcji debug `actor_models`, domyślnie wyłączonej. Zapis obserwacji sprite'ów działa; obsługi modeli nie wdrożono. |
| Porcjowanie i zachowywanie indeksu Guide Mode | Małe porcjowanie zaakceptowane; cache odłożony | Po diagnozie użytkownik zezwolił na prostą optymalizację. Wdrożono jedną mapę na klatkę z pierwszeństwem aktualnej lokacji; Windows: wejście ok. 0,35–0,36 s zamiast 3,1–3,2 s. Zachowywanie indeksu między trybami nie zostało wdrożone. |
| Niestandardowe kadrowanie i odwracanie ilustracji regionów przez `MapPresentation` | Oczekuje na decyzję | Audyt 2026-10-28: aktywne paczki MM7/MM8 nie zawierają katalogów `ui/maps/catalog.yml`; atlas i minimapa korzystają z tej samej standardowej projekcji. Atlas nie odczytuje jeszcze takich katalogów dla przyszłych map/modów. Nie rozpoczęto implementacji. |
| Wybór języka i wspólne tłumaczenia interfejsu gry/atlasu | Oczekuje na decyzję | Audyt 2026-10-28: loader wskazuje jawnie `engine/data_tables/english/`; istniejące menu mają teksty w kodzie i katalogach YAML, bez wspólnego przełącznika języka. Atlas używa obecnych angielskich danych gry. Nie dodano osobnego systemu tłumaczeń ani wyboru języka tylko dla atlasu. |

Ta lista nie jest listą warunków ukończenia obecnego etapu. Nie dopisujemy nowych pomysłów do kryteriów
odbioru bez zgody użytkownika. Dotychczas wdrożone rozszerzenia pozostają; nie cofamy ich bez osobnej prośby.

Priorytet bieżący: poprawność map MM7/MM8, położenia drużyny, odkrywania i mgły, podstawowych markerów,
wyszukiwania, filtrów i nawigacji, zachowania zapisów oraz sprawdzenie wspólnego UI na Androidzie/kontrolerze.
Istniejące błędy tych funkcji naprawiamy w uzgodnionym zakresie.
