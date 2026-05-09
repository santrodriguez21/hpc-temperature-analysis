#!/usr/bin/env python3
"""
Generador de datos meteorológicos NOAA simulados para benchmarking
Crea un archivo CSV con miles de registros para evaluar rendimiento
"""

import random
import sys

def generate_noaa_data(num_records=100000, output_file="data/sample_data.csv"):
    """
    Genera datos meteorológicos simulados
    
    Args:
        num_records: Número de registros a generar
        output_file: Ruta del archivo de salida
    """
    
    # Estaciones simuladas
    stations = [f"US{i:06d}" for i in range(1, 5001)]  # 5000 estaciones
    
    print(f"[*] Generando {num_records} registros...")
    print(f"[*] Archivo de salida: {output_file}")
    
    with open(output_file, 'w') as f:
        # Escribir encabezado
        f.write("STATION,TMAX\n")
        
        # Generar datos
        for i in range(num_records):
            station = random.choice(stations)
            # Temperatura entre 15 y 35 grados (con decimales)
            tmax = round(random.uniform(15.0, 35.0), 2)
            f.write(f"{station},{tmax}\n")
            
            # Mostrar progreso cada 10000 registros
            if (i + 1) % 10000 == 0:
                print(f"    {i+1}/{num_records} registros generados...")
    
    print(f"[✓] Archivo generado exitosamente: {output_file}")
    print(f"[✓] Total de registros: {num_records}")

if __name__ == "__main__":
    # Por defecto: 2,000,000 registros para que el procesamiento secuencial demore unos segundos
    num_records = 2000000
    
    if len(sys.argv) > 1:
        try:
            num_records = int(sys.argv[1])
        except ValueError:
            print(f"Error: {sys.argv[1]} no es un número válido")
            sys.exit(1)
    
    generate_noaa_data(num_records)
