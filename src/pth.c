#include "common.h"
#include <pthread.h>

#ifndef MAX_THREADS
#define MAX_THREADS 64
#endif

// Estructura para pasar datos a cada hilo
typedef struct {
    int thread_id;
    Reading *readings;
    int start_idx;
    int end_idx;
    StationData *local_stations;
    int local_num_stations;
} ThreadData;

// Función que ejecutará cada hilo
void *worker_function(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    
    // Cada hilo calcula los promedios de su porción de datos
    data->local_num_stations = calculate_averages(
        data->readings + data->start_idx,
        data->end_idx - data->start_idx,
        data->local_stations
    );
    
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <archivo_datos> [num_registros] [num_hilos]\n", argv[0]);
        fprintf(stderr, "Ejemplo: %s data/sample_data.csv 4000000 4\n", argv[0]);
        return 1;
    }

    const char *input_file = argv[1];
    int max_readings = MAX_READINGS;
    int num_threads = 4; // Por defecto 4 hilos
    
    // Parámetro opcional: número máximo de registros a procesar
    if (argc > 2) {
        max_readings = atoi(argv[2]);
        if (max_readings <= 0) {
            fprintf(stderr, "Error: num_registros debe ser > 0\n");
            return 1;
        }
    }
    
    // Parámetro opcional: número de hilos
    if (argc > 3) {
        num_threads = atoi(argv[3]);
        if (num_threads <= 0 || num_threads > MAX_THREADS) {
            fprintf(stderr, "Error: num_hilos debe estar entre 1 y %d\n", MAX_THREADS);
            return 1;
        }
    }

    printf("========== VERSIÓN POSIX THREADS ==========\n");
    printf("Archivo: %s\n", input_file);
    printf("Max registros: %d\n", max_readings);
    printf("Número de hilos: %d\n\n", num_threads);

    // Asignar memoria para lecturas
    Reading *readings = (Reading *)malloc(max_readings * sizeof(Reading));
    if (!readings) {
        fprintf(stderr, "Error: No hay memoria para las lecturas\n");
        return 1;
    }

    // Asignar memoria para resultados finales
    StationData *final_stations = (StationData *)malloc(MAX_STATIONS * sizeof(StationData));
    if (!final_stations) {
        fprintf(stderr, "Error: No hay memoria para las estaciones\n");
        free(readings);
        return 1;
    }

    // Fase 1: Leer archivo CSV
    printf("[1/3] Leyendo archivo CSV...\n");
    int num_readings = read_csv_file(input_file, readings, max_readings);
    
    if (num_readings <= 0) {
        fprintf(stderr, "Error: No se pudieron leer datos del archivo\n");
        free(readings);
        free(final_stations);
        return 1;
    }
    
    printf("      ✓ Se leyeron %d registros\n\n", num_readings);

    // Registrar tiempo de inicio de cómputo (excluyendo la lectura I/O por ser común)
    double start_time = get_time_us();

    // Fase 2: Calcular promedios
    printf("[2/3] Calculando promedios por estación con %d hilos...\n", num_threads);
    
    pthread_t threads[MAX_THREADS];
    ThreadData thread_data[MAX_THREADS];
    
    // Distribuir carga equitativamente
    int chunk_size = num_readings / num_threads;
    int remainder = num_readings % num_threads;
    int current_idx = 0;

    for (int i = 0; i < num_threads; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].readings = readings;
        thread_data[i].start_idx = current_idx;
        
        // Repartir el resto entre los primeros hilos
        int current_chunk = chunk_size + (i < remainder ? 1 : 0);
        thread_data[i].end_idx = current_idx + current_chunk;
        current_idx += current_chunk;
        
        // Cada hilo tiene su propio array de estaciones locales
        thread_data[i].local_stations = (StationData *)malloc(MAX_STATIONS * sizeof(StationData));
        
        pthread_create(&threads[i], NULL, worker_function, &thread_data[i]);
    }

    // Esperar a que terminen los hilos
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    // Fase 2.5: Fusionar resultados (Reducción en hilo principal)
    int final_num_stations = 0;
    
    for (int i = 0; i < num_threads; i++) {
        for (int j = 0; j < thread_data[i].local_num_stations; j++) {
            StationData *local_st = &thread_data[i].local_stations[j];
            int found = 0;
            
            // Buscar estación en resultados finales
            for (int k = 0; k < final_num_stations; k++) {
                if (strcmp(final_stations[k].station, local_st->station) == 0) {
                    final_stations[k].sum_temps += local_st->sum_temps;
                    final_stations[k].count += local_st->count;
                    found = 1;
                    break;
                }
            }
            
            // Agregar si es nueva
            if (!found && final_num_stations < MAX_STATIONS) {
                strcpy(final_stations[final_num_stations].station, local_st->station);
                final_stations[final_num_stations].sum_temps = local_st->sum_temps;
                final_stations[final_num_stations].count = local_st->count;
                final_num_stations++;
            }
        }
        // Liberar memoria local de cada hilo
        free(thread_data[i].local_stations);
    }
    
    // Calcular promedios finales
    for (int i = 0; i < final_num_stations; i++) {
        final_stations[i].avg_temp = final_stations[i].sum_temps / final_stations[i].count;
    }

    printf("      ✓ Se encontraron %d estaciones únicas\n\n", final_num_stations);

    // Registrar tiempo final
    double end_time = get_time_us();
    double elapsed_time_us = end_time - start_time;
    double elapsed_time_ms = elapsed_time_us / 1000.0;
    double elapsed_time_s = elapsed_time_ms / 1000.0;

    // Fase 3: Mostrar resultados
    printf("[3/3] Mostrando resultados...\n");
    print_results(final_stations, final_num_stations, 0);

    // Mostrar estadísticas de rendimiento
    printf("\n========== ESTADÍSTICAS DE RENDIMIENTO ==========\n");
    printf("Tiempo de cómputo (sin E/S): %.3f ms (%.6f s)\n", elapsed_time_ms, elapsed_time_s);
    printf("Registros procesados: %d\n", num_readings);
    printf("Estaciones únicas: %d\n", final_num_stations);
    printf("Tiempo por registro: %.3f µs\n", elapsed_time_us / num_readings);
    printf("Throughput: %.2f registros/ms\n", (double)num_readings / elapsed_time_ms);
    printf("==============================================\n");

    // Liberar memoria
    free(readings);
    free(final_stations);

    return 0;
}
