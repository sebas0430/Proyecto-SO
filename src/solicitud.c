#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

typedef struct { // estructura para almacenar la solicitud
    char operacion; // 'P', 'D', 'R', 'Q'
    char nombre_libro[50];
    int isbn;
} Solicitud;

void mostrar_menu() { // función para mostrar el menú de opciones
    printf("\n=== MENÚ DE PRÉSTAMO DE LIBROS ===\n");
    printf("1. Solicitar préstamo de un libro (P)\n");
    printf("2. Renovar un libro (R)\n");
    printf("3. Devolver un libro (D)\n");
    printf("4. Salir(Q)\n");
    printf("Seleccione una opción: ");
}

Solicitud leer_solicitud_teclado() { // función para leer la solicitud desde el teclado
    Solicitud s;
    char opcion;
    mostrar_menu();
    scanf(" %c", &opcion);

    switch(opcion) {
        case '1': s.operacion = 'P'; break;
        case '2': s.operacion = 'R'; break;
        case '3': s.operacion = 'D'; break;
        case '4': s.operacion = 'Q'; break;
        default:
            printf("Opción no válida. Intente nuevamente.\n");
            return leer_solicitud_teclado();
    }
    if(s.operacion == 'Q') return s; // en caso de salir, no se pide más información

    scanf(" %49[^\n]", s.nombre_libro); // leer el nombre del libro con límite
    printf("Ingrese el nombre del libro: ");
    scanf(" %49[^\n]", s.nombre_libro); // leer el nombre del libro con límite
    printf("Ingrese el código del libro: ");
    scanf(" %d", &s.isbn); // leer el código del libro

    return s;
}

void enviar_solicitud(int pipe_fd, Solicitud solicitud) {
    if (write(pipe_fd, &solicitud, sizeof(Solicitud)) == -1) { // escribir la solicitud en el pipe para enviar al receptor
        perror("Error al escribir en el pipe");
    }
}

int main(int argc, char *argv[]) {
    char* nombre_pipe = NULL;
    char* input_file = NULL;

    for (int i = 1 ; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) // si se encuentra la opción -p
            nombre_pipe = argv[++i]; // asignar el nombre del pipe
        else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) // si se encuentra la opción -i
            input_file = argv[++i]; // asignar el nombre del archivo de entrada
    }

    if (!nombre_pipe) {
        fprintf(stderr, "Uso: %s -p <nombre_pipe> -i(opcional) <input_file>\n", argv[0]);
        return 1;
    }

    char ruta_pipe[64], ruta_pipe_respuesta[64];
    snprintf(ruta_pipe, sizeof(ruta_pipe), "/tmp/%s", nombre_pipe); // crear la ruta del pipe de solicitud
    snprintf(ruta_pipe_respuesta, sizeof(ruta_pipe_respuesta), "/tmp/pipe_respuesta"); // crear la ruta del pipe de respuesta

    printf("Abriendo pipes...\n");

    int pipe_fd = open(ruta_pipe, O_WRONLY); // abrir el pipe de solicitud este solo escribe
    if (pipe_fd == -1) {
        perror("Error al abrir pipe de solicitud");
        exit(EXIT_FAILURE);
    }
    printf("Pipe de solicitud abierto correctamente.\n");

    int pipe_fd_respuesta = open(ruta_pipe_respuesta, O_RDONLY); // abrir el pipe de respuesta este solo lee
    if (pipe_fd_respuesta == -1) {
        perror("Error al abrir pipe de respuesta");
        exit(EXIT_FAILURE);
    }
    printf("Pipe de respuesta abierto correctamente.\n");

    if (input_file) { // si se pasa un archivo de entrada
        FILE *file = fopen(input_file, "r"); // abrir el archivo de entrada donde estan las instrucciones o solicitudes
        if (!file) {
            perror("Error al abrir el archivo de entrada");
            close(pipe_fd);
            exit(EXIT_FAILURE);
        }

        Solicitud s; // estructura para almacenar la solicitud
        char linea[128]; // buffer para almacenar la línea leída del archivo
        while (fgets(linea, sizeof(linea), file)) { // leer una línea del archivo
            if (sscanf(linea, " %c , %49[^,] , %d", &s.operacion, s.nombre_libro, &s.isbn) == 3) { // leer la operación, el nombre del libro y el ISBN
                printf("Enviando solicitud de %c para libro '%s' con código %d...\n", s.operacion, s.nombre_libro, s.isbn); // imprimir mensaje de confirmación
                if (s.operacion == 'Q') break; // si la operación es 'Q' salir del bucle
                enviar_solicitud(pipe_fd, s); // se envia la solicitud al receptor
            } 
        }
        
        fclose(file); // cerrar el archivo de entrada
    } else {
        Solicitud s;
        do {
            s = leer_solicitud_teclado(); // leer la solicitud desde el teclado
            printf("Enviando solicitud de %c para libro '%s' con código %d...\n", s.operacion, s.nombre_libro, s.isbn); // imprimir mensaje de confirmación
            enviar_solicitud(pipe_fd, s); // se envia la solicitud al receptor

            char respuesta[100]; // buffer para almacenar la respuesta
            ssize_t n = read(pipe_fd_respuesta, respuesta, sizeof(respuesta) - 1); // leer la respuesta del receptor
            if (n > 0) { // si se leyeron bytes
                respuesta[n] = '\0'; // agregar el terminador nulo al final de la cadena
                printf("Servidor: %s\n", respuesta); // imprimir la respuesta del receptor
            } else {
                perror("Error al leer respuesta del servidor"); // si no se pudo leer la respuesta
            }
        } while (s.operacion != 'Q');
    }

    printf("Cerrando programa solicitud.\n");
    // cerrar los pipes
    close(pipe_fd); 
    close(pipe_fd_respuesta);
    return 0;
}
