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
#define MAX_OPERACIONES 1000

typedef struct {
    char operacion; // 'Q', 'P', 'D', 'R', 'S'
    char nombre_libro[50];
    int isbn;
} Solicitud;

typedef struct {
    int linea_encabezado;
    int cantidad_ejemplares;
    int linea_ejemplar_estado;
} InfoLibro;

typedef struct {
    char operacion; // 'P', 'D', 'R'
    char nombre_libro[50];
    int isbn;
    int ejemplar; // línea del ejemplar en el archivo
    char fecha[20]; // dd-mm-yyyy
} RegistroOperacion;

// Variables globales para registro y control
RegistroOperacion operaciones[MAX_OPERACIONES];
int num_operaciones = 0;
pthread_mutex_t mutex_operaciones = PTHREAD_MUTEX_INITIALIZER;

Solicitud Buffer[BUFFER_SIZE];
int in = 0;
int out = 0;

sem_t espacios_disponibles;
sem_t solicitudes_pendientes;
sem_t acceso_buffer;

pthread_mutex_t mutex_archivo = PTHREAD_MUTEX_INITIALIZER;

volatile int ejecucion_receptor = 1; // variable compartida para controlar ejecución

typedef struct {
    FILE* archivo_entrada;
} Datos_hilo;

// Declaraciones
void agregar_operacion(char operacion, const char* nombre, int isbn, int ejemplar, const char* fecha);
void* hilo_auxiliar01(void* arg);
void* hilo_auxiliar02(void* arg);
InfoLibro buscar_info_libro(FILE *archivo, const char *nombre, int isbn, char estado_objetivo);
int cambiar_estado_libro(FILE *archivo, int numero_linea, char nuevo_estado);
int actualizar_fecha_linea(FILE *archivo, int numero_linea, int modo);
void enviar_respuesta(int fd_respuesta, const char* mensaje);

void agregar_operacion(char operacion, const char* nombre, int isbn, int ejemplar, const char* fecha) { // función para agregar una operación al registro
    pthread_mutex_lock(&mutex_operaciones); // proteger acceso a operaciones
    if (num_operaciones < MAX_OPERACIONES) { // verificar si hay espacio en el registro
        operaciones[num_operaciones].operacion = operacion; // asignar operación
        strncpy(operaciones[num_operaciones].nombre_libro, nombre, sizeof(operaciones[num_operaciones].nombre_libro) - 1); // copiar nombre del libro
        operaciones[num_operaciones].nombre_libro[sizeof(operaciones[num_operaciones].nombre_libro) - 1] = '\0'; // asegurar terminación de cadena
        operaciones[num_operaciones].isbn = isbn; // asignar ISBN
        operaciones[num_operaciones].ejemplar = ejemplar; // asignar línea del ejemplar
        strncpy(operaciones[num_operaciones].fecha, fecha, sizeof(operaciones[num_operaciones].fecha) - 1); // copiar fecha
        operaciones[num_operaciones].fecha[sizeof(operaciones[num_operaciones].fecha) - 1] = '\0'; // asegurar terminación de cadena
        num_operaciones++; // incrementar contador de operaciones
    }
    pthread_mutex_unlock(&mutex_operaciones); // liberar mutex
}

void* hilo_auxiliar01(void* arg) { // hilo auxiliar para manejar solicitudes de devolución y renovación
    Datos_hilo* datos = (Datos_hilo*)arg; // recibir datos del hilo
    FILE* archivo_entrada = datos->archivo_entrada; // archivo de base de datos

    int fd_respuesta = open("/tmp/pipe_respuesta", O_WRONLY); // abrir pipe de respuesta
    if (fd_respuesta == -1) {
        perror("Error al abrir pipe de respuesta en hilo_auxiliar01");
    }

    while (ejecucion_receptor) { // ciclo principal del hilo
        sem_wait(&solicitudes_pendientes);// esperar por solicitudes pendientes
        sem_wait(&acceso_buffer); // asegurar acceso al buffer

        Solicitud solicitud = Buffer[out]; // obtener solicitud del buffer
        out = (out + 1) % BUFFER_SIZE; // actualizar índice de salida

        sem_post(&acceso_buffer); // liberar acceso al buffer
        sem_post(&espacios_disponibles); // liberar un espacio en el buffer

        if (!ejecucion_receptor) break; // verificar si se debe salir

        if (solicitud.operacion == 'D') { // operación de devolución
            InfoLibro info = buscar_info_libro(archivo_entrada, solicitud.nombre_libro, solicitud.isbn, 'P'); // buscar información del libro
            if (info.linea_ejemplar_estado != -1 && cambiar_estado_libro(archivo_entrada, info.linea_ejemplar_estado, 'D') == 1) { // cambiar estado del libro a 'D'
                actualizar_fecha_linea(archivo_entrada, info.linea_ejemplar_estado, 0);// actualizar fecha de devolución

                // Formatear fecha actual
                char fecha[20];
                time_t t = time(NULL);
                strftime(fecha, sizeof(fecha), "%d-%m-%Y", localtime(&t));

                agregar_operacion('D', solicitud.nombre_libro, solicitud.isbn, info.linea_ejemplar_estado, fecha); 

                printf("Devolución exitosa del libro: %s con ISBN: %d\n", solicitud.nombre_libro, solicitud.isbn); // enviar respuesta al cliente
                char respuesta[256]; //
                snprintf(respuesta, sizeof(respuesta), "Libro %s con ISBN %d fue devuelto exitosamente.\n", solicitud.nombre_libro, solicitud.isbn); // formatear respuesta 
                enviar_respuesta(fd_respuesta, respuesta);
            } else {
                char respuesta[256];
                snprintf(respuesta, sizeof(respuesta), "Error al devolver el libro %s con ISBN %d.\n", solicitud.nombre_libro, solicitud.isbn);
                enviar_respuesta(fd_respuesta, respuesta);
            }
        } else if (solicitud.operacion == 'R') { // operación de renovación
            InfoLibro info = buscar_info_libro(archivo_entrada, solicitud.nombre_libro, solicitud.isbn, 'P'); // buscar información del libro
            if (info.linea_ejemplar_estado != -1 && actualizar_fecha_linea(archivo_entrada, info.linea_ejemplar_estado, 1) == 1) { // cambiar estado del libro a 'R'

                // Formatear fecha actual
                char fecha[20];
                time_t t = time(NULL);
                strftime(fecha, sizeof(fecha), "%d-%m-%Y", localtime(&t));

                agregar_operacion('R', solicitud.nombre_libro, solicitud.isbn, info.linea_ejemplar_estado, fecha);

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

void* hilo_auxiliar02(void* arg) { // hilo auxiliar para manejar comandos de reporte y salida
    (void)arg; // no usado

    char comando;
    while (ejecucion_receptor) {
        printf("Ingrese comando (r para reporte, s para salir): "); // solicitar comando al usuario
        fflush(stdout);

        comando = getchar(); // leer comando del usuario

        // Limpiar buffer stdin
        while(getchar() != '\n' && !feof(stdin)); // limpiar el buffer de entrada

        if (comando == 'r') { // comando de reporte
            pthread_mutex_lock(&mutex_operaciones);
            printf("\n--- REPORTE DE OPERACIONES REALIZADAS ---\n");
            for (int i = 0; i < num_operaciones; i++) {
                printf("%c, %s, ISBN %d, Ejemplar %d, Fecha: %s\n",
                       operaciones[i].operacion,
                       operaciones[i].nombre_libro,
                       operaciones[i].isbn,
                       operaciones[i].ejemplar,
                       operaciones[i].fecha);
            }
            if (num_operaciones == 0) // si no hay operaciones registradas
                printf("No se han registrado operaciones aún.\n");
            printf("----------------------------------------\n");
            pthread_mutex_unlock(&mutex_operaciones);
        } else if (comando == 's') { // comando de salida
            printf("Comando salir recibido. Terminando ejecución...\n");
            ejecucion_receptor = 0;

            // Liberar semáforos para que el hilo 1 no quede bloqueado
            sem_post(&solicitudes_pendientes); // liberar un espacio en el semáforo de solicitudes pendientes
        } else {
            printf("Comando no reconocido. Intente nuevamente.\n");
        }
    }
    return NULL;
}

InfoLibro buscar_info_libro(FILE *archivo, const char *nombre, int isbn, char estado_objetivo) { // función para buscar información de un libro en el archivo
    char linea[256];
    InfoLibro info = {-1, 0, -1}; // inicializar estructura de información del libro
    int linea_actual = 0;

    pthread_mutex_lock(&mutex_archivo); // proteger acceso al archivo
    rewind(archivo); // reiniciar el puntero del archivo al inicio

    while (fgets(linea, sizeof(linea), archivo)) {
        linea_actual++;
        char nombre_bd[100];
        int isbn_bd, cantidad_bd;

        if (sscanf(linea, " %99[^,], %d, %d", nombre_bd, &isbn_bd, &cantidad_bd) == 3) { // leer línea del archivo y extraer nombre, ISBN y cantidad
            if (strcmp(nombre_bd, nombre) == 0 && isbn_bd == isbn) { // comparar con los parámetros de búsqueda
                info.linea_encabezado = linea_actual; // guardar línea del encabezado
                info.cantidad_ejemplares = cantidad_bd; // guardar cantidad de ejemplares

                for (int i = 0; i < cantidad_bd; i++) { // iterar sobre los ejemplares del libro
                    if (fgets(linea, sizeof(linea), archivo)) { // leer la línea del ejemplar
                        linea_actual++; // verificar si se leyó correctamente
                        char estado; // variable para almacenar el estado del ejemplar
                        if (sscanf(linea, " %*[^,], %c,", &estado) == 1) { // extraer el estado del ejemplar
                            if (estado == estado_objetivo && info.linea_ejemplar_estado == -1) {// si el estado coincide con el objetivo y aún no se ha encontrado un ejemplar
                                info.linea_ejemplar_estado = linea_actual; // guardar línea del ejemplar
                            }
                        }
                    }
                }
                pthread_mutex_unlock(&mutex_archivo); // liberar mutex antes de retornar
                return info; // retornar información del libro
            } else {
                for (int i = 0; i < cantidad_bd; i++) { // si no coincide, saltar las líneas de los ejemplares
                    fgets(linea, sizeof(linea), archivo); // leer la línea del ejemplar
                    linea_actual++; // incrementar el contador de líneas
                }
            }
        }
    }
    pthread_mutex_unlock(&mutex_archivo); // liberar mutex después de terminar la búsqueda
    return info;
}

int cambiar_estado_libro(FILE *archivo, int numero_linea, char nuevo_estado) { // función para cambiar el estado de un libro en el archivo
    char linea[256];
    int linea_actual = 0;
    long posicion_inicio = 0;

    pthread_mutex_lock(&mutex_archivo); // proteger acceso al archivo
    rewind(archivo); // reiniciar el puntero del archivo al inicio

    while (fgets(linea, sizeof(linea), archivo)) { // leer línea del archivo
        linea_actual++; // incrementar el contador de líneas
        if (linea_actual == numero_linea) { // si se ha alcanzado la línea del ejemplar
            posicion_inicio = ftell(archivo) - strlen(linea); // guardar la posición de inicio de la línea

            char *primera_coma = strchr(linea, ','); // buscar la primera coma en la línea
            if (!primera_coma) { // si no se encuentra la coma, liberar mutex y retornar 
                pthread_mutex_unlock(&mutex_archivo); // liberar mutex
                return 0;
            }
            char *segunda_coma = strchr(primera_coma + 1, ','); // buscar la segunda coma en la línea
            if (!segunda_coma) { // si no se encuentra la segunda coma, liberar mutex y retornar 
                pthread_mutex_unlock(&mutex_archivo); // liberar mutex
                return 0;
            }

            int index_estado = (int)(segunda_coma - linea - 1); // calcular el índice del estado del ejemplar
            if (index_estado < 0 || index_estado >= (int)strlen(linea)) { // verificar si el índice es válido
                pthread_mutex_unlock(&mutex_archivo); // liberar mutex
                return 0;
            }

            fseek(archivo, posicion_inicio + index_estado, SEEK_SET); // mover el puntero del archivo a la posición del estado del ejemplar
            fputc(nuevo_estado, archivo); // escribir el nuevo estado en el archivo
            fflush(archivo); // asegurar que los cambios se escriban en el archivo

            pthread_mutex_unlock(&mutex_archivo); // liberar mutex
            return 1;
        }
    }

    pthread_mutex_unlock(&mutex_archivo); // liberar mutex si no se encuentra la línea
    return 0;
}

int actualizar_fecha_linea(FILE *archivo, int numero_linea, int modo) { // función para actualizar la fecha de un libro en el archivo
    char linea[256];
    int linea_actual = 0;

    pthread_mutex_lock(&mutex_archivo); // proteger acceso al archivo
    rewind(archivo);

    time_t t = time(NULL); // obtener tiempo actual
    struct tm fecha = *localtime(&t); // convertir a estructura tm

    if (modo == 1) {
        fecha.tm_mday += 7; // sumar 7 días para renovación
        mktime(&fecha); // normalizar la fecha
    }

    char nueva_fecha[20];
    strftime(nueva_fecha, sizeof(nueva_fecha), "%d-%m-%Y", &fecha); // formatear la fecha a dd-mm-yyyy

    while (fgets(linea, sizeof(linea), archivo)) {
        linea_actual++;
        if (linea_actual == numero_linea) {
            long pos_inicio = ftell(archivo) - strlen(linea); // guardar la posición de inicio de la línea

            char *ultima_coma = strrchr(linea, ','); // buscar la última coma en la línea
            if (!ultima_coma) {
                pthread_mutex_unlock(&mutex_archivo); // liberar mutex si no se encuentra la coma
                return 0;
            }

            int offset_fecha = (int)(ultima_coma - linea + 1); //  calcular el offset de la fecha

           

            fseek(archivo, pos_inicio + offset_fecha, SEEK_SET); // mover el puntero del archivo a la posición de la fecha

            fprintf(archivo, "%s\n", nueva_fecha); // escribir la nueva fecha en el archivo


            fflush(archivo); // asegurar que los cambios se escriban en el archivo
            pthread_mutex_unlock(&mutex_archivo); // liberar mutex
            return 1;
        }
    }

    pthread_mutex_unlock(&mutex_archivo); // liberar mutex si no se encuentra la línea
    return 0;
}

void enviar_respuesta(int fd_respuesta, const char* mensaje) { // función para enviar una respuesta a través del pipe de respuesta
    if (fd_respuesta == -1) {
        perror("Error: pipe de respuesta no abierto");
        return;
    }
    ssize_t n = write(fd_respuesta, mensaje, strlen(mensaje)); // escribir mensaje en el pipe de respuesta
    if (n == -1) {
        perror("Error al escribir en el pipe de respuesta");
    }
}

int main(int argc, char *argv[]) {
    char fifo_respuesta[50] = "/tmp/pipe_respuesta";
    char* nombre_pipe = NULL;
    char* nombre_archivo = NULL;
    int verbose = 0;
    Solicitud solicitud;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc)
            nombre_pipe = argv[++i]; // asignar nombre del pipe
        else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc)
            nombre_archivo = argv[++i]; // asignar nombre del archivo de base de datos
        else if (strcmp(argv[i], "-v") == 0)
            verbose = 1; // activar modo verbose
    }

    if (!nombre_pipe || !nombre_archivo) {
        fprintf(stderr, "Uso: %s -p <pipe> -f <archivo_BD>\n", argv[0]); // mensaje de uso correcto
        exit(EXIT_FAILURE);
    }

    char fifo_file[64];
    snprintf(fifo_file, sizeof(fifo_file), "/tmp/%s", nombre_pipe); // crear nombre del pipe de solicitud

    if (access(fifo_file, F_OK) == -1 && mkfifo(fifo_file, 0660) == -1) { // verificar si el pipe de solicitud ya existe o crear uno nuevo
        perror("Error al crear pipe de solicitud");
        exit(EXIT_FAILURE);
    }
    if (access(fifo_respuesta, F_OK) == -1 && mkfifo(fifo_respuesta, 0660) == -1) { // verificar si el pipe de respuesta ya existe o crear uno nuevo
        perror("Error al crear pipe de respuesta");
        exit(EXIT_FAILURE);
    }

    int fd = open(fifo_file, O_RDONLY); // abrir el pipe de solicitud en modo lectura
    if (fd == -1) {
        perror("Error al abrir pipe de solicitud");
        exit(EXIT_FAILURE);
    }
    int fd_respuesta = open(fifo_respuesta, O_WRONLY); // abrir el pipe de respuesta en modo escritura
    if (fd_respuesta == -1) {
        perror("Error al abrir pipe de respuesta");
        exit(EXIT_FAILURE);
    }

    FILE* archivo_entrada = fopen(nombre_archivo, "r+"); // abrir el archivo de base de datos en modo lectura y escritura
    if (!archivo_entrada) {
        perror("Error al abrir archivo de base de datos");
        exit(EXIT_FAILURE);
    }

    if (verbose) {
        printf("Archivo: %s\n", nombre_archivo); 
        printf("Pipe: %s\n", nombre_pipe);
    }

    printf("====Bienvenido al sistema receptor de solicitudes====\n\n");

    sem_init(&espacios_disponibles, 0, BUFFER_SIZE); // inicializar semáforo de espacios disponibles
    sem_init(&solicitudes_pendientes, 0, 0); // inicializar semáforo de solicitudes pendientes
    sem_init(&acceso_buffer, 0, 1); // inicializar semáforo de acceso al buffer

    pthread_mutex_init(&mutex_archivo, NULL); // inicializar mutex para acceso al archivo

    pthread_t hilo01, hilo02; // declarar hilos auxiliares
    Datos_hilo datos_hilo01 = {archivo_entrada};// inicializar datos para el hilo auxiliar 1

    if (pthread_create(&hilo01, NULL, hilo_auxiliar01, (void*)&datos_hilo01) != 0) { // crear hilo auxiliar 1
        perror("Error al crear el hilo auxiliar 1");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&hilo02, NULL, hilo_auxiliar02, NULL) != 0) { // crear hilo auxiliar 2
        perror("Error al crear el hilo auxiliar 2");
        exit(EXIT_FAILURE);
    }

    while(ejecucion_receptor) {
        ssize_t read_bytes = read(fd, &solicitud, sizeof(Solicitud)); // leer solicitud del pipe
        if (read_bytes == 0) {
            printf("No hay más datos que leer del pipe.\n");
            break;
        } else if (read_bytes < (ssize_t)sizeof(Solicitud)) { // verificar si se leyeron suficientes bytes
            printf("Error al leer del pipe. Bytes leídos: %zd\n", read_bytes);
            break;
        } else if (read_bytes == -1) {
            perror("Error al leer del pipe");
            exit(EXIT_FAILURE);
        }

        if (!ejecucion_receptor) break;

        if (solicitud.operacion == 'Q') {
            printf("Recibida solicitud de salida. Terminando...\n");
            ejecucion_receptor = 0;
            sem_post(&solicitudes_pendientes); // liberar un espacio en el semáforo de solicitudes pendientes
            break;
        } else if (solicitud.operacion == 'P') {
            InfoLibro info = buscar_info_libro(archivo_entrada, solicitud.nombre_libro, solicitud.isbn, 'D'); // buscar información del libro con estado 'D' (disponible)
            if (info.linea_ejemplar_estado != -1 && cambiar_estado_libro(archivo_entrada, info.linea_ejemplar_estado, 'P') == 1) { // cambiar estado del libro a 'P' (prestado)

                // Fecha actual
                char fecha[20];
                time_t t = time(NULL);
                strftime(fecha, sizeof(fecha), "%d-%m-%Y", localtime(&t));

                agregar_operacion('P', solicitud.nombre_libro, solicitud.isbn, info.linea_ejemplar_estado, fecha); // agregar operación de préstamo al registro

                printf("Préstamo exitoso del libro: %s con ISBN: %d\n", solicitud.nombre_libro, solicitud.isbn); 
                char respuesta[256];
                snprintf(respuesta, sizeof(respuesta), "Libro %s con ISBN %d fue prestado exitosamente.\n", solicitud.nombre_libro, solicitud.isbn); // formatear respuesta
                write(fd_respuesta, respuesta, strlen(respuesta));
            } else {
                char respuesta[256];
                snprintf(respuesta, sizeof(respuesta), "No esta disponible el libro %s con ISBN %d.\n", solicitud.nombre_libro, solicitud.isbn); // formatear respuesta
                write(fd_respuesta, respuesta, strlen(respuesta));
            }
        } else if (solicitud.operacion == 'D' || solicitud.operacion == 'R') { // operaciones de devolución y renovación
            sem_wait(&espacios_disponibles); // esperar por un espacio disponible en el buffer
            sem_wait(&acceso_buffer); // asegurar acceso al buffer

            Buffer[in] = solicitud; // agregar solicitud al buffer
            in = (in + 1) % BUFFER_SIZE; // actualizar índice de entrada

            sem_post(&acceso_buffer); // liberar acceso al buffer
            sem_post(&solicitudes_pendientes); // indicar que hay una solicitud pendiente
        } else {
            printf("Operación desconocida en main: %c\n", solicitud.operacion);
        }
    }

    // Esperar que los hilos terminen
    pthread_join(hilo01, NULL); // esperar a que el hilo auxiliar 1 termine
    pthread_join(hilo02, NULL); // esperar a que el hilo auxiliar 2 termine

    // Cerrar recursos
    close(fd_respuesta);
    close(fd);
    fclose(archivo_entrada);

    sem_destroy(&espacios_disponibles);
    sem_destroy(&solicitudes_pendientes);
    sem_destroy(&acceso_buffer);
    pthread_mutex_destroy(&mutex_archivo);
    pthread_mutex_destroy(&mutex_operaciones);

    printf("Receptor finalizado.\n");
    return 0;
}