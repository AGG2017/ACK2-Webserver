#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <netinet/in.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <sys/types.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "httpd.h"

typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef uint64_t QWORD;
typedef int8_t S8;
typedef uint8_t U8;
typedef uint16_t U16;
typedef int16_t S16;
typedef uint32_t U32;
typedef int32_t S32;
typedef uint64_t U64;
typedef int64_t S64;

const BYTE GRID[] = {
    0,
    1,
    0,
    0,
    2,  // 4
    0,
    0,
    0,
    0,
    3,  // 9
    0,
    0,
    0,
    0,
    0,
    0,
    4,  // 16
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    5,  // 25
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    6,  // 36
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    7,  // 49
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    8,  // 64
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    9,  // 81
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    10,  // 100
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    11,  // 121
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    12,  // 144
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0};

#define SYSTEM_BUFFER_MAX 1024
char system_buffer[SYSTEM_BUFFER_MAX];

int system_with_output(const char *cmd, int line_number) {
    FILE *fp;
    int n = 0;

    /* Open the command for reading. */
    fp = popen(cmd, "r");
    if (fp == NULL) {
        return 1;
    }
    /* Read the output a line at a time - output it. */
    while (fgets(system_buffer, SYSTEM_BUFFER_MAX, fp) != NULL) {
        if (debug) {
            fprintf(stderr, "+++ system: %s\n", system_buffer);
        }
        n++;
        if (n == line_number)
            break;
    }
    /* close */
    pclose(fp);
    return 0;
}

U32 file_lenght(const char *fname) {
    struct stat st;
    if (access(fname, F_OK) != -1) {
        stat(fname, &st);
        return (U32)st.st_size;
    } else {
        return 0;
    }
}

#define COPY_BUF_SIZE 4096

// if buffer==NULL then do not copy the string at the end of the source file
// if from==NULL then skip copy the source file
// if to==NULL then do nothing
// mode="wb" for creating a new dest file, or "ab" for appending to the destination file
int custom_copy_file(const char *from, const char *to, const char *mode, const char *buffer) {
    FILE *src, *dst;
    size_t in, out;
    if (!to)
        return 0;  // nothing to do
    char *buf = (char *)malloc(COPY_BUF_SIZE * sizeof(char));
    if (!buf)
        return -1;
    dst = fopen(to, mode);  // "wb" or "ab"
    if (dst < 0) {
        free(buf);
        return -2;
    }
    if (from) {
        // need to copy a source file
        src = fopen(from, "rb");
        if (NULL == src) {
            fflush(dst);
            fclose(dst);
            free(buf);
            return -3;
        }
        while (1) {
            in = fread(buf, sizeof(char), COPY_BUF_SIZE, src);
            if (0 == in)
                break;
            out = fwrite(buf, sizeof(char), in, dst);
            if (0 == out)
                break;
        }
        fclose(src);
    }
    if (buffer) {
        fwrite(buffer, sizeof(char), strlen(buffer), dst);
    }
    fflush(dst);
    fclose(dst);
    return 0;
}

// detect the printer default parameters:
// printer_model: printer model (K2Pro, K2Plus, K2Max)
// cfg_filename: "/user/printer.cfg", "/user/printer_plus.cfg" or "/user/printer_max.cfg"
// grid_size: 5 for K2Pro or 7 for K2Plus and K2Max
// return 0 if success, or the error code
int detect_printer_defaults(const char **printer_model, const char **cfg_path, const char **cfg_filename, int *grid_size) {
    const char *k2pro_file = "printer.cfg";
    const char *k2plus_file = "printer_plus.cfg";
    const char *k2max_file = "printer_max.cfg";
    const char *k2pro_path = "/user/printer.cfg";
    const char *k2plus_path = "/user/printer_plus.cfg";
    const char *k2max_path = "/user/printer_max.cfg";
    const char *k2_file = NULL;
    const char *k2_path = NULL;
    const char *model_name = NULL;
    int grid = 0;

    // find out the model and the config file name
    if (file_lenght(k2pro_path) > 0) {
        k2_file = k2pro_file;
        k2_path = k2pro_path;
        model_name = "K2Pro";
        grid = 5;
    } else {
        if (file_lenght(k2plus_path) > 0) {
            k2_file = k2pro_file;
            k2_path = k2pro_path;
            model_name = "K2Plus";
            grid = 7;
        } else {
            if (file_lenght(k2max_path) > 0) {
                k2_file = k2pro_file;
                k2_path = k2pro_path;
                model_name = "K2Max";
                grid = 7;
            }
        }
    }

    if (cfg_path)
        *cfg_path = k2_path;
    if (cfg_filename)
        *cfg_filename = k2_file;
    if (grid_size)
        *grid_size = grid;
    if (printer_model)
        *printer_model = model_name;

    if (!k2_path) {
        return 1;
    } else {
        return 0;
    }
}

#define MAX_DATA_SLOTS 100
#define MIN_SUPPORTED_GRID_SIZE 2
#define MAX_SUPPORTED_GRID_SIZE 10
#define BYTES_PER_GRID_ELEMENT 15
#define MESH_BUFFER_SIZE (MAX_SUPPORTED_GRID_SIZE * MAX_SUPPORTED_GRID_SIZE * BYTES_PER_GRID_ELEMENT + 1)
#define MESH_MATRIX_ELEMENTS (MAX_SUPPORTED_GRID_SIZE * MAX_SUPPORTED_GRID_SIZE + 1)

// keep the last used mesh in format printer*.cfg
char mesh_config[MESH_BUFFER_SIZE];
// keep the last used mesh in matrix format for the 3d visualizer
char mesh_matrix[MESH_BUFFER_SIZE];
// keeps the last detected grid size from printer*.cfg, 0-unknown
int mesh_grid = 0;
// last detected probe_count_x & probe_count_y, 0-unknown
int probe_count_x = 0;
int probe_count_y = 0;
// last detected x_count & y_count, 0-unknown
int x_count = 0;
int y_count = 0;
// web page grid size
int new_grid_size = 0;
// bed temperature
int bed_temp = 60;
// used profile number
int used_profile = 1;
int saved_profile = 1;

// the number of mesh accumulation in mesh_acc[]
int mesh_accumulations = 0;
// mesh accumulator
double mesh_acc[MESH_MATRIX_ELEMENTS];
// mesh values (current bed leveling mesh)
double mesh_values[MESH_MATRIX_ELEMENTS];
// mesh average (calculated mesh average) or temp values during the average calculation
double mesh_average[MESH_MATRIX_ELEMENTS];
// selected average precision from the config file
double precision = 0.01;
// set Z-offset in the printer*.cfg file
double z_offset = 0.0;

// clear the buffer for mesh from config
void mesh_config_clear(void) {
    memset(mesh_config, 0, MESH_BUFFER_SIZE);
}

// clear the buffer for mesh in 3d visualizer format
void mesh_matrix_clear(void) {
    memset(mesh_matrix, 0, MESH_BUFFER_SIZE);
}

// clear the mesh values
void mesh_clear(double *mesh) {
    int i;
    for (i = 0; i < MESH_MATRIX_ELEMENTS; i++)
        mesh[i] = 0.0;
}

// clear the mesh accumulator
void mesh_acc_clear(void) {
    mesh_clear(mesh_acc);
    mesh_accumulations = 0;
}

// mesh_acc[] += mesh_values[] or mesh_acc[] += mesh_average[]
// mesh_accumulations++
void mesh_acc_add(double *value) {
    int i;
    for (i = 0; i < MESH_MATRIX_ELEMENTS; i++)
        mesh_acc[i] += value[i];
    mesh_accumulations++;
}

// mesh_average[] = mesh_acc[] / mesh_accumulations
void mesh_acc_average(void) {
    int i;
    if (mesh_accumulations) {
        if (mesh_accumulations == 1) {
            // mesh_accumulations==1
            for (i = 0; i < MESH_MATRIX_ELEMENTS; i++)
                mesh_average[i] = mesh_acc[i];
        } else {
            // mesh_accumulations>=2
            for (i = 0; i < MESH_MATRIX_ELEMENTS; i++)
                mesh_average[i] = mesh_acc[i] / mesh_accumulations;
        }
    } else {
        // mesh_accumulations==0
        mesh_clear(mesh_average);
    }
}

// convert mesh_values[grid x grid] or mesh_average[grid x grid] to mesh_matrix[grid x grid]
// used format "%+1.4f"
void mesh_matrix_export(double *mesh, int grid) {
    int x, y, i, ii;
    i = 0;
    ii = 0;
    mesh_matrix_clear();
    for (y = 0; y < grid; y++) {
        for (x = 0; x < grid; x++) {
            if (x == (grid - 1)) {
                sprintf(&mesh_matrix[ii], " %+1.4f\n", mesh[i]);
                ii += 9;
            } else {
                if (x == 0) {
                    sprintf(&mesh_matrix[ii], "%+1.4f", mesh[i]);
                    ii += 7;
                } else {
                    sprintf(&mesh_matrix[ii], " %+1.4f", mesh[i]);
                    ii += 8;
                }
            }
            i++;
        }
    }
}

// export mesh_average[] to buffer[] for printer.cfg setup
void mesh_average_export(int grid, char *buffer) {
    int x, y, i, ii;
    i = 0;
    ii = 0;
    for (y = 0; y < grid; y++) {
        for (x = 0; x < grid; x++) {
            if ((x == (grid - 1)) && (y == (grid - 1))) {
                // last point
                sprintf(&buffer[ii], " %+1.6f", mesh_average[i]);
                ii += 10;
                buffer[ii] = 0;
            } else {
                if ((x == 0) && (y == 0)) {
                    // first point
                    sprintf(&buffer[ii], "%+1.6f,", mesh_average[i]);
                    ii += 10;
                    buffer[ii] = 0;
                } else {
                    // any other point
                    sprintf(&buffer[ii], " %+1.6f,", mesh_average[i]);
                    ii += 11;
                    buffer[ii] = 0;
                }
            }
            i++;
        }
    }
}

// return the number of the next available data slot (1-MAX_DATA_SLOTS)
// or 0 if all slots are used
int next_free_data_slot(void) {
    int i;
    char fn_buf[64];
    int next = 0;
    FILE *file;
    for (i = 1; i < MAX_DATA_SLOTS; i++) {
        sprintf(fn_buf, "/user/webfs/data_slot_%d.txt", i);
        file = fopen(fn_buf, "r");
        if (!file) {
            next = i;
            break;
        } else {
            fclose(file);
        }
    }
    return next;
}

// parse the mesh values from config file string to mesh_values[] or mesh_average[]
// return the number of parsed elements (not the grid size!)
int parse_mesh_values(char *buffer, double *mesh) {
    int i;
    int nn = 1;
    mesh_clear(mesh);
    if ((buffer[0] == '\n') || (buffer[0] == 0)) {
        return 0;
    }
    mesh[0] = atof(buffer);
    for (i = 0;; i++) {
        if ((buffer[i] == '\n') || (buffer[i] == 0))
            break;
        if (buffer[i] == ',') {
            mesh[nn] = atof(&buffer[i + 2]);
            nn++;
        }
    }
    return nn;
}

// calculate mesh average for all used data slots
// return the number of detected data slots
// accumulate only data slots with provided grid_size
// mesh_values[] = mesh_acc[all used data slots] / mesh_accumulations
int calculate_mesh_average(int grid_size) {
    int i, n, elements;
    char fn_buf[64];
    FILE *file;

    mesh_acc_clear();

    elements = grid_size * grid_size;

    for (i = 1; i < MAX_DATA_SLOTS; i++) {
        sprintf(fn_buf, "/user/webfs/data_slot_%d.txt", i);
        file = fopen(fn_buf, "r");
        if (file) {
            fread(mesh_config, 1, MESH_BUFFER_SIZE, file);
            fclose(file);
            n = parse_mesh_values(mesh_config, mesh_average);
            if (n == elements) {
                mesh_acc_add(mesh_average);
            }
        }
    }
    mesh_acc_average();

    return mesh_accumulations;
}

int export_selected_slot(int slot) {
    int n, nn;
    char fn_buf[64];
    FILE *file;

    n = 0;
    sprintf(fn_buf, "/user/webfs/data_slot_%d.txt", slot);
    file = fopen(fn_buf, "r");
    if (file) {
        fread(mesh_config, 1, MESH_BUFFER_SIZE, file);
        fclose(file);
        nn = parse_mesh_values(mesh_config, mesh_values);
        n = GRID[nn & 0xFF];
        mesh_matrix_export(mesh_values, n);
    }
    return n;
}

// apply provided precision to the provided buffer
// buffer = mesh_values, precision=0.01
void apply_precision(double *mesh, double precision) {
    int i;
    if (precision != 0.0) {
        for (i = 0; i < MESH_MATRIX_ELEMENTS; i++)
            mesh[i] = round(mesh[i] / precision) * precision;
    }
}

// results:
// return the error code or 0 for success
// result=1 >>> missing config file
// result=2 >>> missing mesh information
// mesh_grid = detected grid size from the printer config file
// mesh_config[] = copy of the mesh as shown in the printer config file
// mesh_values[] = parsed values
// mesh_matrix[] = formatted matrix for use in the 3d visualizer
int read_mesh_from_printer_config(void) {
    const char *k2_cfg = NULL;

    int result = 2;

    int rr = detect_printer_defaults(NULL, &k2_cfg, NULL, NULL);

    if (rr) {
        return 1;
    } else {
        FILE *file;
        char *b = NULL;
        size_t len = 0;
        ssize_t read;
        int i, n, nn;

        // clear the result
        mesh_config_clear();
        mesh_clear(mesh_values);
        mesh_grid = 0;
        bed_temp = 60;
        probe_count_x = 0;
        probe_count_y = 0;
        x_count = 0;
        y_count = 0;
        z_offset = 0.0;

        // read the file line by line
        file = fopen(k2_cfg, "r");
        if (file) {
            while (1) {
                read = getline(&b, &len, file);
                if (read == -1) {
                    break;
                }
                n = strlen(b);
                if (n == 0)
                    continue;

                // line processing
                if (b[0] == 'p' && b[1] == 'o' && b[2] == 'i' && b[3] == 'n' && b[4] == 't' && b[5] == 's' && b[6] == ' ' && b[7] == ':' && b[8] == ' ') {
                    // found "points : "

                    // keep a copy of the original line
                    strcpy(mesh_config, &b[9]);

                    nn = parse_mesh_values(&b[9], mesh_values);

                    n = GRID[nn & 0xFF];
                    mesh_grid = n;

                    mesh_matrix_export(mesh_values, mesh_grid);

                    result = 0;
                } else if (b[0] == 'p' && b[1] == 'r' && b[2] == 'o' && b[3] == 'b' && b[4] == 'e' && b[5] == '_' && b[6] == 'c' &&
                           b[7] == 'o' && b[8] == 'u' && b[9] == 'n' && b[10] == 't' && b[11] == ' ' && b[12] == ':' && b[13] == ' ') {
                    sscanf(&b[14], "%d,%d", &probe_count_x, &probe_count_y);
                } else if (b[0] == 'x' && b[1] == '_' && b[2] == 'c' && b[3] == 'o' && b[4] == 'u' &&
                           b[5] == 'n' && b[6] == 't' && b[7] == ' ' && b[8] == ':' && b[9] == ' ') {
                    sscanf(&b[10], "%d", &x_count);
                } else if (b[0] == 'y' && b[1] == '_' && b[2] == 'c' && b[3] == 'o' && b[4] == 'u' &&
                           b[5] == 'n' && b[6] == 't' && b[7] == ' ' && b[8] == ':' && b[9] == ' ') {
                    sscanf(&b[10], "%d", &y_count);
                } else if (b[0] == 'z' && b[1] == '_' && b[2] == 'o' && b[3] == 'f' && b[4] == 'f' &&
                           b[5] == 's' && b[6] == 'e' && b[7] == 't' && b[8] == ' ' && b[9] == ':' && b[10] == ' ') {
                    z_offset = atof(&b[11]);
                } else if (b[0] == 'b' && b[1] == 'e' && b[2] == 'd' && b[3] == '_' && b[4] == 'm' &&
                           b[5] == 'e' && b[6] == 's' && b[7] == 'h' && b[8] == '_' && b[9] == 't' && b[10] == 'e' &&
                           b[11] == 'm' && b[12] == 'p' && b[13] == ' ' && b[14] == ':' && b[15] == ' ') {
                    bed_temp = atoi(&b[16]);
                }
            }
            fclose(file);
            if (b)
                free(b);
        }
    }
    return result;
}

char static_template_buffer[1024];
char *static_template_ptr;
config_option_t leveling_config = NULL;
int error_code;
int response_code;

int get_ssh_status(void) {
    int ssh_status = 0;  // not installed
    if (file_exists("/opt/etc/init.d/S51dropbear")) {
        system_buffer[0] = 0;
        system_with_output("pidof dropbear", 1);
        if ((system_buffer[0] >= '0') && (system_buffer[0] <= '9')) {
            ssh_status = 2;  // started
        } else {
            ssh_status = 1;  // stopped
        }
    }
    return ssh_status;
}

// update /mnt/UDISK/webfs/api/info.json
int update_api(void) {
    U32 total_mem = sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGESIZE);
    U32 free_mem = sysconf(_SC_AVPHYS_PAGES) * sysconf(_SC_PAGESIZE);
    U32 free_mem_per = (free_mem * 100) / total_mem;
    U32 cpu_use = 0;
    U32 cpu_usr_use = 0;
    U32 cpu_sys_use = 0;
    U32 cpu_idle = 0;
    char cpu[10];
    int t0, t1, t2, t3, t4, t5, t6, t7, t8, t9;
    system_with_output("cat /proc/stat", 1);
    int n = sscanf(system_buffer, "%s %d %d %d %d %d %d %d %d %d %d", cpu, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7, &t8, &t9);
    if (n == 11) {
        cpu_use = ((t0 + t2) * 100) / (t0 + t2 + t3);
        cpu_usr_use = (t0 * 100) / (t0 + t2 + t3);
        cpu_sys_use = (t2 * 100) / (t0 + t2 + t3);
        cpu_idle = (t3 * 100) / (t0 + t2 + t3);
    }
    int ssh_status = get_ssh_status();
    sprintf(static_template_buffer, "{\"api_ver\":1, \"total_mem\":%d, \"free_mem\":%d, \"free_mem_per\":%d, \"cpu_use\":%d, \"cpu_usr_use\":%d, \"cpu_sys_use\":%d, \"cpu_idle\":%d, \"ssh_status\":%d}", total_mem, free_mem, free_mem_per, cpu_use, cpu_usr_use, cpu_sys_use, cpu_idle, ssh_status);
    // export buffer to file
    return custom_copy_file(NULL, "/mnt/UDISK/webfs/api/info.json", "wb", static_template_buffer);
}

// process action query and update /mnt/UDISK/webfs/api/do.json
int control_api(config_option_t query) {
    int result = -1;
    char *action = get_key_value(query, "action", "unknown");
    if (!strcmp(action, "reboot")) {
        system_with_output("reboot", 1);
        result = 1;
    }
    if (!strcmp(action, "ssh_start")) {
        int ssh = get_ssh_status();
        result = 0;
        if (ssh == 1) {
            system_with_output("/opt/etc/init.d/S51dropbear start 2>&1", 1);
            result = 2;
        } else {
            if (ssh == 2)
                result = 2;
        }
    }
    if (!strcmp(action, "ssh_stop")) {
        int ssh = get_ssh_status();
        result = 0;
        if (ssh == 2) {
            system_with_output("/opt/etc/init.d/S51dropbear stop 2>&1", 1);
            result = 1;
        } else {
            if (ssh == 1)
                result = 1;
        }
    }
    sprintf(static_template_buffer, "{\"api_ver\":1, \"result\":%d}", result);
    return custom_copy_file(NULL, "/mnt/UDISK/webfs/api/do.json", "wb", static_template_buffer);
}

char *leveling_template_callback(char key) {
    // response code replacement
    if (key == '@') {
        if (response_code == 1) {
            static_template_ptr = "SUCCESS: The printer is preparing to reboot now...";
            return static_template_ptr;
        }
        if (response_code == 2) {
            static_template_ptr = "SUCCESS: All data slots have been cleared!";
            return static_template_ptr;
        }
        if (response_code == 3) {
            static_template_ptr = "SUCCESS: Selected data slot has been cleared!";
            return static_template_ptr;
        }
        if (response_code == 4) {
            static_template_ptr = "SUCCESS: Current mesh has been saved in the selected data slot!";
            return static_template_ptr;
        }
        if (response_code == 5) {
            static_template_ptr = "SUCCESS: Calculated mesh average was set. Please reboot the printer to activate it!";
            return static_template_ptr;
        }
        if (response_code == 6) {
            static_template_ptr = "SUCCESS: Selected new arithmetic precision has been set!";
            return static_template_ptr;
        }
        if (response_code == 7) {
            return system_buffer;
        }
        if (response_code == 8) {
            static_template_ptr = "SUCCESS: The requested command has been executed!";
            return static_template_ptr;
        }
        if (response_code == 9) {
            sprintf(static_template_buffer, "SUCCESS: Current printer configuration has been saved as a profile number %d.", saved_profile);
            return static_template_buffer;
        }
        if (response_code == 10) {
            sprintf(static_template_buffer, "SUCCESS: Profile %d is set to be used. Please reboot the printer!", saved_profile);
            return static_template_buffer;
        }
        if (response_code == 11) {
            sprintf(static_template_buffer, "SUCCESS: Selected bed mesh temperature %d C has been set!", bed_temp);
            return static_template_buffer;
        }

        // default - no response shown
        static_template_buffer[0] = 0;
        return static_template_buffer;
    }
    // error code replacement
    if (key == '#') {
        if (error_code == 1) {
            static_template_ptr = "ERROR: No more free data slots are available!";
            return static_template_ptr;
        }
        if (error_code == 2) {
            static_template_ptr = "ERROR: Missing printer configuration file! Try to upload it from a backup.";
            return static_template_ptr;
        }
        if (error_code == 3) {
            static_template_ptr = "WARNING: No mesh data available in the config file! Please level the bed first!";
            return static_template_ptr;
        }
        if (error_code == 4) {
            static_template_ptr = "ERROR: Square grid is supported only! Select a new grid size and level the bed!";
            return static_template_ptr;
        }
        if (error_code == 5) {
            static_template_ptr = "ERROR: Probe grid size and bed mesh size do not match! Level the bed or select a new grid size!";
            return static_template_ptr;
        }
        if (error_code == 6) {
            static_template_ptr = "WARNING: The grid size has changed! Please reboot the printer and then level the bed.";
            return static_template_ptr;
        }
        if (error_code == 7) {
            static_template_ptr = "WARNING: The printer config file has changed! Please reboot the printer for the changes to be accepted.";
            return static_template_ptr;
        }
        if (error_code == 8) {
            static_template_ptr = "ERROR: The average cannot be used with all data slot cleared. Please save current mesh in a data slot first!";
            return static_template_ptr;
        }
        if (error_code == 9) {
            static_template_ptr = "ERROR: Selected data slot is out of the supported range!";
            return static_template_ptr;
        }
        if (error_code == 10) {
            static_template_ptr = "ERROR: Unsupported grid size!";
            return static_template_ptr;
        }
        if (error_code == 11) {
            static_template_ptr = "ERROR: Unsupported arithmetic precision!";
            return static_template_ptr;
        }
        if (error_code == 12) {
            static_template_ptr = "WARNING: No change detected in the requested parameters. These values are already set!";
            return static_template_ptr;
        }
        if (error_code == 13) {
            static_template_ptr = "ERROR: SSH service is not installed!";
            return static_template_ptr;
        }
        if (error_code == 14) {
            static_template_ptr = "ERROR: Invalid profile number!";
            return static_template_ptr;
        }
        if (error_code == 15) {
            static_template_ptr = "ERROR: Unable to detect the printer configuration!";
            return static_template_ptr;
        }
        if (error_code == 16) {
            static_template_ptr = "ERROR: Selected profile is already the current used profile!";
            return static_template_ptr;
        }
        if (error_code == 17) {
            static_template_ptr = "ERROR: Cannot save this profile!";
            return static_template_ptr;
        }
        if (error_code == 18) {
            static_template_ptr = "ERROR: Cannot read this profile!";
            return static_template_ptr;
        }
        if (error_code == 19) {
            static_template_ptr = "ERROR: Cannot use this profile!";
            return static_template_ptr;
        }
        if (error_code == 20) {
            static_template_ptr = "ERROR: Unsupported bed temperature!";
            return static_template_ptr;
        }
        if (error_code == 99) {
            static_template_ptr = "ERROR: Unsupported function requested!";
            return static_template_ptr;
        }
        // default - no error shown
        static_template_buffer[0] = 0;
        return static_template_buffer;
    }
    if (key == 'E') {
        // ---current mesh data---
        return mesh_matrix;
    }
    if (key == 'F') {
        // ---average mesh data---
        apply_precision(mesh_average, precision);
        mesh_matrix_export(mesh_average, mesh_grid);
        return mesh_matrix;
    }
    if (key == 'A') {
        // precision
        char *precision_str = get_key_value(leveling_config, "precision", "0.01");
        double precision = atof(precision_str);
        sprintf(static_template_buffer, "%g", precision);
        return static_template_buffer;
    }
    if (key == 'H') {
        // used profile
        char *profile_str = get_key_value(leveling_config, "used_profile", "1");
        used_profile = atoi(profile_str);
        if ((used_profile < 1) || (used_profile > 9))
            used_profile = 1;
        sprintf(static_template_buffer, "%d", used_profile);
        return static_template_buffer;
    }
    if (key == 'Z') {
        // Z-offset
        sprintf(static_template_buffer, " Z-offset: %+g mm, Grid size used: %d, Grid size next: %d ", z_offset, mesh_grid, probe_count_x);
        return static_template_buffer;
    }
    if (key == 'B') {
        // grid size
        if (new_grid_size != 0) {
            // new grid size was set
            sprintf(static_template_buffer, "%d", new_grid_size);
        } else {
            if (mesh_grid != 0) {
                // current leveling grid size exists
                sprintf(static_template_buffer, "%d", mesh_grid);
            } else {
                // next leveling grid size
                sprintf(static_template_buffer, "%d", probe_count_x);
            }
        }
        return static_template_buffer;
    }
    if (key == 'C') {
        // next free slot
        int next_free = next_free_data_slot();
        sprintf(static_template_buffer, "%d", next_free);
        return static_template_buffer;
    }
    if (key == 'X') {
        // the number of slots to average
        calculate_mesh_average(mesh_grid);
        sprintf(static_template_buffer, "%d", mesh_accumulations);
        return static_template_buffer;
    }
    if (key == 'T') {
        // current bed mesh temperature
        sprintf(static_template_buffer, "%d", bed_temp);
        return static_template_buffer;
    }
    if (key == 'D') {
        // slot to clear
        static_template_buffer[0] = 0;
        return static_template_buffer;
    }
    // default - empty string
    static_template_buffer[0] = 0;
    return static_template_buffer;
}

// convert a template to a result file in the help of template_element callback function
// "{x}" is the template element "x", one char that will be expanded to the returned string
// NOTE: just one template key per line is allowed!
int populate_template_file(const char *src_file, const char *dst_file, char *(*template_element)(char key)) {
    FILE *ifile;
    FILE *ofile;
    char *b = NULL;
    size_t len = 0;
    ssize_t read;
    int i, n;
    char key;
    int key_beg;
    int key_expanded_size;
    char *key_expanded_value;

    ifile = fopen(src_file, "r");
    if (ifile) {
        ofile = fopen(dst_file, "w");
        if (ofile) {
            while (1) {
                read = getline(&b, &len, ifile);
                if (read == -1) {
                    break;
                }
                // line size
                n = strlen(b);
                if (n == 0)
                    continue;

                // line processing...
                key_beg = -1;
                for (i = 0; i < n; i++) {
                    if ((b[i] == '{') && (b[i + 2] == '}')) {
                        key_beg = i;
                        key = b[i + 1];
                        break;
                    }
                }
                if (key_beg == -1) {
                    // no template key found in this line, copy the line to the output
                    fwrite(b, 1, n, ofile);
                } else {
                    // template key was found, so expand the key
                    if (key_beg > 0) {
                        // constant prefix found
                        fwrite(b, 1, key_beg, ofile);
                    }
                    key_expanded_value = template_element(key);
                    key_expanded_size = strlen(key_expanded_value);
                    fwrite(key_expanded_value, 1, key_expanded_size, ofile);
                    if (n > (key_beg + 3)) {
                        fwrite(&b[key_beg + 3], 1, n - key_beg - 3, ofile);
                    }
                }
            }
            fclose(ifile);
            fflush(ofile);
            fclose(ofile);
            if (b)
                free(b);
            return 0;
        } else {
            fclose(ifile);
            return 2;
        }
    }
    return 1;
}

// replace the value of a given parameter name for the provided config file
// it will use a temporary file
// return 0 for success or the error code
int update_printer_config_file(const char *config_file, const char *parameter_name, const char *replacement_value) {
    FILE *ifile;
    FILE *ofile;
    char *b = NULL;
    size_t len = 0;
    ssize_t read;
    int i, n;
    char par[128];
    int par_size;
    char eol[2];

    // make a copy of the original config file
    custom_copy_file(config_file, "/user/printer-config.bak", "wb", NULL);

    sprintf(par, "%s : ", parameter_name);
    par_size = strlen(par);
    eol[0] = '\n';
    eol[1] = 0;

    ifile = fopen(config_file, "r");
    if (ifile) {
        ofile = fopen("/user/printer-config.tmp", "w");
        if (ofile) {
            while (1) {
                read = getline(&b, &len, ifile);
                if (read == -1) {
                    break;
                }
                // line size
                n = strlen(b);
                if (n == 0)
                    continue;

                // line processing...
                if (strncmp(b, par, par_size) == 0) {
                    // this is the line with the requested parameter, modify it
                    fwrite(par, 1, par_size, ofile);
                    fwrite(replacement_value, 1, strlen(replacement_value), ofile);
                    fwrite(eol, 1, 1, ofile);
                } else {
                    // not match, copy this entire line to the output file
                    fwrite(b, 1, n, ofile);
                }
            }
            fclose(ifile);
            fflush(ofile);
            fclose(ofile);
            if (b)
                free(b);
            // move the temp file to the original config file
            remove(config_file);
            custom_copy_file("/user/printer-config.tmp", config_file, "wb", NULL);
            remove("/user/printer-config.tmp");
            return 0;
        } else {
            fclose(ifile);
            return 2;
        }
    }
    return 1;
}

//==============================================================================================================================
// CUSTOM PAGES
void process_custom_pages(char *filename_str, struct REQUEST *req) {
    // parse the query
    config_option_t query, co;
    query = read_config_file_from_get_request(req->query);

    // parse the configuration file if not already done
    if (!leveling_config) {
        leveling_config = read_config_file("/user/webfs/parameters.cfg");
        // get the most important parameters once
        char *precision_str = get_key_value(leveling_config, "precision", "0.01");
        precision = atof(precision_str);
        if ((precision < 0.0001) || (precision > 0.1)) {
            precision = 0.01;
        }
    }

    if (debug) {
        fprintf(stderr, "+++ requested file: %s\n", filename_str);
    }

    if ((strstr(filename_str, "/mnt/UDISK/webfs/files/"))) {
        // turn off the cache for the files folder
        req->cache_turn_off = 'Y';
    }

    if ((strstr(filename_str, "/mnt/UDISK/webfs/api/"))) {
        // turn off the cache for the api folder
        req->cache_turn_off = 'Y';
    }

    // ----------------------------- access to the 3d visualizer index.html -----------------------------
    if ((!strcmp(filename_str, "/mnt/UDISK/webfs/mesh/index.html"))) {
        // turn off the cache
        req->cache_turn_off = 'Y';

        int rr = read_mesh_from_printer_config();

        if (rr == 1) {
            custom_copy_file(NULL, "/mnt/UDISK/webfs/mesh/index.html", "wb", "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\"><title>3D-Visualizer</title></head><body>Missing printer configuration file! Try to upload it from a backup.</body></html>");
        } else {
            if (rr == 2) {
                custom_copy_file(NULL, "/mnt/UDISK/webfs/mesh/index.html", "wb", "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\"><title>3D-Visualizer</title></head><body>Missing configuration data! Level the bed first!</body></html>");
            } else {
                int rrr = -1;

                // check the requested action
                char *source = get_key_value(query, "source", "config");
                if (!strcmp(source, "average")) {
                    // calculate the average and export it in mesh_matrix
                    calculate_mesh_average(mesh_grid);
                    apply_precision(mesh_average, precision);
                    mesh_matrix_export(mesh_average, mesh_grid);
                } else {
                    if (strcmp(source, "config")) {
                        // maybe a data slot number?
                        int slot = atoi(source);
                        if ((slot >= 1) && (slot < MAX_DATA_SLOTS)) {
                            rrr = export_selected_slot(slot);
                        }
                    }
                }
                if (rrr == 0) {
                    custom_copy_file(NULL, "/mnt/UDISK/webfs/mesh/index.html", "wb", "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\"><title>3D-Visualizer</title></head><body>Selected Data Slot is empty! Select another Data Slot!</body></html>");
                } else {
                    remove("/mnt/UDISK/webfs/mesh/index.html");
                    custom_copy_file("/opt/webfs/mesh/index1.html", "/mnt/UDISK/webfs/mesh/index.tmp", "wb", mesh_matrix);
                    custom_copy_file("/opt/webfs/mesh/index2.html", "/mnt/UDISK/webfs/mesh/index.tmp", "ab", NULL);
                    rename("/mnt/UDISK/webfs/mesh/index.tmp", "/mnt/UDISK/webfs/mesh/index.html");
                }
            }
        }
        goto e_x_i_t;
    }

    // ----------------------------- access to the cam.jpg file -----------------------------
    if ((!strcmp(filename_str, "/mnt/UDISK/webfs/webcam/cam.jpg"))) {
        // turn off the cache
        req->cache_turn_off = 'Y';

        // capture one frame to the file "/mnt/UDISK/webfs/webcam/cam.jpg"
        remove("/mnt/UDISK/webfs/webcam/cam.jpg");
        int result = v_capture_image("/mnt/UDISK/webfs/webcam/cam.tmp", 1);
        if (!result) {
            // errors, use the default image
            custom_copy_file("/mnt/UDISK/webfs/webcam/default.jpg", "/mnt/UDISK/webfs/webcam/cam.tmp", "wb", NULL);
        }
        rename("/mnt/UDISK/webfs/webcam/cam.tmp", "/mnt/UDISK/webfs/webcam/cam.jpg");
        goto e_x_i_t;
    }

    // ----------------------------- access to the api.json file ----------------------------
    if ((!strcmp(filename_str, "/mnt/UDISK/webfs/api/info.json"))) {
        // turn off the cache
        req->cache_turn_off = 'Y';

        update_api();
        goto e_x_i_t;
    }

    // ----------------------------- access to the do.json file -----------------------------
    if ((!strcmp(filename_str, "/mnt/UDISK/webfs/api/do.json"))) {
        // turn off the cache
        req->cache_turn_off = 'Y';

        control_api(query);
        goto e_x_i_t;
    }

    // ----------------------------- access to the tools index.html -------------------------
    if ((!strcmp(filename_str, "/mnt/UDISK/webfs/tools/index.html"))) {
        response_code = 0;
        error_code = 0;

        // turn off the cache
        req->cache_turn_off = 'Y';

        // calculate all needed data for the template
        int rr = read_mesh_from_printer_config();
        if (rr) {
            // missing config or missing mesh data:
            error_code = rr + 1;
        } else {
            if ((probe_count_x != probe_count_y) || (x_count != y_count)) {
                // not square grid setup
                error_code = 4;
            } else {
                if (mesh_grid != probe_count_x) {
                    // mesh grid different from probe grid
                    error_code = 5;
                }
            }
        }
        // calculate the average and export it in mesh_matrix
        // calculate_mesh_average(mesh_grid);
        // apply_precision(mesh_average, precision);
        // mesh_matrix_export(mesh_average, mesh_grid);

        // Fill the index.html template
        remove("/mnt/UDISK/webfs/tools/index.html");
        int rrr = populate_template_file("/opt/webfs/tools/index.html", "/mnt/UDISK/webfs/tools/index.tmp", leveling_template_callback);
        rename("/mnt/UDISK/webfs/tools/index.tmp", "/mnt/UDISK/webfs/tools/index.html");
        goto e_x_i_t;
    }

    // process all actions from /leveling/index.html and go back to the index.html
    if ((!strcmp(filename_str, "/mnt/UDISK/webfs/tools/response.html"))) {
        // set no error and no information messages
        response_code = 0;
        error_code = 0;

        // turn off the cache
        req->cache_turn_off = 'Y';

        // check the requested action first
        char *action = get_key_value(query, "action", "unknown");
        if (!strcmp(action, "unknown")) {
            // unknown action
            error_code = 99;
        }
        if (!strcmp(action, "reboot")) {
            // reboot
            system("sync && reboot &");
            response_code = 1;
        }

        if (!strcmp(action, "ssh_status")) {
            // ssh status
            if (file_exists("/opt/etc/init.d/S51dropbear")) {
                system_with_output("/opt/etc/init.d/S51dropbear status 2>&1", 1);
                response_code = 7;
            } else {
                error_code = 13;
            }
        }
        if (!strcmp(action, "ssh_start")) {
            // ssh start
            if (file_exists("/opt/etc/init.d/S51dropbear")) {
                system_with_output("/opt/etc/init.d/S51dropbear start 2>&1", 1);
                response_code = 8;
            } else {
                error_code = 13;
            }
        }
        if (!strcmp(action, "ssh_stop")) {
            // ssh stop
            if (file_exists("/opt/etc/init.d/S51dropbear")) {
                system_with_output("/opt/etc/init.d/S51dropbear stop 2>&1", 1);
                response_code = 8;
            } else {
                error_code = 13;
            }
        }

        else if (!strcmp(action, "clear_all")) {
            // clear all slots
            int i;
            char fn_buf[64];
            for (i = 1; i < MAX_DATA_SLOTS; i++) {
                sprintf(fn_buf, "/user/webfs/data_slot_%d.txt", i);
                remove(fn_buf);
            }
            response_code = 2;
        } else if (!strcmp(action, "clear_slot")) {
            char *data_slot = get_key_value(query, "data_slot", "0");
            int data_slot_int = atoi(data_slot);
            if ((data_slot_int > 0) && (data_slot_int < MAX_DATA_SLOTS)) {
                char fn_buf[64];
                sprintf(fn_buf, "/user/webfs/data_slot_%d.txt", data_slot_int);
                remove(fn_buf);
                response_code = 3;
            } else {
                error_code = 9;
            }
        } else if (!strcmp(action, "save_mesh")) {
            int rrrr = read_mesh_from_printer_config();
            if (rrrr) {
                // missing config or mesh data
                error_code = rrrr + 1;
            } else {
                char *selected_slot = get_key_value(query, "selected_slot", "0");
                int selected_slot_int = atoi(selected_slot);
                if ((selected_slot_int > 0) && (selected_slot_int < MAX_DATA_SLOTS)) {
                    char fn_buf[64];
                    sprintf(fn_buf, "/user/webfs/data_slot_%d.txt", selected_slot_int);
                    custom_copy_file(NULL, fn_buf, "wb", mesh_config);
                    response_code = 4;
                } else {
                    error_code = 1;
                }
            }
        } else if (!strcmp(action, "save_profile")) {
            char *profile = get_key_value(query, "profile", "0");
            int profile_int = atoi(profile);
            if ((profile_int >= 1) && (profile_int <= 9)) {
                char dst_file[64];
                const char *config_file;
                const char *config_path;
                if (!detect_printer_defaults(NULL, &config_path, &config_file, NULL)) {
                    // save printer*cfg
                    sprintf(dst_file, "/user/webfs/profiles/%d/%s", profile_int, config_file);
                    int r1 = custom_copy_file(config_path, dst_file, "wb", NULL);
                    // save unmodifiable.cfg
                    sprintf(dst_file, "/user/webfs/profiles/%d/unmodifiable.cfg", profile_int);
                    int r2 = custom_copy_file("/user/unmodifiable.cfg", dst_file, "wb", NULL);
                    if ((r1 == 0) && (r2 == 0)) {
                        saved_profile = profile_int;
                        response_code = 9;
                    } else {
                        error_code = 17;  // error while saving the profile
                    }
                } else {
                    error_code = 15;  // cannot detect printer configuration
                }
            } else {
                error_code = 14;  // invalid profile number
            }
        } else if (!strcmp(action, "use_profile")) {
            char *profile = get_key_value(query, "profile", "0");
            int profile_int = atoi(profile);
            if ((profile_int >= 1) && (profile_int <= 9)) {
                char *profile_str = get_key_value(leveling_config, "used_profile", "1");
                used_profile = atoi(profile_str);
                if ((used_profile < 1) || (used_profile > 9))
                    used_profile = 1;
                if (profile_int != used_profile) {
                    char src_file1[64];
                    char src_file2[64];
                    const char *config_file;
                    const char *config_path;
                    if (!detect_printer_defaults(NULL, &config_path, &config_file, NULL)) {
                        sprintf(src_file1, "/user/webfs/profiles/%d/%s", profile_int, config_file);
                        sprintf(src_file2, "/user/webfs/profiles/%d/unmodifiable.cfg", profile_int);
                        if ((file_exists(src_file1)) && (file_exists(src_file2))) {
                            // load printer*cfg
                            int r1 = custom_copy_file(src_file1, config_path, "wb", NULL);
                            // load unmodifiable.cfg
                            int r2 = custom_copy_file(src_file2, "/user/unmodifiable.cfg", "wb", NULL);
                            if ((r1 == 0) && (r2 == 0)) {
                                leveling_config = set_key_value(leveling_config, "used_profile", profile);
                                write_config_file("/user/webfs/parameters.cfg", leveling_config);
                                saved_profile = profile_int;
                                used_profile = profile_int;
                                response_code = 10;
                            } else {
                                error_code = 19;  // cannot use this profile
                            }
                        } else {
                            error_code = 18;  // cannot read this profile
                        }
                    } else {
                        error_code = 15;  // cannot detect printer configuration
                    }
                } else {
                    error_code = 16;  // the same profile
                }
            } else {
                error_code = 14;  // invalid profile number
            }
        } else if (!strcmp(action, "set_average")) {
            // set the average
            int rrrr = read_mesh_from_printer_config();
            if (rrrr) {
                // missing config or mesh data
                error_code = rrrr + 1;
            } else {
                if ((mesh_grid >= MIN_SUPPORTED_GRID_SIZE) && (mesh_grid <= MAX_SUPPORTED_GRID_SIZE)) {
                    const char *config_file;
                    if (!detect_printer_defaults(NULL, &config_file, NULL, NULL)) {
                        int avg_slots = calculate_mesh_average(mesh_grid);
                        if (avg_slots > 0) {
                            apply_precision(mesh_average, precision);
                            mesh_average_export(mesh_grid, mesh_matrix);
                            update_printer_config_file(config_file, "points", mesh_matrix);
                            response_code = 5;
                        } else {
                            error_code = 8;
                        }
                    } else {
                        error_code = 15;  // cannot detect printer configuration
                    }
                }
            }
        } else if (!strcmp(action, "set_parameters")) {
            read_mesh_from_printer_config();
            char *precision_str = get_key_value(query, "precision", "0.01");
            double precision_float = atof(precision_str);
            char *grid = get_key_value(query, "grid", "0");
            int grid_int = atoi(grid);
            char *bed_temp_str = get_key_value(query, "bed_temp", "60");
            int bed_temp_int = atoi(bed_temp_str);
            if ((bed_temp_int >= 0) && (bed_temp_int <= 90)) {
                if ((grid_int >= MIN_SUPPORTED_GRID_SIZE) && (grid_int <= MAX_SUPPORTED_GRID_SIZE)) {
                    if ((precision_float >= 0.0001) && (precision_float <= 0.1)) {
                        if (bed_temp_int != bed_temp) {
                            const char *config_file;
                            if (!detect_printer_defaults(NULL, &config_file, NULL, NULL)) {
                                char replacement_value[8];
                                sprintf(replacement_value, "%d", bed_temp_int);
                                update_printer_config_file(config_file, "bed_mesh_temp", replacement_value);
                                bed_temp = bed_temp_int;
                                response_code = 11;
                            } else {
                                response_code = 0;
                                error_code = 15;  // cannot detect printer configuration
                            }
                        }
                        if (precision_float != precision) {
                            precision = precision_float;
                            leveling_config = set_key_value(leveling_config, "precision", precision_str);
                            write_config_file("/user/webfs/parameters.cfg", leveling_config);
                            response_code = 6;
                        }
                        if (new_grid_size != grid_int) {
                            // set the "probe_count : grid,grid"
                            const char *config_file;
                            if (!detect_printer_defaults(NULL, &config_file, NULL, NULL)) {
                                char replacement_value[8];
                                sprintf(replacement_value, "%d,%d", grid_int, grid_int);
                                update_printer_config_file(config_file, "probe_count", replacement_value);
                                new_grid_size = grid_int;
                                error_code = 6;
                                response_code = 0;
                            } else {
                                response_code = 0;
                                error_code = 15;  // cannot detect printer configuration
                            }
                        }
                        if ((response_code == 0) && (error_code == 0)) {
                            // no changes
                            response_code = 0;
                            error_code = 12;
                        }
                    } else {
                        // unsupported precision
                        response_code = 0;
                        error_code = 11;
                    }
                } else {
                    // unsupported grid size
                    response_code = 0;
                    error_code = 10;
                }
            } else {
                // unsupported bed temperature
                response_code = 0;
                error_code = 20;
            }
        }

        remove("/mnt/UDISK/webfs/tools/response.html");
        int rrr = populate_template_file("/opt/webfs/tools/response.html", "/mnt/UDISK/webfs/tools/response.tmp", leveling_template_callback);
        rename("/mnt/UDISK/webfs/tools/response.tmp", "/mnt/UDISK/webfs/tools/response.html");
        goto e_x_i_t;
    }

e_x_i_t:

    // free the config file, will keep it forever in memory
    // free_config_file(leveling_config);

    // free the query
    free_config_file(query);
}
// CUSTOM PAGES
//==============================================================================================================================

void read_request(struct REQUEST *req, int pipelined) {
    int rc;
    char *h;

restart:

    rc = read(req->fd, req->hreq + req->hdata, MAX_HEADER - req->hdata);
    switch (rc) {
        case -1:
            if (errno == EAGAIN) {
                if (pipelined)
                    break; /* check if there is already a full request */
                else
                    return;
            }
            if (errno == EINTR)
                goto restart;
            xperror(LOG_INFO, "read", req->peerhost);
            /* fall through */
        case 0:
            req->state = STATE_CLOSE;
            return;
        default:
            req->hdata += rc;
            req->hreq[req->hdata] = 0;
    }

    /* check if this looks like a http request after
             the first few bytes... */
    if (req->hdata < 5)
        return;
    if (strncmp(req->hreq, "GET ", 4) != 0 &&
        strncmp(req->hreq, "PUT ", 4) != 0 &&
        strncmp(req->hreq, "HEAD ", 5) != 0 &&
        strncmp(req->hreq, "POST ", 5) != 0) {
        mkerror(req, 400, 0);
        return;
    }

    /* header complete ?? */
    if (NULL != (h = strstr(req->hreq, "\r\n\r\n")) ||
        NULL != (h = strstr(req->hreq, "\n\n"))) {
        if (*h == '\r') {
            h += 4;
            *(h - 2) = 0;
        } else {
            h += 2;
            *(h - 1) = 0;
        }
        req->lreq = h - req->hreq;
        req->state = STATE_PARSE_HEADER;
        return;
    }

    if (req->hdata == MAX_HEADER) {
        /* oops: buffer full, but found no complete request ... */
        mkerror(req, 400, 0);
        return;
    }
    return;
}

/* ---------------------------------------------------------------------- */

static off_t
parse_off_t(char *str, int *pos) {
    off_t value = 0;

    while (isdigit(str[*pos])) {
        value *= 10;
        value += str[*pos] - '0';
        (*pos)++;
    }
    return value;
}

static int
parse_ranges(struct REQUEST *req) {
    char *h, *line = req->range_hdr;
    int i, off;

    for (h = line, req->ranges = 1; *h != '\n' && *h != '\0'; h++)
        if (*h == ',')
            req->ranges++;
    if (debug)
        fprintf(stderr, "%03d: %d ranges:", req->fd, req->ranges);
    req->r_start = malloc(req->ranges * sizeof(off_t));
    req->r_end = malloc(req->ranges * sizeof(off_t));
    req->r_head = malloc((req->ranges + 1) * BR_HEADER);
    req->r_hlen = malloc((req->ranges + 1) * sizeof(int));
    if (NULL == req->r_start || NULL == req->r_end ||
        NULL == req->r_head || NULL == req->r_hlen) {
        if (req->r_start)
            free(req->r_start);
        if (req->r_end)
            free(req->r_end);
        if (req->r_head)
            free(req->r_head);
        if (req->r_hlen)
            free(req->r_hlen);
        if (debug)
            fprintf(stderr, "oom\n");
        return 500;
    }
    for (i = 0, off = 0; i < req->ranges; i++) {
        if (line[off] == '-') {
            off++;
            if (!isdigit(line[off]))
                goto parse_error;
            req->r_start[i] = req->bst.st_size - parse_off_t(line, &off);
            req->r_end[i] = req->bst.st_size;
        } else {
            if (!isdigit(line[off]))
                goto parse_error;
            req->r_start[i] = parse_off_t(line, &off);
            if (line[off] != '-')
                goto parse_error;
            off++;
            if (isdigit(line[off]))
                req->r_end[i] = parse_off_t(line, &off) + 1;
            else
                req->r_end[i] = req->bst.st_size;
        }
        off++; /* skip "," */
        /* ranges ok? */
        if (debug)
            fprintf(stderr, " %d-%d",
                    (int)(req->r_start[i]),
                    (int)(req->r_end[i]));
        if (req->r_start[i] > req->r_end[i] ||
            req->r_end[i] > req->bst.st_size)
            goto parse_error;
    }
    if (debug)
        fprintf(stderr, " ok\n");
    return 0;

parse_error:
    req->ranges = 0;
    if (debug)
        fprintf(stderr, " range error\n");
    return 400;
}

static int
unhex(unsigned char c) {
    if (c < '@')
        return c - '0';
    return (c & 0x0f) + 9;
}

/* handle %hex quoting, also split path / querystring */
static void
unquote(unsigned char *path, unsigned char *qs, unsigned char *src) {
    int q;
    unsigned char *dst;

    q = 0;
    dst = path;
    while (src[0] != 0) {
        if (!q && *src == '?') {
            q = 1;
            *dst = 0;
            dst = qs;
            src++;
            continue;
        }
        if (q && *src == '+') {
            *dst = ' ';
        } else if ((*src == '%') && isxdigit(src[1]) && isxdigit(src[2])) {
            *dst = (unhex(src[1]) << 4) | unhex(src[2]);
            src += 2;
        } else {
            *dst = *src;
        }
        dst++;
        src++;
    }
    *dst = 0;
}

/* delete unneeded path elements */
static void
fixpath(char *path) {
    char *dst = path;
    char *src = path;

    for (; *src;) {
        if (0 == strncmp(src, "//", 2)) {
            src++;
            continue;
        }
        if (0 == strncmp(src, "/./", 3)) {
            src += 2;
            continue;
        }
        *(dst++) = *(src++);
    }
    *dst = 0;
}

static int base64_table[] = {
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    62,
    -1,
    -1,
    -1,
    63,
    52,
    53,
    54,
    55,
    56,
    57,
    58,
    59,
    60,
    61,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    24,
    25,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    26,
    27,
    28,
    29,
    30,
    31,
    32,
    33,
    34,
    35,
    36,
    37,
    38,
    39,
    40,
    41,
    42,
    43,
    44,
    45,
    46,
    47,
    48,
    49,
    50,
    51,
    -1,
    -1,
    -1,
    -1,
    -1,
};

static void
decode_base64(unsigned char *dest, unsigned char *src, int maxlen) {
    int a, b, d;

    for (a = 0, b = 0, d = 0; *src != 0 && d < maxlen; src++) {
        if (*src >= 128 || -1 == base64_table[*src])
            break;
        a = (a << 6) | base64_table[*src];
        b += 6;
        if (b >= 8) {
            b -= 8;
            dest[d++] = (a >> b) & 0xff;
        }
    }
    dest[d] = 0;
}

static int sanity_checks(struct REQUEST *req) {
    int i;

    /* path: must start with a '/' */
    if (req->path[0] != '/') {
        mkerror(req, 400, 0);
        return -1;
    }

    /* path: must not contain "/../" */
    if (strstr(req->path, "/../")) {
        mkerror(req, 403, 1);
        return -1;
    }

    if (req->hostname[0] == '\0')
        /* no hostname specified */
        return 0;

    /* validate hostname */
    for (i = 0; req->hostname[i] != '\0'; i++) {
        switch (req->hostname[i]) {
            case 'A' ... 'Z':
                req->hostname[i] += 32; /* lowercase */
            case 'a' ... 'z':
            case '0' ... '9':
            case '-':
                /* these are fine as-is */
                break;
            case '.':
                /* some extra checks */
                if (0 == i) {
                    /* don't allow a dot as first character */
                    mkerror(req, 400, 0);
                    return -1;
                }
                if ('.' == req->hostname[i - 1]) {
                    /* don't allow two dots in sequence */
                    mkerror(req, 400, 0);
                    return -1;
                }
                break;
            default:
                /* invalid character */
                mkerror(req, 400, 0);
                return -1;
        }
    }
    return 0;
}

// ================================================================================================================

void parse_request(struct REQUEST *req) {
    char filename[MAX_PATH + 1], proto[MAX_MISC + 1], *h;
    int port, rc, len;
    struct passwd *pw = NULL;

    if (debug)
        fprintf(stderr, "%s\n", req->hreq);

    /* parse request. Here, scanf is powerful :-) */
    if (4 != sscanf(req->hreq,
                    "%" S(MAX_MISC) "[A-Z] "
                                    "%" S(MAX_PATH) "[^ \t\r\n] HTTP/%d.%d",
                    req->type, filename, &(req->major), &(req->minor))) {
        mkerror(req, 400, 0);
        return;
    }
    if (filename[0] == '/') {
        strncpy(req->uri, filename, sizeof(req->uri) - 1);
    } else {
        port = 0;
        *proto = 0;
        if (4 != sscanf(filename,
                        "%" S(MAX_MISC) "[a-zA-Z]://"
                                        "%" S(MAX_HOST) "[a-zA-Z0-9.-]:%d"
                                                        "%" S(MAX_PATH) "[^ \t\r\n]",
                        proto, req->hostname, &port, req->uri) &&
            3 != sscanf(filename,
                        "%" S(MAX_MISC) "[a-zA-Z]://"
                                        "%" S(MAX_HOST) "[a-zA-Z0-9.-]"
                                                        "%" S(MAX_PATH) "[^ \t\r\n]",
                        proto, req->hostname, req->uri)) {
            mkerror(req, 400, 0);
            return;
        }
        if (*proto != 0 && 0 != strcasecmp(proto, "http")) {
            mkerror(req, 400, 0);
            return;
        }
    }

    unquote(req->path, req->query, req->uri);
    fixpath(req->path);
    if (debug)
        fprintf(stderr, "%03d: %s \"%s\" HTTP/%d.%d\n",
                req->fd, req->type, req->path, req->major, req->minor);

    if (debug)
        fprintf(stderr, "query: \"%s\"\n", req->query);

    if (0 != strcmp(req->type, "GET") &&
        0 != strcmp(req->type, "HEAD")) {
        mkerror(req, 501, 0);
        return;
    }

    if (0 == strcmp(req->type, "HEAD")) {
        req->head_only = 1;
    }

    /* parse header lines */
    req->keep_alive = req->minor;
    for (h = req->hreq; h - req->hreq < req->lreq;) {
        h = strchr(h, '\n');
        if (NULL == h)
            break;
        h++;

        h[-2] = 0;
        h[-1] = 0;
        list_add(&req->header, h, 0);

        if (0 == strncasecmp(h, "Connection: ", 12)) {
            req->keep_alive = (0 == strncasecmp(h + 12, "Keep-Alive", 10));
        } else if (0 == strncasecmp(h, "Host: ", 6)) {
            if (2 != sscanf(h + 6, "%" S(MAX_HOST) "[a-zA-Z0-9.-]:%d",
                            req->hostname, &port))
                sscanf(h + 6, "%" S(MAX_HOST) "[a-zA-Z0-9.-]",
                       req->hostname);
        } else if (0 == strncasecmp(h, "If-Modified-Since: ", 19)) {
            req->if_modified = h + 19;
        } else if (0 == strncasecmp(h, "If-Unmodified-Since: ", 21)) {
            req->if_unmodified = h + 21;
        } else if (0 == strncasecmp(h, "If-Range: ", 10)) {
            req->if_range = h + 10;
        } else if (0 == strncasecmp(h, "Authorization: Basic ", 21)) {
            decode_base64(req->auth, h + 21, sizeof(req->auth) - 1);
            if (debug)
                fprintf(stderr, "%03d: auth: %s\n", req->fd, req->auth);
        } else if (0 == strncasecmp(h, "Range: bytes=", 13)) {
            /* parsing must be done after fstat, we need the file size
                     for the boundary checks */
            req->range_hdr = h + 13;
        }
    }
    if (debug) {
        if (req->if_modified)
            fprintf(stderr, "%03d: if-modified-since: \"%s\"\n",
                    req->fd, req->if_modified);
        if (req->if_unmodified)
            fprintf(stderr, "%03d: if-unmodified-since: \"%s\"\n",
                    req->fd, req->if_unmodified);
        if (req->if_range)
            fprintf(stderr, "%03d: if-range: \"%s\"\n",
                    req->fd, req->if_range);
    }

    /* take care about the hostname */
    if (virtualhosts) {
        if (req->hostname[0] == 0) {
            if (req->minor > 0) {
                /* HTTP/1.1 clients MUST specify a hostname */
                mkerror(req, 400, 0);
                return;
            }
            strncpy(req->hostname, server_host, sizeof(req->hostname) - 1);
        }
    } else {
        if (req->hostname[0] == '\0' || canonicalhost)
            strncpy(req->hostname, server_host, sizeof(req->hostname) - 1);
    }

    /* checks */
    if (0 != sanity_checks(req))
        return;

    /* check basic auth */
    if (NULL != userpass && 0 != strcmp(userpass, req->auth)) {
        mkerror(req, 401, 1);
        return;
    }

    /* build filename */
    if (userdir && '~' == req->path[1]) {
        /* expand user directories, i.e.
                 /~user/path/file => $HOME/public_html/path/file */
        h = strchr(req->path + 2, '/');
        if (NULL == h) {
            mkerror(req, 404, 1);
            return;
        }
        *h = 0;
        pw = getpwnam(req->path + 2);
        *h = '/';
        if (NULL == pw) {
            mkerror(req, 404, 1);
            return;
        }
        len = snprintf(filename, sizeof(filename) - 1,
                       "%s/%s/%s", pw->pw_dir, userdir, h + 1);
    } else {
        len = snprintf(filename, sizeof(filename) - 1,
                       "%s%s%s%s",
                       do_chroot ? "" : doc_root,
                       virtualhosts ? "/" : "",
                       virtualhosts ? req->hostname : "",
                       req->path);
    }

    req->cache_turn_off = 'N';

    // process the custom pages
    process_custom_pages(filename, req);

    h = filename + len - 1;
    if (*h == '/') {
        /* looks like the client asks for a directory */
        if (indexhtml) {
            /* check for index file */
            strncpy(h + 1, indexhtml, sizeof(filename) - len - 1);
            if (-1 != (req->bfd = open(filename, O_RDONLY))) {
                /* ok, we have one */
                close_on_exec(req->bfd);
                goto regular_file;
            } else {
                if (errno == ENOENT) {
                    /* no such file or directory => listing */
                    h[1] = '\0';
                } else {
                    mkerror(req, 403, 1);
                    return;
                }
            }
        }

        if (no_listing) {
            mkerror(req, 403, 1);
            return;
        };

        if (-1 == stat(filename, &(req->bst))) {
            if (errno == EACCES) {
                mkerror(req, 403, 1);
            } else {
                mkerror(req, 404, 1);
            }
            return;
        }
        strftime(req->mtime, sizeof(req->mtime), RFC1123, gmtime(&req->bst.st_mtime));
        req->mime = "text/html";
        req->dir = get_dir(req, filename);
        if (NULL == req->body) {
            /* We arrive here if opendir failed, probably due to -EPERM
             * It does exist (see the stat() call above) */
            mkerror(req, 403, 1);
            return;
        } else if (NULL != req->if_modified &&
                   0 == strcmp(req->if_modified, req->mtime)) {
            /* 304 not modified */
            mkheader(req, 304);
            req->head_only = 1;
        } else {
            /* 200 OK */
            mkheader(req, 200);
        }
        return;
    }

    /* it is /probably/ a regular file */
    if (-1 == (req->bfd = open(filename, O_RDONLY))) {
        if (errno == EACCES) {
            mkerror(req, 403, 1);
        } else {
            mkerror(req, 404, 1);
        }
        return;
    }

regular_file:

    fstat(req->bfd, &(req->bst));
    if (req->range_hdr)
        if (0 != (rc = parse_ranges(req))) {
            mkerror(req, rc, 1);
            return;
        }

    if (!S_ISREG(req->bst.st_mode)) {
        /* /not/ a regular file */
        close(req->bfd);
        req->bfd = -1;
        if (S_ISDIR(req->bst.st_mode)) {
            /* oops: a directory without trailing slash */
            strcat(req->path, "/");
            mkredirect(req);
        } else {
            /* anything else is'nt allowed here */
            mkerror(req, 403, 1);
        }
        return;
    }

    /* it is /really/ a regular file */

    req->mime = get_mime(filename);

    //  current time
    time_t curtime;
    time(&curtime);
    if (req->cache_turn_off == 'Y') {
        strftime(req->mtime, sizeof(req->mtime), RFC1123, localtime(&curtime));
    } else {
        strftime(req->mtime, sizeof(req->mtime), RFC1123, gmtime(&req->bst.st_mtime));
    }
    strftime(req->ctime, sizeof(req->mtime), RFC1123, localtime(&curtime));
    if (NULL != req->if_range && 0 != strcmp(req->if_range, req->mtime))
        /* mtime mismatch -> no ranges */
        req->ranges = 0;
    if (NULL != req->if_unmodified && 0 != strcmp(req->if_unmodified, req->mtime)) {
        /* 412 precondition failed */
        mkerror(req, 412, 1);
    } else if (NULL != req->if_modified && 0 == strcmp(req->if_modified, req->mtime)) {
        /* 304 not modified */
        mkheader(req, 304);
        req->head_only = 1;
    } else if (req->ranges > 0) {
        /* send byte range(s) */
        mkheader(req, 206);
    } else {
        /* normal */
        mkheader(req, 200);
    }
    return;
}
