#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define BLOCK_SIZE (256 * 1024)  // 256K
#define MAX_FILES 250
#define MAX_PATH 256

// Estructura para el encabezado del archivo empaquetado
struct StarHeader {
    int num_files;
    int first_free_block;
};

// Estructura para mantener la información de cada archivo
struct FileEntry {
    char filename[MAX_PATH];
    size_t size;
    int first_block;
    int is_used;
};

// Estructura para el bloque de datos
struct Block {
    int next_block;
    char data[BLOCK_SIZE - sizeof(int)];
};

// Estructura global para el archivo star
struct StarFile {
    struct StarHeader header;
    struct FileEntry file_table[MAX_FILES];
    int fd;  // File descriptor
    char verbose;
};

// Funciones principales
void init_star_file(struct StarFile *star, const char *filename, char verbose) {
    star->fd = open(filename, O_RDWR | O_CREAT, 0644);
    if (star->fd == -1) {
        perror("Error opening file");
        exit(1);
    }
    star->verbose = verbose;
    
    // Inicializar header
    star->header.num_files = 0;
    star->header.first_free_block = -1;
    
    // Escribir header
    write(star->fd, &star->header, sizeof(struct StarHeader));
    
    // Inicializar tabla de archivos
    memset(star->file_table, 0, sizeof(struct FileEntry) * MAX_FILES);
    write(star->fd, star->file_table, sizeof(struct FileEntry) * MAX_FILES);
}

int add_file(struct StarFile *star, const char *filename) {
    struct stat st;
    if (stat(filename, &st) == -1) {
        perror("Error getting file stats");
        return -1;
    }
    
    // Buscar espacio en la tabla de archivos
    int file_index = -1;
    for (int i = 0; i < MAX_FILES; i++) {
        if (!star->file_table[i].is_used) {
            file_index = i;
            break;
        }
    }
    
    if (file_index == -1) {
        fprintf(stderr, "No space in file table\n");
        return -1;
    }
    
    // Abrir archivo fuente
    int src_fd = open(filename, O_RDONLY);
    if (src_fd == -1) {
        perror("Error opening source file");
        return -1;
    }
    
    // Preparar entrada en la tabla
    strncpy(star->file_table[file_index].filename, filename, MAX_PATH - 1);
    star->file_table[file_index].size = st.st_size;
    star->file_table[file_index].is_used = 1;
    
    // Calcular número de bloques necesarios
    int num_blocks = (st.st_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    struct Block block;
    
    // Copiar datos
    int current_block = star->header.first_free_block;
    star->file_table[file_index].first_block = current_block;
    
    for (int i = 0; i < num_blocks; i++) {
        // Leer datos del archivo fuente
        ssize_t bytes_read = read(src_fd, block.data, sizeof(block.data));
        if (bytes_read == -1) {
            perror("Error reading source file");
            close(src_fd);
            return -1;
        }
        
        // Configurar siguiente bloque
        if (i == num_blocks - 1) {
            block.next_block = -1;
        } else {
            block.next_block = current_block + 1;
        }
        
        // Escribir bloque
        lseek(star->fd, current_block * sizeof(struct Block), SEEK_SET);
        write(star->fd, &block, sizeof(struct Block));
        
        current_block = block.next_block;
    }
    
    close(src_fd);
    
    // Actualizar header
    star->header.num_files++;
    lseek(star->fd, 0, SEEK_SET);
    write(star->fd, &star->header, sizeof(struct StarHeader));
    
    // Actualizar tabla de archivos
    lseek(star->fd, sizeof(struct StarHeader), SEEK_SET);
    write(star->fd, star->file_table, sizeof(struct FileEntry) * MAX_FILES);
    
    if (star->verbose) {
        printf("Added file: %s\n", filename);
    }
    
    return 0;
}

int extract_file(struct StarFile *star, const char *filename) {
    // Buscar archivo en la tabla
    int file_index = -1;
    for (int i = 0; i < MAX_FILES; i++) {
        if (star->file_table[i].is_used && 
            strcmp(star->file_table[i].filename, filename) == 0) {
            file_index = i;
            break;
        }
    }
    
    if (file_index == -1) {
        fprintf(stderr, "File not found: %s\n", filename);
        return -1;
    }
    
    // Abrir archivo destino
    int dst_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dst_fd == -1) {
        perror("Error creating destination file");
        return -1;
    }
    
    // Copiar bloques
    struct Block block;
    int current_block = star->file_table[file_index].first_block;
    size_t remaining = star->file_table[file_index].size;
    
    while (current_block != -1 && remaining > 0) {
        // Leer bloque
        lseek(star->fd, current_block * sizeof(struct Block), SEEK_SET);
        read(star->fd, &block, sizeof(struct Block));
        
        // Escribir datos
        size_t to_write = (remaining > sizeof(block.data)) ? 
                         sizeof(block.data) : remaining;
        write(dst_fd, block.data, to_write);
        
        remaining -= to_write;
        current_block = block.next_block;
    }
    
    close(dst_fd);
    
    if (star->verbose) {
        printf("Extracted file: %s\n", filename);
    }
    
    return 0;
}


void extract_all_files(struct StarFile *star) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (star->file_table[i].is_used) {
            if (star->verbose) {
                printf("Extracting: %s\n", star->file_table[i].filename);
            }
            extract_file(star, star->file_table[i].filename);
        }
    }
}

// Función para listar el contenido del archivo
void list_files(struct StarFile *star) {
    printf("Contents of archive:\n");
    printf("%-40s %15s\n", "Filename", "Size");
    printf("---------------------------------------- ---------------\n");
    
    for (int i = 0; i < MAX_FILES; i++) {
        if (star->file_table[i].is_used) {
            printf("%-40s %15zu bytes\n", 
                   star->file_table[i].filename, 
                   star->file_table[i].size);
            
            if (star->verbose > 1) {  // Si se usa -vv
                int block = star->file_table[i].first_block;
                printf("  Block chain: ");
                while (block != -1) {
                    printf("%d -> ", block);
                    struct Block current_block;
                    lseek(star->fd, block * sizeof(struct Block), SEEK_SET);
                    read(star->fd, &current_block, sizeof(struct Block));
                    block = current_block.next_block;
                }
                printf("END\n");
            }
        }
    }
}

// Función mejorada para borrar archivos
int delete_file(struct StarFile *star, const char *filename) {
    int file_index = -1;
    
    // Buscar el archivo
    for (int i = 0; i < MAX_FILES; i++) {
        if (star->file_table[i].is_used && 
            strcmp(star->file_table[i].filename, filename) == 0) {
            file_index = i;
            break;
        }
    }
    
    if (file_index == -1) {
        fprintf(stderr, "File not found: %s\n", filename);
        return -1;
    }
    
    // Obtener el primer bloque del archivo
    int current_block = star->file_table[file_index].first_block;
    
    // Añadir la cadena de bloques a la lista de bloques libres
    if (current_block != -1) {
        // Encontrar el último bloque del archivo
        struct Block block;
        int last_block = current_block;
        
        while (1) {
            lseek(star->fd, last_block * sizeof(struct Block), SEEK_SET);
            read(star->fd, &block, sizeof(struct Block));
            if (block.next_block == -1) break;
            last_block = block.next_block;
        }
        
        // Conectar la cadena con la lista de bloques libres
        block.next_block = star->header.first_free_block;
        lseek(star->fd, last_block * sizeof(struct Block), SEEK_SET);
        write(star->fd, &block, sizeof(struct Block));
        
        // Actualizar el primer bloque libre en el header
        star->header.first_free_block = current_block;
    }
    
    // Marcar el archivo como no usado
    star->file_table[file_index].is_used = 0;
    star->header.num_files--;
    
    // Actualizar el header y la tabla de archivos
    lseek(star->fd, 0, SEEK_SET);
    write(star->fd, &star->header, sizeof(struct StarHeader));
    lseek(star->fd, sizeof(struct StarHeader), SEEK_SET);
    write(star->fd, star->file_table, sizeof(struct FileEntry) * MAX_FILES);
    
    if (star->verbose) {
        printf("Deleted file: %s\n", filename);
    }
    
    return 0;
}

// Función para actualizar un archivo
int update_file(struct StarFile *star, const char *filename) {
    struct stat st;
    if (stat(filename, &st) == -1) {
        perror("Error getting file stats");
        return -1;
    }
    
    // Buscar si el archivo ya existe
    int file_index = -1;
    for (int i = 0; i < MAX_FILES; i++) {
        if (star->file_table[i].is_used && 
            strcmp(star->file_table[i].filename, filename) == 0) {
            file_index = i;
            break;
        }
    }
    
    // Si el archivo no existe, agregarlo normalmente
    if (file_index == -1) {
        return add_file(star, filename);
    }
    
    // Verificar si el tamaño del archivo ha cambiado
    if (star->file_table[file_index].size == (size_t)st.st_size) {
        // Comparar contenido para ver si realmente necesita actualización
        int src_fd = open(filename, O_RDONLY);
        if (src_fd == -1) {
            perror("Error opening source file");
            return -1;
        }
        
        struct Block old_block, new_block;
        int current_block = star->file_table[file_index].first_block;
        int needs_update = 0;
        
        while (current_block != -1) {
            // Leer bloque del archivo star
            lseek(star->fd, current_block * sizeof(struct Block), SEEK_SET);
            read(star->fd, &old_block, sizeof(struct Block));
            
            // Leer bloque del archivo fuente
            read(src_fd, new_block.data, sizeof(new_block.data));
            
            // Comparar contenido
            if (memcmp(old_block.data, new_block.data, sizeof(new_block.data)) != 0) {
                needs_update = 1;
                break;
            }
            
            current_block = old_block.next_block;
        }
        
        close(src_fd);
        
        if (!needs_update) {
            if (star->verbose) {
                printf("File %s is already up to date\n", filename);
            }
            return 0;
        }
    }
    
    // Si llegamos aquí, necesitamos actualizar el archivo
    // Primero borramos el archivo existente
    delete_file(star, filename);
    
    // Luego agregamos el nuevo contenido
    return add_file(star, filename);
}

// Función para agregar contenido a un archivo existente
int append_to_file(struct StarFile *star, const char *filename, const char *content_file) {
    // Buscar el archivo en la tabla
    int file_index = -1;
    for (int i = 0; i < MAX_FILES; i++) {
        if (star->file_table[i].is_used && 
            strcmp(star->file_table[i].filename, filename) == 0) {
            file_index = i;
            break;
        }
    }
    
    if (file_index == -1) {
        fprintf(stderr, "File not found: %s\n", filename);
        return -1;
    }

    // Abrir el archivo con el contenido a agregar
    int src_fd = open(content_file, O_RDONLY);
    if (src_fd == -1) {
        perror("Error opening content file");
        return -1;
    }

    // Obtener el tamaño del archivo a agregar
    struct stat st;
    if (stat(content_file, &st) == -1) {
        perror("Error getting content file stats");
        close(src_fd);
        return -1;
    }

    // Encontrar el último bloque del archivo existente
    struct Block block;
    int current_block = star->file_table[file_index].first_block;
    int last_block = current_block;
    size_t last_block_used = star->file_table[file_index].size % (BLOCK_SIZE - sizeof(int));
    
    while (1) {
        lseek(star->fd, last_block * sizeof(struct Block), SEEK_SET);
        read(star->fd, &block, sizeof(struct Block));
        if (block.next_block == -1) break;
        last_block = block.next_block;
    }

    // Llenar el espacio restante en el último bloque
    if (last_block_used > 0) {
        lseek(star->fd, last_block * sizeof(struct Block) + sizeof(int) + last_block_used, SEEK_SET);
        size_t space_left = sizeof(block.data) - last_block_used;
        char buffer[space_left];
        ssize_t bytes_read = read(src_fd, buffer, space_left);
        if (bytes_read > 0) {
            write(star->fd, buffer, bytes_read);
            st.st_size -= bytes_read;
        }
    }

    // Calcular bloques adicionales necesarios
    int additional_blocks = (st.st_size + BLOCK_SIZE - sizeof(int) - 1) / (BLOCK_SIZE - sizeof(int));
    
    // Agregar nuevos bloques
    for (int i = 0; i < additional_blocks; i++) {
        // Obtener siguiente bloque libre
        int new_block = star->header.first_free_block;
        if (new_block == -1) {
            // No hay bloques libres, crear uno nuevo al final
            new_block = lseek(star->fd, 0, SEEK_END) / sizeof(struct Block);
        } else {
            // Actualizar lista de bloques libres
            lseek(star->fd, new_block * sizeof(struct Block), SEEK_SET);
            read(star->fd, &block, sizeof(struct Block));
            star->header.first_free_block = block.next_block;
        }

        // Leer datos del archivo fuente
        memset(block.data, 0, sizeof(block.data));
        ssize_t bytes_read = read(src_fd, block.data, sizeof(block.data));
        if (bytes_read == -1) {
            perror("Error reading source file");
            close(src_fd);
            return -1;
        }

        // Configurar siguiente bloque
        block.next_block = -1;

        // Escribir nuevo bloque
        lseek(star->fd, new_block * sizeof(struct Block), SEEK_SET);
        write(star->fd, &block, sizeof(struct Block));

        // Actualizar el enlace del bloque anterior
        if (i == 0) {
            block.next_block = new_block;
            lseek(star->fd, last_block * sizeof(struct Block), SEEK_SET);
            write(star->fd, &block, sizeof(struct Block));
        }

        last_block = new_block;
    }

    // Actualizar tamaño del archivo en la tabla
    star->file_table[file_index].size += st.st_size;

    // Actualizar header y tabla de archivos
    lseek(star->fd, 0, SEEK_SET);
    write(star->fd, &star->header, sizeof(struct StarHeader));
    lseek(star->fd, sizeof(struct StarHeader), SEEK_SET);
    write(star->fd, star->file_table, sizeof(struct FileEntry) * MAX_FILES);

    close(src_fd);

    if (star->verbose) {
        printf("Appended content from %s to %s\n", content_file, filename);
    }

    return 0;
}

// Función para desfragmentar el archivo
int pack_file(struct StarFile *star) {
    // Crear archivo temporal
    char temp_filename[] = "temp_XXXXXX";
    int temp_fd = mkstemp(temp_filename);
    if (temp_fd == -1) {
        perror("Error creating temporary file");
        return -1;
    }

    // Copiar header y tabla de archivos al archivo temporal
    lseek(star->fd, 0, SEEK_SET);
    struct StarHeader new_header = star->header;
    new_header.first_free_block = -1; // No habrá bloques libres después de desfragmentar
    write(temp_fd, &new_header, sizeof(struct StarHeader));
    write(temp_fd, star->file_table, sizeof(struct FileEntry) * MAX_FILES);

    // Recorrer todos los archivos y reescribirlos de manera contigua
    int next_block = 0;
    struct Block block;
    
    for (int i = 0; i < MAX_FILES; i++) {
        if (star->file_table[i].is_used) {
            int old_first_block = star->file_table[i].first_block;
            star->file_table[i].first_block = next_block;

            // Copiar todos los bloques del archivo
            int current_block = old_first_block;
            size_t remaining = star->file_table[i].size;
            
            while (current_block != -1 && remaining > 0) {
                // Leer bloque original
                lseek(star->fd, current_block * sizeof(struct Block), SEEK_SET);
                read(star->fd, &block, sizeof(struct Block));

                // Ajustar el siguiente bloque
                size_t current_size = (remaining > sizeof(block.data)) ? 
                                    sizeof(block.data) : remaining;
                
                if (remaining > sizeof(block.data)) {
                    block.next_block = next_block + 1;
                } else {
                    block.next_block = -1;
                }

                // Escribir bloque en nueva posición
                lseek(temp_fd, next_block * sizeof(struct Block), SEEK_SET);
                write(temp_fd, &block, sizeof(struct Block));

                remaining -= current_size;
                current_block = block.next_block;
                next_block++;
            }
        }
    }

    // Actualizar la tabla de archivos en el archivo temporal
    lseek(temp_fd, sizeof(struct StarHeader), SEEK_SET);
    write(temp_fd, star->file_table, sizeof(struct FileEntry) * MAX_FILES);

    // Cerrar el archivo original
    close(star->fd);

    // Reemplazar el archivo original con el temporal
    rename(temp_filename, "temp.star"); // Primero renombrar a un nombre fijo
    star->fd = temp_fd;

    if (star->verbose) {
        printf("File packed successfully\n");
    }

    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <options> <archive> [files...]\n", argv[0]);
        fprintf(stderr, "Options:\n");
        fprintf(stderr, "  -c: Create new archive\n");
        fprintf(stderr, "  -x: Extract files\n");
        fprintf(stderr, "  -t, --list: List contents\n");
        fprintf(stderr, "  -u: Update files\n");
        fprintf(stderr, "  -d, --delete: Delete files\n");
        fprintf(stderr, "  -r, --append: Append content to file\n");
        fprintf(stderr, "  -p, --pack: Defragment archive\n");
        fprintf(stderr, "  -v: Verbose output\n");
        fprintf(stderr, "  -f: Specify archive file\n");
        return 1;
    }
    
    struct StarFile star;
    char verbose = 0;
    char *archive_name = NULL;
    char operation = 0;
    char *append_to = NULL;   
    
    // Procesar opciones
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "--delete") == 0) {
                operation = 'd';
            } else if (strcmp(argv[i], "--list") == 0) {
                operation = 't';
            } else if (strcmp(argv[i], "--append") == 0 || strcmp(argv[i], "-r") == 0) {
                operation = 'r';
                if (i + 1 < argc) {
                    append_to = argv[++i];
                }
            } else if (strcmp(argv[i], "--pack") == 0 || strcmp(argv[i], "-p") == 0) {
                operation = 'p';
            } else {
                for (int j = 1; argv[i][j]; j++) {
                    switch (argv[i][j]) {
                        case 'c': operation = 'c'; break;
                        case 'x': operation = 'x'; break;
                        case 't': operation = 't'; break;
                        case 'u': operation = 'u'; break;
                        case 'r': 
                            operation = 'r';
                            if (i + 1 < argc) {
                                append_to = argv[++i];
                            }
                            break;
                        case 'p': operation = 'p'; break;
                        case 'v': verbose++; break;
                        case 'f': 
                            if (i + 1 < argc) {
                                archive_name = argv[++i];
                            }
                            break;
                    }
                }
            }
        }
    }
    
    if (!archive_name) {
        fprintf(stderr, "Archive name must be specified\n");
        return 1;
    }
    
    // Inicializar archivo star
    init_star_file(&star, archive_name, verbose);
    
    // Leer la tabla de archivos existente si no estamos creando un nuevo archivo
    if (operation != 'c') {
        lseek(star.fd, 0, SEEK_SET);
        read(star.fd, &star.header, sizeof(struct StarHeader));
        read(star.fd, star.file_table, sizeof(struct FileEntry) * MAX_FILES);
    }
    
    switch (operation) {
        case 'c':
            for (int i = 3; i < argc; i++) {
                add_file(&star, argv[i]);
            }
            break;
            
        case 'x':
            if (argc == 3) {
                extract_all_files(&star);
            } else {
                for (int i = 3; i < argc; i++) {
                    extract_file(&star, argv[i]);
                }
            }
            break;
            
        case 't':
            list_files(&star);
            break;
            
        case 'd':
            for (int i = 3; i < argc; i++) {
                delete_file(&star, argv[i]);
            }
            break;
            
        case 'u':
            for (int i = 3; i < argc; i++) {
                update_file(&star, argv[i]);
            }
            break;
            
        case 'r':
            if (!append_to) {
                fprintf(stderr, "Missing destination filename for append operation\n");
                close(star.fd);
                return 1;
            }
            // El archivo a agregar será el siguiente argumento después del nombre destino
            for (int i = 3; i < argc; i++) {
                if (strcmp(argv[i], append_to) != 0) {  // Evitar el nombre del archivo destino
                    append_to_file(&star, append_to, argv[i]);
                }
            }
            break;
            
        case 'p':
            pack_file(&star);
            break;
            
        default:
            fprintf(stderr, "No operation specified\n");
            break;
    }
    
    close(star.fd);
    return 0;
}