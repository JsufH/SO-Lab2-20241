#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <ctype.h>
#include "wish_utils.h"

#define MAX_HISTORY 10
#define MAX_LINE_LENGTH 256

// === Declaración de funciones ===
void set_terminal_raw_mode(struct termios *old_term, struct termios *new_term);
void restore_terminal_mode(struct termios *old_term);
void navigate_command_history(char *history[], char *line, int *history_count, int *history_idx, int *line_idx, char *ch);
void handle_backspace_key(char *line, int *line_idx);
void add_command_to_history(char *history[], char *line, int *line_idx, int *history_count, int *history_idx);
void handle_input(char *line, int *line_idx, char *ch);
char *trimString(char *str);
void printError();
void redirection(char *line);

// === Constantes globales ===
char *mypath[] = {"/bin/", "", NULL, NULL, NULL, NULL};
char error_message[30] = "An error has occurred\n";

// === Funciones auxiliares ===

// Configura la terminal en modo "raw" para capturar entradas sin buffer
void set_terminal_raw_mode(struct termios *old_term, struct termios *new_term) {
    tcgetattr(STDIN_FILENO, old_term);
    *new_term = *old_term;
    new_term->c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, new_term);
}

// Restaura el modo de terminal anterior
void restore_terminal_mode(struct termios *old_term) {
    tcsetattr(STDIN_FILENO, TCSANOW, old_term);
}

// Navega por el historial de comandos con las teclas de flecha
void navigate_command_history(char *history[], char *line, int *history_count, int *history_idx, int *line_idx, char *ch) {
    *ch = getchar(); // '[' character
    *ch = getchar(); // Arrow key code

    if (*ch == 'A' && *history_count > 0 && *history_idx > 0) { // Flecha arriba
        (*history_idx)--;
    } else if (*ch == 'B' && *history_count > 0 && *history_idx < *history_count - 1) { // Flecha abajo
        (*history_idx)++;
    }

    memset(line, 0, MAX_LINE_LENGTH);
    strncpy(line, history[*history_idx], MAX_LINE_LENGTH - 1);
    *line_idx = strlen(line);
    printf("\033[2K\r%s", line);
    fflush(stdout);
}

// Maneja el retroceso (tecla Backspace)
void handle_backspace_key(char *line, int *line_idx) {
    if (*line_idx > 0) {
        putchar('\b');
        putchar(' ');
        putchar('\b');
        (*line_idx)--;
        line[*line_idx] = 0;
    }
}

// Agrega un comando al historial
void add_command_to_history(char *history[], char *line, int *line_idx, int *history_count, int *history_idx) {
    putchar('\n');
    if (*line_idx > 0) {
        if (*history_count == MAX_HISTORY) {
            free(history[0]);
            memmove(history, history + 1, (MAX_HISTORY - 1) * sizeof(char *));
            (*history_count)--;
        }
        history[*history_count] = malloc((*line_idx + 1) * sizeof(char));
        strncpy(history[*history_count], line, *line_idx);
        history[*history_count][*line_idx] = '\0';
        (*history_count)++;
        *history_idx = *history_count;
    }
    *line_idx = 0;
}

// Maneja la entrada de caracteres
void handle_input(char *line, int *line_idx, char *ch) {
    if (*line_idx < MAX_LINE_LENGTH - 1) {
        putchar(*ch);
        line[*line_idx] = *ch;
        (*line_idx)++;
    }
}

// Recorta espacios al inicio y al final de una cadena
char *trimString(char *str) {
    int start = 0, end = strlen(str) - 1;
    while (isspace(str[start])) start++;
    while ((end >= start) && isspace(str[end])) end--;
    str[end + 1] = '\0';
    return &str[start];
}

// Imprime un mensaje de error
void printError() {
    write(STDERR_FILENO, error_message, strlen(error_message));
    exit(0);
}

// Maneja la redirección de salida
void redirection(char *line) {
    // Parseo de redirección
    int a = 0;
    char *redirections[512];
    redirections[0] = strtok(strdup(line), " \n\t>");
    while (redirections[a] != NULL) {
        a++;
        redirections[a] = strtok(NULL, " \n\t>");
    }
    if (a == 1) {
        printError();
        exit(0);
    }
    // Configuración de archivos para redirección
    char *command_out = strdup(redirections[1]);
    int file_out = open(command_out, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (file_out == -1) {
        printError();
        exit(0);
    }
    dup2(file_out, STDOUT_FILENO);
    close(file_out);
}

// === Función principal ===
int main(int argc, char *argv[]) {
    // Verificación de argumentos
    if (argc > 2) {
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
    }

    // Inicialización de variables
    char *line = malloc(sizeof(char) * MAX_LINE_LENGTH);
    char ch;
    char *history[MAX_HISTORY] = {NULL};
    int history_idx = 0, line_idx = 0, history_count = 0;
    struct termios old_term, new_term;
    FILE *fp;

    if (argc == 2) { // Batch mode
        fp = fopen(argv[1], "r");
        if (fp == NULL) {
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1);
        }
    } else { // Modo interactivo
        set_terminal_raw_mode(&old_term, &new_term);
        printf("wish> ");
    }

    while (1) {
        if (argc == 1) { // Modo interactivo
            ch = getchar();
            if (ch == '\033') navigate_command_history(history, line, &history_count, &history_idx, &line_idx, &ch);
            else if (ch == '\n') add_command_to_history(history, line, &line_idx, &history_count, &history_idx);
            else if (ch == 127) handle_backspace_key(line, &line_idx);
            else if (ch >= 32 && ch <= 126) handle_input(line, &line_idx, &ch);
        } else { // Batch mode
            if (getline(&line, &(size_t){0}, fp) == EOF) break;
        }
    }

    // Limpieza final
    free(line);
    restore_terminal_mode(&old_term);
    return 0;
}
