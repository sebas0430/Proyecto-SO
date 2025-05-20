#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h> 

typedef struct {
    char nombreLibro[50];
    char codigo[10];
    char operacion; // 'P' para préstamo, 'R' para renovación, 'D' para devolución
} Solicitud;

void mostrar_menu() {
    printf("\n=== MENÚ DE PRÉSTAMO DE LIBROS ===\n");
    printf("1. Solicitar préstamo de un libro (P)\n");
    printf("2. Renovar un libro (R)\n");
    printf("3. Devolver un libro (D)\n");
    printf("4. Salir (s)\n");
    printf("5. Mostrar reporte (t)\n");
    printf("Seleccione una opción: ");
}

/* Función para leer una solicitud desde el teclado,
esta función muestra un menú y solicita al usuario que ingrese una opción
y los datos del libro correspondiente a la opción seleccionada.
si el usuario selecciona salir, se devuelve una solicitud con operacion 'Q'.
si se selecciona una opción válida, se devuelve una solicitud con los datos ingresados.
*/
Solicitud leer_solicitud_teclado() {
    Solicitud s;
    char opcion;
    mostrar_menu();
    scanf(" %c", &opcion);

    switch(opcion) {
        case '1':
            s.operacion = 'P';
            break;
        case '2':
            s.operacion = 'R';
            break;
        case '3':
            s.operacion = 'D';
            break;
        case '4':
            s.operacion = 's';
            return s;
        case '5':
            s.operacion = 't';
            return s;
        default:
            printf("Opción no válida. Intente nuevamente.\n");
            return leer_solicitud_teclado();
    }

    if (s.operacion != 's') {
        printf("Ingrese el nombre del libro: ");
        scanf(" %[^\n]", s.nombreLibro);
        printf("Ingrese el código del libro: ");
        scanf(" %[^\n]", s.codigo);
    }
    return s;
}


// Función para enviar una solicitud al receptor (RP) y esperar respuesta
void enviar_solicitud(int pipe_fd, Solicitud solicitud) {
    if (write(pipe_fd, &solicitud, sizeof(Solicitud)) == -1) {
        perror("Error al escribir en el pipe");
    }
    printf("Solicitud enviada: %c, %s, %s\n", solicitud.operacion, solicitud.nombreLibro, solicitud.codigo);
}

int main(int argc, char *argv[]) {

    char* nombre_pipe = NULL;
    char* input_file = NULL;


    // Procesar argumentos de línea de comandos
    for (int i = 1 ; i < argc; i++)
    {   //este es para leer el nombre del pipe
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            nombre_pipe = argv[i + 1];
            i++;
        }
        //este es para leer el nombre del archivo de entrada 
        else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
            input_file = argv[i + 1];
            i++;
        }
    }
    // Verificar si se proporcionó el nombre del pipe
    if (nombre_pipe == NULL ) {
        fprintf(stderr, "Uso: %s -p <nombre_pipe> -i(opcional) <input_file>\n", argv[0]);
        return 1;
    }

    // Crear el pipe en modo lectura o escritura
    int pipe_fd = open(nombre_pipe,  O_RDWR);   
    
    if (pipe_fd == -1) {
        perror("Error al abrir el pipe");
        exit(EXIT_FAILURE);
    }
    // Leer solicitudes desde el archivo de entrada en caso de que se haya proporcionado
   
    if(input_file != NULL) {
        FILE *file = fopen(input_file, "r");
        printf("existe un archivo de entrada\n");
        if (file == NULL) {
            perror("Error al abrir el archivo de entrada");
            close(pipe_fd);
            exit(EXIT_FAILURE);
        }
        Solicitud s;
        while (fscanf(file," %c, %49[^,], %9s" , &s.operacion,s.nombreLibro, s.codigo) == 3) {
            if(s.operacion == 'Q') {
                break;
            }else{
            printf("\nse envio la solicitud\n");    
            enviar_solicitud(pipe_fd, s);
        }
        }
        fclose(file);
    } 
    //menu para leer solicitudes desde el teclado
        Solicitud s;
        do {
            s = leer_solicitud_teclado();              
            enviar_solicitud(pipe_fd, s);
        } while (s.operacion != 's');
    


    // Cerrar el pipe
    close(pipe_fd);  // Usa close() en lugar de pclose()
    return 0;
}
