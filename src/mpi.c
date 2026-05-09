#include "common.h"
#include <mpi.h>

int main(int argc, char *argv[]) {
    // Inicializar entorno MPI
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int max_readings = MAX_READINGS;
    const char *input_file = NULL;

    if (argc < 2) {
        if (rank == 0) {
            fprintf(stderr, "Uso: %s <archivo_datos> [num_registros]\n", argv[0]);
            fprintf(stderr, "Ejemplo: mpirun -n 4 ./bin/mpi data/sample_data.csv\n");
        }
        MPI_Finalize();
        return 1;
    }

    input_file = argv[1];
    
    // Parámetro opcional: número máximo de registros a procesar
    if (argc > 2) {
        max_readings = atoi(argv[2]);
        if (max_readings <= 0) {
            if (rank == 0) fprintf(stderr, "Error: num_registros debe ser > 0\n");
            MPI_Finalize();
            return 1;
        }
    }

    int total_readings = 0;
    Reading *all_readings = NULL;

    // El proceso maestro (rank 0) lee el archivo completo
    if (rank == 0) {
        printf("========== VERSIÓN OPENMPI ==========\n");
        printf("Archivo: %s\n", input_file);
        printf("Max registros: %d\n", max_readings);
        printf("Número de procesos: %d\n\n", size);

        // Asignar memoria para lecturas
        all_readings = (Reading *)malloc(max_readings * sizeof(Reading));
        if (!all_readings) {
            fprintf(stderr, "Error: No hay memoria para las lecturas en master\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        // Fase 1: Leer archivo CSV
        printf("[1/3] Leyendo archivo CSV en master...\n");
        total_readings = read_csv_file(input_file, all_readings, max_readings);
        
        if (total_readings <= 0) {
            fprintf(stderr, "Error al leer archivo\n");
            free(all_readings);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        
        printf("      ✓ Se leyeron %d registros\n\n", total_readings);
        printf("[2/3] Distribuyendo datos y calculando promedios...\n");
    }

    // Sincronizar procesos y tomar tiempo de inicio
    MPI_Barrier(MPI_COMM_WORLD);
    double start_time = 0;
    if (rank == 0) {
        start_time = get_time_us();
    }

    // Transmitir el número total de lecturas a todos los procesos
    MPI_Bcast(&total_readings, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Preparar variables para distribuir datos
    int *sendcounts = (int *)malloc(size * sizeof(int));
    int *displs = (int *)malloc(size * sizeof(int));

    int chunk = total_readings / size;
    int remainder = total_readings % size;
    int current_displ = 0;

    for (int i = 0; i < size; i++) {
        int c = chunk + (i < remainder ? 1 : 0);
        sendcounts[i] = c * sizeof(Reading);
        displs[i] = current_displ;
        current_displ += sendcounts[i];
    }

    // Asignar memoria para la porción local de lecturas de cada proceso
    int local_count = chunk + (rank < remainder ? 1 : 0);
    Reading *local_readings = (Reading *)malloc(local_count * sizeof(Reading));

    // Distribuir datos desde el proceso 0 a todos usando MPI_Scatterv
    MPI_Scatterv(all_readings, sendcounts, displs, MPI_BYTE,
                 local_readings, local_count * sizeof(Reading), MPI_BYTE,
                 0, MPI_COMM_WORLD);

    // Ya podemos liberar el array principal en el maestro para ahorrar memoria
    if (rank == 0) {
        free(all_readings);
    }

    // Cada proceso calcula sus promedios locales
    StationData *local_stations = (StationData *)malloc(MAX_STATIONS * sizeof(StationData));
    int local_num_stations = calculate_averages(local_readings, local_count, local_stations);

    // Ahora el maestro necesita recolectar los resultados
    // Primero, saber cuántas estaciones encontró cada proceso
    int *all_num_stations = NULL;
    if (rank == 0) {
        all_num_stations = (int *)malloc(size * sizeof(int));
    }

    MPI_Gather(&local_num_stations, 1, MPI_INT, all_num_stations, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Maestro prepara para recibir los arreglos de StationData de cada proceso
    int *recvcounts = NULL;
    int *rdispls = NULL;
    StationData *all_gathered_stations = NULL;
    int total_gathered_stations = 0;

    if (rank == 0) {
        recvcounts = (int *)malloc(size * sizeof(int));
        rdispls = (int *)malloc(size * sizeof(int));
        int current_rdispl = 0;
        for (int i = 0; i < size; i++) {
            recvcounts[i] = all_num_stations[i] * sizeof(StationData);
            rdispls[i] = current_rdispl;
            current_rdispl += recvcounts[i];
            total_gathered_stations += all_num_stations[i];
        }
        all_gathered_stations = (StationData *)malloc(total_gathered_stations * sizeof(StationData));
    }

    // Recolectar todas las estaciones
    MPI_Gatherv(local_stations, local_num_stations * sizeof(StationData), MPI_BYTE,
                all_gathered_stations, recvcounts, rdispls, MPI_BYTE,
                0, MPI_COMM_WORLD);

    // Fase final: Maestro consolida resultados
    if (rank == 0) {
        StationData *final_stations = (StationData *)malloc(MAX_STATIONS * sizeof(StationData));
        int final_num_stations = 0;

        for (int i = 0; i < total_gathered_stations; i++) {
            StationData *st = &all_gathered_stations[i];
            int found = 0;
            
            // Buscar si la estación ya existe
            for (int j = 0; j < final_num_stations; j++) {
                if (strcmp(final_stations[j].station, st->station) == 0) {
                    final_stations[j].sum_temps += st->sum_temps;
                    final_stations[j].count += st->count;
                    found = 1;
                    break;
                }
            }
            
            // Agregar nueva estación
            if (!found && final_num_stations < MAX_STATIONS) {
                strcpy(final_stations[final_num_stations].station, st->station);
                final_stations[final_num_stations].sum_temps = st->sum_temps;
                final_stations[final_num_stations].count = st->count;
                final_num_stations++;
            }
        }

        // Calcular promedios finales
        for (int i = 0; i < final_num_stations; i++) {
            final_stations[i].avg_temp = final_stations[i].sum_temps / final_stations[i].count;
        }

        // Tomar tiempo final
        double end_time = get_time_us();
        double elapsed_time_us = end_time - start_time;
        double elapsed_time_ms = elapsed_time_us / 1000.0;
        double elapsed_time_s = elapsed_time_ms / 1000.0;

        printf("      ✓ Se encontraron %d estaciones únicas\n\n", final_num_stations);
        
        // Mostrar resultados
        printf("[3/3] Mostrando resultados...\n");
        print_results(final_stations, final_num_stations, 0);

        // Mostrar estadísticas de rendimiento
        printf("\n========== ESTADÍSTICAS DE RENDIMIENTO MPI ==========\n");
        printf("Tiempo de cómputo y red: %.3f ms (%.6f s)\n", elapsed_time_ms, elapsed_time_s);
        printf("Registros procesados: %d\n", total_readings);
        printf("Estaciones únicas: %d\n", final_num_stations);
        printf("Tiempo por registro: %.3f µs\n", elapsed_time_us / total_readings);
        printf("Throughput: %.2f registros/ms\n", (double)total_readings / elapsed_time_ms);
        printf("==============================================\n");

        free(final_stations);
        free(all_gathered_stations);
        free(all_num_stations);
        free(recvcounts);
        free(rdispls);
    }

    // Liberar memoria local
    free(local_readings);
    free(local_stations);
    free(sendcounts);
    free(displs);

    MPI_Finalize();
    return 0;
}
