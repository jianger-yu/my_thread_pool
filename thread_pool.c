#include <pthread.h>
#include <stdlib.h>
#include <fcntl.h>
#include <error.h>
#include <stdbool.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>


//任务队列（链表）结构体
typedef struct task{
    void *(*function)(void *); //任务处理函数
    void *arg;                //任务参数
    struct task * next;       //链表实现
} task;

//线程池结构体
typedef struct pthread_pool{
    pthread_mutex_t lock;          //互斥锁
    pthread_cond_t cond;           //条件变量
    struct task * task_queue;      //任务队列
    pthread_t * pthreads;          //线程数组
    int pth_cnt;                   //线程数量
    bool stop;                     //是否运行
}pthread_pool;

pthread_pool pool;
int ret = 0;

void sys_err(int ret,char * str){
    if(ret != 0){
        sprintf(stderr,"%s error：%s\n",str,strerror(ret));
        exit(1);
    }
}

void *calcu(void* dig){
    int ans = 1,tmp = (int) dig;
    while(tmp)  ans*=tmp--;
    printf("%d 的阶乘为：%d\n",(int)dig,ans);
}

void *consumer(void *){
    while(1){
        pthread_mutex_lock(&pool.lock); //上锁

        while(pool.task_queue == NULL && pool.stop == false) {
            pthread_cond_wait(&pool.cond,&pool.lock);
        }
        //出循环，表示已经拿到了锁且有任务
        //检查是否需要终止线程
        if (pool.stop) {
            pthread_mutex_unlock(&pool.lock);
            pthread_exit(NULL);
        }
        
        //拿取任务
        task *mytask;
        mytask = pool.task_queue;
        pool.task_queue = pool.task_queue -> next;
        //拿完任务直接解锁
        pthread_mutex_unlock(&pool.lock);

        //执行任务
        mytask->function(mytask->arg);
        free(mytask);
    }
}

//初始化线程池
void ThreadPoolInit(int ThreadCount){
    ret = pthread_mutex_init(&pool.lock,NULL);    //互斥锁
    sys_err(ret,"mutex_init");
    
    ret = pthread_cond_init(&pool.cond,NULL);     //条件变量
    sys_err(ret,"cond_init");

    pool.task_queue = NULL;                 //任务队列
    pool.pthreads = (pthread_t *)malloc(ThreadCount*sizeof(pthread_t));     //线程数组
    pool.pth_cnt = ThreadCount;             //线程数量
    //生成线程
    for(int i = 0; i < ThreadCount; i++){
        ret = pthread_create(&pool.pthreads[i],NULL,consumer,NULL);
        sys_err(ret,"pthread_create");
    }
    pool.stop = false;
}

//添加任务到线程池
void PushTask(void* (*function)(void *), void *arg){
    task * NewTesk = (task*)malloc(sizeof(task));
    NewTesk -> function = function;
    NewTesk -> arg = arg;
    pthread_mutex_lock(&pool.lock); //上锁，写入公共空间

    if(pool.task_queue == NULL){
        pool.task_queue = NewTesk;
    }
    else{
        task*p = pool.task_queue;
        while(p -> next != NULL) p = p->next;
        p->next = NewTesk;
    }

    pthread_cond_signal(&pool.cond);    //唤醒工作线程
    pthread_mutex_unlock(&pool.lock);   //解锁
}

void * producter(int n){
    while(n--){
        int arg = rand() % 12 + 1;
        PushTask(calcu,(void *)arg);
        usleep(100000);
    }
}

//释放线程池
void ThreadPoolDestroy(){
    pthread_mutex_lock(&pool.lock);     //上锁
    pool.stop = true;                   //准备令全部线程开始停止
    pthread_cond_broadcast(&pool.cond); //叫醒全部线程
    pthread_mutex_unlock(&pool.lock);   //解锁

    //回收所有线程
    for(int i = 0; i < pool.pth_cnt; i++){
        pthread_join(pool.pthreads[i],NULL);
    }

    free(pool.pthreads);    //释放线程数组

    //释放任务队列
    task *pt;
    while (pool.task_queue) {
        pt = pool.task_queue;
        pool.task_queue = pt->next;
        free(pt);
    }

    //销毁锁和条件变量
    pthread_mutex_destroy(&pool.lock);
    pthread_cond_destroy(&pool.cond);
}

int main(){
    srand(time(NULL));

    ThreadPoolInit(10);     //初始化10个线程

    producter(30);            //生产任务
    //PushTask(calcu,5);

    sleep(5);  // 等待任务执行
    ThreadPoolDestroy();

    return 0;
}
