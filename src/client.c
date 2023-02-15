#include <errno.h>
#include <fcntl.h>
#include <gtk/gtk.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "defined_data_type.h"
#include "read_write_functions.h"
#include "string_functions.h"

extern int errno;

// Portul la care se face comunicarea
int port;
// Endpointul la care se face comunicarea
int sd;

// Date despre userul curent
struct user current_user;

// Functii comunicare cu serverul
void user_info_interaction(GtkButton *btn, gpointer data);
static gboolean search_book(GtkButton *btn, gpointer data);
static gboolean recommend_book(GtkButton *btn, gpointer data);
static gboolean rate_book(GtkButton *btn, gpointer data);
static gboolean ask_to_download(GtkButton *, gpointer);
static gboolean ask_to_not_download(GtkButton *, gpointer);
static gboolean download_book(GtkButton *, gpointer);

// Structuri pentru grafica
typedef struct user_info_container {
    GtkEntry *username;
    GtkEntry *password;
};
struct user_info_container user_data;

typedef struct search_book_container {
    GtkEntry *title;
    GtkEntry *author_first_name;
    GtkEntry *author_last_name;
    GtkEntry *isbn;
    GtkEntry *genre;
    GtkEntry *publish_year;
    GtkEntry *rating;
};
struct search_book_container book_data;

typedef struct rate_book_container {
    GtkEntry *title;
    GtkEntry *rating;
};
struct rate_book_container rate_data;

typedef struct container {
    char titles[1201];
};
struct container books_container;

// Functii de grafica
GtkWindow *window;
void remove_children(GtkContainer *container);
static void load_css();
GtkWindow *initialize_window();
void Menu();
static gboolean SignUpMenu(GtkButton *btn, gpointer data);
static gboolean LogInMenu(GtkButton *btn, gpointer data);
static gboolean RateMenu(GtkButton *btn, gpointer data);
static gboolean SearchBook(GtkButton *btn, gpointer data);
static gboolean CloseClient(GtkButton *btn, gpointer data);
static gboolean CloseApp(GtkButton *btn, gpointer data);
static gboolean DeleteAccount(GtkButton *btn, gpointer data);

static gboolean BackUserInfoInteraction(GtkButton *btn, gpointer data);
static gboolean BackRecommend(GtkButton *btn, gpointer data);
static gboolean BackSearch(GtkButton *btn, gpointer data);
static gboolean BackRate(GtkButton *btn, gpointer data);
static gboolean BackDownload(GtkButton *btn, gpointer data);
static gboolean BackAskWhatBook(GtkButton *btn, gpointer data);

static gboolean AskToDownloadBookMenu(char[]);
static gboolean AskWhatBook();
static gboolean ShowMessage(char msg[]);

int main(int argc, char **argv) {
    // Realizarea conexiunii
    // ---------------------
    struct sockaddr_in server;
    if (argc != 3) {
        printf("Sintaxa: %s <adresa_server> <port>\n", argv[0]);
        return -1;
    }
    // portul e stabilit in server ! 
    port = atoi(argv[2]);
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Eroare la socket().\n");
        return errno;
    }
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_port = htons(port);
    if (connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1) {
        perror("[client]Eroare la connect().\n");
        return errno;
    }
    // ---------------------

    bzero(&current_user, sizeof(current_user));
    strcpy(current_user.username, "");
    strcpy(current_user.password, "");

    // Initializare grafica 
    // -------------------
    gtk_init(&argc, &argv);
    window = initialize_window();

    Menu();
    gtk_main();
    // -------------------
    return 0;
}

// LOGICA

void user_info_interaction(GtkButton *btn, gpointer p) {
    // citeste datele de input si trimite spre server
    // -> mesaj daca nu sunt furnizate datele
    if (GTK_IS_ENTRY(user_data.username) == TRUE && gtk_entry_get_text_length(GTK_ENTRY(user_data.username)) != 0) {
        strcpy(current_user.username, gtk_entry_get_text(GTK_ENTRY(user_data.username)));
        strtrim(current_user.username);
    }
    if (GTK_IS_ENTRY(user_data.password) == TRUE && gtk_entry_get_text_length(GTK_ENTRY(user_data.password)) != 0) {
        strcpy(current_user.password, gtk_entry_get_text(GTK_ENTRY(user_data.password)));
        strtrim(current_user.password);
    }

    write_string_to_fd(sd, current_user.username, "[client]Eroare la scrierea catre server!");
    write_string_to_fd(sd, current_user.password, "[client]Eroare la scrierea catre server!");

    char return_msg[101] = "";
    read_string_from_fd(sd, return_msg, "[client]Eroare la citirea de la server!");
    ShowMessage(return_msg);
}

static gboolean search_book(GtkButton *btn, gpointer data) {
    // cauta carti pe baza unuia sau mai multe filtre
    // -> mesaj daca filtre nu sunt introduse
    struct book_search book;
    bzero(&book, sizeof(book));

    strcpy(book.title, "");
    strcpy(book.author_first_name, "");
    strcpy(book.author_last_name, "");
    strcpy(book.isbn, "");
    strcpy(book.genre, "");
    strcpy(book.rating, "");
    strcpy(book.publish_year, "");

    // Extragerea filtrelor de cautare
    if (GTK_IS_ENTRY(book_data.title) == TRUE && gtk_entry_get_text_length(GTK_ENTRY(book_data.title)) != 0) {
        strcpy(book.title, gtk_entry_get_text(GTK_ENTRY(book_data.title)));
        strtrim(book.title);
    }
    if (GTK_IS_ENTRY(book_data.author_first_name) == TRUE && gtk_entry_get_text_length(GTK_ENTRY(book_data.author_first_name)) != 0) {
        strcpy(book.author_first_name, gtk_entry_get_text(GTK_ENTRY(book_data.author_first_name)));
        strtrim(book.author_first_name);
    }
    if (GTK_IS_ENTRY(book_data.author_last_name) == TRUE && gtk_entry_get_text_length(GTK_ENTRY(book_data.author_last_name)) != 0) {
        strcpy(book.author_last_name, gtk_entry_get_text(GTK_ENTRY(book_data.author_last_name)));
        strtrim(book.author_last_name);
    }
    if (GTK_IS_ENTRY(book_data.isbn) == TRUE && gtk_entry_get_text_length(GTK_ENTRY(book_data.isbn)) != 0) {
        strcpy(book.isbn, gtk_entry_get_text(GTK_ENTRY(book_data.isbn)));
        strtrim(book.isbn);
    }
    if (GTK_IS_ENTRY(book_data.genre) == TRUE && gtk_entry_get_text_length(GTK_ENTRY(book_data.genre)) != 0) {
        strcpy(book.genre, gtk_entry_get_text(GTK_ENTRY(book_data.genre)));
        strtrim(book.genre);
    }
    if (GTK_IS_ENTRY(book_data.publish_year) == TRUE && gtk_entry_get_text_length(GTK_ENTRY(book_data.publish_year)) != 0) {
        strcpy(book.publish_year, gtk_entry_get_text(GTK_ENTRY(book_data.publish_year)));
        strtrim(book.publish_year);
    }
    if (GTK_IS_ENTRY(book_data.rating) == TRUE && gtk_entry_get_text_length(GTK_ENTRY(book_data.rating)) != 0) {
        strcpy(book.rating, gtk_entry_get_text(GTK_ENTRY(book_data.rating)));
        strtrim(book.rating);
    }

    // Scriem spre server filtrele
    if (-1 == write(sd, &book, sizeof(book))) {
        perror("[client]Eroare la write() spre server!\n");
        exit(1);
    }

    char return_msg[301] = "";
    read_string_from_fd(sd, return_msg, "[client]Eroare la citirea de la server!");
    strtrim(return_msg);

    // Optiunea de download
    int download;
    if (read(sd, &download, 4) != 4) {
        perror("[client]Eroare la read() de la server!\n");
        exit(1);
    }

    if (download == 1) {
        AskToDownloadBookMenu(return_msg);
    } else {
        ShowMessage(return_msg);
    }
}

static gboolean ask_to_not_download(GtkButton *btn, gpointer data) {
    int download = 0;
    if (-1 == write(sd, &download, sizeof(download))) {
        perror("[client]Eroare la write() spre server!\n");
        exit(0);
    }
    Menu();
}
static gboolean ask_to_download(GtkButton *btn, gpointer data) {
    int download = 1;
    if (-1 == write(sd, &download, sizeof(download))) {
        perror("[client]Eroare la write() spre server!\n");
        exit(0);
    }
    AskWhatBook();
}

static gboolean download_book(GtkButton *btn, gpointer data) {
    // citeste titlul cartii, trimite informatiile catre server 
    // -> mesaj daca clientul nu e conectat
    // in caz de succes, primeste cartea
    char title[101] = "";
    if (GTK_IS_ENTRY(book_data.title) == TRUE && gtk_entry_get_text_length(book_data.title) != 0) {
        strcpy(title, gtk_entry_get_text(GTK_ENTRY(book_data.title)));
        strtrim(title);
    }

    if (strcmp(current_user.username, "") == 0) {
        char msg[101] = "Descarcare esuata. Va rugam sa va conectati mai intai.\n";
        ShowMessage(msg);
        return 0;
    }

    write_string_to_fd(sd, title, "[client]Eroare la scrierea catre server!");

    if (strcmp(title, "") == 0) {
        char msg[101] = "Descarcare esuata. Nu ati introdus datele cerute\n";
        ShowMessage(msg);
        return 0;
    }

    int exists;
    if (read(sd, &exists, sizeof(int)) != 4) {
        perror("[clinet] Eroare la citirea de la server!");
        exit(1);
    }

    if (exists == 1) {
        char path[121] = "";
        strlower(title);
        sprintf(path, "../downloads/%s.txt", title);

        int book_desc = open(path, O_WRONLY | O_TRUNC | O_CREAT, 00666);
        char buff[4097] = "";
        int cod_term, index = 0, nr_bytes;
        char ch;
        while (1) {
            if (read(sd, &nr_bytes, sizeof(int)) != 4) {
                perror("[client] Eroare la citirea de la server!");
                exit(1);
            }
            // nr_bytes == -1 => am terminat de citit cartea
            if (nr_bytes == -1) {
                break;
            }
            while (nr_bytes--) {
                cod_term = read(sd, &ch, 1);
                if (cod_term <= 0) {
                    break;
                }
                buff[index++] = ch;
            }
            write(book_desc, buff, strlen(buff));

            strcpy(buff, "");
            index = 0;
        }
    }

    char msg[101] = "";
    read_string_from_fd(sd, msg, "[client]Eroare la citirea de la server!");
    ShowMessage(msg);
}

static gboolean recommend_book(GtkButton *btn, gpointer data) {
    // 4 = recomandare
    int opt = 4;
    if (write(sd, &opt, sizeof(int)) != 4) {
        perror("[client] Eroare la scriere in recomandari!");
        exit(1);
    }
    char msg[2010] = "";
    read_string_from_fd(sd, msg, "[client]Eroare la citirea de la server!");

    char print[2050] = "V-ar putea placea:\n";
    strcat(print, msg);
    AskToDownloadBookMenu(print);
}

static gboolean rate_book(GtkButton *btn, gpointer data) {
    // furnizarea unui rating 
    // -> mesaj daca clientul nu e conectat sau datele sunt invalide
    if (strcmp(current_user.username, "") == 0) {
        char msg[101] = "";
        read_string_from_fd(sd, msg, "[client]Eroare la citirea de la server!");
        ShowMessage(msg);
    } else {
        printf(""); fflush(stdout);
        char title[101] = "";
        if (GTK_IS_ENTRY(rate_data.title) && gtk_entry_get_text_length(GTK_ENTRY(rate_data.title)) != 0) {
            strcpy(title, gtk_entry_get_text(GTK_ENTRY(rate_data.title)));
            strtrim(title);  
        }
        write_string_to_fd(sd, title, "[client] Eroare la scrierea catre server. \n");

        if (strcmp(title, "") != 0) {
            char rating[3] = "";
            if (GTK_IS_ENTRY(rate_data.rating) && gtk_entry_get_text_length(GTK_ENTRY(rate_data.rating)) != 0) {
                strcpy(rating, gtk_entry_get_text(GTK_ENTRY(rate_data.rating)));
                strtrim(rating);
            }
            write_string_to_fd(sd, rating, "[client] Eroare la write()\n");
        }

        char msg[101] = "";
        read_string_from_fd(sd, msg, "[client] Eroare la primirea mesajului");
        ShowMessage(msg);
    }
}

// GRAFICA

void remove_children(GtkContainer *container) {
    GList *children, *iter;

    children = gtk_container_get_children(GTK_CONTAINER(container));
    for (iter = children; iter != NULL; iter = g_list_next(iter))
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    g_list_free(children);
}

void Menu() {
    remove_children(GTK_CONTAINER(window));

    GtkBox *sign_up_box, *log_in_box, *search_box, *recommend_box, *close_box, *rate_box, *delete_box, *log_out_box;
    GtkButton *sign_up_btn, *log_in_btn, *search, *recommend, *close_btn, *rate, *delete_btn, *log_out_btn;

    GtkBox *menu = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));

    // Sign up
    sign_up_btn = GTK_BUTTON(gtk_button_new_with_label("Sign Up"));
    sign_up_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_container_add(GTK_CONTAINER(sign_up_box), GTK_WIDGET(sign_up_btn));
    gtk_widget_set_focus_on_click(GTK_WIDGET(sign_up_box), TRUE);
    g_signal_connect(sign_up_btn, "button-press-event", G_CALLBACK(SignUpMenu), NULL);
    gtk_container_add(GTK_CONTAINER(menu), GTK_WIDGET(sign_up_box));

    // Log in
    log_in_btn = GTK_BUTTON(gtk_button_new_with_label("Log in"));
    log_in_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_container_add(GTK_CONTAINER(log_in_box), GTK_WIDGET(log_in_btn));
    gtk_widget_set_focus_on_click(GTK_WIDGET(log_in_box), TRUE);
    g_signal_connect(log_in_btn, "button-press-event", G_CALLBACK(LogInMenu), NULL);
    gtk_container_add(GTK_CONTAINER(menu), GTK_WIDGET(log_in_box));

    // Search
    search = GTK_BUTTON(gtk_button_new_with_label("Search book"));
    search_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_container_add(GTK_CONTAINER(search_box), GTK_WIDGET(search));
    gtk_widget_set_focus_on_click(GTK_WIDGET(search_box), TRUE);
    g_signal_connect(search, "button-press-event", G_CALLBACK(SearchBook), NULL);
    gtk_container_add(GTK_CONTAINER(menu), GTK_WIDGET(search_box));

    // Rate
    rate = GTK_BUTTON(gtk_button_new_with_label("Rate book"));
    rate_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_container_add(GTK_CONTAINER(rate_box), GTK_WIDGET(rate));
    gtk_widget_set_focus_on_click(GTK_WIDGET(rate_box), TRUE);
    g_signal_connect(rate, "button-press-event", G_CALLBACK(RateMenu), NULL);
    gtk_container_add(GTK_CONTAINER(menu), GTK_WIDGET(rate_box));

    // Recommend
    recommend = GTK_BUTTON(gtk_button_new_with_label("Recommend book"));
    recommend_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_container_add(GTK_CONTAINER(recommend_box), GTK_WIDGET(recommend));
    gtk_widget_set_focus_on_click(GTK_WIDGET(recommend_box), TRUE);
    g_signal_connect(recommend, "button-press-event", G_CALLBACK(recommend_book), NULL);
    gtk_container_add(GTK_CONTAINER(menu), GTK_WIDGET(recommend_box));

    // Log out
    log_out_btn = GTK_BUTTON(gtk_button_new_with_label("Logout"));
    log_out_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_container_add(GTK_CONTAINER(log_out_box), GTK_WIDGET(log_out_btn));
    gtk_widget_set_focus_on_click(GTK_WIDGET(log_out_box), TRUE);
    g_signal_connect(log_out_btn, "button-press-event", G_CALLBACK(CloseClient), NULL);
    gtk_container_add(GTK_CONTAINER(menu), GTK_WIDGET(log_out_box));

    // Close
    close_btn = GTK_BUTTON(gtk_button_new_with_label("Close App"));
    close_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_container_add(GTK_CONTAINER(close_box), GTK_WIDGET(close_btn));
    gtk_widget_set_focus_on_click(GTK_WIDGET(close_box), TRUE);
    g_signal_connect(close_btn, "button-press-event", G_CALLBACK(CloseApp), NULL);
    gtk_container_add(GTK_CONTAINER(menu), GTK_WIDGET(close_box));

    // Delete Account
    delete_btn = GTK_BUTTON(gtk_button_new_with_label("Delete Account"));
    delete_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_container_add(GTK_CONTAINER(delete_box), GTK_WIDGET(delete_btn));
    gtk_widget_set_focus_on_click(GTK_WIDGET(delete_box), TRUE);
    g_signal_connect(delete_btn, "button-press-event", G_CALLBACK(DeleteAccount), NULL);
    gtk_container_add(GTK_CONTAINER(menu), GTK_WIDGET(delete_box));

    gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(menu));

    gtk_widget_show_all(GTK_WIDGET(window));
}

GtkWindow *initialize_window() {
    GtkWindow *window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));

    load_css();
    gtk_window_set_title(window, "ReadsProfiler");
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_window_set_default_size(GTK_WINDOW(window), 700, 500);
    gtk_window_set_resizable(window, 0);

    g_signal_connect(window, "destroy", G_CALLBACK(CloseApp), NULL);
    return window;
}

static gboolean CloseApp(GtkButton *btn, gpointer data) {
    remove_children(GTK_CONTAINER(window));
    strcpy(current_user.username, "");
    strcpy(current_user.password, "");
    int opt = 6;
    if (write(sd, &opt, sizeof(int)) != 4) {
        perror("[client] Eroare la write in close app!\n");
        exit(1);
    }
    gtk_main_quit();
}

static gboolean SignUpMenu(GtkButton *btn, gpointer p) {
    // 1 = sign up
    int opt = 1;
    if (write(sd, &opt, sizeof(int)) != 4) {
        perror("[client] Eroare la write in sign up!\n");
        exit(1);
    }

    remove_children(GTK_CONTAINER(window));

    GtkEntry *username = GTK_ENTRY(gtk_entry_new());
    GtkEntry *password = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(
        GTK_ENTRY(username), "username...");
    gtk_entry_set_placeholder_text(
        GTK_ENTRY(password), "password...");
    gtk_entry_set_visibility(password, FALSE);

    user_data.username = username;
    user_data.password = password;
    GtkButton *sign_up_btn = GTK_BUTTON(gtk_button_new_with_label("Sign up"));
    g_signal_connect(sign_up_btn, "button-press-event", G_CALLBACK(user_info_interaction), NULL);

    GtkButton *back = GTK_BUTTON(gtk_button_new_with_label("Back"));
    g_signal_connect(back, "button-press-event", G_CALLBACK(BackUserInfoInteraction), NULL);

    GtkBox *wrapper = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_container_add(GTK_CONTAINER(wrapper), GTK_WIDGET(username));
    gtk_container_add(GTK_CONTAINER(wrapper), GTK_WIDGET(password));
    gtk_container_add(GTK_CONTAINER(wrapper), GTK_WIDGET(sign_up_btn));
    gtk_container_add(GTK_CONTAINER(wrapper), GTK_WIDGET(back));

    gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(wrapper));
    gtk_widget_show_all(GTK_WIDGET(window));
}

static gboolean LogInMenu(GtkButton *btn, gpointer p) {
    // 2 = log in
    int opt = 2;
    if (write(sd, &opt, sizeof(int)) != 4) {
        perror("[client] Eroare la scriere in log in!\n");
        exit(1);
    }

    remove_children(GTK_CONTAINER(window));

    GtkEntry *username = GTK_ENTRY(gtk_entry_new());
    GtkEntry *password = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(
        GTK_ENTRY(username), "username...");
    gtk_entry_set_placeholder_text(
        GTK_ENTRY(password), "password...");
    gtk_entry_set_visibility(password, FALSE);

    user_data.username = username;
    user_data.password = password;
    GtkButton *log_in_btn = GTK_BUTTON(gtk_button_new_with_label("Log in"));
    g_signal_connect(log_in_btn, "button-press-event", G_CALLBACK(user_info_interaction), NULL);

    GtkButton *back = GTK_BUTTON(gtk_button_new_with_label("Back"));
    g_signal_connect(back, "button-press-event", G_CALLBACK(BackUserInfoInteraction), NULL);

    GtkBox *wrapper = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_container_add(GTK_CONTAINER(wrapper), GTK_WIDGET(username));
    gtk_container_add(GTK_CONTAINER(wrapper), GTK_WIDGET(password));
    gtk_container_add(GTK_CONTAINER(wrapper), GTK_WIDGET(log_in_btn));
    gtk_container_add(GTK_CONTAINER(wrapper), GTK_WIDGET(back));

    gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(wrapper));
    gtk_widget_show_all(GTK_WIDGET(window));
}

static gboolean SearchBook(GtkButton *btn, gpointer data) {
    // 3 = search book
    int opt = 3;
    if (write(sd, &opt, sizeof(int)) != 4) {
        perror("[client] Eroare la scriere in procesul de cautare a cartii!");
        exit(1);
    }

    remove_children(GTK_CONTAINER(window));

    GtkEntry *title = GTK_ENTRY(gtk_entry_new());
    GtkEntry *author_first_name = GTK_ENTRY(gtk_entry_new());
    GtkEntry *author_last_name = GTK_ENTRY(gtk_entry_new());
    GtkEntry *isbn = GTK_ENTRY(gtk_entry_new());
    GtkEntry *genre = GTK_ENTRY(gtk_entry_new());
    GtkEntry *publish_year = GTK_ENTRY(gtk_entry_new());
    GtkEntry *rating = GTK_ENTRY(gtk_entry_new());

    gtk_entry_set_placeholder_text(
        GTK_ENTRY(title), "title...");
    gtk_entry_set_placeholder_text(
        GTK_ENTRY(author_first_name), "author's first name...");
    gtk_entry_set_placeholder_text(
        GTK_ENTRY(author_last_name), "author's last name...");
    gtk_entry_set_placeholder_text(
        GTK_ENTRY(isbn), "isbn...");
    gtk_entry_set_placeholder_text(
        GTK_ENTRY(genre), "genre...");
    gtk_entry_set_placeholder_text(
        GTK_ENTRY(publish_year), "year of publishing...");
    gtk_entry_set_placeholder_text(
        GTK_ENTRY(rating), "rating...");

    book_data.title = title;
    book_data.author_first_name = author_first_name;
    book_data.author_last_name = author_last_name;
    book_data.isbn = isbn;
    book_data.genre = genre;
    book_data.publish_year = publish_year;
    book_data.rating = rating;

    GtkButton *search_btn = GTK_BUTTON(gtk_button_new_with_label("Search"));
    g_signal_connect(search_btn, "button-press-event", G_CALLBACK(search_book), NULL);

    GtkButton *back = GTK_BUTTON(gtk_button_new_with_label("Back"));
    g_signal_connect(back, "button-press-event", G_CALLBACK(BackSearch), NULL);

    GtkBox *wrapper = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_container_add(GTK_CONTAINER(wrapper), GTK_WIDGET(title));
    gtk_container_add(GTK_CONTAINER(wrapper), GTK_WIDGET(author_first_name));
    gtk_container_add(GTK_CONTAINER(wrapper), GTK_WIDGET(author_last_name));
    gtk_container_add(GTK_CONTAINER(wrapper), GTK_WIDGET(isbn));
    gtk_container_add(GTK_CONTAINER(wrapper), GTK_WIDGET(genre));
    gtk_container_add(GTK_CONTAINER(wrapper), GTK_WIDGET(publish_year));
    gtk_container_add(GTK_CONTAINER(wrapper), GTK_WIDGET(rating));
    gtk_container_add(GTK_CONTAINER(wrapper), GTK_WIDGET(search_btn));
    gtk_container_add(GTK_CONTAINER(wrapper), GTK_WIDGET(back));

    gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(wrapper));
    gtk_widget_show_all(GTK_WIDGET(window));
}

static gboolean RateMenu(GtkButton *btn, gpointer data) {
    // 7 = rating
    int opt = 7;
    if (write(sd, &opt, sizeof(int)) != 4) {
        perror("[client] Eroare la scrierea din meniul de rating!");
        exit(1);
    }
    remove_children(GTK_CONTAINER(window));

    GtkEntry *title = GTK_ENTRY(gtk_entry_new());
    GtkEntry *rating = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(
        GTK_ENTRY(title), "title...");
    gtk_entry_set_placeholder_text(
        GTK_ENTRY(rating), "rating...");

    rate_data.title = title;
    rate_data.rating = rating;
    GtkButton *rate_btn = GTK_BUTTON(gtk_button_new_with_label("Rate"));
    g_signal_connect(rate_btn, "button-press-event", G_CALLBACK(rate_book), NULL);

    GtkButton *back = GTK_BUTTON(gtk_button_new_with_label("Back"));
    g_signal_connect(back, "button-press-event", G_CALLBACK(BackRate), NULL);

    GtkBox *wrapper = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));

    gtk_container_add(GTK_CONTAINER(wrapper), GTK_WIDGET(title));
    gtk_container_add(GTK_CONTAINER(wrapper), GTK_WIDGET(rating));
    gtk_container_add(GTK_CONTAINER(wrapper), GTK_WIDGET(rate_btn));
    gtk_container_add(GTK_CONTAINER(wrapper), GTK_WIDGET(back));

    gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(wrapper));
    gtk_widget_show_all(GTK_WIDGET(window));
}

static gboolean AskToDownloadBookMenu(char message[]) {
    remove_children(GTK_CONTAINER(window));
    GtkLabel *books = GTK_LABEL(gtk_label_new(message));
    gtk_widget_set_name(GTK_WIDGET(books), "label-white");
    char msg[101] = "";
    strcpy(msg, "Doriti sa descarcati vreun titlu?\n");
    GtkLabel *label = GTK_LABEL(gtk_label_new(msg));
    gtk_widget_set_name(GTK_WIDGET(label), "label-white");
    GtkButton *yes_btn = GTK_BUTTON(gtk_button_new_with_label("Yes"));
    GtkButton *no_btn = GTK_BUTTON(gtk_button_new_with_label("No"));
    strcpy(books_container.titles, message);
    g_signal_connect(yes_btn, "button-press-event", G_CALLBACK(ask_to_download), NULL);
    g_signal_connect(no_btn, "button-press-event", G_CALLBACK(ask_to_not_download), NULL);
    GtkBox *opt_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_widget_set_name(GTK_WIDGET(opt_box), "center-container");
    gtk_container_add(GTK_CONTAINER(opt_box), GTK_WIDGET(yes_btn));
    gtk_container_add(GTK_CONTAINER(opt_box), GTK_WIDGET(no_btn));

    GtkButton *back = GTK_BUTTON(gtk_button_new_with_label("Back"));
    g_signal_connect(back, "button-press-event", G_CALLBACK(BackDownload), NULL);

    GtkBox *box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_container_add(GTK_CONTAINER(box), GTK_WIDGET(books));
    gtk_container_add(GTK_CONTAINER(box), GTK_WIDGET(label));
    gtk_container_add(GTK_CONTAINER(box), GTK_WIDGET(opt_box));
    gtk_container_add(GTK_CONTAINER(box), GTK_WIDGET(back));

    gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(box));
    gtk_widget_show_all(GTK_WIDGET(window));
}

static gboolean ShowMessage(char message[]) {
    remove_children(GTK_CONTAINER(window));
    GtkLabel *label = GTK_LABEL(gtk_label_new(message));
    gtk_widget_set_name(GTK_WIDGET(label), "label-white");

    GtkButton *back = GTK_BUTTON(gtk_button_new_with_label("Back"));
    g_signal_connect(back, "button-press-event", G_CALLBACK(Menu), NULL);

    GtkBox *box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_container_add(GTK_CONTAINER(box), GTK_WIDGET(label));
    gtk_container_add(GTK_CONTAINER(box), GTK_WIDGET(back));

    gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(box));
    gtk_widget_show_all(GTK_WIDGET(window));
}

static gboolean CloseClient(GtkButton *btn, gpointer data) {
    remove_children(GTK_CONTAINER(window));
    strcpy(current_user.username, "");
    strcpy(current_user.password, "");
    int opt = 5;
    if (write(sd, &opt, sizeof(int)) != 4) {
        perror("[client] Eroare la scrierea cererii de logout!");
        exit(1);
    }
    char msg[101] = "";
    read_string_from_fd(sd, msg, "[client]Eroare la citirea de la server!");
    ShowMessage(msg);
}

static gboolean DeleteAccount(GtkButton *btn, gpointer data) {
    strcpy(current_user.username, "");
    strcpy(current_user.password, "");
    // 8 = delete account
    int opt = 8;
    if (write(sd, &opt, sizeof(int)) != 4) {
        perror("[client] Eroare la write in delete account!\n");
        exit(1);
    }
    remove_children(GTK_CONTAINER(window));

    GtkEntry *username = GTK_ENTRY(gtk_entry_new());
    GtkEntry *password = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(
        GTK_ENTRY(username), "username...");
    gtk_entry_set_placeholder_text(
        GTK_ENTRY(password), "password...");
    gtk_entry_set_visibility(password, FALSE);

    user_data.username = username;
    user_data.password = password;
    GtkButton *sign_up_btn = GTK_BUTTON(gtk_button_new_with_label("Delete Account"));
    g_signal_connect(sign_up_btn, "button-press-event", G_CALLBACK(user_info_interaction), NULL);

    GtkButton *back = GTK_BUTTON(gtk_button_new_with_label("Back"));
    g_signal_connect(back, "button-press-event", G_CALLBACK(BackUserInfoInteraction), NULL);

    GtkBox *wrapper = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_container_add(GTK_CONTAINER(wrapper), GTK_WIDGET(username));
    gtk_container_add(GTK_CONTAINER(wrapper), GTK_WIDGET(password));
    gtk_container_add(GTK_CONTAINER(wrapper), GTK_WIDGET(sign_up_btn));
    gtk_container_add(GTK_CONTAINER(wrapper), GTK_WIDGET(back));

    gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(wrapper));
    gtk_widget_show_all(GTK_WIDGET(window));
}

static gboolean AskWhatBook() {
    remove_children(GTK_CONTAINER(window));
    GtkLabel *books = GTK_LABEL(gtk_label_new(books_container.titles));
    gtk_label_set_line_wrap(books, TRUE);
    GtkLabel *label = GTK_LABEL(gtk_label_new("Care e titlul pe care doriti sa il descarcati? \n"));
    gtk_widget_set_name(GTK_WIDGET(books), "label-white");
    gtk_widget_set_name(GTK_WIDGET(label), "label-white");
    GtkEntry *title = GTK_ENTRY(gtk_entry_new());
    GtkButton *btn = GTK_BUTTON(gtk_button_new_with_label("Download"));
    gtk_entry_set_placeholder_text(
        GTK_ENTRY(title), "title...");
    book_data.title = title;
    g_signal_connect(btn, "button-press-event", G_CALLBACK(download_book), NULL);

    GtkButton *back = GTK_BUTTON(gtk_button_new_with_label("Back"));
    g_signal_connect(back, "button-press-event", G_CALLBACK(BackAskWhatBook), NULL);

    GtkBox *box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_container_add(GTK_CONTAINER(box), GTK_WIDGET(books));
    gtk_container_add(GTK_CONTAINER(box), GTK_WIDGET(label));
    gtk_container_add(GTK_CONTAINER(box), GTK_WIDGET(title));
    gtk_container_add(GTK_CONTAINER(box), GTK_WIDGET(btn));
    gtk_container_add(GTK_CONTAINER(box), GTK_WIDGET(back));

    gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(box));
    gtk_widget_show_all(GTK_WIDGET(window));
}

static void load_css() {
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_path(provider, "styles.css", NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
}

// handlere pentru butonul de back

static gboolean BackUserInfoInteraction(GtkButton *btn, gpointer data) {
    user_info_interaction(NULL, NULL);
    Menu();
}

static gboolean BackAskWhatBook(GtkButton *btn, gpointer data) {
    char title[101] = "";
    write_string_to_fd(sd, title, "[client]Eroare la scrierea catre server!");
    Menu();
}

static gboolean BackRate(GtkButton *btn, gpointer data) {
    rate_book(NULL, NULL);
    Menu();
}
static gboolean BackRecommend(GtkButton *btn, gpointer data) {
    recommend_book(NULL, NULL);
    Menu();
}
static gboolean BackSearch(GtkButton *btn, gpointer data) {
    search_book(NULL, NULL);
    Menu();
}
static gboolean BackDownload(GtkButton *btn, gpointer data) {
    int down = 0;
    if(write(sd, &down, 4) != 4) {
        perror("[client] Eroare la scriere!");
        exit(1);
    }
    Menu();
}

