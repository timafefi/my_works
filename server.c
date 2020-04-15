#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <unistd.h>

enum arr_ext{queue = 5, piece = 4, buf_size = 1024};
enum exits{noteq_show, noteq_sum, eq_sum, wrong_sum_args};

struct elem{
    int fd;
    int last_index;
    char *buf;
    struct elem *next;
};

int arrsize(int *arr){
    int i=0, size=0;
    while(arr && arr[i++]!=0)
        size++;
    return size;
}

int global_number = 0;

void loading_screen(struct elem *user,int u_count, int u_max)
{
    static const char l_str1[] = {"The game has not started yet\n"}; 
    char l_str2[buf_size];
    int l=sprintf(l_str2, "%d/%d users connected\n", u_count, u_max);
    write(user->fd, l_str1, sizeof(l_str1));
    write(user->fd, l_str2, l+1);
}

void start_screen(int new_fd)
{
    const char str[] = {"Welcome to the best server\n"};
    const char str1[] = {
        "List of commands:\n" 
        "add <number> - add a number to the global value\n"
        "show - print the global value\n"
        };
    write(new_fd, str, sizeof(str));
    write(new_fd, str1, sizeof(str1));
}

void shutdown_user(struct elem **user)
{
    (*user)->fd = -1;
    shutdown((*user)->fd, 2);
    close((*user)->fd);
}

void too_many_users(int ls)
{
    char str[] = {
        "---------------------------------------------------------\n"
        "Sorry, too many users are already connected to the server\n"
        "---------------------------------------------------------\n"};
    int new_fd;
    if (-1 ==  (new_fd = accept(ls, NULL, NULL)))
        perror("accept");
    else{
        write(new_fd, str, sizeof(str));
        shutdown(new_fd, 2);
        close(new_fd);
    }
}

void handle_new_user(struct elem **user, int ls)
{
    int new_fd;
    if (-1 ==  (new_fd = accept(ls, NULL, NULL)))
        perror("accept");
    if (*user){
        struct elem *tmp = *user;
        while(tmp->next!=0)
            tmp = tmp->next;
        tmp->next = malloc(sizeof(struct elem));
        tmp->next->fd = new_fd;
        tmp->next->buf = malloc(buf_size*sizeof(char));
        tmp->next->next = NULL;
        tmp->last_index = 0;
    }
    else{
        (*user) = malloc(sizeof(struct elem));
        (*user)->fd = new_fd;
        (*user)->buf = malloc(buf_size*sizeof(char));
        (*user)->next = NULL;
        (*user)->last_index = 0;
    }
    start_screen(new_fd);
}

void delete_fd(struct elem **user)
{
    if ((*user)->fd == -1)
        *user = (*user)->next;
    struct elem *tmp = *user;
    struct elem *del;
    while(tmp){
        if (tmp->next && tmp->next->fd == -1){
            del = tmp->next;
            tmp->next = tmp->next->next;
            free(del);
        }
        tmp = tmp->next;
    }
}

int buildfds(fd_set *readfds, struct elem **user, int max_fd)
{
    struct elem *tmp = *user;
    while(tmp!=NULL){
        if (-1 != tmp->fd)
            FD_SET(tmp->fd, readfds);
        else
            delete_fd(user);
        if (tmp->fd>max_fd)
            max_fd = tmp->fd;
        tmp = tmp->next;
    }
    return max_fd;
}

int str_to_int(char *str)
{
    int i = 0, port = 0;
    while(str[i]!=0){
        port = 10*port+str[i]-'0';
        i++;
    }
    return port;
}

void bindls(int ls, char *str)
{
    struct sockaddr_in addr;
    int opt = 1;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(str_to_int(str));
    addr.sin_addr.s_addr = INADDR_ANY;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (0 != bind(ls, (struct sockaddr *) &addr, sizeof(addr))){
        perror("bind");
        exit(1);
    }
    if (-1 == listen(ls, queue)){
        perror("listen");
        exit(1);
    }
}

int cmp_cmd(char *str1, char *str2)
{
    int i = 0;
    if (!str1 || !str2)
        return 0;
    while (str1[i]!=0 || str2[i]!=0){
        if (str1[i] != str2[i])
            return 0;
        i++;
    }
    return 1;
}

int  check_for_sum(struct elem **user)
{
    int minus_flag = 1;
    char str[] = {"add"};
    int i = 0;
    while(str[i]!=0){
        if (((*user)->buf[i] != str[i]) || (*user)->buf[i]==0)
            return noteq_sum;
        i++;
    }
    i++;
    int num = 0;
    if ((*user)->buf[i] == '-'){
        minus_flag = -1;
        i++;
    }
    while((*user)->buf[i]!=0){
        if (((*user)->buf[i]<'0'||(*user)->buf[i] >'9'))
            return wrong_sum_args;
        num = 10*num+(*user)->buf[i]-'0';
        i++;
    }
    num = minus_flag*num;
    global_number+= num;
    return eq_sum;
}

void sayhi(struct elem *u_all)
{
    char str[] = "Hi\n";
    while (u_all){
        write(u_all->fd, str, sizeof(str));
        u_all = u_all->next;
    }
}

void  execute_user_cmd(struct elem **user, struct elem **u_all)
{
    static const char err[] = "error : wrong command\n";
    static const char err_sum[] = "sum: wrong argument\n";
    char message[10];
    int l, hi, add, show;
    if((show = cmp_cmd((*user)->buf, "show"))){
        l = sprintf(message, "%d\n", global_number);
        write((*user)->fd, message, l+1);
    }
    else if(wrong_sum_args == (add = check_for_sum(user)))
        write((*user)->fd, err_sum, sizeof(err));
    if ((hi = cmp_cmd((*user)->buf, "sayhi")))
        sayhi(*u_all);
    if (hi == noteq_show && show == noteq_show && add == noteq_sum)
        write((*user)->fd, err, sizeof(err));
}

int read_user_cmd(struct elem **user,struct elem **u_all,int u_count, int u_max)
{
    int res;
    char *tmp_buf = malloc(buf_size*sizeof(char));;
    if (-1 == (res = read((*user)->fd, tmp_buf, buf_size-1)))
        perror("read");
    else if (0 == res){
        shutdown_user(user);
        return 1;
    }
    else{
        if (u_count == u_max){
            int i = 0;
            tmp_buf[res] = 0;
            while(tmp_buf[i]!='\r'&&tmp_buf[i]!=0&&tmp_buf[i]!='\n'){
                (*user)->buf[(*user)->last_index] = tmp_buf[i];
                i++;
                (*user)->last_index++;
            }
            (*user)->buf[(*user)->last_index] = 0;
            if (tmp_buf[i]=='\r'||tmp_buf[i]=='\n'){
                (*user)->last_index = 0;
                execute_user_cmd(user, u_all);
            }
            free(tmp_buf);
        }
        else
            loading_screen(*user, u_count, u_max);
    }
    return 0;
}

int main(int argc, char **argv)
{   
    int res, ls, u_count = 0, u_max = 2;
    struct elem *user = NULL, *tmp = NULL;
    if (argc < 2){
        fprintf(stderr, "You need to specify your port\n");
        exit(1);
    }
    if (argc > 2){
        u_max = str_to_int(argv[2]);
    }
    ls = socket(AF_INET, SOCK_STREAM, 0);
    if (ls==-1){
        perror("socket()");
        exit(1);
    }
    bindls(ls, argv[1]);
    for (;;){
        fd_set readfds;
        int max_fd = ls;
        FD_ZERO(&readfds);
        FD_SET(ls, &readfds);
        max_fd = buildfds(&readfds, &user, max_fd);
        res = select(max_fd+1, &readfds, NULL, NULL, NULL);
        if (res == -1){
            perror("Select");
            continue;
        }
        if (FD_ISSET(ls, &readfds)){
            if (u_count < u_max){
                handle_new_user(&user, ls);
                u_count++;
            }
            else
                too_many_users(ls);
        }
        tmp = user;
        while(tmp!=NULL){
            if (FD_ISSET(tmp->fd, &readfds)){
                u_count -= read_user_cmd(&tmp, &user, u_count, u_max);    
            }
            tmp = tmp->next;
        }

    }
}
