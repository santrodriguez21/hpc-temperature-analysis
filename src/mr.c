#include "common.h"
#include <pthread.h>

#ifndef MAX_THREADS
#define MAX_THREADS 64
#endif

// Función Hash (djb2) para asignar estaciones a Reducers
unsigned int hash_station(const char *str) {
    unsigned int hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

// Estructura para almacenar pares emitidos por Mappers (o combinados)
typedef struct {
    StationData *stations;
    int count;
} Bucket;

// Datos para la fase MAP
typedef struct {
    int thread_id;
    Reading *readings;
    int start_idx;
    int end_idx;
    int num_reducers;
    Bucket *map_buckets; // Array de num_reducers buckets
} MapperData;

// Datos para la fase REDUCE
typedef struct {
    int reducer_id;
    int num_mappers;
    MapperData *mappers;
    StationData *reduced_stations;
    int final_count;
} ReducerData;

// Fase MAP: Lee lecturas y las acumula localmente particionadas por Reducer
// 1. Cada hilo "Mapper" procesa una porción (chunk) del archivo CSV.
// 2. Por cada lectura, aplica una función Hash al nombre de la estación.
// 3. El Hash determina a qué "Bucket" (cesta) correspondiente a un Reducer debe ir la estación.
// 4. Actúa también como "Combiner" local: si la estación ya está en el bucket, simplemente suma la temperatura, reduciendo el tráfico de datos para la siguiente fase.
void *map_function(void *arg) {
    MapperData *data = (MapperData *)arg;
    int R = data->num_reducers;

    // Inicializar buckets del Mapper
    data->map_buckets = (Bucket *)malloc(R * sizeof(Bucket));
    for (int i = 0; i < R; i++) {
        // Asignamos suficiente memoria para albergar las estaciones locales del bucket
        data->map_buckets[i].stations = (StationData *)malloc(MAX_STATIONS * sizeof(StationData));
        data->map_buckets[i].count = 0;
    }

    // Procesar asignación de lecturas
    for (int i = data->start_idx; i < data->end_idx; i++) {
        Reading *r = &data->readings[i];
        
        // El Hash divide las estaciones equitativamente entre los reducers disponibles
        unsigned int h = hash_station(r->station) % R;
        Bucket *b = &data->map_buckets[h];

        // Buscar en el bucket local
        int found = 0;
        for (int j = 0; j < b->count; j++) {
            if (strcmp(b->stations[j].station, r->station) == 0) {
                b->stations[j].sum_temps += r->tmax;
                b->stations[j].count++;
                found = 1;
                break;
            }
        }

        // Si es una estación nueva en este bucket, la agregamos
        if (!found && b->count < MAX_STATIONS) {
            strcpy(b->stations[b->count].station, r->station);
            b->stations[b->count].sum_temps = r->tmax;
            b->stations[b->count].count = 1;
            b->count++;
        }
    }
    return NULL;
}

// Fase REDUCE: Toma los buckets correspondientes de todos los Mappers y los consolida
// 1. Cada hilo "Reducer" tiene un ID único (ej: Reducer 0, Reducer 1...).
// 2. El Reducer X visita a TODOS los Mappers y extrae exclusivamente el Bucket X.
// 3. Ya que la función Hash garantizó que la "Estación A" siempre vaya al Bucket X en cualquier Mapper, 
//    el Reducer X puede consolidar de forma segura sin preocuparse por bloqueos de memoria (mutex) con otros Reducers.
void *reduce_function(void *arg) {
    ReducerData *data = (ReducerData *)arg;
    int r_id = data->reducer_id;
    int M = data->num_mappers;

    data->reduced_stations = (StationData *)malloc(MAX_STATIONS * sizeof(StationData));
    data->final_count = 0;

    // Extraer datos del Bucket 'r_id' de todos los Mappers
    for (int m = 0; m < M; m++) {
        Bucket *b = &data->mappers[m].map_buckets[r_id];
        
        for (int i = 0; i < b->count; i++) {
            StationData *st = &b->stations[i];
            int found = 0;

            // Consolidar la estación en el array final de este Reducer
            for (int j = 0; j < data->final_count; j++) {
                if (strcmp(data->reduced_stations[j].station, st->station) == 0) {
                    data->reduced_stations[j].sum_temps += st->sum_temps;
                    data->reduced_stations[j].count += st->count;
                    found = 1;
                    break;
                }
            }

            if (!found && data->final_count < MAX_STATIONS) {
                strcpy(data->reduced_stations[data->final_count].station, st->station);
                data->reduced_stations[data->final_count].sum_temps = st->sum_temps;
                data->reduced_stations[data->final_count].count = st->count;
                data->final_count++;
            }
        }
    }

    // Calcular el promedio final de las estaciones consolidadas por este Reducer
    for (int i = 0; i < data->final_count; i++) {
        data->reduced_stations[i].avg_temp = data->reduced_stations[i].sum_temps / data->reduced_stations[i].count;
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <archivo_datos> [num_registros] [num_hilos_map] [num_hilos_reduce]\n", argv[0]);
        return 1;
    }

    const char *input_file = argv[1];
    int max_readings = MAX_READINGS;
    int num_mappers = 4;
    int num_reducers = 4;
    
    if (argc > 2) {
        max_readings = atoi(argv[2]);
        if (max_readings <= 0) return 1;
    }
    if (argc > 3) {
        num_mappers = atoi(argv[3]);
        if (num_mappers <= 0 || num_mappers > MAX_THREADS) return 1;
    }
    if (argc > 4) {
        num_reducers = atoi(argv[4]);
        if (num_reducers <= 0 || num_reducers > MAX_THREADS) return 1;
    }

    printf("========== VERSIÓN MAPREDUCE (PTHREADS) ==========\n");
    printf("Archivo: %s\n", input_file);
    printf("Max registros: %d\n", max_readings);
    printf("Mappers: %d | Reducers: %d\n\n", num_mappers, num_reducers);

    Reading *readings = (Reading *)malloc(max_readings * sizeof(Reading));
    if (!readings) return 1;

    printf("[1/4] Leyendo archivo CSV...\n");
    int num_readings = read_csv_file(input_file, readings, max_readings);
    if (num_readings <= 0) {
        free(readings);
        return 1;
    }
    printf("      ✓ Se leyeron %d registros\n\n", num_readings);

    double start_time = get_time_us();

    // FASE MAP
    printf("[2/4] Ejecutando fase MAP...\n");
    pthread_t map_threads[MAX_THREADS];
    MapperData map_data[MAX_THREADS];

    int chunk_size = num_readings / num_mappers;
    int remainder = num_readings % num_mappers;
    int current_idx = 0;

    for (int i = 0; i < num_mappers; i++) {
        map_data[i].thread_id = i;
        map_data[i].readings = readings;
        map_data[i].start_idx = current_idx;
        int current_chunk = chunk_size + (i < remainder ? 1 : 0);
        map_data[i].end_idx = current_idx + current_chunk;
        current_idx += current_chunk;
        map_data[i].num_reducers = num_reducers;

        pthread_create(&map_threads[i], NULL, map_function, &map_data[i]);
    }

    for (int i = 0; i < num_mappers; i++) {
        pthread_join(map_threads[i], NULL);
    }

    // FASE REDUCE
    printf("[3/4] Ejecutando fase REDUCE...\n");
    pthread_t reduce_threads[MAX_THREADS];
    ReducerData reduce_data[MAX_THREADS];

    for (int i = 0; i < num_reducers; i++) {
        reduce_data[i].reducer_id = i;
        reduce_data[i].num_mappers = num_mappers;
        reduce_data[i].mappers = map_data;
        pthread_create(&reduce_threads[i], NULL, reduce_function, &reduce_data[i]);
    }

    int final_num_stations = 0;
    StationData *final_stations = (StationData *)malloc(MAX_STATIONS * sizeof(StationData));

    for (int i = 0; i < num_reducers; i++) {
        pthread_join(reduce_threads[i], NULL);
        
        // Copiar resultados del Reducer al array final
        for (int j = 0; j < reduce_data[i].final_count; j++) {
            if (final_num_stations < MAX_STATIONS) {
                final_stations[final_num_stations] = reduce_data[i].reduced_stations[j];
                final_num_stations++;
            }
        }
        free(reduce_data[i].reduced_stations);
    }

    double end_time = get_time_us();
    double elapsed_time_ms = (end_time - start_time) / 1000.0;
    double elapsed_time_s = elapsed_time_ms / 1000.0;

    printf("      ✓ Se encontraron %d estaciones únicas\n\n", final_num_stations);

    printf("[4/4] Mostrando resultados...\n");
    print_results(final_stations, final_num_stations, 0);

    printf("\n========== ESTADÍSTICAS DE RENDIMIENTO MAPREDUCE ==========\n");
    printf("Tiempo de cómputo (MAP+REDUCE): %.3f ms (%.6f s)\n", elapsed_time_ms, elapsed_time_s);
    printf("Registros procesados: %d\n", num_readings);
    printf("Estaciones únicas: %d\n", final_num_stations);
    printf("Tiempo por registro: %.3f µs\n", (elapsed_time_ms * 1000.0) / num_readings);
    printf("Throughput: %.2f registros/ms\n", (double)num_readings / elapsed_time_ms);
    printf("===========================================================\n");

    // Limpieza
    for (int i = 0; i < num_mappers; i++) {
        for (int j = 0; j < num_reducers; j++) {
            free(map_data[i].map_buckets[j].stations);
        }
        free(map_data[i].map_buckets);
    }
    free(readings);
    free(final_stations);

    return 0;
}
