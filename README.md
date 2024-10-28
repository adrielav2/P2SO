# Proyecto de Empaquetador de Archivos

## Descripción
Este proyecto implementa un programa de empaquetado de archivos en un entorno Linux Ubuntu 22.04 LTS. El programa permite crear, extraer, actualizar y eliminar archivos, con un enfoque en la eficiencia y el rendimiento. A través de pruebas exhaustivas, se han evaluado diversas funcionalidades del programa, incluida su capacidad para manejar operaciones concurrentes y su rendimiento con diferentes tamaños de archivos.

## Requisitos
- Sistema operativo Linux Ubuntu 22.04 LTS
- Compilador de C (gcc)

## Compilación
Para compilar el programa, abre una terminal y navega al directorio donde se encuentra el archivo `star.c`. Luego, ejecuta el siguiente comando:
```bash
gcc -o star star.c
```

## Funcionamiento 

Para ejecutar el programa `star.c`, utiliza la siguiente sintaxis en la terminal:

```bash
./star [opciones]
```

Donde las opciones incluyen:

-c, --create: Crea un nuevo archivo.
-x, --extract: Extrae el contenido de un archivo.
-t, --list: Lista los contenidos de un archivo.
--delete: Borra entradas desde un archivo.
-u, --update: Actualiza el contenido de un archivo existente.
-v, --verbose: Muestra un reporte de las acciones a medida que se van realizando.
-f, --file: Empaca contenidos de un archivo. Si no está presente, asume la entrada estándar.
-r, --append: Agrega contenido a un archivo existente.
-p, --pack: Desfragmenta el contenido del archivo.

Luego de eso dependiendo de la opción se agregans los archivos a extraer, a crear, a actualizar, borrar, agregar, o fragmentar por medio de su nombre.

Por ejemplo:

```bash
./star -cvf archivos.star archivo1.txt archivo2.txt archivo3.txt
./star -xv archivos.star
./star -rvf archivos.star archivo4.txt
./star -uvf archivos.star archivo2.txt
```



 
