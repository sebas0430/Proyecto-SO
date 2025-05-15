#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>

#define MAX_LIBROS 100

typedef struct {
    char nombreLibro[50];
    char codigo[10];
    char operacion; // 'P' para préstamo, 'R' para renovación, 'D' para devolución
} Solicitud;

typedef struct {
    char status; //'P' para préstamo, 'R' para renovación, 'D' para devolución
    char nombreLibro[50];
    char codigo[10];
    int cantidad;
    time_t fecha; // Formato: YYYY-MM-DD
} Libro;

Solicitud leer_solicitud(char* nombre_pipe) {
    int pipe_fd = open(nombre_pipe, O_RDONLY);
    if (pipe_fd == -1) {
        perror("Error al abrir el pipe");
        exit(EXIT_FAILURE);
    }

    Solicitud s;
    read(pipe_fd, &s, sizeof(Solicitud));
    close(pipe_fd);
}
//Esta funcion lee el archivo de la base de datos y lo guarda en una estructura Libro
void leer_BD(char* archivo, Libro* libros[]) {
    Libro libro;
    FILE *file = fopen(archivo, "r");
    if (file == NULL) {
        perror("Error al abrir el archivo de entrada");
        exit(EXIT_FAILURE);
    }// REVISAR EL FORMATO DE LA FECHA
    // Leer libros desde el archivo
    while(fscanf(file, "%s %s %d %ld", libro.nombreLibro, libro.codigo, &libro.cantidad, &libro.fecha) != EOF) {
        // Aquí puedes procesar cada libro leído y lo almaceno en un array de estructura Libro
        libros[libro.cantidad] = malloc(sizeof(Libro));
        //agregar la cantidad de libros
        libros[libro.cantidad]->cantidad = libro.cantidad;
}
}

void Presatamo_libros(Solicitud s, Libro* libros[], int cantidad_libros) { 
    for (int i = 0; i < cantidad_libros; i++) {
        if (strcmp(libros[i]->nombreLibro, s.nombreLibro) == 0) {
            if (libros[i]->cantidad > 0) {
                libros[i]->cantidad--;
                printf("Préstamo exitoso del libro: %s\n", libros[i]->nombreLibro);
            } else {
                printf("No hay copias disponibles del libro: %s\n", libros[i]->nombreLibro);
            }
            break;
        }
    }
}

void Devolucion_libros(Solicitud s, Libro* libros[], int cantidad_libros) {
    for (int i = 0; i < cantidad_libros; i++) {
        if (strcmp(libros[i]->nombreLibro, s.nombreLibro) == 0) {
            libros[i]->cantidad++;
            printf("Devolución exitosa del libro: %s\n", libros[i]->nombreLibro);
            break;
        }
    }
}


int main(int argc, char *argv[]) {
    Libro* libros[MAX_LIBROS];

    if (argc != 2) {
        fprintf(stderr, "Uso: %s <nombre_pipe>\n", argv[0]);
        return 1;
    }

    char* nombre_pipe = argv[1];
    Solicitud s = leer_solicitud(nombre_pipe);

    switch (s.operacion) {
        case 'P':
            printf("Solicitud de préstamo recibida:\n");
            break;
        case 'R':
            printf("Solicitud de renovación recibida:\n");
            break;
        case 'D':
            printf("Solicitud de devolución recibida:\n");
            break;
        case 't':
            printf("Solicitud de reporte recibida:\n");
            break;
        case 's':
            printf("Solicitud de salida recibida:\n");
            break;
        default:
            printf("Operación no válida.\n");
            return 1;


    }

}