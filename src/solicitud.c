#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

typedef struct {
    char operacion; // 'P', 'D', etc.
    char nombre_libro[50];
    int isbn;
} Solicitud;

void mostrar_menu() {
    printf("\n=== MENÚ DE PRÉSTAMO DE LIBROS ===\n");
    printf("1. Solicitar préstamo de un libro (P)\n");
    printf("2. Renovar un libro (R)\n");
    printf("3. Devolver un libro (D)\n");
    printf("Seleccione una opción: ");
}

Solicitud leer_solicitud_teclado() {
    Solicitud s;
    char opcion;
    mostrar_menu();
    scanf(" %c", &opcion);

    switch(opcion) {
        case '1': s.operacion = 'P'; break;
        case '2': s.operacion = 'R'; break;
        case '3': s.operacion = 'D'; break;
        default:
            printf("Opción no válida. Intente nuevamente.\n");
            return leer_solicitud_teclado();
    }

    printf("Ingrese el nombre del libro: ");
    scanf(" %[^\n]", s.nombre_libro);
    printf("Ingrese el código del libro: ");
    scanf(" %d", &s.isbn);

    return s;
}

void enviar_solicitud(int pipe_fd, Solicitud solicitud) {
    if (write(pipe_fd, &solicitud, sizeof(Solicitud)) == -1) {
        perror("Error al escribir en el pipe");
    }
}

int main(int argc, char *argv[]) {
    char* nombre_pipe = NULL;
    char* input_file = NULL;

    for (int i = 1 ; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc)
            nombre_pipe = argv[++i];
        else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc)
            input_file = argv[++i];
    }

    if (!nombre_pipe) {
        fprintf(stderr, "Uso: %s -p <nombre_pipe> -i(opcional) <input_file>\n", argv[0]);
        return 1;
    }

    char ruta_pipe[64], ruta_pipe_respuesta[64];
    snprintf(ruta_pipe, sizeof(ruta_pipe), "/tmp/%s", nombre_pipe);
    snprintf(ruta_pipe_respuesta, sizeof(ruta_pipe_respuesta), "/tmp/pipe_respuesta");

    printf("Abriendo pipes...\n");

    int pipe_fd = open(ruta_pipe, O_WRONLY);
    if (pipe_fd == -1) {
        perror("Error al abrir pipe de solicitud");
        exit(EXIT_FAILURE);
    }
    printf("Pipe de solicitud abierto correctamente.\n");

    int pipe_fd_respuesta = open(ruta_pipe_respuesta, O_RDONLY);
    if (pipe_fd_respuesta == -1) {
        perror("Error al abrir pipe de respuesta");
        exit(EXIT_FAILURE);
    }
    printf("Pipe de respuesta abierto correctamente.\n");

    if (input_file) {
        FILE *file = fopen(input_file, "r");
        if (!file) {
            perror("Error al abrir el archivo de entrada");
            close(pipe_fd);
            exit(EXIT_FAILURE);
        }

        Solicitud s;
        while (fscanf(file," %c, %49[^,], %d", &s.operacion, s.nombre_libro, &s.isbn) == 3) {
            if (s.operacion == 'Q') break;
            enviar_solicitud(pipe_fd, s);
        }
        fclose(file);
    } else {
        Solicitud s;
        do {
            s = leer_solicitud_teclado();
            printf("Enviando solicitud de %c para libro '%s' con código %d...\n", s.operacion, s.nombre_libro, s.isbn);
            enviar_solicitud(pipe_fd, s);

            char respuesta[100];
            ssize_t n = read(pipe_fd_respuesta, respuesta, sizeof(respuesta) - 1);
            if (n > 0) {
                respuesta[n] = '\0';
                printf("Servidor: %s\n", respuesta);
            } else {
                perror("Error al leer respuesta del servidor");
            }
        } while (s.operacion != 'Q');
    }

    printf("Cerrando programa solicitud.\n");
    close(pipe_fd);
    close(pipe_fd_respuesta);
    return 0;
}
