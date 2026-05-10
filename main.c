#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    int id;
    char inspector[64];
    double latitude;
    double longitude;
    char category[50];
    int severity;
    time_t timestamp;
    char description[128];
} Report;

typedef struct {
    long value;
    int valid;
} MonitorPidInfo;

void logger(const char* district, const char* role, const char* user, const char* action);

void conversiePermisiuni(mode_t mode, char* out)
{
    if (mode & S_IRUSR)
        out[0] = 'r';
    else
        out[0] = '-';
    if (mode & S_IWUSR)
        out[1] = 'w';
    else
        out[1] = '-';
    if (mode & S_IXUSR)
        out[2] = 'x';
    else
        out[2] = '-';

    if (mode & S_IRGRP)
        out[3] = 'r';
    else
        out[3] = '-';
    if (mode & S_IWGRP)
        out[4] = 'w';
    else
        out[4] = '-';
    if (mode & S_IXGRP)
        out[5] = 'x';
    else
        out[5] = '-';

    if (mode & S_IROTH)
        out[6] = 'r';
    else
        out[6] = '-';
    if (mode & S_IWOTH)
        out[7] = 'w';
    else
        out[7] = '-';
    if (mode & S_IXOTH)
        out[8] = 'x';
    else
        out[8] = '-';

    out[9] = '\0';
}

void generatePath(char* filePath, const char* district, const char* filename)
{
    strcpy(filePath, district);
    strcat(filePath, "/");
    strcat(filePath, filename);
}

void generateLinkName(char* link_name, const char* district)
{
    strcpy(link_name, "active_reports-");
    strcat(link_name, district);
}

int districtNameIsSafe(const char* district)
{
    return district != NULL && district[0] != '\0' && strchr(district, '/') == NULL && strcmp(district, ".") != 0 && strcmp(district, "..") != 0;
}

int readFileContent(const char* path, char* buffer, size_t buffer_size)
{
    int fd = open(path, O_RDONLY);
    if (fd == -1)
        return -1;

    ssize_t total = read(fd, buffer, buffer_size - 1);
    close(fd);
    if (total < 0)
        return -1;
    buffer[total] = '\0';
    return 0;
}

MonitorPidInfo readMonitorPid(void)
{
    MonitorPidInfo info;
    info.value = -1;
    info.valid = 0;

    char buffer[64];
    if (readFileContent(".monitor_pid", buffer, sizeof(buffer)) == -1)
        return info;

    char* end = NULL;
    errno = 0;
    long pid = strtol(buffer, &end, 10);
    if (errno != 0 || end == buffer || pid <= 0)
        return info;

    info.value = pid;
    info.valid = 1;
    return info;
}

int notifyMonitorAndReport(const char* district, const char* role, const char* user, const char* action)
{
    MonitorPidInfo info = readMonitorPid();
    char message[256];

    if (!info.valid) {
        snprintf(message, sizeof(message), "%s: monitor could not be informed (%s)", action, "missing or invalid .monitor_pid");
        logger(district, role, user, message);
        return 0;
    }

    if (kill((pid_t)info.value, SIGUSR1) == -1) {
        snprintf(message, sizeof(message), "%s: monitor could not be informed (kill failed: %s)", action, strerror(errno));
        logger(district, role, user, message);
        return 0;
    }

    snprintf(message, sizeof(message), "%s: monitor informed successfully (pid=%ld)", action, info.value);
    logger(district, role, user, message);
    return 1;
}

int removeLinkedReportsSymlink(const char* district)
{
    char link_name[256];
    generateLinkName(link_name, district);

    struct stat lst;
    if (lstat(link_name, &lst) == -1)
        return 0;

    return unlink(link_name) == 0;
}

int esteRolValid(const char* role)
{
    return role != NULL && (strcmp(role, "manager") == 0 || strcmp(role, "inspector") == 0);
}

void logger(const char* district, const char* role, const char* user, const char* action)
{
    char path[256];
    generatePath(path, district, "logged_district");

    struct stat st;
    if (stat(path, &st) == -1) {
        int cfd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (cfd == -1)
            return;
        close(cfd);
        chmod(path, 0644);
    }

    if (strcmp(role, "manager") != 0) {
        printf("Rolul %s nu are drept de scriere in %s\n", role, path);
        return;
    }

    if (stat(path, &st) == 0) {
        if (!(st.st_mode & S_IWUSR)) {
            printf("managerul nu are drept de scriere in %s\n", path);
            return;
        }
    }

    int fd = open(path, O_WRONLY | O_APPEND);
    if (fd == -1)
        return;

    time_t now = time(NULL);
    char line[512];
    int len = snprintf(line, sizeof(line), "%ld\t%s\t%s\t%s\n", (long)now, user, role, action);
    write(fd, line, len);
    close(fd);
}

void init_district(const char* district)
{
    struct stat st;

    if (!districtNameIsSafe(district))
        return;

    if (stat(district, &st) == -1) {
        mkdir(district, 0750);
        chmod(district, 0750);
    }

    char path[256];

    generatePath(path, district, "reports.dat");
    if (stat(path, &st) == -1) {
        int fd = open(path, O_WRONLY | O_CREAT, 0664);
        if (fd != -1)
            close(fd);
        chmod(path, 0664);
    }

    generatePath(path, district, "district.cfg");
    if (stat(path, &st) == -1) {
        int fd = open(path, O_WRONLY | O_CREAT, 0640);
        if (fd != -1) {
            write(fd, "threshold=1\n", 12);
            close(fd);
        }
        chmod(path, 0640);
    }

    generatePath(path, district, "logged_district");
    if (stat(path, &st) == -1) {
        int fd = open(path, O_WRONLY | O_CREAT, 0644);
        if (fd != -1)
            close(fd);
        chmod(path, 0644);
    }

    char link_name[256];
    char target[256];
    generatePath(target, district, "reports.dat");
    generateLinkName(link_name, district);

    struct stat lst;
    if (lstat(link_name, &lst) == 0) {
        unlink(link_name);
    }
    symlink(target, link_name);
}

int districtHasExpectedCfgPermissions(const char* path)
{
    struct stat st;
    if (stat(path, &st) == -1)
        return 0;
    return (st.st_mode & 0777) == 0640;
}

int verificaPermisiuniCitire(const char* filepath, const char* role)
{
    struct stat st;
    if (stat(filepath, &st) == -1) {
        printf("fisierul %s nu exista\n", filepath);
        return 0;
    }

    if (!esteRolValid(role)) {
        printf("rol invalid: %s\n", role ? role : "(null)");
        return 0;
    }

    mode_t mode = st.st_mode;

    if (strcmp(role, "manager") == 0) {
        if (!(mode & S_IRUSR)) {
            printf("managerul nu are drept de citire\n");
            return 0;
        }
    } else {
        if (!(mode & S_IRGRP)) {
            printf("inspectorul nu are drept de citire");
            return 0;
        }
    }
    return 1;
}

int verificaPermisiuniScriere(const char* filepath, const char* role)
{
    struct stat st;
    if (stat(filepath, &st) == -1) {
        printf("fisierul %s nu exista\n", filepath);
        return 0;
    }

    if (!esteRolValid(role)) {
        printf("rol invalid: %s\n", role ? role : "(null)");
        return 0;
    }

    mode_t mode = st.st_mode;

    if (strcmp(role, "manager") == 0) {
        if (!(mode & S_IWUSR)) {
            printf("managerul nu are drept de scriere");
            return 0;
        }
    } else {
        if (!(mode & S_IWGRP)) {
            printf("inspectorul nu are drept de scriere");
            return 0;
        }
    }
    return 1;
}

void adaugaRaport(const char* district, const char* role, const char* user)
{
    init_district(district);

    char path[256];
    generatePath(path, district, "reports.dat");

    if (!verificaPermisiuniScriere(path, role))
        return;

    struct stat st;
    stat(path, &st);
    int num_records = (int)(st.st_size / sizeof(Report));

    Report r;
    memset(&r, 0, sizeof(Report));

    r.id = num_records + 1;
    strncpy(r.inspector, user, sizeof(r.inspector) - 1);
    r.timestamp = time(NULL);

    printf("Latitudine: ");
    scanf("%lf", &r.latitude);
    printf("Longitudine: ");
    scanf("%lf", &r.longitude);

    printf("Categ: ");
    scanf("%49s", r.category);

    printf("Severitate(1/2/3): ");
    scanf("%d", &r.severity);

    getchar();
    printf("Descriere: ");
    fgets(r.description, sizeof(r.description), stdin);

    int dlen = strlen(r.description);
    if (dlen > 0 && r.description[dlen - 1] == '\n') {
        r.description[dlen - 1] = '\0';
    }

    int fd = open(path, O_WRONLY | O_APPEND, 0664);
    if (fd == -1) {
        printf("Eroare la deschidere fisier: %s\n", path);
        return;
    }

    write(fd, &r, sizeof(Report));
    close(fd);

    printf("Raport adaugat");

    if (strcmp(role, "manager") == 0)
        logger(district, role, user, "add");
    else
        printf("\nInspectorul nu are drept de scriere in logged_district; raportul a fost adaugat fara jurnalizare.");

    notifyMonitorAndReport(district, role, user, "add");
}

void afiseazaRapoarte(const char* district, const char* role)
{
    char path[256];
    generatePath(path, district, "reports.dat");

    if (!verificaPermisiuniCitire(path, role))
        return;

    struct stat st;
    if (stat(path, &st) == -1) {
        printf("Districtul nu exista/nu are rapoarte \n");
        return;
    }

    char perms[10];
    conversiePermisiuni(st.st_mode, perms);
    char* mtime = ctime(&st.st_mtime);
    mtime[strlen(mtime) - 1] = '\0';

    printf("Permisiuni: %s  \t  Marime: %ld bytes  \t  Modificat: %s\n\n", perms, (long)st.st_size, mtime);

    int num_records = (int)(st.st_size / sizeof(Report));
    if (num_records == 0) {
        printf("Nu exista rapoarte in district");
        return;
    }

    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        printf("Eroare la deschidere fisier %s \n", path);
        return;
    }

    Report r;
    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        char* ts = ctime(&r.timestamp);
        ts[strlen(ts) - 1] = '\0';
        printf("%d. Inspector: %s \t Categorie: %s \t Severitate: %d \t %s\n", r.id, r.inspector, r.category, r.severity, ts);
    }

    close(fd);
}

void veziRaport(const char* district, const char* role, int report_id)
{
    char path[256];
    generatePath(path, district, "reports.dat");

    if (!verificaPermisiuniCitire(path, role))
        return;

    struct stat st;
    if (stat(path, &st) == -1) {
        printf("Districtul %s nu exista.\n", district);
        return;
    }

    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        printf("Eroare la deschidere fisier %s \n", path);
        return;
    }

    Report r;
    int found = 0;
    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        if (r.id == report_id) {
            found = 1;
            break;
        }
    }
    close(fd);

    if (!found) {
        printf("raportul nu s-a gasit\n");
        return;
    }

    char* ts = ctime(&r.timestamp);
    ts[strlen(ts) - 1] = '\0';

    printf("Raport %d \n", r.id);
    printf("Inspector: %s\n", r.inspector);
    printf("Coordonate: lat=%.6f, lon=%.6f\n", r.latitude, r.longitude);
    printf("Categorie: %s\n", r.category);
    printf("Severitate: %d\n", r.severity);
    printf("Timestamp: %s\n", ts);
    printf("Descriere: %s\n", r.description);
}

void stergeRaport(const char* district, const char* role, const char* user, int report_id)
{
    if (strcmp(role, "manager") != 0) {
        printf("Eroare permisiuni\n");
        return;
    }

    char path[256];
    generatePath(path, district, "reports.dat");

    if (!verificaPermisiuniScriere(path, role))
        return;

    struct stat st;
    if (stat(path, &st) == -1) {
        printf("Districtul nu exista.\n");
        return;
    }
    int num_records = (int)(st.st_size / sizeof(Report));

    int fd = open(path, O_RDWR);
    if (fd == -1) {
        printf("Eroare la deschidere fisier %s \n", path);
        return;
    }

    Report r;
    int idx = -1;
    for (int i = 0; i < num_records; i++) {
        if (lseek(fd, i * sizeof(Report), SEEK_SET) == -1) {
            printf("lseek error\n");
            close(fd);
            return;
        }
        if (read(fd, &r, sizeof(Report)) != sizeof(Report)) {
            printf("read error\n");
            close(fd);
            return;
        }
        if (r.id == report_id) {
            idx = i;
            break;
        }
    }

    if (idx == -1) {
        printf("Raportul %d nu exista in districtul %s.\n", report_id, district);
        close(fd);
        return;
    }

    /* Shift records after idx one position earlier using lseek/read/write */
    Report tmp;
    for (int i = idx + 1; i < num_records; i++) {
        if (lseek(fd, i * sizeof(Report), SEEK_SET) == -1) {
            printf("lseek error\n");
            close(fd);
            return;
        }
        if (read(fd, &tmp, sizeof(Report)) != sizeof(Report)) {
            printf("read error during shift\n");
            close(fd);
            return;
        }
        if (lseek(fd, (i - 1) * sizeof(Report), SEEK_SET) == -1) {
            printf("lseek error\n");
            close(fd);
            return;
        }
        if (write(fd, &tmp, sizeof(Report)) != sizeof(Report)) {
            printf("write error during shift\n");
            close(fd);
            return;
        }
    }

    if (ftruncate(fd, (num_records - 1) * sizeof(Report)) == -1) {
        printf("ftruncate error\n");
    }

    close(fd);

    printf("Raportul %d a fost sters din districtul %s.\n", report_id, district);
    logger(district, role, user, "remove_report");
}

void actualizeazaThreshold(const char* district, const char* role, const char* user, int value)
{
    if (strcmp(role, "manager") != 0) {
        printf("Eroare permisiuni\n");
        return;
    }

    char path[256];
    generatePath(path, district, "district.cfg");

    struct stat st;
    if (stat(path, &st) == -1) {
        printf("Eroare fisierul %s nu exista\n", path);
        return;
    }

    mode_t perms = st.st_mode & 0777;
    if (perms != 0640) {
        printf("Eroare permisiuni\n");
        return;
    }

    int fd = open(path, O_WRONLY | O_TRUNC);
    if (fd == -1) {
        printf("Eroare deschidere fisier");
        return;
    }

    char line[32];
    int len = snprintf(line, sizeof(line), "threshold=%d\n", value);
    write(fd, line, len);
    close(fd);

    printf("Threshold actualizat");
    logger(district, role, user, "update_threshold");
}

void stergeDistrict(const char* district, const char* role, const char* user)
{
    if (strcmp(role, "manager") != 0) {
        printf("Eroare permisiuni\n");
        return;
    }

    if (!districtNameIsSafe(district)) {
        printf("District invalid\n");
        return;
    }

    char district_path[256];
    snprintf(district_path, sizeof(district_path), "%s", district);

    pid_t pid = fork();
    if (pid == -1) {
        printf("Eroare fork\n");
        return;
    }

    if (pid == 0) {
        execlp("rm", "rm", "-rf", district_path, (char*)NULL);
        _exit(127);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) == -1) {
        printf("Eroare waitpid\n");
        return;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        printf("Stergerea districtului a esuat\n");
        return;
    }

    removeLinkedReportsSymlink(district);
    printf("Districtul %s a fost sters\n", district);
    logger(district, role, user, "remove_district");
}

int parse_condition(const char* input, char* field, char* op, char* value)
{
    /* size limits must match buffers used in filter_reports */
    #define FIELD_SZ 32
    #define OP_SZ 8
    #define VAL_SZ 64

    const char* p1 = strchr(input, ':');
    if (!p1)
        return 0;

    int field_len = (int)(p1 - input);
    if (field_len <= 0 || field_len >= FIELD_SZ)
        return 0;
    memcpy(field, input, field_len);
    field[field_len] = '\0';

    const char* p2 = strchr(p1 + 1, ':');
    if (!p2)
        return 0;

    int op_len = (int)(p2 - p1 - 1);
    if (op_len <= 0 || op_len >= OP_SZ)
        return 0;
    memcpy(op, p1 + 1, op_len);
    op[op_len] = '\0';

    const char* val = p2 + 1;
    size_t val_len = strlen(val);
    if (val_len == 0 || val_len >= VAL_SZ)
        return 0;
    strncpy(value, val, VAL_SZ - 1);
    value[VAL_SZ - 1] = '\0';
    return 1;
}

int validateReportFilePermissions(const char* path, const char* role, int for_write)
{
    struct stat st;
    if (stat(path, &st) == -1) {
        printf("fisierul %s nu exista\n", path);
        return 0;
    }

    if (!esteRolValid(role)) {
        printf("rol invalid: %s\n", role ? role : "(null)");
        return 0;
    }

    if (for_write) {
        if (strcmp(role, "manager") == 0) {
            if (!(st.st_mode & S_IWUSR)) {
                printf("managerul nu are drept de scriere\n");
                return 0;
            }
        } else {
            if (!(st.st_mode & S_IWGRP)) {
                printf("inspectorul nu are drept de scriere\n");
                return 0;
            }
        }
    } else {
        if (strcmp(role, "manager") == 0) {
            if (!(st.st_mode & S_IRUSR)) {
                printf("managerul nu are drept de citire\n");
                return 0;
            }
        } else {
            if (!(st.st_mode & S_IRGRP)) {
                printf("inspectorul nu are drept de citire\n");
                return 0;
            }
        }
    }

    return 1;
}

int match_condition(Report* r, const char* field, const char* op, const char* value)
{
    if (strcmp(field, "severity") == 0) {
        int val = atoi(value);
        int sev = r->severity;
        if (strcmp(op, "==") == 0)
            return sev == val;
        if (strcmp(op, "!=") == 0)
            return sev != val;
        if (strcmp(op, "<") == 0)
            return sev < val;
        if (strcmp(op, "<=") == 0)
            return sev <= val;
        if (strcmp(op, ">") == 0)
            return sev > val;
        if (strcmp(op, ">=") == 0)
            return sev >= val;
        /* unsupported operator */
        return 0;
    }

    if (strcmp(field, "category") == 0) {
        int cmp = strcmp(r->category, value);
        if (strcmp(op, "==") == 0)
            return cmp == 0;
        if (strcmp(op, "!=") == 0)
            return cmp != 0;
        return 0;
    }

    if (strcmp(field, "inspector") == 0) {
        int cmp = strcmp(r->inspector, value);
        if (strcmp(op, "==") == 0)
            return cmp == 0;
        if (strcmp(op, "!=") == 0)
            return cmp != 0;
        return 0;
    }

    if (strcmp(field, "timestamp") == 0) {
        long val = atol(value);
        long ts = (long)r->timestamp;
        if (strcmp(op, "==") == 0)
            return ts == val;
        if (strcmp(op, "!=") == 0)
            return ts != val;
        if (strcmp(op, "<") == 0)
            return ts < val;
        if (strcmp(op, "<=") == 0)
            return ts <= val;
        if (strcmp(op, ">") == 0)
            return ts > val;
        if (strcmp(op, ">=") == 0)
            return ts >= val;
        return 0;
    }

    return 0;
}

void filter_reports(const char* district, const char* role, char** conditions, int num_conditions)
{
    char path[256];
    generatePath(path, district, "reports.dat");

    if (!verificaPermisiuniCitire(path, role))
        return;

    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        printf("Eroare deschidere fisier");
        return;
    }

    char fields[10][32];
    char ops[10][8];
    char values[10][64];
    for (int i = 0; i < num_conditions; i++) {
        if (!parse_condition(conditions[i], fields[i], ops[i], values[i])) {
            printf("Eroare conditie: %s\n", conditions[i]);
            close(fd);
            return;
        }
    }

    Report r;
    int found = 0;

    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        int match = 1;
        for (int i = 0; i < num_conditions; i++) {
            if (!match_condition(&r, fields[i], ops[i], values[i])) {
                match = 0;
                break;
            }
        }
        if (match) {
            found++;
            char* ts = ctime(&r.timestamp);
            ts[strlen(ts) - 1] = '\0';
            printf("%d.) %-20s \t %-10s \t sev:%d \t %s \t %s\n", r.id, r.inspector, r.category, r.severity, ts, r.description);
        }
    }

    close(fd);

    if (found == 0) {
        printf("Nu s-au gasit rapoarte\n");
    }
}

int main(int argc, char* argv[])
{
    char* role = NULL;
    char* user = NULL;
    char* command = NULL;
    int cmd_idx = -1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--role") == 0 && i + 1 < argc) {
            role = argv[i + 1];
            i++;
        } else if (strcmp(argv[i], "--user") == 0 && i + 1 < argc) {
            user = argv[i + 1];
            i++;
        } else if (argv[i][0] == '-' && argv[i][1] == '-') {
            command = argv[i] + 2;
            cmd_idx = i;
            break;
        }
    }

    if (command == NULL) {
        fprintf(stderr, "No command specified\n");
        return 1;
    }

    if (!esteRolValid(role)) {
        fprintf(stderr, "Invalid or missing --role. Use manager or inspector.\n");
        return 1;
    }

    if (user == NULL || user[0] == '\0') {
        fprintf(stderr, "Invalid or missing --user.\n");
        return 1;
    }

    if (strcmp(command, "add") == 0) {
        if (cmd_idx + 1 >= argc) {
            fprintf(stderr, "Usage: --add <district_id>\n");
            return 1;
        }
        adaugaRaport(argv[cmd_idx + 1], role, user);
    } else if (strcmp(command, "list") == 0) {
        if (cmd_idx + 1 >= argc) {
            fprintf(stderr, "Usage: --list <district_id>\n");
            return 1;
        }
        afiseazaRapoarte(argv[cmd_idx + 1], role);
    } else if (strcmp(command, "view") == 0) {
        if (cmd_idx + 2 >= argc) {
            fprintf(stderr, "Usage: --view <district_id> <report_id>\n");
            return 1;
        }
        int rid = atoi(argv[cmd_idx + 2]);
        veziRaport(argv[cmd_idx + 1], role, rid);
    } else if (strcmp(command, "remove_report") == 0) {
        if (cmd_idx + 2 >= argc) {
            fprintf(stderr, "Usage: --remove_report <district_id> <report_id>\n");
            return 1;
        }
        int rid = atoi(argv[cmd_idx + 2]);
        stergeRaport(argv[cmd_idx + 1], role, user, rid);
    } else if (strcmp(command, "update_threshold") == 0) {
        if (cmd_idx + 2 >= argc) {
            fprintf(stderr, "Usage: --update_threshold <district_id> <value>\n");
            return 1;
        }
        int val = atoi(argv[cmd_idx + 2]);
        actualizeazaThreshold(argv[cmd_idx + 1], role, user, val);
    } else if (strcmp(command, "filter") == 0) {
        if (cmd_idx + 1 >= argc) {
            fprintf(stderr, "Usage: --filter <district_id> <condition> [condition...]\n");
            return 1;
        }
        char* district = argv[cmd_idx + 1];
        char** conditions = &argv[cmd_idx + 2];
        int num_cond = argc - cmd_idx - 2;
        filter_reports(district, role, conditions, num_cond);
    } else if (strcmp(command, "remove_district") == 0) {
        if (cmd_idx + 1 >= argc) {
            fprintf(stderr, "Usage: --remove_district <district_id>\n");
            return 1;
        }
        stergeDistrict(argv[cmd_idx + 1], role, user);
    } else {
        fprintf(stderr, "Unknown command: %s\n", command);
        return 1;
    }

    return 0;
}