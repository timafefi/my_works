#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>

enum arr_ext{queue = 5, piece = 4, buf_size = 1024};
enum exits{noteq_show, noteq_sum, eq_sum, wrong_sum_args};
enum prices{
    build_fab = 5000, 
    raw_tax = 300, 
    prod_tax = 500, 
    fab_tax = 1000,
    makep_money = 2000,
    makep_raw = 1,
    makep_prod = 2,
};

struct pl{
    int fd;
    int name;
    int ready;
    int lost;
    int disconnected;
    int last_index;
    char *buf;
    int money; 
    int fab;
    int fab_buf;
    int raw;
    int prod;
    int prod_buf;
    int money_buf;
    int raw_buf;
    int bought;
    int sold;
    struct fablist *fabild;
    struct pl *next;
};

struct fablist{
    int delay;
    struct fablist *next;
};

const int level_change[5][5] = {
    {4, 4, 2, 1, 1},
    {3, 4, 3, 1, 1},
    {1, 3, 4, 3, 1},
    {1, 1, 3, 4, 3},
    {1, 1, 2, 4, 4}
};

const int buy_p[5] = {800, 650, 500, 400, 300};
const double buy_i[5] = {1.0, 1.5, 2.0, 2.5, 3.0}; 
const int sell_p[5] = {6500, 6000, 5500, 5000, 4500};
const double sell_i[5] = {3.0, 2.5, 2.0, 1.5, 1.0};

enum argvector_size{dflt_str = 4, cmd_cnt = 3};

struct list{
    int price;
    int amount;
    int name;
    int fd;
    struct list *next;
};

struct mkt{
    int level;
    int sunset;
    int month;
    int u_count;
    int u_max;
    int game_on;
    struct winner_list *w;
    struct list *r_list;
    struct list *p_list;
};

struct winner_list{
    int buy_win_stat;
    int sell_win_stat;
    int buy_price;
    int buy_amount;
    int sell_price;
    int sell_amount;
};

void print_help(int fd)
{
    const char msg[] = {
        "List of commands:\n"
        "market:                see a table of current bank offers\n"
        "player:                see a table of players' resources\n"
        "turn:                  when entered indicates that you are ready to\n"
        "                       end the month\n"
        "prod <amount>:         make <amount> pieces of production.\n"
        "                       Costs 2000\n"
        "build <amount>:        build <amount> factories. Costs 5000\n"
        "buy <amount> <price>:  buy <amount> of raw materials for <price>\n"
        "                       each from the bank\n"
        "sell <amount> <price>: sell <amount> of production for <price> each\n"
        "                       to the bank\n"
        "___________________________________________________________________\n"
        "Each month you have to pay a tax of:\n"
        "300 per each raw material left\n"
        "500 per each piece of production left\n"
        "1000 per each factory built\n"
        "One piece of production costs one piece of raw materials and 2000\n "
    };
    write(fd, msg, sizeof(msg));
}

void change_level(struct mkt *bank)
{
    const int lvl_size = 5;
    int r = 1 + (int)(12.0*rand()/(RAND_MAX+1.0));
    int sum = 0, i = 0, row = bank->level;
    srand(time(NULL));
    for (i = 0; i<lvl_size; i++){
        sum += level_change[row][i];
        if (sum>=(r-1)){
            bank->level = level_change[row][i];
            return;
        }
    }
}

void free_list(struct list **l)
{
    if (*l != NULL){
        free_list(&(*l)->next);
        free(*l);
        *l = NULL;
    }
}

void loading_screen(struct pl *user, int u_count, int u_max)
{
    char l_str1[] = "The game has not started yet\n"; 
    char l_str2[buf_size];
    int l=sprintf(l_str2, "%d/%d users connected\n", u_count, u_max);
    write(user->fd, l_str1, sizeof(l_str1));
    write(user->fd, l_str2, l+1);
}

void too_many_users(int ls)
{
    char str1[] = {
        "----------------------------\n"
        "The game has already started\n"
        "----------------------------\n"};

    int new_fd;
    if (-1 ==  (new_fd = accept(ls, NULL, NULL)))
        perror("accept");
    else{
        write(new_fd, str1, sizeof(str1));
        shutdown(new_fd, 2);
        close(new_fd);
    }
}

void set_default_player(struct pl **user, int new_fd, int num)
{
   (*user)->fd = new_fd;
   (*user)->buf = malloc(buf_size);
   (*user)->next = NULL;
   (*user)->last_index = 0;
   (*user)->ready = 0;
   (*user)->money = 10000;
   (*user)->fab = 2;
   (*user)->raw = 4;
   (*user)->prod = 2;
   (*user)->money_buf = 0;
   (*user)->raw_buf = 0;
   (*user)->prod_buf = 0;
   (*user)->bought = 0;
   (*user)->sold = 0;
   (*user)->disconnected = 0;
   (*user)->fabild = NULL;
   (*user)->name = num;
   (*user)->fab_buf = 0;
   (*user)->lost = 0;
}

void accept_user(struct pl **user, int ls, int num)
{
    int new_fd;
    if (-1 ==  (new_fd = accept(ls, NULL, NULL)))
        perror("accept");
    if (*user){
        struct pl *tmp = *user;
        while(tmp->next!=0)
            tmp = tmp->next;
        tmp->next = malloc(sizeof(struct pl));
        set_default_player(&(tmp->next), new_fd, num);
    }
    else{
        (*user) = malloc(sizeof(struct pl));
        set_default_player(user, new_fd, num);
    }
    print_help(new_fd);
}

void free_fab(struct fablist **f)
{
    if (*f){
        free_fab(&(*f)->next);
        free((*f));
        f = NULL;
    }
}

void free_fd(struct pl **user)
{
    free((*user)->buf);
    free_fab(&(*user)->fabild);
    free(*user);
}

void delete_fd(struct pl **user)
{
    struct pl *del;
    if ((*user)->disconnected){
        del = *user;
        *user = (*user)->next;
        free_fd(&del);
    }
    struct pl *tmp = *user;
    while(tmp){
        if (tmp->next && tmp->next->disconnected){
            del = tmp->next;
            tmp->next = tmp->next->next;
            free_fd(&del);
        }
        tmp = tmp->next;
    }
}

int  buildfds(struct pl **user, fd_set *readfds, int ls)
{
    int max_fd = ls;
    struct pl *tmp = *user;
    FD_ZERO(readfds);
    FD_SET(ls, readfds);
    while(tmp!=NULL){
        if (!tmp->disconnected){
            FD_SET(tmp->fd, readfds);
            if (tmp->fd>max_fd)
                max_fd = tmp->fd;
        }
        else
            delete_fd(user);
        if (tmp)
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
    while ((str1[i]!=0&&str1[i]!=' ')||(str2[i]!=' ' && str2[i]!=0)){
        if (str1[i] != str2[i])
            return 0;
        i++;
    }
    return 1;
}

int  check_for_sum(struct pl **user)
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
    return eq_sum;
}

void show_mkt(struct pl **user, struct mkt *bank)
{
    char *str_time = malloc(buf_size);
    char *info = malloc(buf_size);
    const char p_info[] = "Players still active\n";
    const char header1[] = "Bank sells:  items  min.price\n";
    const char header2[] = "Bank buys:   items  max.price\n";
    const char fab_info[] = "A fabric costs 5000\n";
    int l = sprintf(str_time, "Current month is %dth\n", bank->month);
    int r = bank->level, amnt = bank->u_count;
    write((*user)->fd, str_time, l+1);
    write((*user)->fd, p_info, sizeof(p_info));
    l = sprintf(info, "%% %12d\n", bank->u_count);
    write((*user)->fd, info, l+1);
    l=sprintf(info, "%% %14d %7d\n",(int) buy_i[r]*amnt,(int) buy_p[r]);
    write((*user)->fd, header1, sizeof(header1));
    write((*user)->fd, info, l+1);
    l=sprintf(info,"%% %14d %7d\n",(int) sell_i[r]*amnt,(int)sell_p[r]);
    write((*user)->fd, header2, sizeof(header2));
    write((*user)->fd, info, l+1);
    write((*user)->fd, fab_info, sizeof(fab_info));
    write((*user)->fd, "\n", 1);
    free(info);
    free(str_time);
}

void show_player_info(struct pl *user, struct pl *p)
{
    int l, i = 1;
    char *info = malloc(buf_size);
    char header[] = "Player   Money   Raw   Production";
    char header2[] = "   Factories   Factories being built\n";
    char arrow[] = " <-\n";
    char enter[] = "\n";
    write(user->fd, header, sizeof(header));
    write(user->fd, header2, sizeof(header2));
    while(p){
        if (p->disconnected){
            p = p->next;
            continue;
        }
        l=sprintf(info,"%% %d%11d%5d%8d",p->name,p->money,p->raw,p->prod);
        write(user->fd, info, l+1);
        l=sprintf(info,"%14d%16d", p->fab, p->fab_buf);
        write(user->fd, info, l+1);
        if (user->fd == p->fd)
            write(user->fd, arrow, sizeof(arrow));
        else
            write(user->fd, enter, sizeof(enter));
        p = p->next;
        i++;
    }
    write(user->fd, enter, sizeof(enter));
    free(info);
}

void player_done(struct pl **user)
{
    const char msg[]  = {
        "The auction will start shortly.\n"
        "Wait until other players are ready.\n"
    };
    (*user)->ready = 1;
    write((*user)->fd, msg, sizeof(msg));
}

void print_dollar_all(struct pl *user)
{
    while(user){
        write(user->fd, "$:", 2);
        user = user->next;
    }
}

char **get_arguments(char *str)
{
    char **args = malloc(cmd_cnt*sizeof(char *));
    char *arg = malloc(dflt_str);
    int i=0, j=0, k=0, cmd_counter = 0, word_flag=0, connect_flag=0;
    for(i = 0; i<cmd_cnt; i++)
        args[i] = NULL;
    i=0;
    while(str[i] != 0){
        if (str[i] == ' '||str[i] == 0||word_flag=='\n')
            word_flag = 0;
        else
            word_flag = 1;
        if (word_flag){
            if (((k+1)>=dflt_str)&&((k+1)%dflt_str==0)){
                char *tmp = malloc(sizeof(arg)+dflt_str);
                do{
                    tmp[j] = arg[j];
                    j++;
                }while(arg[j]!=0);
                j = 0;
                free(arg);
                arg = tmp;
            }
            arg[k] = str[i];
            arg[k+1] = 0;
            if(str[i+1]==0||str[i+1]=='\n'||str[i+1]==' ')
                connect_flag = 1;
            k++;
        }
        if (connect_flag){
            args[cmd_counter] = arg;
            arg = malloc(dflt_str);
            k = 0;
            cmd_counter++;
            connect_flag = 0;
        }
        i++;
    }
    free(arg);
    return args;
}

void free_args(char **args)
{
    int i = 0;
    if (args){
        for (i =0; i<cmd_cnt;i++){
            if (args[i])
                free(args[i]);
        }
        free(args);
    }
}

void produce(struct pl **user, char **args)
{
    const char err0[] = "Error: wrong command arguments\n";
    const char err1[] = "Error: you don't have enough factories\n";
    const char err2[] = "Error: you don't have enough raw materials\n";
    int amount;
    amount = str_to_int(args[1]);
    if (((*user)->raw + (*user)->raw_buf) < amount)
        write((*user)->fd, err2, sizeof(err2));
    else if (amount+(*user)->prod_buf > (*user)->fab)
        write((*user)->fd, err1, sizeof(err1));
    else if (!args[1])
        write((*user)->fd, err0, sizeof(err0));
    else{
        (*user)->raw_buf-=makep_raw*amount;
        (*user)->money_buf-=makep_money*amount;
        (*user)->prod_buf+=amount;
    }
}

int put_bet_prod(int fd, int name, int amount, int price, struct list **l)
{
    struct list *elem;
    struct list *tmp = *l;
    elem = malloc(sizeof(struct list));
    elem->price = price;
    elem->amount = amount;
    elem->name = name;
    elem->fd = fd;
    elem->next = NULL;
    if (!tmp){
        *l = elem;
        return 0;
    }
    if(tmp->price >= elem->price){
        elem->next = *l;
        *l = elem;
        return 0;
    }
    while(tmp->next){
        if (tmp->next->price >= elem->price){
            elem->next = tmp->next;
            tmp->next = elem;
            return 0;
        }
        tmp = tmp->next;
    }
    tmp->next = elem;
    return 0;
}

int put_bet_raw(int fd, int name, int amount, int price, struct list **l)
{
    struct list *elem;
    struct list *tmp = *l;
    elem = malloc(sizeof(struct list));
    elem->price = price;
    elem->amount = amount;
    elem->name = name;
    elem->fd = fd;
    elem->next = NULL;
    if (!tmp){
        *l = elem;
        return 0;
    }
    if(tmp->price <= elem->price){
        elem->next = *l;
        *l = elem;
        return 0;
    }
    while(tmp->next){
        if (tmp->next->price <= elem->price){
            elem->next = tmp->next;
            tmp->next = elem;
            return 0;
        }
        tmp = tmp->next;
    }
    tmp->next = elem;
    return 0;
}

int apply_for_auction(int type, struct pl **user, struct mkt *bank, char **c)
{
    const char err0[] = "Error: wrong arguments\n";
    const char err1[] = "Error: your price is too small\n";
    const char err2[] = "Error: not enough raw materials on the mkt\n";
    const char err3[] = "Error: you already made a bet\n";
    const char err4[] = "Error: your price is too high\n";
    const char err5[] = "Error: the bank cannot sell so much production\n";
    const char err7[] = "Error: you don't have enough production\n";
    if (((*user)->bought && type) || ((*user)->sold && !type)){
        write((*user)->fd, err3, sizeof(err3));
        return 1;
    }
    int amount = str_to_int(c[1]);
    int price = str_to_int(c[2]);
    if (price<=0)
        write((*user)->fd, err0, sizeof(err0));
    if (type){
        if (price<buy_p[bank->level]){
            write((*user)->fd, err1, sizeof(err1));
            return 1;
        }
        if (amount>(((int) buy_i[bank->level])*bank->u_count)){
            write((*user)->fd, err2, sizeof(err2));
            return 1;
        }
        put_bet_raw((*user)->fd, (*user)->name, amount, price, &bank->r_list);
        (*user)->bought = 1;
    }
    else{
        if (((*user)->prod+(*user)->prod_buf) < amount){
            write((*user)->fd, err7, sizeof(err7));
            return 1;
        }
        if (amount>(((int) sell_i[bank->level])*bank->u_count)){
            write((*user)->fd, err5, sizeof(err5));
            return 1;
        }
        if (price>sell_p[bank->level]){
            write((*user)->fd, err4, sizeof(err4));
            return 1;
        }
        put_bet_prod((*user)->fd, (*user)->name, amount, price, &bank->p_list);
        (*user)->sold = 1;
    }
    return 0;
}

void build_factory(struct pl **user,  char **cmd)
{
    struct fablist *tmp;
    int i, count = str_to_int(cmd[1]);
    for (i = 0; i<count; i++){
        tmp = malloc(sizeof(struct fablist));
        tmp->delay = 5;
        tmp->next = (*user)->fabild;
        (*user)->fabild = tmp;
        (*user)->fab_buf += 1;
        (*user)->money_buf -= build_fab;
    }
}

int active_player_cmds(struct pl **u,struct mkt *b,char **c)
{
    enum auct{buy=1, sell=0};
    if (cmp_cmd(c[0], "buy")&&c[1]&&c[2]){
        apply_for_auction(buy, u, b, c);
        return 1;
    }
    else if (cmp_cmd(c[0], "sell")&&c[1]&&c[2]){
        apply_for_auction(sell, u, b, c);
        return 1;
    }
    else if (cmp_cmd(c[0], "build")&&c[1]){
        build_factory(u, c);
        return 1;
    }
    else if (cmp_cmd(c[0], "prod")&&c[1]){
        produce(u, c);
        return 1;
    }
    return 0;
}

void execute_usr_cmd(struct pl **user,struct pl **p,struct mkt *bank)
{
    const char err[] = {
        "Error: wrong command\n"
        "For list of all game commands enter 'help'\n"
        };
    const char dollar[] = "$:";
    int f = !(*user)->lost&&!(*user)->ready;
    int succ = 0;
    char **cmd = get_arguments((*user)->buf);
    if (!cmd[0]){
        write((*user)->fd, dollar, sizeof(dollar));
        return;
    }
    if (f)
        succ = active_player_cmds(user, bank, cmd);
    if (cmp_cmd(cmd[0],  "market"))
        show_mkt(user, bank);
    else if (cmp_cmd(cmd[0], "help"))
        print_help((*user)->fd);
    else if (cmp_cmd(cmd[0], "player"))
        show_player_info(*user, *p);
    else if (cmp_cmd(cmd[0], "turn"))
        player_done(user);
    else if (cmp_cmd(cmd[0], ""))
        return;
    else if (!succ)
        write((*user)->fd, err, sizeof(err));
    write((*user)->fd, dollar, sizeof(dollar));
    free_args(cmd);
}

void free_winner_list(struct winner_list *w, int count)
{
    int i;
    for(i = 0; i<count; i++){
        w[i].buy_price = 0;
        w[i].buy_amount = 0;
        w[i].buy_win_stat = 0;
        w[i].sell_price = 0;
        w[i].sell_amount = 0;
        w[i].sell_win_stat = 0;
    }
}

void bank_default(struct mkt *bank)
{
    bank->level = 0;
    bank->month = 1;
    bank->u_count = 0;
    bank->r_list = NULL;
    bank->p_list = NULL;
    bank->game_on = 0;
    bank->w = malloc(bank->u_max*sizeof(struct winner_list));
    free_list(&bank->r_list);
    free_list(&bank->p_list);
    free_winner_list(bank->w, bank->u_max);
}

void shutdown_user(struct pl **user)
{
    (*user)->disconnected = 1;
    shutdown((*user)->fd, 2);
    close((*user)->fd);
}

int check_for_winner(struct pl **user, struct mkt *bank)
{
    struct pl *tmp = *user;
    int l, count  = 0, name;
    while (tmp){
        if (!tmp->lost){
            count++;
            name = tmp->name;
        }
        tmp = tmp->next;
    }
    if (count <= 1){
        char *msg = malloc(buf_size);
        if (count == 1)
            l = sprintf(msg, "%%PLAYER %d HAS WON THE GAME!\n\n", name);
        else
            l = sprintf(msg, "%%YOU ARE ALL GODDAMN LOSERS!\n\n");
        tmp = *user;
        while(tmp){
            write(tmp->fd, msg, l+1);
            show_player_info(tmp, *user);
            tmp = tmp->next;
        }
        tmp = *user;
        while(tmp){
            shutdown_user(&tmp);
            tmp = tmp->next;
        }
        bank_default(bank);
        free(msg);
        return 1;
    }
    return 0;
}

int read_cmd(struct pl **user, struct pl **p, struct mkt *bank)
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
        if (bank->game_on){
            int i = 0;
            tmp_buf[res] = 0;
            while(i< res){
                while(tmp_buf[i]!='\r' && i<res){
                    if (tmp_buf[i]!='\n' && tmp_buf[i]!='\000'){
                        (*user)->buf[(*user)->last_index] = tmp_buf[i];
                        (*user)->buf[(*user)->last_index+1] = 0;
                        (*user)->last_index++;
                    }
                    i++;
                }
                if (tmp_buf[i]=='\r' || tmp_buf[i]=='\n'){
                    (*user)->last_index = 0;
                    execute_usr_cmd(user, p, bank);
                }
                i++;
            }
            free(tmp_buf);
        }
        else
            loading_screen(*user, bank->u_count, bank->u_max);
    }
    return 0;
}

int  handle_argv(int argc, char **argv)
{
    if (argc < 2){
        printf("You need to specify your port\n");
        exit(1);
    }
    if (argc > 2)
        return  str_to_int(argv[2]); 
    return 2;
}

int check_turn(struct pl *user)
{
    while(user){
        if (!user->lost && !user->ready)
            return 0;
        user = user->next;
    }
    return 1;
}

int empty_turn(struct pl *user)
{
    while(user){
        user->ready = 0;
        user->money_buf = 0;
        user->raw_buf = 0;
        user->prod_buf = 0;
        user = user->next;
    }
    return 1;
}

void handle_new_user(struct pl **user, struct mkt *bank, int ls)
{
    struct pl *tmp = *user;
    if (bank->u_count == 0){
        bank_default(bank);
    }
    bank->u_count++;
    if ((bank->u_count <= bank->u_max)&&!bank->game_on){
        accept_user(user, ls, bank->u_count);
        if (bank->u_count == bank->u_max){
            bank->game_on = 1;
            while(tmp){
                write(tmp->fd, "\n\n$:", 4);
                tmp = tmp->next;
            }
        }
    }
    else{
        too_many_users(ls);
        bank->u_count--;
    }
}

int give_usr_raw(struct pl **user, int fd, int price, int amount, int change)
{
    const char msg0[] = "Your raw materials bet has been partly accepted\n";
    const char msg1[] = "Your raw materials bet has been fully accepted\n";
    struct pl *tmp = *user;
    while(tmp){
        if (tmp->fd == fd){
            tmp->money_buf -= amount*price;
            tmp->raw_buf += amount;
            if (change)
                write(tmp->fd, msg0, sizeof(msg0));
            else
                write(tmp->fd, msg1, sizeof(msg1));
            return tmp->name;
        }
        tmp = tmp->next;
    }
    return -1;
}

void give_usr_prod(struct pl **user, int fd, int price, int amount, int change)
{
    const char msg0[] = "Your production bet has been partly accepted\n";
    const char msg1[] = "Your production bet has been fully accepted\n";
    struct pl *tmp = *user;
    while(tmp){
        if (tmp->fd == fd){
            tmp->money_buf += price*amount;
            tmp->prod_buf -= amount;
            if (change)
                write(tmp->fd, msg0, sizeof(msg0));
            else
                write(tmp->fd, msg1, sizeof(msg1));

            return;
        }
        tmp = tmp->next;
    }
}

void give_goods(struct list *l,struct pl **usr,struct mkt *bank,int p, int t)
{
    while(l){
        if (l->name== p){
            if (t){
                 give_usr_raw(usr, l->fd, l->price, l->amount, 0);
                 bank->w[p-1].buy_win_stat = 1;
            }
            else{
                 give_usr_prod(usr, l->fd, l->price, l->amount, 0);
                 bank->w[l->name-1].sell_win_stat = 1;
            }
        }
        l = l->next;
    }
}

int rand_goods(struct list *l,struct pl **u,struct mkt *bank, int *arr)
{
    enum param{price = 0, t = 1, max = 2, count};
    int lucky, change = 0;
    srand(time(NULL));
    lucky = rand()%arr[count];
    while(l){
        if ((0 == lucky)&&(l->price == arr[price]))
            break;
        if (l->price == arr[price])
            lucky-=1;
        l = l->next;
    }
    if (l->amount > arr[max]){
        l->amount = arr[max];
        change = 1;
    }
    if (arr[t]){
        give_usr_raw(u, l->fd, arr[price], l->amount, change);
        bank->w[l->name-1].buy_win_stat = 1;
    }
    else{
        give_usr_prod(u, l->fd, arr[price], l->amount, change);
        bank->w[l->name-1].sell_win_stat = 1;
    }
    return 1;
}

void print_auction_table(struct pl *u, struct winner_list *w)
{
    const char header0[] = "             AUCTION RESULTS:\n";
    const char header1[] = "     __________________________________\n";
    const char header2[] = "       raw auction    production auction\n";
    const char header3[] = "     amount price won  amount price won\n";
    const char arrow[] = "  <-\n";
    const char enter[] = "\n";
    char *msg = malloc(buf_size);
    struct pl *bgn = u, *tmp = u;
    int n, l = 0;
    while(u){
        write(u->fd, header0, sizeof(header0));
        write(u->fd, header1, sizeof(header1));
        write(u->fd, header2, sizeof(header2));
        write(u->fd, header3, sizeof(header3));
        while(tmp){
            if (!tmp->lost){
                n = tmp->name;
                l = sprintf(msg, "%% %d%6d", n, w[n-1].buy_amount);
                write(u->fd, msg, l+1);
                l = sprintf(msg,"%7d%5d",w[n-1].buy_price,w[n-1].buy_win_stat);
                write(u->fd, msg, l+1);
                l = sprintf(msg,"%9d%6d",w[n-1].sell_amount,w[n-1].sell_price);
                write(u->fd, msg, l+1);
                l = sprintf(msg,"%4d",w[n-1].sell_win_stat);
                write(u->fd, msg, l+1);
                if (tmp->fd == u->fd)
                    write(u->fd, arrow, sizeof(arrow));
                else
                    write(u->fd, enter, sizeof(enter));
            }
            tmp = tmp->next;
        }
        write(u->fd, enter, sizeof(enter));
        write(u->fd, enter, sizeof(enter));
        tmp = bgn;
        u=u->next;
    }
    free(msg);
}

void fill_winner_line(int t, struct winner_list *w, int n, int p, int a)
{
    if (t){
        w[n-1].buy_price = p;
        w[n-1].buy_amount = a;
    }
    else{
        w[n-1].sell_price = p;
        w[n-1].sell_amount = a;
    }
}

void create_winner_list(int t, struct winner_list *w, struct list *l)
{
    while(l){
        fill_winner_line(t, w, l->name, l->price, l->amount);
        l = l->next;
    }
}

void select_winner(int t, struct pl **u, struct mkt *bank, struct list *l)
{
    int fl, curr_price = 0, amnt = 0, count = 0, m_amnt;
    struct list *bgn = l, *tmp = l;
    int *arr = malloc(3*sizeof(int));
    enum squ{price = 0, type = 1, max = 2, size = 3};
    if (t)
        m_amnt = buy_i[bank->level]*bank->u_count;
    else
        m_amnt = sell_i[bank->level]*bank->u_count;
    curr_price = l->price;
    create_winner_list(t, bank->w, l);
    while(l){
        amnt += l->amount;
        count++;
        if (l->next==NULL || l->price!=l->next->price){
            if (amnt<=m_amnt){
                while(tmp){
                    if (tmp->price == curr_price)
                        give_goods(bgn, u, bank, tmp->name, t);
                    tmp = tmp->next;
                }
                m_amnt -= amnt;
                tmp = bgn;
            }
            else{
                arr[price] = l->price;
                arr[type] = t;
                arr[max] = m_amnt;
                arr[size] = count;
                fl = rand_goods(bgn, u, bank, arr);
                if (fl)
                    m_amnt -= l->amount;
            }
            if (l->next)
                curr_price = l->next->price;
            count = 0;
            amnt = 0;
            if (0 == m_amnt){
                free(arr);
                return;
            }
        }
        l = l->next; 
    }
    free(arr);
}

void check_factories(struct pl **u)
{
    struct pl *user = *u;
    char msg[] = "A new factory has started working!\n";
    struct fablist *f = user->fabild;
    while(user){
        if (f){
            f->delay-= 1;
            if (f->delay == 0){
                user->fab += 1;
                user->fab_buf -=1;
                write(user->fd, msg, sizeof(msg));
                user->fabild = user->fabild->next;
            }
            while(f->next){
                f->next->delay -= 1;
                if (f->next->delay == 0){
                    write(user->fd, msg, sizeof(msg));
                    free(f->next);
                    f->next = f->next->next;
                }
                f = f->next;
            }
        }
        user = user->next;
        if (user)
            f = user->fabild;
    }
}

void take_money(struct pl **user)
{
    struct pl *tmp = *user;
    while(tmp){
        if (tmp->lost == 1){
            tmp = tmp->next;
            continue;
        }
        tmp->money += tmp->money_buf;
        tmp->money -= tmp->fab*fab_tax + tmp->raw*raw_tax;
        tmp->money -= tmp->prod*prod_tax;
        tmp->raw += tmp->raw_buf;
        tmp->prod += tmp->prod_buf;
        tmp->bought = 0;
        tmp->sold = 0;
        tmp = tmp->next;
    }
}

int check_for_losers(struct pl **user)
{
    const char msg[] = "YOU LOST!\n";
    struct pl *tmp = *user;
    int count = 0;
    while (tmp){
        if (tmp->money < 0){
            write(tmp->fd, msg, sizeof(msg));
            tmp->lost = 1;
        }
        if (tmp->lost == 1)
            count++;
        tmp = tmp->next;
    }
    return count;
}

void end_the_month(struct pl **user, struct mkt *bank)
{
    int disc, one_won = 0, no_players = 0;
    if (bank->r_list)
        select_winner(1, user, bank, bank->r_list);
    if (bank->p_list)
        select_winner(0, user, bank, bank->p_list);
    free_list(&bank->r_list);
    free_list(&bank->p_list);
    check_factories(user);
    take_money(user);
    print_auction_table(*user, bank->w);
    print_dollar_all(*user);
    change_level(bank);
    disc = check_for_losers(user);
    one_won = check_for_winner(user, bank);
    bank->month++;
    if (disc == bank->u_max)
        no_players = 1;
    if (one_won || no_players)
        exit(0);
}

int main(int argc, char **argv)
{   
    int res, ls, max_fd;
    struct pl *user = NULL, *tmp = NULL;
    struct mkt bank;
    bank.u_max = handle_argv(argc, argv);
    bank_default(&bank);
    ls = socket(AF_INET, SOCK_STREAM, 0);
    if (ls==-1)
        printf("Error: socket()");
    bindls(ls, argv[1]);
    for (;;){
        fd_set readfds;
        max_fd = buildfds(&user, &readfds, ls);
        free_winner_list(bank.w, bank.u_max);
        res = select(max_fd+1, &readfds, NULL, NULL, NULL);
        if (res == -1)
            perror("Select");
        if (FD_ISSET(ls, &readfds))
            handle_new_user(&user, &bank, ls);
        tmp = user;
        while(tmp!=NULL){
            if (FD_ISSET(tmp->fd, &readfds)){
                bank.u_count -= read_cmd(&tmp, &user, &bank);    
            }
            tmp = tmp->next;
        }
        if (check_turn(user)){
            end_the_month(&user, &bank);
            empty_turn(user);
        }
    }
    return 0;
}
