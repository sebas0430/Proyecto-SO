#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#define MAX_LIBROS 100
#define BUFFER_SIZE 10

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
sem_t lleno, vacio;

// Estructura para solicitud
typedef struct {
    char nombreLibro[50];
    char codigo[10];
    char operacion; // 'P', 'R', 'D', 's', 't'
} Solicitud;

// Estructura para libro
typedef struct {
    char nombreLibro[50];
    char codigo[10];
    int cantidad;
    int prestados;
} Libro;

// Estructura para pasar datos al hilo
typedef struct {
    Solicitud* buffer;
    int* in;
    int* out;
    Libro** libros;
    int* cantidad_libros;
    int verbose;
} DatosHilo;

// Leer solicitud desde el pipe
/*
Solicitud leer_solicitud(char* nombre_pipe) {
    int fd = open(nombre_pipe, O_RDONLY);
    if (fd == -1) {
        perror("Error abriendo pipe");
        exit(EXIT_FAILURE);
    }
    Solicitud s;
    read(fd, &s, sizeof(Solicitud));
    close(fd);
    return s;
}*/

// Buscar libro por nombre
int buscar_libro(const char* nombre, Libro* libros[], int cantidad_libros) {
    for (int i = 0; i < cantidad_libros; i++) {
        if (strcmp(libros[i]->nombreLibro, nombre) == 0) {
            return i;
        }
    }
    return -1;
}

// Procesar solicitud de prestamo
void procesar_prestamo(Solicitud s, Libro* libros[], int cantidad_libros, int verbose) {
    int idx = buscar_libro(s.nombreLibro, libros, cantidad_libros);
    if (idx == -1) {
        printf("Libro no encontrado: %s\n", s.nombreLibro);
        return;
    }
    if (libros[idx]->cantidad > 0) {
        libros[idx]->cantidad--;
        libros[idx]->prestados++;
        if (verbose) printf("[P] Prestamo: %s\n", libros[idx]->nombreLibro);
    } else {
        printf("❌ No hay copias disponibles de: %s\n", libros[idx]->nombreLibro);
    }


    
}

// Hilo consumidor
void* hilo_auxiliar(void* args) {
    DatosHilo* datos = (DatosHilo*) args;
    Solicitud* buffer = datos->buffer;
    int* in = datos->in;
    int* out = datos->out;
    Libro** libros = datos->libros;
    int* cantidad_libros = datos->cantidad_libros;

    while (1) {
        sem_wait(&lleno);
        pthread_mutex_lock(&mutex);

        Solicitud s = buffer[*out];
        *out = (*out + 1) % BUFFER_SIZE;

        pthread_mutex_unlock(&mutex);
        sem_post(&vacio);

        if (s.operacion == 's') {
            printf("🛑 Hilo auxiliar termina.\n");
            break;
        }

        int idx = buscar_libro(s.nombreLibro, libros, *cantidad_libros);
        if (idx == -1) {
            printf("Libro no encontrado: %s\n", s.nombreLibro);
            continue;
        }

        if (s.operacion == 'D') {
            libros[idx]->cantidad++;
            libros[idx]->prestados--;
            if (datos->verbose) printf("[D] Devolucion: %s\n", libros[idx]->nombreLibro);
        } else if (s.operacion == 'R') {
            if (datos->verbose) printf("[R] Renovacion: %s\n", libros[idx]->nombreLibro);
        }
    }
    return NULL;
}

// Cargar libros desde archivo
void cargar_libros(Libro* libros[], int* cantidad_libros, const char* archivo) {
    FILE* file = fopen(archivo, "r");
    if (!file) {
        perror("Error al abrir archivo de BD");
        exit(EXIT_FAILURE);
    }

    char nombre[50], codigo[10];
    int cantidad;

    while (fscanf(file, " %49[^,], %9[^,], %d", nombre, codigo, &cantidad) == 3) {
        libros[*cantidad_libros] = malloc(sizeof(Libro));
        strcpy(libros[*cantidad_libros]->nombreLibro, nombre);
        strcpy(libros[*cantidad_libros]->codigo, codigo);
        libros[*cantidad_libros]->cantidad = cantidad;
        libros[*cantidad_libros]->prestados = 0;
        (*cantidad_libros)++;
    }

    fclose(file);
}

// Mostrar reporte
void mostrar_reporte(Libro* libros[], int cantidad_libros) {
    printf("\n=== REPORTE ===\n");
    for (int i = 0; i < cantidad_libros; i++) {
        printf("Libro: %s | Codigo: %s | Disponibles: %d | Prestados: %d\n",
            libros[i]->nombreLibro,
            libros[i]->codigo,
            libros[i]->cantidad,
            libros[i]->prestados);
    }
    printf("================\n");
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Uso: %s -p pipe -f fileBD [-v]\n", argv[0]);
        return 1;
    }

    char* nombre_pipe = NULL;
    char* archivo_BD = NULL;
    int verbose = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0) {
            nombre_pipe = argv[++i];
        } else if (strcmp(argv[i], "-f") == 0) {
            archivo_BD = argv[++i];
        } else if (strcmp(argv[i], "-v") == 0) {
            verbose = 1;
        }
    }

    Libro* libros[MAX_LIBROS];
    int cantidad_libros = 0;
    cargar_libros(libros, &cantidad_libros, archivo_BD);

    sem_init(&lleno, 0, 0);
    sem_init(&vacio, 0, BUFFER_SIZE);

    Solicitud buffer[BUFFER_SIZE];
    int in = 0, out = 0;

    DatosHilo datos = {buffer, &in, &out, libros, &cantidad_libros, verbose};
    pthread_t hilo;
    pthread_create(&hilo, NULL, hilo_auxiliar, &datos);

    int fd = open(nombre_pipe, O_RDONLY);
    if (fd == -1) {
        perror("Error abriendo pipe");
        exit(EXIT_FAILURE);
    }
    
    while (1) {
        Solicitud s;
        read(fd, &s, sizeof(Solicitud));

        if (s.operacion == 'P') {
            procesar_prestamo(s, libros, cantidad_libros, verbose);
        } else if (s.operacion == 'D' || s.operacion == 'R') {
            sem_wait(&vacio);
            pthread_mutex_lock(&mutex);
            buffer[in] = s;
            in = (in + 1) % BUFFER_SIZE;
            pthread_mutex_unlock(&mutex);
            sem_post(&lleno);
        } else if (s.operacion == 't') {
            mostrar_reporte(libros, cantidad_libros);
        } else if (s.operacion == 's') {
            sem_wait(&vacio);
            pthread_mutex_lock(&mutex);
            buffer[in] = s;
            in = (in + 1) % BUFFER_SIZE;
            pthread_mutex_unlock(&mutex);
            sem_post(&lleno);
            break;
        } else {
            printf("Operación inválida: %c\n", s.operacion);
        }
    }

    pthread_join(hilo, NULL);

    for (int i = 0; i < cantidad_libros; i++) {
        free(libros[i]);
    }
    sem_destroy(&lleno);
    sem_destroy(&vacio);
    pthread_mutex_destroy(&mutex);

    return 0;
}
