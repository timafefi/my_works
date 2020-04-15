#include<stdlib.h>
#include<stdio.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>

enum sys_info{buf_size = 1024, word_size = 4, mkt_size = 4, player_size = 8};
enum mkt_items{players = 0, sell_i = 1, sell_p = 2, buy_i = 3, buy_p = 4};
enum player_items{num = 0, money = 1, raw = 2, prod  = 3, fab = 4, fabb = 5};

struct mp_info{
    int **plinfo;
    int *mkt;
    char *cmd;
    int self_indx;
};

void handle_argv(int argc)
{
    if (argc<2){
        fprintf(stderr, "Error: wrong input arguments\n");
        exit(1);
    }
}
void empty_array(char *arr, int size)
{
    for(int i = 0; i<size; i++)
        arr[i] = 0;
}

int cmp_str(const char *s1, const char *s2)
{
    int i = 0;
    while(s1[i]==s2[i]){
        if (s1[i] == 0 && s2[i] == 0)
            return 1;
        i++;
    }
    return 0;
}

int str_length(const char *arr)
{
    int i = 0;
    while(arr[i++]!= '\0');
    return i;
}

int str_to_int(char *str)
{
    int i = 0, res = 0;
    while(str[i]!=0){
        res = 10*res + str[i] - '0';
        i++;
    }
    return res;
}

void bind_game(int sockfd, char *ip_addr, char *port)
{
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(str_to_int(port));
    if (!inet_aton(ip_addr, &(addr.sin_addr))){
        fprintf(stderr, "Error: wrong ip address\n");
        exit(1);
    }
    if (0 != (connect(sockfd, (struct sockaddr *) &addr, sizeof(addr)))){
        perror("connect():");
        exit(1);
    }
}

 void read_answer(int sockfd, struct mp_info *bot)
{
    char ch;
    int fbgn = 0, fend = 0, i = 0;
    while(0 != read(sockfd, &ch, sizeof(char))){
        putchar(ch);
        if (fbgn){
            bot->cmd[i] = ch;
            bot->cmd[i+1] = '\0';
            if (ch!=0)
                i++;
        }
        if (ch == '\n'){
            fbgn = 0;
            fend++;
        }
        else
            if (ch != '\0')
                fend = 0;
        if (fend == 2)
            return;
        if (ch == '%')
            fbgn = 1;

    }
}

void separate_market(struct mp_info *bot)
{
    int i = 0, fword = 0, fconnect = 0, wd = 0, indx = 0;
    char *str = new char[word_size];
    char *cmd = bot->cmd;
    int *ans = bot->mkt;
    empty_array(str, word_size);
    while(bot->cmd[i+1]!=0){
        if ((cmd[i]== ' '||cmd[i] == '\n')&&(cmd[i+1]!= ' '&&cmd[i+1]!='\n')){
            fword = 1;
        }
        if((cmd[i]!=' '&&cmd[i]!='\n')&&(cmd[i+1]==' '||cmd[i+1]=='\n')){
            fword = 0;
            fconnect = 1;
        }
        if(fword){
            str[indx] = cmd[i+1];
            str[indx+1] = 0;
            indx++;
        }
        if (fconnect){
            ans[wd] = str_to_int(str);
            wd++;
            fconnect = 0;
            indx = 0;
            empty_array(str, word_size);
        }
        i++;
    }
    delete str;
    empty_array(bot->cmd, buf_size);
}

void separate_player(struct mp_info *bot)
{
    int i = 0, fword = 0, fconnect = 0, snt = 0, wd = 0, indx = 0;
    char *str = new char[word_size];
    char *cmd = bot->cmd;
    int **ans = bot->plinfo;
    empty_array(str, word_size);
    while(bot->cmd[i+1]!=0){
        if ((cmd[i]== ' '||cmd[i] == '\n')&&(cmd[i+1]!= ' '&&cmd[i+1]!='\n')){
            fword = 1;
        }
        if((cmd[i]!=' '&&cmd[i]!='\n')&&(cmd[i+1]==' '||cmd[i+1]=='\n')){
            fword = 0;
            fconnect = 1;
        }
        if(fword){
            str[indx] = cmd[i+1];
            str[indx+1] = 0;
            indx++;
        }
        if (cmd[i] == '<'&& cmd[i+1] == '-')
            bot->self_indx = snt;
        if (fconnect){
            ans[snt][wd] = str_to_int(str);
            wd++;
            fconnect = 0;
            indx = 0;
            empty_array(str, word_size);
        }
        if (cmd[i+1] == '\n'){
            wd = 0;
            snt++;
        }
        i++;
    }
    delete str;
    empty_array(bot->cmd, buf_size);
}
void send_serv(int sockfd, const char *str)
{
    int len = str_length(str);
    char *msg = new char[len];
    for(int i = 0; i<len; i++)
        msg[i] = str[i];
    write(sockfd, msg, sizeof(msg));
    delete msg;
}

void get_info(int sockfd, struct mp_info *bot)
{
    send_serv(sockfd, "market\r\n");
    read_answer(sockfd, bot);
    separate_market(bot);
    bot->plinfo = new int*[bot->mkt[players]];
    for(int i = 0; i<bot->mkt[players]; i++)
        bot->plinfo[i] = new int[player_size];
    send_serv(sockfd, "player\r\n");
    read_answer(sockfd, bot);
    separate_player(bot);
}

int get_prod_amount(struct mp_info *bot)
{
    enum well{cost = 2000};
    int p, m, f;
    m = bot->plinfo[bot->self_indx][money];
    p = bot->plinfo[bot->self_indx][raw];
    f = bot->plinfo[bot->self_indx][fab];
    while((p*cost>m)||(p>f))
        p--;
    return p;
}

void easiest_strategy(int sockfd, struct mp_info *bot)
{
    char *str = new char[buf_size];
    int c, c1, c2, d, l;
    c1 = bot->mkt[buy_i];
    c2 = bot->plinfo[bot->self_indx][prod];
    c1 > c2? c = c2 : c = c1;
    c1 = bot->mkt[sell_i];
    c1 > 2? d = 2:d = c1;
    if (c>0){
        l=sprintf(str,"sell %d %d\r\n",c ,bot->mkt[buy_p]);
        write(sockfd, str, l+1);
        write(1, str, l+1);
    }
    if (d>0){
        l = sprintf(str, "buy %d %d\r\n", d, bot->mkt[sell_p]);
        write(sockfd, str, l+1);
        write(1, str, l+1);
    }
    c = get_prod_amount(bot);
    l = sprintf(str, "prod %d\r\n", c);
    write(sockfd, str, l+1);
    write(1, str, l+1);
    l = sprintf(str, "turn\r\n");
    write(sockfd, str, l+1);
    write(1, str, l+1);


}

void wait_for_start(int sockfd)
{
    char n_ch, p_ch = 0;
    while(1){
        read(sockfd, &n_ch, 1);
        putchar(n_ch);
        if (n_ch=='\n' && p_ch == '\n')
            return;
        if (n_ch != '\0')
            p_ch = n_ch;
    }
}

int main(int argc, char **argv)
{
    int sockfd;
    struct mp_info bot;
    bot.cmd = new char[buf_size];
    bot.mkt = new int[mkt_size];
    handle_argv(argc);
    if (-1 == (sockfd = socket(AF_INET, SOCK_STREAM, 0))){
        perror("socket():");
        exit(1);
    }
    bind_game(sockfd, argv[1], argv[2]);
    while(1){
        wait_for_start(sockfd);
        get_info(sockfd, &bot);
        easiest_strategy(sockfd, &bot);
    }
    return 0;
}
