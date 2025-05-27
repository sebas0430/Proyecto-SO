#  Sistema para Préstamo de Libros

Sistema concurrente implementado en C para la gestión de préstamos de libros en una biblioteca. Utiliza procesos e hilos POSIX, sincronización con semáforos y comunicación mediante pipes nombrados (FIFO). Este proyecto fue desarrollado como entrega final del proyecto para la materia de **Sistemas Operativos**.

## Estructura del Proyecto

```
Proyecto-SO/
├── receptor.c           # Proceso receptor que administra solicitudes y actualiza la base de datos
├── solicitante.c        # Proceso solicitante, modo interactivo o lectura desde archivo
├── db_file.txt          # Base de datos con libros y ejemplares
├── solicitudes.txt      # Archivo con solicitudes de prueba
├── makefile             # Script de compilación
└── README.md            # Este archivo
```

##  Compilación

Ejecuta el siguiente comando en la raíz del proyecto:

```bash
make
```

Esto compila `receptor.c` y `solicitante.c` generando los ejecutables `receptor` y `solicitante`.

## Ejecución

### Receptor (proceso principal)

```bash
./receptor -p pipebiblioteca -f db_file.txt -v
```

- `-p`: nombre del pipe a usar
- `-f`: archivo de base de datos
- `-v`: modo verbose (opcional para mayor información en consola)

### Solicitante (modo por archivo)

```bash
./solicitante -p pipebiblioteca -i solicitudes.txt
```

### Solicitante (modo interactivo)

```bash
./solicitante -p pipebiblioteca
```

##  Pruebas

El sistema fue validado con múltiples casos: préstamo, devolución, renovación y generación de reportes. La descripción detallada del plan de pruebas se encuentra en el documento PDF adjunto.


##  Autores

SEBASTIÁN SÁNCHEZ OLAYA
TOMÁS OSPINA ULLOA
DAVID SANTIAGO RODRÍGUEZ PRIETO 
IVÁN CORTÉS CONSTAIN


