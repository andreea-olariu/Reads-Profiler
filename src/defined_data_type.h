
typedef struct user {
    char username[101];
    char password[101];
};

typedef struct book_search {
    char title[101];
    char author_first_name[101];
    char author_last_name[101];
    char isbn[11];
    char genre[101];
    char publish_year[5];
    char rating[3];
};

typedef struct thData {
    int idThread;
    int client;
    struct user current_user;
};
