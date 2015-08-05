/*
 *
 */
#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define MAX_THREAD 5

/* You won't lose style points for including these long lines in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *http_version_hdr = "HTTP/1.0\r\n";
static const char *connection_hdr = "Connection: close\r\n";
static const char *proxy_connection_hdr = "Proxy-Connection: close\r\n";
static const char *accept_hdr = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding_hdr = "Accept-Encoding: gzip, deflate\r\n";

void* thread();
void destroy();
void do_proxy(int client_fd);
void get_port(char* host_port, char* host, char* port);
void read_requesthdrs(rio_t *rp);
int parse_uri(char* uri, char* hostname, char* filename);
void clienterror(int fd, char *cause, char *errnum,
		 char *shortmsg, char *longmsg);

//shared buffer data structure
typedef struct {
    int *buf;
    int n;
    int front;
    int rear;
    sem_t mutex;
    sem_t slots;
    sem_t items;
} sbuf_t;
sbuf_t sbuf;


//cache block data structure
struct cache_block{
    char uri[MAXLINE];
    char response[MAX_OBJECT_SIZE];
    struct cache_block *next;
};
struct cache_block* head;
struct cache_block* tail;
int rcnt = 0;
sem_t cache_mutex, cache_w;
int total = 0;
sem_t total_mutex;

void sbuf_init(sbuf_t *sp, int n);
void sbuf_deinit(sbuf_t * sp);
void sbuf_insert(sbuf_t *sp, int item);
int sbuf_remove(sbuf_t *sp);
void cache_init();
struct cache_block* find_cache(char* cache_key);
void insert_block(char* cache_key, char* buf);
void evict_block(char* cache_key, char* buf);
int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    sbuf_init(&sbuf, MAX_THREAD);
    cache_init();
    Signal(SIGPIPE, SIG_IGN);

    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    int i = 0;
    pthread_t tid[MAX_THREAD];
    //thread pool of 5 threads
    for (; i < MAX_THREAD; i ++) {
        Pthread_create(&tid[i], NULL, thread, NULL);
    }

    listenfd = Open_listenfd(argv[1]);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE,
                port, MAXLINE, 0);
        sbuf_insert(&sbuf, connfd);
    }

    destroy();
    return 0;
}

/*
 * routine for every thread
 */

void* thread() {
    Pthread_detach(pthread_self());
    while (1) {
        int connfd = sbuf_remove(&sbuf);
        do_proxy(connfd);
        Close(connfd);
    }
}

/*
 * do proxy job
 */

void do_proxy(int client_fd) {
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE];
    char server_host_port[MAXLINE], server_port[MAXLINE], server_host[MAXLINE];
    char request_buf[MAXLINE];
    char cache_key[MAXLINE];
    int accept_encoding = 0, accept = 0, host = 0;
    rio_t rio;

    /* Read request line and headers */
    Rio_readinitb(&rio, client_fd);
    if (!Rio_readlineb(&rio, buf, MAXLINE))
        return;

    /* parse request line */
    sscanf(buf, "%s %s %s", method, uri, version);
    if (strcasecmp(method, "GET")) {
        clienterror(client_fd, method, "501", "Not Implemented",
                "Tiny does not implement this method");
        return;
    }
    strcpy(cache_key, "");
    strcpy(server_host_port, "");
    strcpy(server_port, "80");

    if (parse_uri(uri, server_host_port, filename) < 0)
        return;
    strcpy(request_buf, method);
    strcat(request_buf, " ");
    strcat(request_buf, filename);
    strcat(request_buf, " ");
    strcat(request_buf, http_version_hdr);
    get_port(server_host_port, server_host, server_port);

    /* Get header key-value pair */
    Rio_readlineb(&rio, buf, MAXLINE);

    while (strcmp(buf, "\r\n")) {
        if (strstr(buf, "Accept-Encoding: ") != NULL) {
            strcat(request_buf, buf);
            accept_encoding = 1;
        } else if (strstr(buf, "Accept: ") != NULL) {
            strcat(request_buf, buf);
            accept = 1;
        } else if (strstr(buf, "Host: ") != NULL) {
            strcat(request_buf, buf);
            if (strlen(server_host_port) <= 1) {
                sscanf(buf, "Host: %s", server_host_port);
                get_port(server_host_port, server_host, server_port);
            }
            host = 1;
        } else {
            strcat(request_buf, buf);
        }

        Rio_readlineb(&rio, buf, MAXLINE);
    }

    strcat(request_buf, user_agent_hdr);
    strcat(request_buf, connection_hdr);
    strcat(request_buf, proxy_connection_hdr);
    if (!accept_encoding)
        strcat(request_buf, accept_encoding_hdr);
    if (!accept)
        strcat(request_buf, accept_hdr);
    if (!host) {
        sprintf(buf, "Host: %s\r\n", server_host_port);
        strcat(request_buf, buf);
        get_port(server_host_port, server_host, server_port);
    }
    strcat(request_buf, "\r\n");
    if (strcmp(server_host_port, "") == 0)
        return;
    strcat(cache_key, server_host_port);
    strcat(cache_key, filename);


    /* find if there exists cache */
    char response[MAX_OBJECT_SIZE];
    P(&cache_mutex);
    rcnt ++;
    if (rcnt == 1)
        P(&cache_w);
    V(&cache_mutex);
    struct cache_block* pre_block = find_cache(cache_key);
    if (pre_block != NULL)
        strcpy(response, pre_block->next->response);
    P(&cache_mutex);
    rcnt --;
    if (rcnt == 0)
        V(&cache_w);
    V(&cache_mutex);

    if (pre_block == NULL) {
        /* forward to server */
        int forward_fd = Open_clientfd(server_host, server_port);
        rio_t server_rio;
        size_t num_of_bytes;
        char server_buf[MAXLINE];
        char total_buf[10 * MAX_OBJECT_SIZE];
        strcpy(total_buf, "");
        if (forward_fd == -1)
            return;
        Rio_readinitb(&server_rio, forward_fd);
        Rio_writen(forward_fd, request_buf, strlen(request_buf));

        /* forward to client */

        while ((num_of_bytes = Rio_readlineb(&server_rio, server_buf, MAXLINE)) != 0) {
            Rio_writen(client_fd, server_buf, num_of_bytes);
            strcat(total_buf, server_buf);
        }
        Close(forward_fd);

        //insert the buf into cache
        num_of_bytes = strlen(total_buf);
        if (num_of_bytes <= MAX_OBJECT_SIZE) {
            if (num_of_bytes + total <= MAX_CACHE_SIZE) {
                P(&cache_w);
                insert_block(cache_key, total_buf);
                V(&cache_w);
            } else {
                P(&cache_w);
                evict_block(cache_key, total_buf);
                V(&cache_w);
            }
            P(&total_mutex);
            total += num_of_bytes;
            V(&total_mutex);
        }
    } else {
        Rio_writen(client_fd, response, strlen(response));
    }

}

/*
 * parse request line
 */
int parse_uri(char* uri, char* hostname, char* filename) {
    char protocol[MAXLINE];
    strcpy(filename, "/");
    if (strstr(uri, "://") != NULL) {
       sscanf(uri, "%[^:]://%[^/]%s", protocol, hostname, filename);
       if (strcasecmp(protocol, "http"))
           return -1;
       else
           return 0;
    } else {
        sscanf(uri, "%[^/]%s", hostname, filename);
        return 0;
    }
}

/*
 * get host and port
 */

void get_port(char* hostport, char* host, char* port) {
    char* pos = NULL;
    pos = index(hostport, ':');
    if (pos != NULL) {
        strcpy(port, pos + 1);
        *pos = '\0';
    } else {
        strcpy(port, "80");
    }
    strcpy(host, hostport);
}

/* initialize cache list */
void cache_init() {
    Sem_init(&cache_mutex, 0, 1);
    Sem_init(&cache_w, 0, 1);
    Sem_init(&total_mutex, 0, 1);
    head = Malloc(sizeof(struct cache_block));
    tail = Malloc(sizeof(struct cache_block));
    head->next = NULL;
    tail->next = NULL;
    tail = head;
}

/* find cache by cache_key */
struct cache_block* find_cache(char* cache_key) {
    struct cache_block* pre = head;
    struct cache_block* walker = head->next;
    while (walker != NULL) {
        if (!strcasecmp(walker->uri, cache_key))
            return pre;
        pre = pre->next;
        walker = walker->next;
    }
    return NULL;
}

/* insert a new buf into cache */
void insert_block(char* cache_key, char* buf) {
    struct cache_block* new_block = Malloc(sizeof(struct cache_block*));
    strcpy(new_block->uri, cache_key);
    strcpy(new_block->response, buf);
    new_block->next = NULL;
    if (head->next == NULL) {
        head->next = new_block;
        tail = new_block;
    } else {
        tail->next = new_block;
        tail = new_block;
    }
}

/* evict a block using LRU */
void evict_block(char* cache_key, char* buf) {
    struct cache_block* new_block = Malloc(sizeof(struct cache_block*));
    strcpy(new_block->uri, cache_key);
    strcpy(new_block->response, buf);
    new_block->next = NULL;
    struct cache_block* pre = head;
    struct cache_block* walker = head->next;
    int buf_size = strlen(buf);
    int temp = 0;
    while (walker != NULL) {
        int size = strlen(walker->response);
        pre->next = walker->next;
        if (walker == tail)
            tail = pre;
        Free(walker);
        walker = pre->next;
        temp += size;
        if (temp >= buf_size) {
            tail->next = new_block;
            tail = new_block;
            P(&total_mutex);
            total -= temp;
            V(&total_mutex);
            return;
        }
    }
    return;
}

/* create an empty, bounded, shared FIFO buffer withn slots */
void sbuf_init(sbuf_t *sp, int n) {
    sp->buf = Calloc(n, sizeof(int));
    sp->n = n;
    sp->front  = sp->rear = 0;
    Sem_init(&sp->mutex, 0, 1);
    Sem_init(&sp->slots, 0, n);
    Sem_init(&sp->items, 0, 0);
}

/* clean up buffer sp */
void sbuf_deinit(sbuf_t *sp) {
    Free(sp->buf);
}

/* clean up extra allocated memory */
void destroy() {
    sbuf_deinit(&sbuf);
    struct cache_block* pre = head;
    struct cache_block* walker = head->next;
    while (walker != NULL) {
        pre->next = walker->next;
        Free(walker);
        walker = pre->next;
    }
    Free(head);
}

/* Insert item onto the rear of shared buffer sp */
void sbuf_insert(sbuf_t *sp, int item) {
    P(&sp->slots);
    P(&sp->mutex);
    sp->buf[(++sp->rear) % (sp->n)] = item;
    V(&sp->mutex);
    V(&sp->items);
}

/* Remove and return the first item from buffer sp */
int sbuf_remove(sbuf_t *sp) {
    int item;

    P(&sp->items);
    P(&sp->mutex);
    item = sp->buf[(++sp->front) % (sp->n)];
    V(&sp->mutex);
    V(&sp->slots);
    return item;
}

/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum,
		 char *shortmsg, char *longmsg) {
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Proxy Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}
/* $end clienterror */
