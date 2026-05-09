#include "common.h"

/*
 * SOLUCIÓN SECUENCIAL
 * 
 * Analiza temperaturas máximas de estaciones meteorológicas NOAA
 * Calcula el promedio de temperaturas máximas por estación
 * 
 * Uso: seq <archivo_datos> [num_registros]
 * 
 * Ejemplo:
 *   seq data/sample_data.csv
 *   seq data/sample_data.csv 10000
 */

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <archivo_datos> [num_registros]\n", argv[0]);
        fprintf(stderr, "Ejemplo: %s data/sample_data.csv 10000\n", argv[0]);
        return 1;
    }

    const char *input_file = argv[1];
    int max_readings = MAX_READINGS;
    
    // Parámetro opcional: número máximo de registros a procesar
    if (argc > 2) {
        max_readings = atoi(argv[2]);
        if (max_readings <= 0) {
            fprintf(stderr, "Error: num_registros debe ser > 0\n");
            return 1;
        }
    }

    printf("========== VERSIÓN SECUENCIAL ==========\n");
    printf("Archivo: %s\n", input_file);
    printf("Max registros: %d\n\n", max_readings);

    // Asignar memoria para lecturas
    Reading *readings = (Reading *)malloc(max_readings * sizeof(Reading));
    if (!readings) {
        fprintf(stderr, "Error: No hay memoria para las lecturas\n");
        return 1;
    }

    // Asignar memoria para resultados
    StationData *stations = (StationData *)malloc(MAX_STATIONS * sizeof(StationData));
    if (!stations) {
        fprintf(stderr, "Error: No hay memoria para las estaciones\n");
        free(readings);
        return 1;
    }

    // Registrar tiempo de inicio
    double start_time = get_time_us();

    // Fase 1: Leer archivo CSV
    printf("[1/3] Leyendo archivo CSV...\n");
    int num_readings = read_csv_file(input_file, readings, max_readings);
    
    if (num_readings <= 0) {
        fprintf(stderr, "Error: No se pudieron leer datos del archivo\n");
        free(readings);
        free(stations);
        return 1;
    }
    
    printf("      ✓ Se leyeron %d registros\n\n", num_readings);

    // Fase 2: Calcular promedios
    printf("[2/3] Calculando promedios por estación...\n");
    int num_stations = calculate_averages(readings, num_readings, stations);
    printf("      ✓ Se encontraron %d estaciones únicas\n\n", num_stations);

    // Registrar tiempo final
    double end_time = get_time_us();
    double elapsed_time_us = end_time - start_time;
    double elapsed_time_ms = elapsed_time_us / 1000.0;
    double elapsed_time_s = elapsed_time_ms / 1000.0;

    // Fase 3: Mostrar resultados
    printf("[3/3] Mostrando resultados...\n");
    print_results(stations, num_stations, 0);  // Mostrar todas

    // Mostrar estadísticas de rendimiento
    printf("\n========== ESTADÍSTICAS DE RENDIMIENTO SECUENCIAL ==========\n");
    printf("Tiempo total: %.3f ms (%.6f s)\n", elapsed_time_ms, elapsed_time_s);
    printf("Registros procesados: %d\n", num_readings);
    printf("Estaciones únicas: %d\n", num_stations);
    printf("Tiempo por registro: %.3f µs\n", elapsed_time_us / num_readings);
    printf("Throughput: %.2f registros/ms\n", (double)num_readings / elapsed_time_ms);
    printf("==============================================\n");

    // Liberar memoria
    free(readings);
    free(stations);

    return 0;
}
