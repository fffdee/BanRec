#include "input_handle.h"
#include <termios.h>
#include <unistd.h>  
#include <stdio.h>
#include <sys/select.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>


static struct termios orig_termios;

static pthread_t monitor_thread;
static volatile int monitor_running = 0;
/* 监控线程:检测按键超时并发送 Note Off */
static void* key_monitor_thread(void* arg)
{
    while (monitor_running) {

        usleep(10000); // 10ms 检查一次
    }
    return NULL;
}
/* 检查是否有键盘输入 */
static int kbhit(void)
{
    struct timeval tv = { 0L, 0L };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

/* 非阻塞读取一个字符 */
static int getch(void)
{
    int r;
    unsigned char c;
    if ((r = read(STDIN_FILENO, &c, sizeof(c))) < 0) {
        return r;
    } else {
        return c;
    }
}

char keyboaed_Loop();
void keyboaed_Init();
void keyboaed_DeInit();


BG_Input_Handle BG_input_handle = {
    
    .KeyBoardInit = keyboaed_Init,
    .KeyBoardDeInit = keyboaed_DeInit,
    .KeyBoardLoop = keyboaed_Loop,
    

};

char keyboaed_Loop()
{
    if (!kbhit()) {
        return 0;  // 没有输入,立即返回
    }
    
    char ch = getch();



    
    return ch;
}

void keyboaed_Init()
{
    struct termios term;
    
    /* 保存原始终端设置 */
    tcgetattr(STDIN_FILENO, &orig_termios);
    
    /* 设置为非规范模式,非阻塞 */
    term = orig_termios;
    term.c_lflag &= ~(ICANON | ECHO);
    term.c_cc[VMIN] = 0;
    term.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &term);

    /* 启动监控线程 */
    monitor_running = 1;
    pthread_create(&monitor_thread, NULL, key_monitor_thread, NULL);
}

void keyboaed_DeInit()
{
    /* 停止监控线程 */
    monitor_running = 0;
    pthread_join(monitor_thread, NULL);

    /* 恢复原始终端设置 */
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
}

