# AI Usage — Phase 1

## **Tool folosit:** ChatGPT

## Context

Am utilizat ChatGPT pentru a construi două funcții cerute în mod explicit în cerință:
`parse_condition()` și `match_condition()`, utilizate în cadrul opțiunii `--filter`.

---

## Prompt 1 — Descrierea structurii

Am oferit structura definită în cod și am formulat cererea astfel:

> "am acest struct in C:
>
> ```c
> typedef struct {
>     int id;
>     char inspector[64];
>     double lat;
>     double lon;
>     char category[32];
>     int severity;
>     time_t timestamp;
>     char description[128];
> } Report;
> ```
>
> Vreau o funcție care primește un string de forma `severity:>=:2`
> și îl separă în trei componente: field, operator și value.
> Semnătura funcției trebuie să fie:
> `int parse_condition(const char *input, char *field, char *op, char *value);`
> Funcția trebuie să returneze 1 dacă parsarea reușește și 0 dacă formatul este invalid."

**Cod generat de AI:**

```c
int parse_condition(const char *input, char *field, char *op, char *value) {
    const char *p1 = strchr(input, ':');
    if (!p1) return 0;

    int field_len = (int)(p1 - input);
    strncpy(field, input, field_len);
    field[field_len] = '\0';

    const char *p2 = strchr(p1 + 1, ':');
    if (!p2) return 0;

    int op_len = (int)(p2 - p1 - 1);
    strncpy(op, p1 + 1, op_len);
    op[op_len] = '\0';

    strcpy(value, p2 + 1);

    return 1;
}
```

**Modificări efectuate:** Nu am modificat logica deoarece functiona corect

---

## Prompt 2 — Implementarea funcției de verificare

> "Am nevoie acum de o funcție care verifică dacă un anumit record respectă o condiție dată.
> Semnătura este:
> `int match_condition(Report *r, const char *field, const char *op, const char *value);`
> Funcția returnează 1 dacă recordul îndeplinește condiția și 0 în caz contrar.
> Câmpurile posibile sunt: severity (int), category (string), inspector (string), timestamp (time_t).
> Operatorii acceptați: ==, !=, <, <=, >, >=.
> Pentru category și inspector sunt valabile doar == și !=."

**Cod generat de AI (versiunea inițială):**

```c
int match_condition(Report *r, const char *field, const char *op, const char *value) {
    if (strcmp(field, "severity") == 0) {
        int val = atoi(value);
        if (strcmp(op, "==") == 0) return r->severity == val;
        if (strcmp(op, "!=") == 0) return r->severity != val;
        if (strcmp(op, "<")  == 0) return r->severity <  val;
        if (strcmp(op, "<=") == 0) return r->severity <= val;
        if (strcmp(op, ">")  == 0) return r->severity >  val;
        if (strcmp(op, ">=") == 0) return r->severity >= val;
    }
    if (strcmp(field, "category") == 0) {
        if (strcmp(op, "==") == 0) return strcmp(r->category, value) == 0;
        if (strcmp(op, "!=") == 0) return strcmp(r->category, value) != 0;
    }
    if (strcmp(field, "inspector") == 0) {
        if (strcmp(op, "==") == 0) return strcmp(r->inspector, value) == 0;
        if (strcmp(op, "!=") == 0) return strcmp(r->inspector, value) != 0;
    }
    if (strcmp(field, "timestamp") == 0) {
        long val = atol(value);
        if (strcmp(op, "==") == 0) return r->timestamp == val;
        if (strcmp(op, "!=") == 0) return r->timestamp != val;
        if (strcmp(op, "<")  == 0) return r->timestamp <  val;
        if (strcmp(op, "<=") == 0) return r->timestamp <= val;
        if (strcmp(op, ">")  == 0) return r->timestamp >  val;
        if (strcmp(op, ">=") == 0) return r->timestamp >= val;
    }
    return 0;
}
```

**Corecții realizate:**

Versiunea inițială avea omisiuni pentru operatorii aplicați câmpului `timestamp`. Am completat manual cazurile lipsă:

```c
// Completate ulterior:
if (strcmp(op, "!=") == 0) return r->timestamp != val;
if (strcmp(op, "<=") == 0) return r->timestamp <= val;
if (strcmp(op, ">=") == 0) return r->timestamp >= val;
```

---

## Evaluare critică

**Aspecte pozitive:**

* Structura ambelor funcții a fost bine organizată și ușor de urmărit.
* Implementarea din `parse_condition` bazată pe pointeri este eficientă și clară comparativ cu alternative precum `strtok`.

**Probleme identificate:**
* Prima variantă propusă pentru `parse_condition` utiliza `sscanf`, dar am preferat varianta alternativă pentru control mai bun asupra procesului.
* Funcția `match_condition` nu acoperea complet toți operatorii pentru `timestamp`, ceea ce putea duce la comportament incorect în filtrare.

