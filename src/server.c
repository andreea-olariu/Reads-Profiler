#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <regex.h>
#include <signal.h>
#include <sqlite3.h>
#include <stdint.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "defined_data_type.h"
#include "read_write_functions.h"
#include "string_functions.h"

#define PORT 2000
extern int errno;

// Functii si structuri de date legate de logica serverului
static void *thFunc(void *);
int get_option_from_client(void *arg);
void treat_option(struct thData *, int);
struct user read_user_info(int);
void sign_up(struct thData *);
void login(struct thData *);
int search_book(struct thData *);
void ask_if_download(struct thData *);
void download_book(struct thData *);
int send_book(struct thData *, char *);
void recommend_books(struct thData *);
void rate_book(struct thData *);
void log_out(struct thData *);
void delete_account(struct thData *);
int validate_regex_string(char *);
int validate_regex_number(char *);

// Functii si structuri de date legate de baza de date
sqlite3 *db;
sqlite3_stmt *res;

int check_user_in_db(char *);
int check_user_in_db_and_validate(struct user);
int insert_user_in_db(char *, char *);
int delete_user_from_db(char *, char *);
int execute_query(char *);
char *check_book_in_db(struct user, char *);
char *show_genres_for_author(char *, char *);
void add_book_in_downloaded(struct user, char *);
void add_book_in_searched(struct user, char *);
void redo_top_popularity(char[]);
void get_book_genre_id(char *, int[]);
int get_popularity_for_book(char[]);
int get_genres_arr(struct thData *, int[]);
char *default_top();
void insert_rating_in_db(char *, int);
void redo_top_rating(char[]);
char *get_top_for_genre_id(int, int);
int modify_table_query(char *);

int main() {
    // Deschidem baza de date
    int rc = sqlite3_open("../database/reads_profiler.db", &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    // Realizam conexiunea cu clientii
    // --------------------------
    struct sockaddr_in server;
    struct sockaddr_in from;
    int sd;
    int pid;
    pthread_t th[100];
    int counter_th = 0;

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("[server]Eroare la socket().\n");
        return errno;
    }

    int on = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    bzero(&server, sizeof(server));
    bzero(&from, sizeof(from));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);

    if (bind(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1) {
        perror("[server]Eroare la bind().\n");
        return errno;
    }
    if (listen(sd, 5) == -1) {
        perror("[server]Eroare la listen().\n");
        return errno;
    }
    // --------------------------

    // Servim clientii in mod concurent, fiecare client cu cate un thread
    while (1) {
        int client;
        struct thData *td;
        int length = sizeof(from);
        printf("[server]Asteptam la portul %d...\n", PORT);
        fflush(stdout);

        if ((client = accept(sd, (struct sockaddr *)&from, &length)) < 0) {
            perror("[server]Eroare la accept().\n");
            continue;
        }

        td = (struct thData *)malloc(sizeof(struct thData));
        td->idThread = counter_th++;
        td->client = client;

        pthread_create(&th[counter_th], NULL, &thFunc, td);
    }
    // Inchidem baza de date la final
    sqlite3_finalize(res);
    sqlite3_close(db);
    return 0;
};

// FUNCTII LOGICE

static void *thFunc(void *arg) {
    // callbackul threadului
    struct thData tdL;
    tdL = *((struct thData *)arg);
    printf("[Thread %d] - Asteptam mesajul...\n", tdL.idThread);
    fflush(stdout);
    pthread_detach(pthread_self());
    get_option_from_client((struct thData *)arg);
    close((intptr_t)arg);
    return (NULL);
};

int get_option_from_client(void *arg) {
    // primirea optiunii din meniul principal
    int option;
    struct thData tdL;
    tdL = *((struct thData *)arg);
    while (1) {
        if (read(tdL.client, &option, sizeof(int)) != 4) {
            printf("[Thread %d]\n", tdL.idThread);
            perror("Eroare la read() de la client in asteptarea optiunii.\n");
            return errno;
        }

        printf("[Thread %d] Optiunea clientului...%d\n", tdL.idThread, option);
        if (option == 6) {
            // clientul a ales optiunea de close app, deci oprim bucla while pentru acest client
            printf("[Thread %d] Client deconectat!\n", tdL.idThread);
            fflush(stdout);
            break;
            return 0;
        }
        treat_option(&tdL, option);
    }
}
void treat_option(struct thData *tdL, int option) {
    switch (option) {
        case 1:
            sign_up(tdL);
            break;
        case 2:
            login(tdL);
            break;
        case 3:
            search_book(tdL);
            break;
        case 4:
            recommend_books(tdL);
            break;
        case 5:
            log_out(tdL);
            break;
        case 7:
            rate_book(tdL);
            break;
        case 8:
            delete_account(tdL);
            break;
        default:
            break;
    }
}
struct user read_user_info(int fd) {
    // citim informatiile de inregistrare / logare / stergere cont de la client
    struct user created_user;
    strcpy(created_user.username, "");
    strcpy(created_user.password, "");
    read_string_from_fd(fd, created_user.username, "[server] Eroare la citirea de la client!");
    read_string_from_fd(fd, created_user.password, "[server] Eroare la citirea de la client!");
    strtrim(created_user.username);
    strtrim(created_user.password);

    return created_user;
}
void sign_up(struct thData *tdL) {
    char msg[101] = "";

    struct user tmp = read_user_info(tdL->client);
    if (strcmp(tmp.username, "") == 0 || strcmp(tmp.password, "") == 0) {
        strcpy(msg, "Va rugam introduceti toate informatiile!");
    } else {
        // verificam data userul exista in baza de date
        // exista -> mesaj
        // nu exista -> adaugam userul in baza de date
        int ok = check_user_in_db(tmp.username);
        if (ok == 0) {
            if (strchr(tmp.username, ' ') != 0) {
                strcpy(msg, "Numele de utilizator nu trebuie sa contina spatii!");
            } else {
                if (1 == insert_user_in_db(tmp.username, tmp.password)) {
                    strcpy(msg, "Inregistrare cu succes!");
                }
            }
        } else {
            strcpy(msg, "Utilizator deja existent!");
        }
    }

    strtrim(msg);
    write_string_to_fd(tdL->client, msg, "[server] Eroare la scrierea catre client!");
}

void login(struct thData *tdL) {
    char msg[101] = "";
    if (strcmp(tdL->current_user.username, "") != 0) {
        strcpy(msg, "Sunteti deja conectat!");
        struct user tmp = read_user_info(tdL->client);
    } else {
        struct user tmp = read_user_info(tdL->client);
        if (strcmp(tmp.username, "") == 0 || strcmp(tmp.password, "") == 0) {
            strcpy(msg, "Va rugam introduceti toate informatiile!");
        } else {
            int ok = check_user_in_db_and_validate(tmp);
            if (ok == 1) {
                // daca exista un user inregistrat in baza de date cu acest username, 
                // setam userul curent al acestui thread cu aceste informatii
                strcpy(msg, "Conectat cu succes!");
                strcpy(tdL->current_user.username, tmp.username);
                strcpy(tdL->current_user.password, tmp.password);
            } else {
                strcpy(msg, "Verificati informatiile!");
            }
        }
    }
    strtrim(msg);
    write_string_to_fd(tdL->client, msg, "[server] Eroare la scrierea catre client!");
}

int validate_regex_string(char *str) {
    regex_t pass_rgx;
    int value;

    if (regcomp(&pass_rgx, "[a-z]*", 0) != 0) {
        perror("[server] Eroare la validarea parolei.");
        exit(1);
    }
    value = regexec(&pass_rgx, str, 0, NULL, 0);
    return value;
}

int validate_regex_number(char *str) {
    regex_t pass_rgx;
    int value;

    if (regcomp(&pass_rgx, "[0-9]*", 0) != 0) {
        perror("[server] Eroare la validarea parolei.");
        exit(1);
    }
    value = regexec(&pass_rgx, str, 0, NULL, 0);
    return value;
}

int search_book(struct thData *tdL) {
    int download = 0;
    struct book_search book;
    if (read(tdL->client, &book, sizeof(struct book_search)) <= 0) {
        perror("[server] Eroare la citirea filtrelor de cautare ale clientului!");
        exit(0);
    }
    char query[1001] = "";
    // preparam query-ul pentru a gasi cartile conform filtrelor
    sprintf(query, "SELECT title FROM books WHERE ");
    // condition = 0 - e primul filtru
    // condition = 1 - nu e primul filtru (e necesar adaugarea unui "AND")
    int condition = 0;
    if (strcmp(book.title, "") != 0) {
        strtrim(book.title);
        char tmp[201] = "";
        sprintf(tmp, "LOWER(title) LIKE LOWER('%s')", book.title);
        strcat(query, tmp);
        condition = 1;
    }

    if (strcmp(book.isbn, "") != 0) {
        strtrim(book.isbn);
        char tmp[101] = "";
        if (condition == 0)
            sprintf(tmp, "isbn = '%s'", book.isbn);
        else
            sprintf(tmp, " AND isbn = '%s'", book.isbn);
        condition = 1;
        strcat(query, tmp);
    }

    // ---------------------------------
    int value;
    if (strcmp(book.author_first_name, "") != 0 && strcmp(book.author_last_name, "") != 0) {
        char tmp[301] = "";
        strcat(tmp, book.author_first_name);
        strcat(tmp, " ");
        strcat(tmp, book.author_last_name);
        value = validate_regex_string(tmp);

        if (value == 0) {
            char aux[305] = "";
            if (condition == 0)
                sprintf(aux, " author_id=(SELECT id FROM authors WHERE LOWER(first_name)=LOWER('%s') AND LOWER(last_name)=LOWER('%s'))", book.author_first_name, book.author_last_name);
            else
                sprintf(aux, " AND author_id=(SELECT id FROM authors WHERE LOWER(first_name)=LOWER('%s') AND LOWER(last_name)=LOWER('%s'))", book.author_first_name, book.author_last_name);
            condition = 1;
            strcat(query, aux);
        } else if (value != 0) {
            char msg[101] = "";
            strcpy(msg, "Numele si prenumele autorului trebuie sa contina doar litere.");
            write_string_to_fd(tdL->client, msg, "[server] Eroare la cautare!");
            if (write(tdL->client, &download, 4) != 4) {
                perror("[server] Eroare la scriere catre client!");
                exit(1);
            }
            return 0;
        }
    } else {
        value = validate_regex_string(book.author_first_name);
        if (strcmp(book.author_first_name, "") != 0 && value == 0) {
            char tmp[201] = "";
            if (condition == 0)
                sprintf(tmp, " author_id=(SELECT id FROM authors WHERE LOWER(first_name)=LOWER('%s'))", book.author_first_name);
            else
                sprintf(tmp, " AND author_id=(SELECT id FROM authors WHERE LOWER(first_name)=LOWER('%s'))", book.author_first_name);
            condition = 1;
            strcat(query, tmp);
        } else if (strcmp(book.author_first_name, "") != 0 && value != 0) {
            char msg[101] = "";
            strcpy(msg, "Numele autorului trebuie sa contina doar litere.");
            write_string_to_fd(tdL->client, msg, "[server] Eroare la cautare!");
            if (write(tdL->client, &download, 4) != 4) {
                perror("[server] Eroare la scriere catre client!");
                exit(1);
            }
            return 0;
        }

        value = validate_regex_string(book.author_last_name);
        if (strcmp(book.author_last_name, "") != 0 && value == 0) {
            char tmp[201] = "";
            if (condition == 0)
                sprintf(tmp, " author_id=(SELECT id FROM authors WHERE LOWER(last_name)=LOWER('%s'))", book.author_last_name);
            else
                sprintf(tmp, " AND author_id=(SELECT id FROM authors WHERE LOWER(last_name)=LOWER('%s'))", book.author_last_name);
            condition = 1;
            strcat(query, tmp);
        } else if (strcmp(book.author_last_name, "") != 0 && value != 0) {
            char msg[101] = "";
            strcpy(msg, "Preumele autorului trebuie sa contina doar litere.");
            write_string_to_fd(tdL->client, msg, "[server] Eroare la cautare!");
            if (write(tdL->client, &download, 4) != 4) {
                perror("[server] Eroare la scriere catre client!");
                exit(1);
            }
            return 0;
        }
    }
    // ---------------------------------
    value = validate_regex_string(book.genre);
    if (strcmp(book.genre, "") != 0 && value == 0) {
        char tmp[301] = "";
        if (condition == 0)
            sprintf(tmp, " isbn in (SELECT isbn FROM book_genre WHERE gen_id=(SELECT id FROM genres WHERE LOWER(name)=LOWER('%s')))", book.genre);
        else
            sprintf(tmp, " AND isbn in (SELECT isbn FROM book_genre WHERE gen_id=(SELECT id FROM genres WHERE LOWER(name)=LOWER('%s')))", book.genre);
        condition = 1;
        strcat(query, tmp);
    } else if (strcmp(book.genre, "") != 0 && value != 0) {
        char msg[101] = "";
        strcpy(msg, "Genul cartii trebuie sa contina doar litere.");
        write_string_to_fd(tdL->client, msg, "[server] Eroare la cautare!");
        if (write(tdL->client, &download, 4) != 4) {
            perror("[server] Eroare la scriere catre client!");
            exit(1);
        }
        return 0;
    }

    value = validate_regex_number(book.rating);
    if (strcmp(book.rating, "") != 0) {
        char msg[101] = "";
        int rating = atoi(book.rating);
        if (value == 0 && rating >= 0 && rating <= 10) {
            char tmp[101] = "";
            if (condition == 0)
                sprintf(tmp, "rating = %d", rating);
            else
                sprintf(tmp, " AND rating = %d", rating);
            condition = 1;
            strcat(query, tmp);
        } else {
            strcpy(msg, "Ratingul cartii trebuie sa fie de la 0 la 10.");
            write_string_to_fd(tdL->client, msg, "[server] Eroare la cautare!");
            if (write(tdL->client, &download, 4) != 4) {
                perror("[server] Eroare la scriere catre client!");
                exit(1);
            }
            return 0;
        }
    }
    value = validate_regex_number(book.publish_year);
    if (strcmp(book.publish_year, "") != 0) {
        char msg[101] = "";
        int year = atoi(book.publish_year);
        if (value == 0 && year >= 1000 && year <= 2022) {
            char tmp[101];
            if (condition == 0)
                sprintf(tmp, "year = %d", year);
            else
                sprintf(tmp, " AND year = %d", year);
            strcat(query, tmp);
            condition = 1;
        } else {
            strcpy(msg, "Anul publicarii cartii trebuie sa contina 4 cifre.");
            write_string_to_fd(tdL->client, msg, "[server] Eroare la cautare!");
            if (write(tdL->client, &download, 4) != 4) {
                perror("[server] Eroare la scriere catre client!");
                exit(1);
            }
            return 0;
        }
    }

    if (condition == 1) {
        strcat(query, ";");
        char *books = check_book_in_db(tdL->current_user, query);
        if (strcmp(books, "") == 0) {
            char msg[101] = "Nu am gasit niciun rezultat!";
            write_string_to_fd(tdL->client, msg, "[server] Eroare la scrierea catre client!");
            if (write(tdL->client, &download, 4) != 4) {
                perror("[server] Eroare la scriere catre client!");
                exit(1);
            }
            return 0;
        } else {
            char *author_genres;
            char msg[551] = "";
            if (strcmp(book.author_first_name, "") != 0 && strcmp(book.author_last_name, "") != 0) {
                author_genres = show_genres_for_author(book.author_first_name, book.author_last_name);
                sprintf(msg, "%s %s a scris ", book.author_first_name, book.author_last_name);
                strcat(msg, author_genres);
            }
            strcat(msg, "\nS-ar putea sa va placa: ");
            strcat(msg, books);
            write_string_to_fd(tdL->client, msg, "[server] Eroare la scrierea catre client!");
            download = 1;
        }
    } else {
        char msg[101] = "Introduceti macar un filtru de cautare!";
        write_string_to_fd(tdL->client, msg, "[server] Eroare la scrierea catre client!");
        if (write(tdL->client, &download, 4) != 4) {
            perror("[server] Eroare la scriere catre client!");
            exit(1);
        }
        return 0;
    }

    if (write(tdL->client, &download, 4) != 4) {
        perror("[server] Eroare la scriere catre client!");
        exit(1);
    }
    ask_if_download(tdL);

    return 1;
}

void ask_if_download(struct thData *tdL) {
    int download_or_not;
    read(tdL->client, &download_or_not, sizeof(download_or_not));
    if (download_or_not == 1) {
        download_book(tdL);
    }
}

void download_book(struct thData *tdL) {
    char book_to_download[101] = "";
    // pentru a putea descarca, clientul trebuie sa fie conectat
    if (strcmp(tdL->current_user.username, "") != 0) {
        read_string_from_fd(tdL->client, book_to_download, "[server] Eroare la citirea de la client!");
        strtrim(book_to_download);
        // validarea introducerii titlului
        if (strcmp(book_to_download, "") != 0) {
            send_book(tdL, book_to_download);
        }
    }
}

int send_book(struct thData *tdL, char *book) {
    char path[120] = "";
    strlower(book);
    strtrim(book);
    sprintf(path, "../books/%s.txt", book);
    int exists;
    // verificarea existentei cartii 
    if (access(path, F_OK) != 0) {
        exists = 0;
        if (write(tdL->client, &exists, sizeof(int)) != 4) {
            perror("[server] Eroare la scriere spre server!");
            exit(1);
        }
        char msg[101] = "Eroare la deschiderea cartii. Verificati inca o data datele cerute.";
        write_string_to_fd(tdL->client, msg, "[server] Eroare la scrierea catre client!");
        return 0;
    }
    exists = 1;
    if (write(tdL->client, &exists, sizeof(int)) != 4) {
        perror("[server] Eroare la scriere spre server!");
        exit(1);
    }
    // adaugam cartea in tabelele downloaded_by si refacem topul pe baza popularitatii (descarcari + cautari)
    add_book_in_downloaded(tdL->current_user, book);
    redo_top_popularity(book);
    int book_desc = open(path, O_RDONLY);
    if (book_desc == -1) {
        perror("[server]  Eroare la deschiderea cartii.");
        exit(0);
    }
    int cod_term;
    char buff[4097] = "";
    while (1) {
        cod_term = read(book_desc, buff, 4096);
        if (cod_term < 0) {
            perror("[server] Eroare la descarcarea cartii!");
            exit(1);
        } else if (cod_term == 0) {
            // trasmit clientului ca am terminat de citit
            int tmp = -1;
            write(tdL->client, &tmp, sizeof(int));
            break;
        }

        write_string_to_fd(tdL->client, buff, "[server] Eroare la scrierea catre client!");
        strcpy(buff, "");
    }

    char msg[101] = "S-a descarcat cartea!";
    write_string_to_fd(tdL->client, msg, "[server] Eroare la scrierea catre client!");
    return 1;
}

void recommend_books(struct thData *tdL) {
    // genres e un container pentru genurile preferate de acest utilizator
    int genres[101];
    int freq_genres[101] = {0};

    if (strcmp(tdL->current_user.username, "") != 0 && get_genres_arr(tdL, genres) == 1) {
        // // generam topul bazat pe preferinte
        int genres_counter = genres[0];
        for (int i = 1; i < genres_counter; i++)
            freq_genres[genres[i]]++;

        for (int i = 1; i < 101; i++)
            genres[i] = i;

        // sortam pe baza preferintelor
        for (int i = 1; i < 101; i++) {
            for (int j = i + 1; j < 101; j++) {
                if (freq_genres[i] < freq_genres[j]) {
                    int aux = genres[i];
                    genres[i] = genres[j];
                    genres[j] = aux;
                }
            }
        }

        char top_books[2010] = "";
        for (int i = 1; i <= 5; i++) {
            if (freq_genres[i] <= 0) {
                break;
            } else {
                char *top_popularity = get_top_for_genre_id(genres[i], 0);
                char *top_rating = get_top_for_genre_id(genres[i], 1);
                if (i == 2 || i == 3) {
                    // 2 books
                    int flag = 0;
                    if (top_popularity != NULL) {
                        for (int j = 0; top_popularity[j] != '\0'; j++) {
                            if (top_popularity[j] == '\n') {
                                flag++;
                            }
                            if (flag == 2) {
                                top_popularity[j + 1] = '\0';
                                break;
                            }
                        }
                    }
                    if (top_rating != NULL) {
                        flag = 0;
                        for (int j = 0; top_rating[j] != '\0'; j++) {
                            if (top_rating[j] == '\n') {
                                flag++;
                            }
                            if (flag == 2) {
                                top_rating[j + 1] = '\0';
                                break;
                            }
                        }
                    }

                } else if (i >= 4) {
                    // 1 book
                    if (top_popularity != NULL) {
                        for (int j = 0; top_popularity[j] != '\0'; j++)
                            if (top_popularity[j] == '\n') {
                                top_popularity[j + 1] = '\0';
                                break;
                            }
                    }
                    if (top_rating != NULL) {
                        for (int j = 0; top_rating[j] != '\0'; j++)
                            if (top_rating[j] == '\n') {
                                top_rating[j + 1] = '\0';
                                break;
                            }
                    }
                }

                if (top_popularity != NULL)
                    strcat(top_books, top_popularity);
                if (top_rating != NULL) {
                    char dup_top_rating[201] = "";
                    char *p = strtok(top_rating, "\n");
                    while (p) {
                        if (!strstr(top_popularity, p)) {
                            strcat(dup_top_rating, p);
                            strcat(dup_top_rating, "\n");
                        }
                        p = strtok(NULL, "\n");
                    }

                    if (strcmp(dup_top_rating, "") != 0)
                        strcat(top_books, dup_top_rating);
                }
            }
        }
        if (strcmp(top_books, "") == 0) {
            char *top = default_top();
            strcat(top_books, top);
        }

        write_string_to_fd(tdL->client, top_books, "[server] Eroare la scrierea recomandarilor catre client!");
    } else {
        char *top = default_top();
        write_string_to_fd(tdL->client, top, "[server] Eroare la scrierea recomandarilor catre client!");
    }
    // am oferit recomandari, intrebam daca userul vrea sa descarce
    ask_if_download(tdL);
}

void rate_book(struct thData *tdL) {
    char msg[101] = "";
    if (strcmp(tdL->current_user.username, "") == 0) {
        strcpy(msg, "Nu puteti oferi un rating daca nu sunteti conectat.\nVa rugam sa va conectati mai intai. \n");
    } else {
        char title[101] = "";
        read_string_from_fd(tdL->client, title, "[server] Eroare la citirea de la client la server! \n (rating)");
        // validarea titlului
        if (strcmp(title, "") != 0) {
            char rating[3] = "";
            read_string_from_fd(tdL->client, rating, "[server] Eroare la citirea de la client la server! \n (rating)");
            // validarea ratingului
            int value = validate_regex_number(rating);

            if (value != 0) {
                strcpy(msg, "Ratingul cartii trebuie sa fie de la 0 la 10.");
            } else {
                int rating_num = atoi(rating);
                if (rating_num >= 0 && rating_num <= 10) {
                    char query[251] = "";
                    // verificarea existentei titlului in baza de date
                    sprintf(query, "SELECT title FROM books WHERE LOWER(title)=LOWER('%s');", title);
                    int val = execute_query(query);
                    if (val == 0) {
                        strcpy(msg, "Cartea nu exista!");
                    } else {
                        // inseram cartea, refacem topul celor mai populare carti dupa rating 
                        insert_rating_in_db(title, rating_num);
                        strcpy(msg, "Multumim de rating!");
                        redo_top_rating(title);
                    }
                } else {
                    strcpy(msg, "Ratingul cartii trebuie sa fie de la 0 la 10.");
                }
            }
        } else {
            strcpy(msg, "Titlu invalid!");
        }
    }
    write_string_to_fd(tdL->client, msg, "[server] Eroare la transmiterea mesajului");
}

void log_out(struct thData *tdL) {
    char msg[101] = "";
    if (strcmp(tdL->current_user.username, "") == 0) {
        strcpy(msg, "Nu sunteti conectat la aplicatie.");
    } else {
        strcpy(tdL->current_user.username, "");
        strcpy(tdL->current_user.password, "");
        strcpy(msg, "Deconectare cu succes.");
    }
    write_string_to_fd(tdL->client, msg, "[server] Eroare la scrierea catre client!");
}

void delete_account(struct thData *tdL) {
    char msg[101] = "";

    struct user tmp = read_user_info(tdL->client);
    if (strcmp(tmp.username, "") == 0 || strcmp(tmp.password, "") == 0) {
        strcpy(msg, "Va rugam introduceti toate informatiile!");
    } else {
        // verificam data userul exista in baza de date
        int ok = check_user_in_db(tmp.username);
        if (ok == 1) {
            if (1 == delete_user_from_db(tmp.username, tmp.password)) {
                strcpy(msg, "Am sters contul!");
            }
        } else {
            strcpy(msg, "Utilizatorul nu exista!");
        }
    }

    strtrim(msg);
    write_string_to_fd(tdL->client, msg, "[server] Eroare la scrierea catre client!");
}

// BAZA DE DATE

int check_user_in_db(char *user) {
    // verificam existenta userului in baza de date
    char query[501] = "";
    strtrim(user);
    sprintf(query, "SELECT * FROM users WHERE username='%s';", user);
    int ok = execute_query(query);
    return ok;
}

int check_user_in_db_and_validate(struct user user_info) {
    // verificam ca userul sa existe in baza de date si sa aibe parola introdusa corect
    char query[501] = "";
    strtrim(user_info.password);
    strtrim(user_info.username);
    sprintf(query, "SELECT * FROM users WHERE username='%s' AND password='%s';", user_info.username, user_info.password);
    int ok = execute_query(query);
    return ok;
}

int insert_user_in_db(char *username, char *password) {
    char query[501] = "";
    sprintf(query, "INSERT INTO users (username, password) VALUES('%s', '%s');", username, password);
    modify_table_query(query);
}

int delete_user_from_db(char *username, char *password) {
    char query[501] = "";
    sprintf(query, "DELETE FROM users WHERE username='%s' AND password='%s';", username, password);
    modify_table_query(query);

    sprintf(query, "DELETE FROM downloaded_books WHERE user_id=(SELECT id FROM users WHERE username='%s');", username);
    modify_table_query(query);
    sprintf(query, "DELETE FROM searched_books WHERE user_id=(SELECT id FROM users WHERE username='%s');", username);
    modify_table_query(query);
    return 1;
}

int modify_table_query(char *query) {
    sqlite3_stmt *res2;
    int rc = sqlite3_prepare_v2(db, query, -1, &res2, 0);
    int step = sqlite3_step(res2);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Eroare!: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        exit(0);
    }
    sqlite3_finalize(res2);
    return 1;
}

int execute_query(char *query) {
    // 0 = nu am gasit nicio inregistrare
    // 1 = am gasit inregistrari
    sqlite3_stmt *res2;
    int rc = sqlite3_prepare_v2(db, query, -1, &res2, 0);
    if (rc) {
        fprintf(stderr, "Eroare!: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 0;
    }
    int step = sqlite3_step(res2);
    if (step == SQLITE_DONE)
        return 0;
    else if (step != SQLITE_OK && step != SQLITE_DONE && step == SQLITE_ROW) {
        return 1;
    }
    sqlite3_finalize(res2);
    return 1;
}

char *show_genres_for_author(char *first_name, char *last_name) {
    // extragem din baza de date genurile preferate de autor
    char *msg = malloc(201);
    strcpy(msg, "");

    char query[301];
    sprintf(query,
            "SELECT name FROM genres WHERE id=(SELECT gen_id FROM book_genre WHERE isbn=(SELECT isbn FROM books "
            "WHERE author_id=(SELECT id FROM authors WHERE LOWER(first_name)=LOWER('%s'))"
            " AND author_id=(SELECT id FROM authors WHERE LOWER(last_name)=LOWER('%s'))));",
            first_name, last_name);

    int rc = sqlite3_prepare_v2(db, query, -1, &res, 0);
    if (rc) {
        fprintf(stderr, "Eroare!: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        exit(0);
    }
    int step = sqlite3_step(res);
    if (step == SQLITE_DONE) {
        // nu am gasit niciun rezultat, returnam ""
        return msg;
    }
    while (step != SQLITE_OK && step != SQLITE_DONE && step == SQLITE_ROW) {
        char *genre = sqlite3_column_text(res, 0);
        if (!strstr(msg, genre)) {
            strcat(msg, genre);
            strcat(msg, ", ");
        }
        step = sqlite3_step(res);
    }
    msg[strlen(msg) - 2] = '\0';
    return msg;
}

char *check_book_in_db(struct user current_user, char *query) {
    // query-ul contine filtrele dupa care returnam titlurile de carti potrivite
    char *msg = malloc(201);

    strcpy(msg, "");
    int rc = sqlite3_prepare_v2(db, query, -1, &res, 0);
    if (rc) {
        fprintf(stderr, "Eroare!: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        exit(0);
    }
    int step = sqlite3_step(res);
    if (step == SQLITE_DONE) {
        // nu am gasit niciun rezultat, returnam ""
        return msg;
    }
    char titles[101][101];
    int index = 0;
    while (step != SQLITE_OK && step != SQLITE_DONE && step == SQLITE_ROW) {
        char *title = sqlite3_column_text(res, 0);

        if (!strstr(msg, title)) {
            strcpy(titles[index++], title);
            strcat(msg, title);
            strcat(msg, "\n ");
        }
        step = sqlite3_step(res);
    }
    msg[strlen(msg) - 2] = '\0';
    sqlite3_finalize(res);
    for (int i = 0; i < index; i++) {
        add_book_in_searched(current_user, titles[i]);
    }
    return msg;
}

void add_book_in_downloaded(struct user current_user, char *book) {
    char query[501] = "";
    strtrim(current_user.username);
    strtrim(book);
    sprintf(query,
            "INSERT INTO downloaded_books VALUES((SELECT id FROM users WHERE username='%s'),"
            "(SELECT isbn FROM books WHERE LOWER(title)=LOWER('%s')));",
            current_user.username, book);

    int rc = sqlite3_prepare_v2(db, query, -1, &res, 0);
    if (rc) {
        fprintf(stderr, "Eroare!: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        exit(0);
    }
    int step = sqlite3_step(res);
    sqlite3_finalize(res);

    sprintf(query,
            "UPDATE books SET downloaded_by = downloaded_by + 1 WHERE "
            "isbn=(SELECT isbn FROM books WHERE LOWER(title)=LOWER('%s'));",
            book);
    modify_table_query(query);
}

void add_book_in_searched(struct user current_user, char *book) {
    char query[501] = "";
    strtrim(current_user.username);
    strtrim(book);
    sprintf(query,
            "INSERT INTO searched_books VALUES((SELECT id FROM users WHERE username='%s'),"
            "(SELECT isbn FROM books WHERE LOWER(title)=LOWER('%s')));",
            current_user.username, book);
    int rc = sqlite3_prepare_v2(db, query, -1, &res, 0);
    if (rc) {
        fprintf(stderr, "Eroare!: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        exit(0);
    }
    int step = sqlite3_step(res);
    sqlite3_finalize(res);
}

void redo_top_popularity(char book[]) {
    // cauta id-ul cartii descarcate
    int ids[5] = {0};
    get_book_genre_id(book, ids);
    int counter = ids[0];
    for (int i = 1; i < counter; i++) {
        int id = ids[i];
        int popularity = get_popularity_for_book(book);

        char query[501] = "";
        // cauta popularitatea cartilor care se afla in topul popularitatii genului cartii tocmai descarcate
        sprintf(query,
                "SELECT (SELECT downloaded_by + searched_by FROM books WHERE isbn=isbn_1),"
                "(SELECT downloaded_by + searched_by FROM books WHERE isbn=isbn_2), (SELECT downloaded_by + searched_by FROM books WHERE isbn=isbn_3)"
                " FROM top_books_by_genre_popularity WHERE gen_id=%d;",
                id);
        int rc = sqlite3_prepare_v2(db, query, -1, &res, 0);
        if (rc) {
            fprintf(stderr, "Eroare!: %s\n", sqlite3_errmsg(db));
            sqlite3_close(db);
        }
        int step = sqlite3_step(res);

        int popularity_1 = 0, popularity_2 = 0, popularity_3 = 0;
        if (step == SQLITE_DONE) {
            char query2[501] = "";
            sprintf(query2,
                    "INSERT INTO top_books_by_genre_popularity(gen_id, isbn_1) "
                    "VALUES(%d, (SELECT isbn FROM books WHERE LOWER(title)=LOWER('%s')));",
                    id, book);

            modify_table_query(query2);
        } else if (step != SQLITE_OK && step != SQLITE_DONE && step == SQLITE_ROW) {
            if (sqlite3_column_type(res, 0) == SQLITE_INTEGER)
                popularity_1 = sqlite3_column_int(res, 0);
            if (sqlite3_column_type(res, 1) == SQLITE_INTEGER)
                popularity_2 = sqlite3_column_int(res, 1);
            if (sqlite3_column_type(res, 2) == SQLITE_INTEGER)
                popularity_3 = sqlite3_column_int(res, 2);
        }
        sqlite3_finalize(res);
        if (popularity >= popularity_1) {
            sprintf(query,
                    "UPDATE top_books_by_genre_popularity SET isbn_3=isbn_2, isbn_2=isbn_1, isbn_1=(SELECT isbn FROM books WHERE LOWER(title)=LOWER('%s'))"
                    " WHERE gen_id=%d AND isbn_1!=(SELECT isbn FROM books WHERE LOWER(title)=LOWER('%s'));",
                    book, id, book);
        } else if (popularity >= popularity_2) {
            sprintf(query,
                    "UPDATE top_books_by_genre_popularity SET isbn_3=isbn_2, isbn_2=(SELECT isbn FROM books WHERE LOWER(title)=LOWER('%s')) WHERE gen_id=%d;",
                    book, id);
        } else if (popularity >= popularity_3) {
            sprintf(query,
                    "UPDATE top_books_by_genre_popularity SET isbn_3=(SELECT isbn FROM books WHERE LOWER(title)=LOWER('%s'))"
                    " WHERE gen_id=%d;",
                    book, id);
        }
        modify_table_query(query);
    }
}
void get_book_genre_id(char *title, int arr[]) {
    int index = 0;
    char query[501] = "";
    sprintf(query, "SELECT gen_id FROM book_genre WHERE isbn=(SELECT isbn FROM books WHERE LOWER(title)=LOWER('%s'));", title);
    int rc = sqlite3_prepare_v2(db, query, -1, &res, 0);
    if (rc) {
        fprintf(stderr, "Eroare!: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        exit(0);
    }
    int step = sqlite3_step(res);
    while (step != SQLITE_OK && step != SQLITE_DONE && step == SQLITE_ROW) {
        arr[++index] = sqlite3_column_int(res, 0);
        step = sqlite3_step(res);
    }
    arr[0] = index + 1;
    sqlite3_finalize(res);
}

int get_popularity_for_book(char book[]) {
    // returneaza cat de populara este o carte bazat pe nr de descarcari si cautari
    char query[501] = "";
    strtrim(book);
    sprintf(query, "SELECT downloaded_by, searched_by FROM books WHERE LOWER(title)=LOWER('%s');", book);
    int rc = sqlite3_prepare_v2(db, query, -1, &res, 0);
    if (rc) {
        fprintf(stderr, "Eroare!: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        exit(0);
    }
    int step = sqlite3_step(res);
    int popularity;
    if (step != SQLITE_OK && step != SQLITE_DONE && step == SQLITE_ROW) {
        popularity = sqlite3_column_int(res, 0) + sqlite3_column_int(res, 1);
    }
    sqlite3_finalize(res);
    return popularity;
}

int get_genres_arr(struct thData *tdL, int genres[]) {
    int index = 0;
    if (strcmp(tdL->current_user.username, "") == 0) {
        // nu returneaza preferinte pentru client neconectat, ci topul default
        return 0;
    }
    char query[501] = "";
    sprintf(query,
            "SELECT (SELECT gen_id FROM book_genre WHERE isbn=b.isbn) FROM downloaded_books d JOIN books b ON b.isbn=d.isbn"
            " WHERE d.user_id=(SELECT id FROM users WHERE LOWER(username)= '%s');",
            tdL->current_user.username);
    int rc = sqlite3_prepare_v2(db, query, -1, &res, 0);
    if (rc) {
        fprintf(stderr, "Eroare!: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 0;
    }
    int step = sqlite3_step(res);
    if (step == SQLITE_DONE) {
        return 0;
    }
    while (step != SQLITE_OK && step != SQLITE_DONE && step == SQLITE_ROW) {
        // pentru fiecare gen, prin variabila ok se verifica daca e pus in containerul genres sau nu
        int genre = sqlite3_column_int(res, 0);
        int ok = 0;
        for (int i = 1; i <= index; i++)
            if (genre == genres[i]) {
                ok = 1;
                break;
            }
        if (ok == 0)
            genres[++index] = genre;
        step = sqlite3_step(res);
    }
    index++;
    // in genres[0] punem dimensiunea
    genres[0] = index;
    sqlite3_finalize(res);
    return 1;
}

char *default_top() {
    char query[501] = "";
    char *titles = malloc(201);
    strcpy(titles, "");
    sprintf(query, "SELECT title FROM books ORDER BY rating DESC LIMIT 4;");

    int rc = sqlite3_prepare_v2(db, query, -1, &res, 0);
    if (rc) {
        fprintf(stderr, "Eroare!: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        exit(0);
    }
    int step = sqlite3_step(res);

    while (step != SQLITE_OK && step != SQLITE_DONE && step == SQLITE_ROW) {
        if (sqlite3_column_type(res, 0) == SQLITE_TEXT) {
            char *title = sqlite3_column_text(res, 0);
            strcat(titles, title);
            strcat(titles, "\n");
        }
        step = sqlite3_step(res);
    }
    sqlite3_finalize(res);
    return titles;
}

void insert_rating_in_db(char *title, int r) {
    char query[501] = "";
    strtrim(title);
    sprintf(query,
            "UPDATE books SET rating = (IFNULL(rating, 0) * IFNULL(rated_by, 0) + %d) / (rated_by + 1),"
            "rated_by = rated_by + 1 WHERE LOWER(title)=LOWER('%s');",
            r, title);

    modify_table_query(query);
}

void redo_top_rating(char book[]) {
    // cauta id-ul cartii descarcate
    int ids[5] = {0};
    get_book_genre_id(book, ids);
    int counter = ids[0];
    for (int i = 1; i < counter; i++) {
        int id = ids[i];
        char query[501] = "";
        // cauta popularitatea cartilor care se afla in topul popularitatii genului cartii tocmai descarcate
        sprintf(query,
                "SELECT (SELECT rating FROM books WHERE LOWER(title)=LOWER('%s')),"
                "(SELECT rating FROM books WHERE isbn=isbn_1), (SELECT rating FROM books WHERE isbn=isbn_2),"
                "(SELECT rating FROM books WHERE isbn=isbn_3) FROM top_books_by_genre_rating WHERE gen_id=%d;",
                book, id);
        int rc = sqlite3_prepare_v2(db, query, -1, &res, 0);
        if (rc) {
            fprintf(stderr, "Eroare!: %s\n", sqlite3_errmsg(db));
            sqlite3_close(db);
        }
        int step = sqlite3_step(res);
        strcpy(query, "");

        int rating = 0, rating_1 = 0, rating_2 = 0, rating_3 = 0;
        if (step == SQLITE_DONE) {
            sprintf(query,
                    "INSERT INTO top_books_by_genre_rating(gen_id, isbn_1)"
                    "VALUES(%d, (SELECT isbn FROM books WHERE LOWER(title)=LOWER('%s')));",
                    id, book);

            modify_table_query(query);
        } else if (step != SQLITE_OK && step != SQLITE_DONE && step == SQLITE_ROW) {
            if (sqlite3_column_type(res, 0) == SQLITE_INTEGER)
                rating = sqlite3_column_int(res, 0);
            if (sqlite3_column_type(res, 1) == SQLITE_INTEGER)
                rating_1 = sqlite3_column_int(res, 0);
            if (sqlite3_column_type(res, 2) == SQLITE_INTEGER)
                rating_2 = sqlite3_column_int(res, 1);
            if (sqlite3_column_type(res, 3) == SQLITE_INTEGER)
                rating_3 = sqlite3_column_int(res, 2);
        }

        if (rating >= rating_1) {
            sprintf(query,
                    "UPDATE top_books_by_genre_rating SET isbn_3=isbn_2,"
                    "isbn_2=isbn_1,"
                    "isbn_1=(SELECT isbn FROM books WHERE LOWER(title)=LOWER('%s')) WHERE "
                    "isbn_1 != (SELECT isbn FROM books WHERE LOWER(title)=LOWER('%s')) AND gen_id=%d;",
                    book, book, id);
        } else if (rating >= rating_2) {
            sprintf(query,
                    "UPDATE top_books_by_genre_rating SET isbn_3=isbn_2,"
                    "isbn_2=(SELECT isbn FROM books WHERE LOWER(title)=LOWER('%s')) WHERE "
                    "isbn_2 != (SELECT isbn FROM books WHERE LOWER(title)=LOWER('%s')) AND gen_id=%d;",
                    book, book, id);
        } else if (rating >= rating_3) {
            sprintf(query,
                    "UPDATE top_books_by_genre_rating SET isbn_3=(SELECT isbn FROM books WHERE LOWER(title)=LOWER('%s'))"
                    " WHERE isbn_3 != (SELECT isbn FROM books WHERE LOWER(title)=LOWER('%s')) AND gen_id=%d;",
                    book, book, id);
        }
        modify_table_query(query);
    }
}

char *get_top_for_genre_id(int id, int x) {
    // x = 0 => cautam in popularity
    // x = 1 => cautam in rating
    char query[501] = "";
    char *titles = malloc(201);
    strcpy(titles, "");
    if (x == 1) {
        sprintf(query,
                "SELECT (SELECT title FROM books WHERE isbn=isbn_1), (SELECT title FROM books WHERE"
                " isbn=isbn_2), (SELECT title FROM books WHERE isbn=isbn_3) FROM top_books_by_genre_rating WHERE gen_id=%d;",
                id);
    } else {
        sprintf(query,
                "SELECT (SELECT title FROM books WHERE isbn=isbn_1), (SELECT title FROM books WHERE isbn=isbn_2),"
                "(SELECT title FROM books WHERE isbn=isbn_3) FROM top_books_by_genre_popularity WHERE gen_id=%d;",
                id);
    }
    int rc = sqlite3_prepare_v2(db, query, -1, &res, 0);
    if (rc) {
        fprintf(stderr, "Eroare!: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        exit(0);
    }
    int step = sqlite3_step(res);
    if (step == SQLITE_DONE) {
        return NULL;
    }
    if (step != SQLITE_OK && step != SQLITE_DONE && step == SQLITE_ROW) {
        if (sqlite3_column_type(res, 0) == SQLITE_TEXT) {
            char *title = sqlite3_column_text(res, 0);
            if (!strstr(titles, title)) {
                strcat(titles, title);
                strcat(titles, "\n");
            }
        }
        if (sqlite3_column_type(res, 1) == SQLITE_TEXT) {
            char *title = sqlite3_column_text(res, 1);
            if (!strstr(titles, title)) {
                strcat(titles, title);
                strcat(titles, "\n");
            }
        }
        if (sqlite3_column_type(res, 2) == SQLITE_TEXT) {
            char *title = sqlite3_column_text(res, 2);
            if (!strstr(titles, title)) {
                strcat(titles, title);
                strcat(titles, "\n");
            }
        }
    }
    sqlite3_finalize(res);
    return titles;
}
