#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>

#define BUFFER_SIZE 10

typedef struct {
    char operacion; // 'Q', 'P', 'D', 'R'
    char nombre_libro[50];
    int isbn;
} Solicitud;

typedef struct {
    int linea_encabezado;
    int cantidad_ejemplares;
    int linea_ejemplar_estado;
} InfoLibro;

typedef struct {
    FILE* archivo_entrada;
    int ejecucion;
} Datos_hilo;

Solicitud Buffer[BUFFER_SIZE];
int in = 0;
int out = 0;
sem_t espacios_disponibles;
sem_t solicitudes_pendientes;
sem_t acceso_buffer;
pthread_mutex_t mutex_archivo = PTHREAD_MUTEX_INITIALIZER;

void enviar_respuesta(int fd_respuesta, const char* mensaje) {
    if (fd_respuesta == -1) {
        perror("Error: pipe de respuesta no abierto");
        return;
    }
    ssize_t n = write(fd_respuesta, mensaje, strlen(mensaje));
    if (n == -1) {
        perror("Error al escribir en el pipe de respuesta");
    }
}

InfoLibro buscar_info_libro(FILE *archivo, const char *nombre, int isbn, char estado_objetivo) {
    char linea[256];
    InfoLibro info = {-1, 0, -1};
    int linea_actual = 0;

    pthread_mutex_lock(&mutex_archivo);
    rewind(archivo);

    while (fgets(linea, sizeof(linea), archivo)) {
        linea_actual++;
        char nombre_bd[100];
        int isbn_bd, cantidad_bd;

        if (sscanf(linea, " %99[^,], %d, %d", nombre_bd, &isbn_bd, &cantidad_bd) == 3) {
            if (strcmp(nombre_bd, nombre) == 0 && isbn_bd == isbn) {
                info.linea_encabezado = linea_actual;
                info.cantidad_ejemplares = cantidad_bd;

                for (int i = 0; i < cantidad_bd; i++) {
                    if (fgets(linea, sizeof(linea), archivo)) {
                        linea_actual++;
                        char estado;
                        if (sscanf(linea, " %*[^,], %c,", &estado) == 1) {
                            if (estado == estado_objetivo && info.linea_ejemplar_estado == -1) {
                                info.linea_ejemplar_estado = linea_actual;
                            }
                        }
                    }
                }
                pthread_mutex_unlock(&mutex_archivo);
                return info;
            } else {
                for (int i = 0; i < cantidad_bd; i++) {
                    fgets(linea, sizeof(linea), archivo);
                    linea_actual++;
                }
            }
        }
    }
    pthread_mutex_unlock(&mutex_archivo);
    return info;
}

int cambiar_estado_libro(FILE *archivo, int numero_linea, char nuevo_estado) {
    char linea[256];
    int linea_actual = 0;
    long posicion_inicio = 0;

    pthread_mutex_lock(&mutex_archivo);
    rewind(archivo);

    while (fgets(linea, sizeof(linea), archivo)) {
        linea_actual++;
        if (linea_actual == numero_linea) {
            posicion_inicio = ftell(archivo) - strlen(linea);

            char *primera_coma = strchr(linea, ',');
            if (!primera_coma) {
                pthread_mutex_unlock(&mutex_archivo);
                return 0;
            }
            char *segunda_coma = strchr(primera_coma + 1, ',');
            if (!segunda_coma) {
                pthread_mutex_unlock(&mutex_archivo);
                return 0;
            }

            int index_estado = (int)(segunda_coma - linea - 1);
            if (index_estado < 0 || index_estado >= (int)strlen(linea)) {
                pthread_mutex_unlock(&mutex_archivo);
                return 0;
            }

            fseek(archivo, posicion_inicio + index_estado, SEEK_SET);
            fputc(nuevo_estado, archivo);
            fflush(archivo);

            pthread_mutex_unlock(&mutex_archivo);
            return 1;
        }
    }

    pthread_mutex_unlock(&mutex_archivo);
    return 0;
}

int actualizar_fecha_linea(FILE *archivo, int numero_linea, int modo) {
    char linea[256];
    int linea_actual = 0;

    pthread_mutex_lock(&mutex_archivo);
    rewind(archivo);

    time_t t = time(NULL);
    struct tm fecha = *localtime(&t);

    if (modo == 1) {
        fecha.tm_mday += 7;
        mktime(&fecha);
    }

    char nueva_fecha[20];
    strftime(nueva_fecha, sizeof(nueva_fecha), "%d-%m-%Y", &fecha);

    while (fgets(linea, sizeof(linea), archivo)) {
        linea_actual++;
        if (linea_actual == numero_linea) {
            long pos_inicio = ftell(archivo) - strlen(linea);

            char *ultima_coma = strrchr(linea, ',');
            if (!ultima_coma) {
                pthread_mutex_unlock(&mutex_archivo);
                return 0;
            }

            int offset_fecha = (int)(ultima_coma - linea + 2);

            fseek(archivo, pos_inicio + offset_fecha, SEEK_SET);
            fprintf(archivo, "%s \n", nueva_fecha);
            fflush(archivo);

            pthread_mutex_unlock(&mutex_archivo);
            return 1;
        }
    }

    pthread_mutex_unlock(&mutex_archivo);
    return 0;
}

void* hilo_auxiliar01(void* arg) {
    Datos_hilo* datos = (Datos_hilo*)arg;
    FILE* archivo_entrada = datos->archivo_entrada;

    int fd_respuesta = open("/tmp/pipe_respuesta", O_WRONLY);
    if (fd_respuesta == -1) {
        perror("Error al abrir pipe de respuesta en hilo_auxiliar01");
    }

    while(datos->ejecucion) {
        sem_wait(&solicitudes_pendientes);
        sem_wait(&acceso_buffer);

        Solicitud solicitud = Buffer[out];
        out = (out + 1) % BUFFER_SIZE;

        sem_post(&acceso_buffer);
        sem_post(&espacios_disponibles);

        if (solicitud.operacion == 'D') {
            InfoLibro info = buscar_info_libro(archivo_entrada, solicitud.nombre_libro, solicitud.isbn, 'P');
            if (info.linea_ejemplar_estado != -1 && cambiar_estado_libro(archivo_entrada, info.linea_ejemplar_estado, 'D') == 1) {
                actualizar_fecha_linea(archivo_entrada, info.linea_ejemplar_estado, 0);
                printf("Devolución exitosa del libro: %s con ISBN: %d\n", solicitud.nombre_libro, solicitud.isbn);
                char respuesta[256];
                snprintf(respuesta, sizeof(respuesta), "Libro %s con ISBN %d fue devuelto exitosamente.\n", solicitud.nombre_libro, solicitud.isbn);
                enviar_respuesta(fd_respuesta, respuesta);
            } else {
                char respuesta[256];
                snprintf(respuesta, sizeof(respuesta), "Error al devolver el libro %s con ISBN %d.\n", solicitud.nombre_libro, solicitud.isbn);
                enviar_respuesta(fd_respuesta, respuesta);
            }
        } else if (solicitud.operacion == 'R') {
            InfoLibro info = buscar_info_libro(archivo_entrada, solicitud.nombre_libro, solicitud.isbn, 'P');
            if (info.linea_ejemplar_estado != -1 && actualizar_fecha_linea(archivo_entrada, info.linea_ejemplar_estado, 1) == 1) {
                printf("Renovación exitosa del libro: %s con ISBN: %d\n", solicitud.nombre_libro, solicitud.isbn);
                char respuesta[256];
                snprintf(respuesta, sizeof(respuesta), "Libro %s con ISBN %d fue renovado exitosamente.\n", solicitud.nombre_libro, solicitud.isbn);
                enviar_respuesta(fd_respuesta, respuesta);
            } else {
                char respuesta[256];
                snprintf(respuesta, sizeof(respuesta), "Error al renovar el libro %s con ISBN %d.\n", solicitud.nombre_libro, solicitud.isbn);
                enviar_respuesta(fd_respuesta, respuesta);
            }
        }
    }

    if (fd_respuesta != -1) close(fd_respuesta);
    return NULL;
}

int main(int argc, char *argv[]) {
    char fifo_respuesta[50] = "/tmp/pipe_respuesta";
    char* nombre_pipe = NULL;
    char* nombre_archivo = NULL;
    int verbose = 0;
    Solicitud solicitud;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc)
            nombre_pipe = argv[++i];
        else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc)
            nombre_archivo = argv[++i];
        else if (strcmp(argv[i], "-v") == 0)
            verbose = 1;
    }

    if (!nombre_pipe || !nombre_archivo) {
        fprintf(stderr, "Uso: %s -p <pipe> -f <archivo_BD>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char fifo_file[64];
    snprintf(fifo_file, sizeof(fifo_file), "/tmp/%s", nombre_pipe);

    if (access(fifo_file, F_OK) == -1 && mkfifo(fifo_file, 0660) == -1) {
        perror("Error al crear pipe de solicitud");
        exit(EXIT_FAILURE);
    }
    if (access(fifo_respuesta, F_OK) == -1 && mkfifo(fifo_respuesta, 0660) == -1) {
        perror("Error al crear pipe de respuesta");
        exit(EXIT_FAILURE);
    }

    int fd = open(fifo_file, O_RDONLY);
    if (fd == -1) {
        perror("Error al abrir pipe de solicitud");
        exit(EXIT_FAILURE);
    }
    int fd_respuesta = open(fifo_respuesta, O_WRONLY);
    if (fd_respuesta == -1) {
        perror("Error al abrir pipe de respuesta");
        exit(EXIT_FAILURE);
    }

    FILE* archivo_entrada = fopen(nombre_archivo, "r+");
    if (!archivo_entrada) {
        perror("Error al abrir archivo de base de datos");
        exit(EXIT_FAILURE);
    }

    if (verbose) {
        printf("Archivo: %s\n", nombre_archivo);
        printf("Pipe: %s\n", nombre_pipe);
    }

    printf("====Bienvenido al sistema receptor de solicitudes====\n\n");

    sem_init(&espacios_disponibles, 0, BUFFER_SIZE);
    sem_init(&solicitudes_pendientes, 0, 0);
    sem_init(&acceso_buffer, 0, 1);

    pthread_mutex_init(&mutex_archivo, NULL);

    int ejecucion = 1;

    pthread_t hilo01;
    Datos_hilo datos_hilo01 = {archivo_entrada, ejecucion};
    if (pthread_create(&hilo01, NULL, hilo_auxiliar01, (void*)&datos_hilo01) != 0) {
        perror("Error al crear el hilo");
        exit(EXIT_FAILURE);
    }

    while(ejecucion) {
        ssize_t read_bytes = read(fd, &solicitud, sizeof(Solicitud));
        if (read_bytes == 0) {
            printf("No hay más datos que leer del pipe.\n");
            break;
        } else if (read_bytes < (ssize_t)sizeof(Solicitud)) {
            printf("Error al leer del pipe. Bytes leídos: %zd\n", read_bytes);
            break;
        } else if (read_bytes == -1) {
            perror("Error al leer del pipe");
            exit(EXIT_FAILURE);
        }

        if (solicitud.operacion == 'Q') {
            ejecucion = 0;
            close(fd);
            printf("Recibida solicitud de salida. Saliendo del sistema...\n");
            break;
        } else if (solicitud.operacion == 'P') {
            InfoLibro info = buscar_info_libro(archivo_entrada, solicitud.nombre_libro, solicitud.isbn, 'D');
            if (info.linea_ejemplar_estado != -1 && cambiar_estado_libro(archivo_entrada, info.linea_ejemplar_estado, 'P') == 1) {
                printf("Préstamo exitoso del libro: %s con ISBN: %d\n", solicitud.nombre_libro, solicitud.isbn);
                char respuesta[256];
                snprintf(respuesta, sizeof(respuesta), "Libro %s con ISBN %d fue prestado exitosamente.\n", solicitud.nombre_libro, solicitud.isbn);
                write(fd_respuesta, respuesta, strlen(respuesta));
            } else {
                char respuesta[256];
                snprintf(respuesta, sizeof(respuesta), "Error al prestar el libro %s con ISBN %d.\n", solicitud.nombre_libro, solicitud.isbn);
                write(fd_respuesta, respuesta, strlen(respuesta));
            }
        } else if (solicitud.operacion == 'D' || solicitud.operacion == 'R') {
            sem_wait(&espacios_disponibles);
            sem_wait(&acceso_buffer);

            Buffer[in] = solicitud;
            in = (in + 1) % BUFFER_SIZE;

            sem_post(&acceso_buffer);
            sem_post(&solicitudes_pendientes);
        } else {
            printf("Operación desconocida en main: %c\n", solicitud.operacion);
        }
    }

    pthread_join(hilo01, NULL);

    close(fd_respuesta);
    close(fd);
    fclose(archivo_entrada);

    sem_destroy(&espacios_disponibles);
    sem_destroy(&solicitudes_pendientes);
    sem_destroy(&acceso_buffer);
    pthread_mutex_destroy(&mutex_archivo);

    return 0;
}

