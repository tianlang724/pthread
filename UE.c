#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#include <errno.h>
#define NUM_ENB_THREADS 1
#define MAX_BUFFER 100
typedef struct{
	int subframe;
	int instance_cnt_tx;
	int instance_cnt_rx;
	pthread_t pthread_tx;
	pthread_t pthread_rx;
	pthread_cond_t cond_tx;
  /// condition variable for rx processing thread
  	pthread_cond_t cond_rx;
  /// mutex for tx processing thread
  	pthread_mutex_t mutex_tx;
  /// mutex for tx processing thread
  	pthread_mutex_t mutex_rx;
}proc_t;

int sock=-1;
char rxbuf[MAX_BUFFER];
char txbuf[MAX_BUFFER];
struct sockaddr_in my_addr;
struct sockaddr_in remote_addr;


volatile int exit_f=0;
pthread_cond_t sync_cond;
pthread_mutex_t sync_mutex;
pthread_mutex_t exit_mutex;
int sync_var=-1;
proc_t proctx;
proc_t procrx;
static void* main_thread(void* arg);
static void* thread_tx(void* params);
static void* thread_rx(void* params);
void init_proc();
void kill_proc();
void create_socket();

int main(){
	pthread_t main_thread_t;
	pthread_cond_init(&sync_cond,NULL);
  	int ret=pthread_mutex_init(&sync_mutex, NULL);
  	if(ret!=0){
  		printf("sync init error\n");
  	}
  	ret=pthread_mutex_init(&exit_mutex, NULL);
  	if(ret!=0){
  		printf("exit_mutex init error\n");
  	}
	init_proc();
	int error_code = pthread_create(&main_thread_t,NULL,main_thread,NULL);
    if (error_code!= 0) {
      printf(" error %d\n",error_code);
      return(error_code);
    } else {
      printf( "main_thread successful\n" );
    }
    create_socket();
	pthread_mutex_lock(&sync_mutex);
	sync_var=0;
	pthread_cond_broadcast(&sync_cond);
  	pthread_mutex_unlock(&sync_mutex);
  	while(!exit_f){

  	}
  	pthread_mutex_destroy(&sync_mutex);
    pthread_cond_destroy(&sync_cond);
    kill_proc();
    printf("exiting...\n");
}

static void* thread_tx(void* params){
	static int return_tx;
	struct sched_param sparam;
	int policy,s;
	memset(&sparam, 0 , sizeof (sparam));
  	sparam.sched_priority = sched_get_priority_max(SCHED_FIFO)-1;
  	policy = SCHED_FIFO;
    proc_t *proc=(proc_t*)params;
  	s = pthread_setschedparam(pthread_self(), policy, &sparam);
  	if (s != 0)
    {
    	 printf("Error setting TX thread priority\n");
    }
  	s = pthread_getschedparam(pthread_self(), &policy, &sparam);
 	if (s != 0)
    {
    	printf("Error getting TX thread priority\n");
    }
    while(!exit_f){
    	if (pthread_mutex_lock(&proc->mutex_tx) != 0) {
       		printf("[thread_tx] error locking mutex for eNB TX proc %d\n", proc->subframe );
       		exit_f=1;
       		break;
    	}
    	printf("[thread_tx] proc=%d,waiting signal\n",proc->subframe);
    	while (proc->instance_cnt_tx < 0) {
      		pthread_cond_wait( &proc->cond_tx, &proc->mutex_tx ); // this unlocks mutex_tx while waiting and then locks it again
    	}
    	if (pthread_mutex_unlock(&proc->mutex_tx) != 0) {
      		printf("[thread_tx] error unlocking mutex for eNB TX proc %d\n",proc->subframe);
       		exit_f=1;
       		break;
    	}
    	printf("[thread_tx] proc=%d,got signal\n",proc->subframe);
    	time_t t = time(0);
    	strftime(txbuf, sizeof(txbuf), "UE %Y/%m/%d %X %A ",localtime(&t));
    	printf("[thread_tx] proc=%d,send %s\n",proc->subframe,txbuf);
    	//sleep(1);
    	if (pthread_mutex_lock(&proc->mutex_tx) != 0) {
            printf(" [thread_tx]ERROR pthread_mutex_lock for eNB TX thread %d \n", proc->subframe);
       		exit_f=1;
       		break;
         }
        proc->instance_cnt_tx--;
        pthread_mutex_unlock( &proc->mutex_tx );
    }
    return_tx=0;
    return &return_tx;
}
static void* thread_rx(void* params){
	static int return_rx;
	struct sched_param sparam;
	int policy,s;
	memset(&sparam, 0 , sizeof (sparam));
  	sparam.sched_priority = sched_get_priority_max(SCHED_FIFO)-1;
  	policy = SCHED_FIFO ;
    proc_t *proc=(proc_t*)params;
  	s = pthread_setschedparam(pthread_self(), policy, &sparam);
  	if (s != 0)
    {
    	 printf("Error setting RX thread priority\n");
    }
  	s = pthread_getschedparam(pthread_self(), &policy, &sparam);
 	if (s != 0)
    {
    	printf("Error getting RX thread priority\n");
    }
    while(!exit_f){
    	printf("[thread_rx] proc=%d,waiting signal\n",proc->subframe);
    	if (pthread_mutex_lock(&proc->mutex_rx) != 0) {
       		printf("[thread_rx] error locking mutex for eNB RX proc %d\n", proc->subframe );
       		exit_f=1;
       		break;
    	}
    	while (proc->instance_cnt_rx < 0) {
      		pthread_cond_wait( &proc->cond_rx, &proc->mutex_rx ); // this unlocks mutex_tx while waiting and then locks it again
    	}
    	if (pthread_mutex_unlock(&proc->mutex_rx) != 0) {
      		printf("[thread_rx] error unlocking mutex for eNB RX proc %d\n",proc->subframe);
       		exit_f=1;
       		break;
    	}
    	printf("[thread_rx] proc=%d,got signal\n",proc->subframe);
    	printf("[thread_rx] proc=%d,receive %s\n",proc->subframe,rxbuf);
    	//sleep(1);
    	if (pthread_mutex_lock(&proc->mutex_rx) != 0) {
            printf(" [thread_rx]ERROR pthread_mutex_lock for eNB RX thread %d \n", proc->subframe);
       		exit_f=1;
       		break;
         }
        proc->instance_cnt_rx--;
        pthread_mutex_unlock( &proc->mutex_rx );
    }
    return_rx=0;
    return &return_rx;
}

static void* main_thread(void* arg){
	static int return_main;
	int frame=0;
	int hw_subframe=0;
	struct sched_param sparam;
	int policy,s;
	memset(&sparam, 0 , sizeof (sparam));
  	sparam.sched_priority = sched_get_priority_max(SCHED_FIFO);
  	policy = SCHED_RR ;
  	s = pthread_setschedparam(pthread_self(), policy, &sparam);
  	if (s != 0)
    {
    	 printf("Error setting main_thread priority\n");;
    }
  	s = pthread_getschedparam(pthread_self(), &policy, &sparam);
 	if (s != 0)
    {
    	printf("Error getting mian_thread priority\n");

    }
    printf( "waiting for sync (main_thread)\n" );
  	pthread_mutex_lock( &sync_mutex );
  	while (sync_var<0)
    	pthread_cond_wait( &sync_cond, &sync_mutex );
  	pthread_mutex_unlock(&sync_mutex);
  	printf( "got sync (main_thread)\n" );
  	int rxlen=0;
  	int txlen=0;
    int remote_addr_len=0;
  	while(!exit_f){
    	rxlen=recvfrom(sock,rxbuf,sizeof(rxbuf),0,(struct sockaddr*)&remote_addr,&(remote_addr_len));
      if(rxlen==-1){
          printf("recvfrom(): %d\n", errno);
      }
    	txlen=sendto(sock,txbuf,sizeof(txbuf),0,(struct sockaddr*)&remote_addr,remote_addr_len);
  		if(txlen==-1){
          printf("sendto(): %d\n", errno);
      }
      if (pthread_mutex_lock(&proctx.mutex_tx) != 0) {
            printf(" [main_thread]ERROR pthread_mutex_lock for eNB TX thread %d (IC %d)\n", hw_subframe, proctx.instance_cnt_tx );
       		exit_f=1;
       		break;
         }
        int cnt_tx = ++proctx.instance_cnt_tx;
        pthread_mutex_unlock( &proctx.mutex_tx );
        if (cnt_tx == 0) {
            // the thread was presumably waiting where it should and can now be woken up
            if (pthread_cond_signal(&proctx.cond_tx) != 0) {
              printf("[main_thread] ERROR pthread_cond_signal for eNB TX thread %d\n", hw_subframe );
       		  exit_f=1;
              break;
            }
        } else {
            printf("[main_thread]TX thread %d busy!! (cnt_tx %i)\n",hw_subframe,cnt_tx );
            exit_f=1;
            break;
        }
        if (pthread_mutex_lock(&procrx.mutex_rx) != 0) {
            printf(" [main_thread]ERROR pthread_mutex_lock for eNB RX thread %d (IC %d)\n", hw_subframe, procrx.instance_cnt_rx );
       		exit_f=1;
            break;
         }
        int cnt_rx = ++procrx.instance_cnt_rx;
        pthread_mutex_unlock( &procrx.mutex_rx );
        if (cnt_rx == 0) {
            // the thread was presumably waiting where it should and can now be woken up
            if (pthread_cond_signal(&procrx.cond_rx) != 0) {
              printf("[main_thread] ERROR pthread_cond_signal for eNB rx thread %d\n", hw_subframe );
       		  exit_f=1;
              break;
            }
        } else {
            printf("[main_thread]rx thread %d busy!! (cnt_rx %i)\n",hw_subframe,cnt_rx );
       		exit_f=1;
            break;
          }
        sleep(3);
    	hw_subframe++;
    	if (hw_subframe == 5) {
      		hw_subframe = 0;
      		frame++;
          printf("now UE frame=%d\n", frame);
    	}
  	}
  	return_main=0;
  	return &return_main;
}
void init_proc(){
		    proctx.instance_cnt_tx = -1;
      	proctx.instance_cnt_rx = -1;

        procrx.instance_cnt_tx = -1;
        procrx.instance_cnt_rx = -1;
      	pthread_mutex_init( &proctx.mutex_tx, NULL);
      	pthread_mutex_init( &procrx.mutex_rx, NULL);
      	pthread_cond_init( &proctx.cond_tx, NULL);
      	pthread_cond_init( &procrx.cond_rx, NULL);

      	pthread_create( &proctx.pthread_tx,NULL,thread_tx, &proctx);
      	pthread_create( &procrx.pthread_rx,NULL,thread_rx, &procrx);
}
void kill_proc(){
	int *status;
	int result;
		proctx.instance_cnt_tx = 0;
		pthread_cond_signal(&proctx.cond_tx );
		result=pthread_join(proctx.pthread_tx, (void**)&status );
		if (result != 0) {
        	printf( "Error joining thread.\n" );
      	} else {
        	if (status) {
          		printf( "status %d\n", *status );
        	} else {
          		printf( "The thread was killed. No status available.\n" );
        	}
      	}
		procrx.instance_cnt_rx = 0;
		pthread_cond_signal(&procrx.cond_rx );
		pthread_join(procrx.pthread_rx, (void**)&status );
		if (result != 0) {
        	printf( "Error joining thread.\n" );
      	} else {
        	if (status) {
          		printf( "status %d\n", *status );
        	} else {
          		printf( "The thread was killed. No status available.\n" );
        	}
      	}
      	pthread_mutex_destroy(&proctx.mutex_tx );
      	pthread_mutex_destroy(&procrx.mutex_rx );
      	pthread_cond_destroy(&proctx.cond_tx );
      	pthread_cond_destroy(&procrx.cond_rx );

}
void create_socket(){
	if((sock=socket(PF_INET,SOCK_DGRAM,0))==-1)
		printf("socket() error: %d\n",errno);
	memset(&my_addr,0,sizeof(&my_addr));
	my_addr.sin_family=AF_INET;
	my_addr.sin_addr.s_addr=htonl(INADDR_ANY);
	my_addr.sin_port=htons(5555);
	if(bind(sock,(struct sockaddr*)&my_addr,sizeof(my_addr))==-1)
	{
		printf("bind() error :%d\n",errno);
	}

	memset(&remote_addr,0,sizeof(&remote_addr));
	remote_addr.sin_family=AF_INET;
	remote_addr.sin_addr.s_addr=inet_addr("127.0.0.1");
	remote_addr.sin_port=htons(6666);
  //char buf[20]="hello,I am UE\n";
  //sendto(sock,buf,sizeof(buf),0,(struct sockaddr*)&remote_addr,sizeof(remote_addr));
}
