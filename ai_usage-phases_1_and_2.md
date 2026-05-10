# AI Usage — Phases 1 and 2

## Tool used
- ChatGPT

## Phase 1
Am folosit AI pentru a implementa și revizui două funcții cerute explicit de temă pentru filtrare:
- `parse_condition(const char *input, char *field, char *op, char *value)`
- `match_condition(Report *r, const char *field, const char *op, const char *value)`

### Prompt folosit pentru `parse_condition`
Am descris structura `Report` și am cerut o funcție care separă un string de forma `severity:>=:2` în câmp, operator și valoare.

###  AI a oferit o variantă inițială
AI a propus o variantă bazată pe căutarea celor două caractere `:` și copierea subșirurilor în bufferele de ieșire.

### Ce am păstrat
- Logica de bază a fost păstrată.
- Am verificat manual limitele bufferelor și tratarea formatelor invalide.

### Prompt folosit pentru `match_condition`
Am cerut o funcție care compară înregistrarea cu o condiție, pentru câmpurile `severity`, `category`, `inspector` și `timestamp`.

### AI a oferit o variantă inițială:
AI a propus o structură bună de tip `if`/`strcmp`, dar prima variantă nu era completă pentru toate operațiile pe `timestamp`.

### Ce am corectat
- Am completat operatorii lipsă pentru `timestamp`.
- Am păstrat conversiile de tip necesare pentru `int` și `time_t`.

### Ce am învățat
- Pentru filtrare, conversia tipurilor trebuie făcută explicit înainte de comparație.
- E mai sigur să validezi manual formatul decât să te bazezi pe parsări prea permisive.

## Phase 2
Pentru Phase 2 am folosit AI ca punct de pornire pentru extinderea aplicației cu procese și semnale, apoi am ajustat manual implementarea pentru cerințele exacte.

### Ce a fost cerut
- Comanda `remove_district`
- Programul separat `monitor_reports`
- Notificarea monitorului prin `.monitor_pid` și `SIGUSR1`
- Păstrarea compatibilității cu Phase 1

### Ce am făcut cu ajutor AI
Am folosit AI pentru a schița:
- un handler de `SIGUSR1` și `SIGINT`
- scrierea PID-ului în `.monitor_pid`
- logica de bază pentru procesul copil care rulează `rm -rf`

### Ce am schimbat manual
- Am adăugat verificări mai stricte pentru numele districtului, ca să evit path traversal.
- Am făcut ștergerea districtului prin `fork()` + `execlp("rm", "rm", "-rf", ...)` + `waitpid()`.
- Am legat notificarea monitorului de jurnalizarea acțiunii și de mesajele de eroare cerute.
- Am păstrat funcțiile de Phase 1 active și am extins doar punctele necesare.

### Limitări și observații
- AI a fost util mai ales pentru structură, nu pentru soluția finală completă.
- Unele detalii de permisiuni și de jurnalizare au trebuit adaptate la cerințele din enunț.
- Am verificat manual că `monitor_reports` folosește `sigaction()` și nu `signal()`.

### Ce am învățat
- Semnalele și procesele trebuie tratate separat de logica de fișiere.
- E important să verifici explicit dacă fișierul `.monitor_pid` există și conține un PID valid.
- La proiecte de sistem, AI ajută cel mai mult când îi dai o cerință foarte precisă și apoi revizuiești fiecare detaliu.
