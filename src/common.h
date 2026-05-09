#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define MAX_STATIONS 1000000
#define MAX_STATION_NAME 50
#define MAX_LINE_LENGTH 500
#define MAX_READINGS 5000000

/*
 * Estructura para almacenar datos de una lectura meteorológica
 */
typedef struct {
    char station[MAX_STATION_NAME];
    float tmax;  // Temperatura máxima
} Reading;

/*
 * Estructura para almacenar resultados agregados por estación
 */
typedef struct {
    char station[MAX_STATION_NAME];
    float sum_temps;
    int count;
    float avg_temp;
} StationData;

/*
 * Lee un archivo CSV en formato NOAA y retorna las lecturas
 * Formato esperado: STATION,TMAX,... (CSV estándar)
 * 
 * Parámetros:
 *   filename: ruta del archivo CSV
 *   readings: array donde se almacenarán las lecturas
 *   max_readings: capacidad máxima del array
 * 
 * Retorna: número de lecturas leídas, -1 si hay error
 */
int read_csv_file(const char *filename, Reading *readings, int max_readings) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: No se puede abrir el archivo '%s'\n", filename);
        return -1;
    }

    int count = 0;
    char line[MAX_LINE_LENGTH];
    
    // Saltar encabezado
    if (fgets(line, MAX_LINE_LENGTH, file) == NULL) {
        fprintf(stderr, "Error: Archivo vacío\n");
        fclose(file);
        return -1;
    }

    // Leer datos
    while (fgets(line, MAX_LINE_LENGTH, file) != NULL && count < max_readings) {
        // Remover salto de línea
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }

        // Parsear línea: STATION,TMAX (formato simple)
        char *saveptr = NULL;
        char *token = strtok_r(line, ",", &saveptr);
        
        if (token == NULL) continue;
        strncpy(readings[count].station, token, MAX_STATION_NAME - 1);
        readings[count].station[MAX_STATION_NAME - 1] = '\0';

        // Obtener TMAX (segunda columna)
        token = strtok_r(NULL, ",", &saveptr);
        if (token == NULL) {
            fprintf(stderr, "Advertencia: Línea mal formada: %s\n", line);
            continue;
        }

        readings[count].tmax = atof(token);
        count++;
    }

    fclose(file);
    return count;
}

/*
 * Calcula promedios de temperaturas por estación
 * 
 * Parámetros:
 *   readings: array de lecturas
 *   num_readings: número de lecturas
 *   stations: array de salida con datos por estación
 * 
 * Retorna: número de estaciones únicas encontradas
 */
int calculate_averages(Reading *readings, int num_readings, StationData *stations) {
    if (num_readings <= 0) {
        return 0;
    }

    int num_stations = 0;

    // Procesar cada lectura
    for (int i = 0; i < num_readings; i++) {
        // Buscar si la estación ya existe
        int station_idx = -1;
        for (int j = 0; j < num_stations; j++) {
            if (strcmp(stations[j].station, readings[i].station) == 0) {
                station_idx = j;
                break;
            }
        }

        if (station_idx == -1) {
            // Nueva estación
            if (num_stations >= MAX_STATIONS) {
                fprintf(stderr, "Advertencia: Se alcanzó el máximo de estaciones (%d)\n", MAX_STATIONS);
                break;
            }
            strncpy(stations[num_stations].station, readings[i].station, MAX_STATION_NAME - 1);
            stations[num_stations].station[MAX_STATION_NAME - 1] = '\0';
            stations[num_stations].sum_temps = readings[i].tmax;
            stations[num_stations].count = 1;
            stations[num_stations].avg_temp = readings[i].tmax;
            num_stations++;
        } else {
            // Actualizar estación existente
            stations[station_idx].sum_temps += readings[i].tmax;
            stations[station_idx].count++;
            stations[station_idx].avg_temp = stations[station_idx].sum_temps / stations[station_idx].count;
        }
    }

    return num_stations;
}

/*
 * Imprime resultados en consola
 * 
 * Parámetros:
 *   stations: array de estaciones con resultados
 *   num_stations: número de estaciones
 *   limit: máximo número de estaciones a mostrar (0 = todas)
 */
void print_results(StationData *stations, int num_stations, int limit) {
    if (limit == 0) limit = num_stations;
    if (limit > num_stations) limit = num_stations;

    printf("\n========== RESULTADOS ==========\n");
    printf("%-40s %12s %10s\n", "STATION", "AVG_TMAX", "COUNT");
    printf("%-40s %12s %10s\n", "--------", "--------", "-----");

    int total_lecturas = 0;
    for (int i = 0; i < num_stations; i++) {
        total_lecturas += stations[i].count;
    }

    for (int i = 0; i < limit; i++) {
        printf("%-40s %12.2f %10d\n", 
               stations[i].station, 
               stations[i].avg_temp, 
               stations[i].count);
    }

    printf("=================================\n");
    printf("Total de estaciones: %d\n", num_stations);
    printf("Total de lecturas: %d\n", total_lecturas);
}

/*
 * Obtiene tiempo en microsegundos (para benchmarking)
 */
double get_time_us() {
    #ifdef _WIN32
    // Windows: usar QueryPerformanceCounter
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (double)count.QuadPart * 1e6 / freq.QuadPart;
    #else
    // Linux/Unix: usar clock_gettime
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec / 1e3;
    #endif
}

#endif // COMMON_H
