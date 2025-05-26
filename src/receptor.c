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

//declaracion de estructuras
typedef struct {
    char operacion; // 'Q' salida, 'P' prestamo, 'D' devolucion, 'R' renovar
    char nombre_libro[50];
    int isbn;
} Solicitud;

typedef struct {
    int linea_encabezado; // linea del encabezado del libro, es -1 si no se encontró
    int cantidad_ejemplares; // cantidad de ejemplares del libro
    int linea_ejemplar_estado; // linea donde esta el libro con el estado que se busca, es -1 si no se encontró
} InfoLibro;

typedef struct {
    FILE* archivo_entrada; // puntero al archivo de la base de datos
     // puntero al archivo de salida
    int ejecucion; // variable para controlar la ejecución del hilo
    int fd_respuesta; // descriptor de archivo para el pipe de respuesta
} Datos_hilo; // estructura para pasar datos a los hilos 

//declaracion de variables globales como el buffer, semáforos y variables de control
Solicitud Buffer[BUFFER_SIZE]; // buffer para almacenar las solicitudes
int in = 0; // indice de entrada
int out = 0; // indice de salida
sem_t espacios_disponibles; // semaforo para controlar los espacios disponibles en el buffer
sem_t solicitudes_pendientes; // semaforo para controlar las solicitudes que estan en la lista
sem_t acceso_buffer; // semaforo para controlar el acceso al buffer


//declaración de funciones
void* hilo_auxiliar01(void* arg); // declaración de la función del hilo auxiliar01
void* hilo_auxiliar02(void* arg); // declaración de la función del hilo auxiliar02
InfoLibro buscar_info_libro(FILE *archivo, const char *nombre, int isbn, char estado_objetivo); // declaración de la función para buscar información del libro
int cambiar_estado_libro(FILE *archivo, int numero_linea, char nuevo_estado); // declaración de la función para actualizar el estado de un libro segun la línea 
int actualizar_fecha_linea(FILE *archivo, int numero_linea, int modo); // declaración de la función para actualizar la fecha de un libro segun la solicitud



//FUNCIONES 
void* hilo_auxiliar01(void* arg) {
    Datos_hilo* datos = (Datos_hilo*)arg; // convertir el argumento a la estructura de datos
    FILE* archivo_entrada = datos->archivo_entrada; // obtener el puntero al archivo de la base de datos
    //FILE* archivo_salida = datos->archivo_salida; // obtener el puntero al archivo de salida

    while(datos->ejecucion) { // bucle infinito para procesar las solicitudes

        sem_wait(&solicitudes_pendientes); // esperar a que haya solicitudes pendientes
        sem_wait(&acceso_buffer); // esperar a que haya espacio en el buffer

        Solicitud solicitud = Buffer[out]; // obtener la solicitud del buffer
        out = (out + 1) % BUFFER_SIZE; // actualizar el índice de salida

        sem_post(&acceso_buffer); // liberar el acceso al buffer
        sem_post(&espacios_disponibles); // indicar que hay un espacio disponible en el buffer

        if (solicitud.operacion == 'D') { // si la operación es 'D' (devolución)
            InfoLibro info = buscar_info_libro(archivo_entrada, solicitud.nombre_libro, solicitud.isbn, 'P'); // buscar información del libro
            if(cambiar_estado_libro(archivo_entrada,info.linea_ejemplar_estado,'D') == 1 ){
                actualizar_fecha_linea(archivo_entrada, info.linea_ejemplar_estado, 0); // actualizar la fecha de devolución
                printf("Devolución exitosa del libro: %s con ISBN: %d\n", solicitud.nombre_libro, solicitud.isbn); // imprimir mensaje de confirmación
                char respuesta[256]; // buffer para almacenar la respuesta
                snprintf(respuesta, sizeof(respuesta), "Libro %s con ISBN %d fue devuelto exitosamente.\n", solicitud.nombre_libro, solicitud.isbn); // formatear la respuesta en caso de éxito
                int fd_respuesta = open("/tmp/pipe_respuesta", O_WRONLY); // abrir el pipe de respuesta
                if (fd_respuesta != -1) { // si se pudo abrir el pipe, que escriba la respuesta
                    write(fd_respuesta, respuesta, strlen(respuesta));
                    close(fd_respuesta); // cerrar el pipe de respuesta
                }
            }else{
                char respuesta[256];
                snprintf(respuesta, sizeof(respuesta), "Error al devolver el libro %s con ISBN %d.\n", solicitud.nombre_libro, solicitud.isbn); // formatear la respuesta en caso de error 
                int fd_respuesta = open("/tmp/pipe_respuesta", O_WRONLY);
                if (fd_respuesta != -1) { // si se pudo abrir el pipe, que escriba la respuesta
                    write(fd_respuesta, respuesta, strlen(respuesta));
                    close(fd_respuesta); // cerrar el pipe de respuesta
                }
            
        }
        sem_wait(&solicitudes_pendientes); // esperar a que haya solicitudes pendientes
        sem_wait(&acceso_buffer); // esperar a que haya espacio en el buffer

        solicitud = Buffer[out]; // obtener la solicitud del buffer
        out = (out + 1) % BUFFER_SIZE; // actualizar el índice de salida

        sem_post(&acceso_buffer); // liberar el acceso al buffer
        sem_post(&espacios_disponibles); // indicar que hay un espacio disponible en el buffer
        }
        if (solicitud.operacion == 'R') { // si la operación es 'R' (Renovar)
            InfoLibro info = buscar_info_libro(archivo_entrada, solicitud.nombre_libro, solicitud.isbn, 'P'); // buscar información del libro
            if(actualizar_fecha_linea(archivo_entrada, info.linea_ejemplar_estado, 1) == 1){// actualizar la fecha de renovación
                printf("Renovacion exitosa del libro: %s con ISBN: %d\n", solicitud.nombre_libro, solicitud.isbn); // imprimir mensaje de confirmación
                char respuesta[256]; // buffer para almacenar la respuesta
                snprintf(respuesta, sizeof(respuesta), "Libro %s con ISBN %d fue renovado exitosamente.\n", solicitud.nombre_libro, solicitud.isbn); // formatear la respuesta en caso de éxito
                int fd_respuesta = open("/tmp/pipe_respuesta", O_WRONLY); // abrir el pipe de respuesta
                if (fd_respuesta != -1) { // si se pudo abrir el pipe, que escriba la respuesta
                    write(fd_respuesta, respuesta, strlen(respuesta));
                    close(fd_respuesta); // cerrar el pipe de respuesta
                }
            } else{
                char respuesta[256];
                snprintf(respuesta, sizeof(respuesta), "Error al renovar el libro %s con ISBN %d.\n", solicitud.nombre_libro, solicitud.isbn); // formatear la respuesta en caso de error 
                int fd_respuesta = open("/tmp/pipe_respuesta", O_WRONLY);
                if (fd_respuesta != -1) { // si se pudo abrir el pipe, que escriba la respuesta
                    write(fd_respuesta, respuesta, strlen(respuesta));
                    close(fd_respuesta); // cerrar el pipe de respuesta
                }
            }
        sem_wait(&solicitudes_pendientes); // esperar a que haya solicitudes pendientes
        sem_wait(&acceso_buffer); // esperar a que haya espacio en el buffer

        solicitud = Buffer[out]; // obtener la solicitud del buffer
        out = (out + 1) % BUFFER_SIZE; // actualizar el índice de salida

        sem_post(&acceso_buffer); // liberar el acceso al buffer
        sem_post(&espacios_disponibles); // indicar que hay un espacio disponible en el buffer
        }           
    } 
    return NULL;
}

// esta función busca el libro en el archivo y devuelve la información del libro, es decir, la línea del encabezado, la cantidad de ejemplares y la línea del ejemplar con el estado objetivo
InfoLibro buscar_info_libro(FILE *archivo, const char *nombre, int isbn, char estado_objetivo) { // file es el archivo de la base de datos, nombre es el nombre del libro, isbn es el ISBN del libro y estado_objetivo es el estado que se busca
    char linea[256]; // buffer para almacenar la línea leída del archivo
    InfoLibro info = {-1, 0, -1}; // inicializar la información del libro
    int linea_actual = 0; // contador de líneas leídas

    rewind(archivo); // volver al inicio del archivo, para asegurarse de que se lea desde el principio

    while (fgets(linea, sizeof(linea), archivo)) { // leer una línea del archivo
        linea_actual++; // incrementar el contador de líneas leídas
        char nombre_bd[100]; // buffer para almacenar el nombre del libro leído del archivo
        int isbn_bd, cantidad_bd; // variables para almacenar el ISBN y la cantidad de ejemplares leídos del archivo

        if (sscanf(linea, " %99[^,], %d, %d", nombre_bd, &isbn_bd, &cantidad_bd) == 3) { // leer el nombre, ISBN y cantidad de ejemplares del libro
            if (strcmp(nombre_bd, nombre) == 0 && isbn_bd == isbn) { // si el nombre y el ISBN coinciden
                info.linea_encabezado = linea_actual; // guardar la línea del encabezado
                info.cantidad_ejemplares = cantidad_bd; // guardar la cantidad de ejemplares

                // Buscar el primer ejemplar con estado_objetivo
                for (int i = 0; i < cantidad_bd; i++) { // leer la cantidad de ejemplares que tiene ese libro
                    if (fgets(linea, sizeof(linea), archivo)) { // leer una línea del archivo
                        linea_actual++; // incrementar el contador de líneas leídas
                        char estado; // variable para almacenar el estado del ejemplar leído del archivo
                        if (sscanf(linea, " %*[^,], %c,", &estado) == 1) { // leer el estado del ejemplar
                            if (estado == estado_objetivo && info.linea_ejemplar_estado == -1) { // si el estado coincide con el objetivo y no se ha encontrado otro ejemplar con ese estado
                                info.linea_ejemplar_estado = linea_actual; // guardar la línea del ejemplar con el estado objetivo
                            }
                        }
                    }
                }

                return info; // ya encontró el libro
            } else {
                // Saltar las líneas de ejemplares si no es el libro deseado
                for (int i = 0; i < cantidad_bd; i++) {
                    fgets(linea, sizeof(linea), archivo);
                    linea_actual++;
                }
            }
        }
    }
    return info; // si no lo encontró, .linea_encabezado == -1 es decir que no se encontró el libro
}

// Sobrescribe el estado en una línea específica
int cambiar_estado_libro(FILE *archivo, int numero_linea, char nuevo_estado) {
    char linea[256]; // buffer para almacenar la línea leída del archivo
    int linea_actual = 0; // contador de líneas leídas
    long posicion_inicio = 0; // variable para almacenar la posición del inicio de la línea

    rewind(archivo); // volver al inicio del archivo, para asegurarse de que se lea desde el principio

    while (fgets(linea, sizeof(linea), archivo)) { // leer una línea del archivo
        linea_actual++; // incrementar el contador de líneas leídas
        if (linea_actual == numero_linea) { // si la línea actual es la que se busca
            // Calcular posición del inicio de la línea
            posicion_inicio = ftell(archivo) - strlen(linea); // guardar la posición del inicio de la línea

            // Buscar posición del segundo campo (estado)
            char *primera_coma = strchr(linea, ','); // buscar la primera coma aprovechando el formato del archivo
            if (!primera_coma) 
            return 0;

            char *segunda_coma = strchr(primera_coma + 1, ','); // buscar la segunda coma aprovechando el formato del archivo
            if (!segunda_coma) 
            return 0;

            // Buscar el carácter justo antes de la segunda coma
            int index_estado = (int)(segunda_coma - linea - 1); // calcular el índice del estado 
            if (index_estado < 0 || index_estado >= strlen(linea)) return 0; // verificar que el índice sea válido

            // Mover el puntero al archivo a la posición del estado
            fseek(archivo, posicion_inicio + index_estado, SEEK_SET); // mover el puntero al inicio del estado
            fputc(nuevo_estado, archivo); // sobrescribir el estado
            fflush(archivo); // forzar la escritura en el archivo
            return 1;
        }
    }
    return 0;
}

int actualizar_fecha_linea(FILE *archivo, int numero_linea, int modo) {
    char linea[256]; // buffer para almacenar la línea leída del archivo
    int linea_actual = 0; // contador de líneas leídas
    rewind(archivo); // volver al inicio del archivo, para asegurarse de que se lea desde el principio

    time_t t = time(NULL); // obtener la fecha y hora actual
    struct tm fecha = *localtime(&t); // convertir a estructura tm

    if (modo == 1) { // Sumar 1 día
        // Sumar 7 días
        fecha.tm_mday += 7;
        mktime(&fecha);  // Normalizar
    }

    char nueva_fecha[20]; // buffer para almacenar la nueva fecha
    strftime(nueva_fecha, sizeof(nueva_fecha), "%d-%m-%Y", &fecha); // formatear la fecha

    while (fgets(linea, sizeof(linea), archivo)) { // leer una línea del archivo
        linea_actual++; // incrementar el contador de líneas leídas
        if (linea_actual == numero_linea) { // si la línea actual es la que se busca
            long pos_inicio = ftell(archivo) - strlen(linea); // guardar la posición del inicio de la línea

            // Buscar la última coma
            char *ultima_coma = strrchr(linea, ','); // buscar la última coma aprovechando el formato del archivo
            if (!ultima_coma) // verificar que la coma exista, si no existe no se puede actualizar la fecha
            return 0;

            int offset_fecha = ultima_coma - linea + 2; // Salto ", " y posiciono al inicio de la fecha

            fseek(archivo, pos_inicio + offset_fecha, SEEK_SET); // mover el puntero al inicio de la fecha
            fprintf(archivo, "%s \n", nueva_fecha);// sobrescribir la fecha
            fflush(archivo); // forzar la escritura en el archivo
            return 1;
        }
    }
    return 0;
}

/*=======================================================================================================================================*/

//MAIN 
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

    // Crear pipes si no existen
    printf("Creando pipes...\n");
    if (access(fifo_file, F_OK) == -1 && mkfifo(fifo_file, 0660) == -1) {
        perror("Error al crear pipe de solicitud");
        exit(EXIT_FAILURE);
    }
    if (access(fifo_respuesta, F_OK) == -1 && mkfifo(fifo_respuesta, 0660) == -1) {
        perror("Error al crear pipe de respuesta");
        exit(EXIT_FAILURE);
    }

    // Abrir pipes
    int fd = open(fifo_file, O_RDONLY);
    if (fd == -1) {
        perror("Error al abrir pipe de solicitud");
        exit(EXIT_FAILURE);
    }
    printf("Pipe de solicitud abierto.\n");

    int fd_respuesta = open(fifo_respuesta, O_WRONLY);
    if (fd_respuesta == -1) {
        perror("Error al abrir pipe de respuesta");
        exit(EXIT_FAILURE);
    }
    printf("Pipe de respuesta abierto.\n");

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
        int read_bytes; // variable para almacenar el número de bytes leídos del pipe
        int write_bytes; // variable para almacenar el número de bytes escritos en el archivo de salida

        // Crear semáforos
        sem_init(&espacios_disponibles, 0, BUFFER_SIZE); // inicializar el semáforo de espacios disponibles
        sem_init(&solicitudes_pendientes, 0, 0); // inicializar el semáforo de solicitudes pendientes
        sem_init(&acceso_buffer, 0, 1); // inicializar el semáforo de acceso al buffer

        int ejecucion = 1; // variable para controlar la ejecución del programa

        // Crear hilos
        pthread_t hilo01; // variable para almacenar el identificador del hilo
        Datos_hilo datos_hilo01 = {archivo_entrada,ejecucion,fd_respuesta}; // variable para almacenar los datos del hilo
        if (pthread_create(&hilo01, NULL, hilo_auxiliar01, (void*)&datos_hilo01) != 0) { // crear el hilo
            perror("Error al crear el hilo");
            exit(EXIT_FAILURE);
        }
        
        while(ejecucion) { // bucle infinito para leer del pipe
            read_bytes = read(fd, &solicitud, sizeof(Solicitud)); // leer del pipe
            printf("Leyendo del pipe...\n");
            printf("el libro que se solicito es %s \n",solicitud.nombre_libro);
            if (read_bytes == 0) { // si no hay más datos que leer, salir del bucle
                printf("No hay más datos que leer del pipe.\n");
                break;
            } else if (read_bytes < sizeof(Solicitud)) { // si no se pudo leer la solicitud completa, salir del bucle
                printf("Error al leer del pipe. Bytes leídos: %d\n", read_bytes);
                break;
            }
            if (read_bytes == -1) { // si no se pudo leer del pipe que salga una advertencia
                perror("Error al leer del pipe");
                exit(EXIT_FAILURE);
            }

            if (solicitud.operacion == 'Q') {
                ejecucion = 0; // si la operación es 'Q' se sale del bucle
                close(fd); // cerrar el pipe
                printf("Recibida solicitud de salida. Saliendo del sistema...\n");
                break; // salir del bucle               
            } else if (solicitud.operacion == 'D'){
                char mensaje[256];
                snprintf(mensaje, sizeof(mensaje), "Solicitud de devolucion recibida para el libro: %s con ISBN: %d\n", solicitud.nombre_libro, solicitud.isbn);
                write_bytes = write(fd_respuesta, mensaje, strlen(mensaje));
                if (write_bytes == -1) { // si no se pudo escribir en el pipe que salga una advertencia
                    perror("Error al escribir en el pipe");
                    exit(EXIT_FAILURE);
                }          
                sem_wait(&espacios_disponibles); // esperar a que haya espacio en el buffer
                sem_wait(&acceso_buffer); // esperar a que haya espacio en el buffer

                Buffer[in] = solicitud; // almacenar la solicitud en el buffer
                in = (in + 1) % BUFFER_SIZE; // actualizar el índice de entrada
                /*borrar*/printf("Solicitud de devolucion almacenada en el buffer. ISBN: %d\n", solicitud.isbn); // imprimir mensaje de confirmación
                sem_post(&acceso_buffer); // liberar el acceso al buffer
                sem_post(&solicitudes_pendientes); // indicar que hay una solicitud pendiente
            } else if (solicitud.operacion == 'R') {
                char mensaje[256];
                snprintf(mensaje, sizeof(mensaje), "Solicitud de renovacion recibida para el libro: %s con ISBN: %d\n", solicitud.nombre_libro, solicitud.isbn);
                write_bytes = write(fd_respuesta, mensaje, strlen(mensaje));
                if (write_bytes == -1) { // si no se pudo escribir en el pipe que salga una advertencia
                    perror("Error al escribir en el pipe");
                    exit(EXIT_FAILURE);
                }          
                sem_wait(&espacios_disponibles); // esperar a que haya espacio en el buffer
                sem_wait(&acceso_buffer); // esperar a que haya espacio en el buffer

                Buffer[in] = solicitud; // almacenar la solicitud en el buffer
                in = (in + 1) % BUFFER_SIZE; // actualizar el índice de entrada
                /*borrar*/printf("Solicitud de renovacion almacenada en el buffer. ISBN: %d\n", solicitud.isbn); // imprimir mensaje de confirmación
                sem_post(&acceso_buffer); // liberar el acceso al buffer
                sem_post(&solicitudes_pendientes); // indicar que hay una solicitud pendiente
            } else if(solicitud.operacion == 'P'){
                char mensaje[256];
                snprintf(mensaje, sizeof(mensaje), "Solicitud de prestamo recibida para el libro: %s con ISBN: %d\n", solicitud.nombre_libro, solicitud.isbn);
                write_bytes = write(fd_respuesta, mensaje, strlen(mensaje));
                if (write_bytes == -1) { // si no se pudo escribir en el pipe que salga una advertencia
                    perror("Error al escribir en el pipe");
                    exit(EXIT_FAILURE);
                }
                InfoLibro info = buscar_info_libro(archivo_entrada, solicitud.nombre_libro, solicitud.isbn, 'D'); // buscar información del libro 
                if(cambiar_estado_libro(archivo_entrada,info.linea_ejemplar_estado,'P') == 1 ){
                    printf("Prestamo exitoso del libro: %s con ISBN: %d\n", solicitud.nombre_libro, solicitud.isbn); // imprimir mensaje de confirmación
                    char respuesta[256]; // buffer para almacenar la respuesta
                    snprintf(respuesta, sizeof(respuesta), "Libro %s con ISBN %d fue prestado exitosamente.\n", solicitud.nombre_libro, solicitud.isbn); // formatear la respuesta en caso de éxito
                    int fd_respuesta = open("/tmp/pipe_respuesta", O_WRONLY); // abrir el pipe de respuesta
                    if (fd_respuesta != -1) { // si se pudo abrir el pipe, que escriba la respuesta
                        write(fd_respuesta, respuesta, strlen(respuesta));
                        close(fd_respuesta); // cerrar el pipe de respuesta
                    }
                }
                       
                    }
            }
    
        // Esperar a que el hilo termine
        pthread_join(hilo01, NULL); // esperar a que el hilo termine
        printf("Hilo auxiliar01 terminado.\n");
        
    
    //liberar recursos
    close(fd_respuesta); // cerrar el pipe de respuesta
    close(fd); // cerrar el pipe de solicitudes
    fclose(archivo_entrada); // cerrar el archivo de la base de datos
    //if (archivo_salida != NULL)
    //fclose(archivo_salida);
    sem_destroy(&espacios_disponibles); // destruir el semáforo de espacios disponibles
    sem_destroy(&solicitudes_pendientes); // destruir el semáforo de solicitudes pendientes
    sem_destroy(&acceso_buffer); // destruir el semáforo de acceso al buffer
    return 0;
}
