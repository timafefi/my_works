#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

enum technical_constants{
    min_arr_size = 4,
    status_size = 6,
    flags_size = 6
};

struct line{
    char *word;
    struct line *next;
};

enum choices{
    end, 
    strt, 
    mdl
};

enum flags{
    fword,
    fconnect,
    fquote,
    fspecs,
    fredir,
    fpipe,
};

struct exit_stat{ /*exit situations*/
    int eof;
    int eol;
    int quote;
    int redir;
    int redirfile;
    int pipe;
    int amper;
    int background;
};

void delete_array(char **arr)
{
    free(*arr);
    *arr = NULL;
}

void delete_line(struct line **l)
{ 
    if (*l != NULL){
        delete_line(&((*l)->next));
        free(*l);
        *l = NULL;
    }
}

void delete_argv(char **arr)
{
    int i = 0;
    while (arr[i]!=NULL){
        free(arr[i]);
        arr[i] = NULL;
        i++;
    }
}

int cmp_strings(char *s1, char *s2)
{
    int i = 0;
    if (!s1 || !s2)
        return 0;
    while ((s1[i]!=0)&&(s2[i]!=0)){
        if (s1[i] != s2[i])
            return 0;
        i++;
    }
    return 1;
}

int count_letters(char *word)
{
    int i = 0;
    while (word[i]!=0)
        i++;
    return i;
}

int count_words(struct line *list)
{
    int count = 0;
    struct line *tmp = NULL;
    tmp = list;
    while (tmp != NULL){
        count++;
        tmp = tmp->next;
    }
    return count;
}

int count_fd(char ***cmd)
{
    int i = 0;
    if (!cmd)
        return 0;
    while (cmd[i]!=NULL)
        i++;
    return i;
}

/*prints your list*/
void print_line(struct line **l)
{ 
    if (*l != NULL){
        puts((*l)->word);
        print_line(&((*l)->next));
    }
}

/*puts a word into a list of words*/
void assemble_words(char *word, struct line **full)         
{
    if (*full == NULL){
        *full = malloc(sizeof(struct line));
        (*full)->next = NULL;
        (*full)->word = word;
    }
    else
        assemble_words(word, &(*full)->next);
}

/*Puts a letter in the end of an array*/
char *put_in_word(char *word, char letter)
{
    int zero_index;
    zero_index = count_letters(word);
    if ((zero_index % (min_arr_size-1)  == 0)&&(zero_index!=0)){
        char *tmp;
        int i=0;
        tmp = malloc((zero_index+min_arr_size)*sizeof(char));
        for (i = 0; i<zero_index; i++)
            tmp[i] = word[i];
        free(word);
        word = tmp;
    }
    word[zero_index] = letter;
    word[zero_index+1] = 0;
    return word;
}

int word_state(int old, int new)
{
    int oldchar_criteria = old == '\n' || old == ' ';
    int newchar_criteria = (new != ' ')&&(new != EOF)&&(new != '\n');
    if ((oldchar_criteria)&&(newchar_criteria))
        return strt; 
    if ((!oldchar_criteria)&&(!newchar_criteria))
        return end;
    return mdl;
}

void met_more(char **tmp_more, char **more_tail, int count, int size, int i)
{
    *tmp_more = malloc((count+1)*sizeof(char));
    (*tmp_more)[0] = '>';
    (*tmp_more)[count-1]='>';
    (*tmp_more)[count]=0;
    *more_tail = malloc((size-i+1)*sizeof(char));
    (*more_tail)[0] = 0;
}

void met_less(char **tmp_less, char **less_tail, int count, int size, int i)
{
    *tmp_less = malloc((count+1)*sizeof(char));
    (*tmp_less)[0] = '<';
    (*tmp_less)[1] = 0;
    *less_tail = malloc((size-i+1)*sizeof(char));
    (*less_tail)[0] = 0;
}

void assemble_filenames(char **redir, char **filename, struct line **l)
{
    if (*redir)
        assemble_words(*redir, l);
    if (*filename){
        if ((*filename)[0]!=0)
            assemble_words(*filename, l);
        else
            free(*filename);
    }
}

void more_or_less(char *word, struct line **l)
{
    char *tmp_more = NULL, *tmp_less = NULL, *more_tail = NULL;
    char *less_tail = NULL;
    int i = 0, k = 0, more_flag = 0, less_flag = 0;
    int size = count_letters(word);
    while(word[i]!=0){
        if(more_flag && word[i]!='<'){
            more_tail[k] = word[i];
            more_tail[k+1] = 0;
            k++;
        }
        if (less_flag && word[i]!='>'){
            less_tail[k] = word[i];
            less_tail[k+1] = 0;
            k++;
        }
        if ((word[i]=='>' || word[i]=='<')){
            int count = 1;
            if (word[i]=='>'){
                if (word[i+1]=='>')
                    count++;
                met_more(&tmp_more, &more_tail, count, size, i);
                more_flag = 1;
                less_flag = 0;
            }
            if (word[i]=='<'){
                met_less(&tmp_less, &less_tail, count, size, i);
                less_flag = 1;
                more_flag = 0;
            }
            word[i] = 0;
            if (count > 1)
                i++;
            k = 0;
        }
        i++; 
    }
    if (word[0]!=0)
        assemble_words(word, l);
    else
        free(word);
    assemble_filenames(&tmp_more, &more_tail, l);
    assemble_filenames(&tmp_less, &less_tail, l);
}

void divide_pipe(char *word, struct line **sentense)
{
    char *tmp_pipe, *tmp_cmd;
    int i = 0, k = 0, c = 0, word_index = 0;
    while(word[i]!=0){
        if (word[i]=='|'){
            tmp_pipe = malloc(2*sizeof(char));
            tmp_pipe[0] = '|';
            tmp_pipe[1] = 0;
            if (i!=word_index){
                tmp_cmd = malloc((i-word_index+1)*sizeof(char));
                for (k = word_index; k<i; k++){
                    tmp_cmd[c] = word[k];
                    c++;
                }
                tmp_cmd[c] = 0;
                c=0;
                more_or_less(tmp_cmd, sentense);
                /*assemble_words(tmp_cmd, sentense);*/
            }
            assemble_words(tmp_pipe, sentense);
            word_index = i+1;
            tmp_cmd = NULL;
            tmp_pipe = NULL;
        }
        i++;
    }
    if (i!=word_index){
        tmp_cmd = malloc((i-word_index+1)*sizeof(char));
        for (k = word_index; k<i; k++){
            tmp_cmd[c] = word[k];
            c++;
        }
        tmp_cmd[c]=0;
        more_or_less(tmp_cmd, sentense);
        /*assemble_words(tmp_cmd, sentense);*/
    }
    free(word);
}

void count_redir_symbols(int *f_more, int *f_less, int n_ch, int p_ch)
{
    if (n_ch=='>' && p_ch!='>')
        f_more++;
    if (n_ch=='<')
        f_less++;
}

void connect_word_with_line(int *flag, char **word, struct line **sentense)
{

    if ((!flag[fredir] && !flag[fpipe]))
        assemble_words(*word, sentense);
    else if (!flag[fpipe]&&flag[fredir])
        more_or_less(*word, sentense);
    else if (flag[fpipe]){
        divide_pipe(*word, sentense);
        flag[fpipe] = 0;
    }
    *word = NULL;
    *word = malloc(min_arr_size*sizeof(char));
    *word[0] = 0;
    flag[fconnect] = 0;
    flag[fredir] = 0;
}

void set_flags(int *flag, int n_ch, int p_ch, int *c1, int *c2)
{
    if (!flag[fquote]){
        if (word_state(p_ch, n_ch)==strt)
            flag[fword] = 1;
        if (word_state(p_ch, n_ch) == end){
            flag[fword] = 0;
            flag[fconnect] = 1;
        }
    }
    if (n_ch=='>' || n_ch=='<'){
        flag[fredir] = 1;
        count_redir_symbols(c1, c2, n_ch, p_ch);
    }
    if (n_ch=='|')
        flag[fpipe] = 1;
}

int get_line(struct line **sentense, struct exit_stat *exits)
{
    char *word;
    int n_ch, p_ch, f_more = 0, f_less = 0; //new_char, old_char
    int flag[6] = {0, 0, 0, 0, 0, 0};
    word = malloc(min_arr_size*sizeof(char));
    word[0] = 0;
    p_ch = ' ';
    n_ch = getchar();
    while ((p_ch != '\n')&&(p_ch!=EOF)){
        set_flags(flag, n_ch, p_ch, &f_more, &f_less);
        if (p_ch=='&' && word_state(p_ch,n_ch)==mdl && !flag[fquote]){
            free(word);
            exits->amper = 1;
            return 1;
        }
        if (f_more>1 || f_less>1){
            free(word);
            exits->redir = 1;
            return 1;
        }
        if (flag[fword] || flag[fquote])
            word = put_in_word(word, n_ch);
        if (flag[fconnect])
            connect_word_with_line(flag, &word, sentense);
        if (n_ch == '\n' || n_ch == EOF){
            if (flag[fquote]){
                free(word);
                exits->quote = 1;
                return 1;
            }
        }
        if (n_ch == '"')
            flag[fquote] = !flag[fquote];
        p_ch = n_ch;
        if ((n_ch != EOF)&&(n_ch!= '\n'))
            n_ch = getchar();
    }
    free(word);
    if (n_ch == EOF){
        exits->eof = 1;
        return 0;
    }
    exits->eol = 1;
    return 0;
}

int count_pipes(struct line *l)
{
    int count = 0;
    while (l!=NULL){
        if (l->word[0] == '|')
            count++;
        l = l->next;
    }
    return count+1;
}

char ***put_line_in_array(struct line *list)
{
    char ***arr_of_arrs = NULL;
    char **array;
    int size,k = 0, i = 0, commands;
    if (list == NULL)
        return NULL;
    size = count_words(list);
    commands = count_pipes(list);
    array = malloc((size+1)*sizeof(char*));
    arr_of_arrs = malloc((commands+1)*sizeof(char**));
    while (list != NULL){
        if (list->word[0]!='|')
            array[i] = list->word;
        else{
            free(list->word);
            array[i] = NULL;
            arr_of_arrs[k] = array;
            array = malloc((size+1)*sizeof(char*));
            k++;
            i = -1;
        }
        i++;
        list = list->next;
    }
    array[i] = NULL;
    if (array[0]!=0){
        arr_of_arrs[k] = array;
        arr_of_arrs[k+1] = NULL;
    }
    else
        arr_of_arrs[k] = NULL;
    i = 0;
    return arr_of_arrs;
}

void delete_quotes(char **cmd)
{
    int i = 0, k = 0, k_new = 0, size = 0;
    char *tmp = NULL;
    while(cmd[i]!=0){
        if (cmd[i][0] == '"'){ 
            while(cmd[i][k]!=0){
                if (cmd[i][k]!='"')
                    size++;
                k++;
            }
            k = 0;
            tmp = malloc((size+1)*sizeof(char));
            while(cmd[i][k]!=0){
                if (cmd[i][k]!='"'){
                    tmp[k_new] = cmd[i][k];
                    k_new++;
                }
                k++;
            }
            tmp[k_new] = 0;
            free(cmd[i]);
            cmd[i] = tmp;
        }
        i++;
        k = k_new = size = 0;
   }
}

int delete_amper(char **cmd)
{   
    int i = 0, k = 0, size = 0;
    while(cmd[i]!=0){
        if (cmd[i][0] != '"'){ 
            size = count_letters(cmd[i]);
            if (cmd[i][0]=='&'){
                k = i;
                while(cmd[k]!=0){
                    free(cmd[k]);
                    cmd[k] = NULL;
                    k++;
                }
                return 1;
            }
            if (cmd[i][size-1]=='&'){
                cmd[i][size-1]= 0;
                k = i+1;
                while(cmd[k]!=0){
                    free(cmd[k]);
                    cmd[k] = NULL;
                    k++;
                }
                return 1;
            }
        }
        i++;
        size = 0;
    }
    return 0;
}

void kill_background()
{
    int w, status;
    do{
        w = waitpid(-1, &status, WNOHANG);
        if (w>0)
            printf("A process has successfully finished\n");
    }while(w>0);
}

int  divide_io(char **cmd, char **strtfile, char **destfile)
{
    int i = 0;
    int dest_flag = 0;
    while(cmd[i]!=0){
        if (cmd[i] &&cmd[i][0]=='>' && cmd[i][1]==0){
            *destfile = cmd[i+1];
            free(cmd[i]);
            cmd[i] = 0;
        }
        if (cmd[i] && cmd[i][0]=='<'){
            *strtfile = cmd[i+1];
            free(cmd[i]);
            cmd[i] = 0;
        }
        if (cmd[i] && cmd[i][0] == '>' && cmd[i][1] == '>'){
            *destfile = cmd[i+1];
            free(cmd[i]);
            cmd[i] = 0;
            dest_flag = 1;
        }
        i++;
    }
    return dest_flag;
}

void exec_chdir(char **cmd)
{
    int s = 0;
    if (cmd[1]!=NULL && cmd[2]==NULL){ 
        s = chdir(cmd[1]);
    }
    if (cmd[1]!=NULL && cmd[2]!=NULL){
        printf("cd: too many arguments\n");
    }
    if (cmd[1]==NULL)
        printf("cd: too little arguments\n");
    if (s == -1)
        printf("cd: wrong directory\n");
}

void exec_fork(char **cmd, char *strt, char *dest, int add_f)
{
    if (dest){
        int fd;
        if (!add_f)
            fd=open(dest, O_CREAT|O_WRONLY|O_TRUNC, 0666);
        else
            fd=open(dest, O_APPEND|O_WRONLY, 0666);
        if (fd == -1){
            perror(dest);
            exit(1);
        }
        dup2(fd, 1);
        close(fd);
    }
    if (strt){
        int fd=open(strt, O_CREAT|O_RDONLY, 0666);
        if (fd == -1){
            perror(strt);
            exit(1);
        }
        dup2(fd, 0);
        close(fd);
    }
    execvp(cmd[0], cmd);
    perror(cmd[0]);
    exit(1);
}

void first_pipe(int *fd)
{
    close(fd[0]);
    dup2(fd[1], 1);
    close(fd[1]);
}

void mid_pipe(int *fd1, int *fd2)
{
    close(fd1[1]);
    close(fd2[0]);
    dup2(fd2[1], 1);
    dup2(fd1[0], 0);
    close(fd2[1]);
    close(fd1[0]);
}

void last_pipe(int *fd1, int *fd2)
{
    close(fd2[0]);
    close(fd2[1]);
    close(fd1[1]);
    dup2(fd1[0], 0);
    close(fd1[0]);
}

int empty_arrpid(int *arr_pid, int len, int pid)
{
    int i = 0;
    for (i = 0; i<len; i++)
        if (arr_pid[i]==pid)
            arr_pid[i]=0;
    for (i = 0; i<len; i++)
        if (arr_pid[i]!=0)
            return 0;
    return 1;
}

void kill_parent_fd(int *fd, int i, int fd_len)
{
    close(fd[0]);
    close(fd[1]);
}

void wait_for_cmd(char **cmd, int *pid, int fd_len)
{
    if (cmd==NULL){
        int w;
        do{
            w =wait(NULL);
        }while(!empty_arrpid(pid, fd_len, w));
    }
}

int **alloc_fd_array()
{
    int i;
    int *arr;
    int **fd;
    fd = malloc(2*sizeof(int *));
    for (i = 0; i<2; i++){
        arr = malloc(2*sizeof(int));
        fd[i] = arr;
    }
    return fd;
}

void fd_switch_places(int **fd)
{
      free(fd[0]);
      fd[0] = fd[1];
      fd[1] = malloc(2*sizeof(int));
}

void close_fd(int i, int fd_len, int **fd)
{
    if (i!=0)
        kill_parent_fd(fd[0], i, fd_len);
    if (i==fd_len-1 && i>1)
        kill_parent_fd(fd[1], i, fd_len);
    fd_switch_places(fd);
}

int execute_cmd(char ***cmd, int fd_len)
{
    int amper = 0, add_to_file = 0, i = 0;
    char *destfile = NULL, *strtfile = NULL;
    int **fd = NULL;
    int *pid = malloc(fd_len*sizeof(int));
    pid[fd_len] = 0;
    fd = alloc_fd_array();
    if (!cmd)
        return 0;
    while (cmd[i]!=NULL){
        amper = delete_amper(cmd[i]);
        add_to_file = divide_io(cmd[i], &strtfile, &destfile);
        delete_quotes(cmd[i]);
        if (cmp_strings(cmd[i][0], "cd"))
            exec_chdir(cmd[i]);
        else if (cmd[i][0]){
            pipe(fd[1]);
            pid[i] = fork();
            if (pid[i] == -1){
                perror("fork");
                exit(1);
            }
            if (pid[i] == 0){
                if (cmd[1]==NULL)
                    exec_fork(cmd[i], strtfile, destfile, add_to_file);
                else{
                    if (i == 0){
                        first_pipe(fd[1]);
                        exec_fork(cmd[i], strtfile, NULL, 0);
                    }
                    if ((i!=0)&&(i!=fd_len-1)){
                        mid_pipe(fd[0], fd[1]);
                        exec_fork(cmd[i], NULL, NULL, 0);
                    }
                    if (i==(fd_len-1)){
                        last_pipe(fd[0], fd[1]);
                        exec_fork(cmd[i], NULL, destfile, add_to_file);
                    }
                }
            }
            close_fd(i, fd_len, fd);
            if (amper)
                printf("PID %d in background\n", pid[i]);
            else
                wait_for_cmd(cmd[i+1], pid, fd_len);
        }
        free(destfile);
        free(strtfile);
        i++;
    }
    return empty_arrpid(pid, 0, fd_len);
}

int check_redir(char ***cmd)
{
    int i = 0, k = 0;
    if (!cmd)
        return 0;
    while(cmd[i]!=NULL){
        while(cmd[i][k]!=NULL){
            if ((cmd[i][k][0]=='>'||cmd[i][k][0]=='<')&&cmd[i+1]!=NULL)
                return 1;
            k++;
        }
    k = 0;
    i++;
    }
    return 0;
}

int filename_empty(char **c)
{
    int i = 1;
    while(c[i]!=NULL){
        if ((c[i][0]=='>'||c[i][0]=='<')&&(c[i-1][0]=='>'||c[i-1][0]=='<'))
            return 1;
        i++;
    }
    if ((c[i-1][0]=='>'||c[i-1][0]=='<')&&(c[i]==NULL))
        return 1;
    return 0;
}

void check_for_errors(struct exit_stat *exits, int *bad_stat, char ***cmd)
{
    if (exits->quote){
        printf("Error: unmatching quotes\n");
        exits->quote = 0;
        *bad_stat = 1;
    }
    if (exits->amper){
        printf("Error: special symbol in an ordinary place:)\n");
        exits->amper = 0;
        *bad_stat = 1;
    }
    if (exits->redir){
        printf("Error: too many redirection symbols\n");
        exits->redir = 0;
        *bad_stat = 1;
    }
    if (exits->redirfile){
        printf("Error: no file for redirection\n");
        exits->redirfile = 0;
        *bad_stat = 1;
    }
    if (check_redir(cmd)){
        printf("Error: attempt to redirect output inside pipes\n");
        exits->pipe = 0;
        *bad_stat = 1;
    }
}

int main()
{
    struct exit_stat exits = {};
    struct line *cmd_l = NULL;
    int  bad_stat = 0, i = 0, fd_len;
    char ***cmd = NULL;
    exits.eol = 1;
    while (!exits.eof){
        if (!exits.eof && !bad_stat)
            printf("$: ");
        exits.eol = !get_line(&cmd_l, &exits);
        cmd = put_line_in_array(cmd_l);
        if (cmd){
            while(cmd[i]!=0 && !exits.redirfile){
                exits.redirfile = filename_empty(cmd[i]);
                i++;
            }
            i=0;
        }
        check_for_errors(&exits, &bad_stat, cmd);
        if (exits.background)
            kill_background();
        if (exits.eol && !bad_stat){
            fd_len=count_fd(cmd);
            exits.background = !execute_cmd(cmd, fd_len);
        }
        if (exits.eol)
            bad_stat = 0;
        if (exits.eof)
            putchar('\n');
        if (cmd){
            while(cmd[i]!=NULL){
                delete_argv(cmd[i]);
                free(cmd[i]);
                i++;
            }
        i = 0;
        free(cmd);
        }
        if (cmd_l){
            delete_line(&cmd_l);
            free(cmd_l);
        }
    }
    return 0;
} 
