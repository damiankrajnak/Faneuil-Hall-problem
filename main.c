#include <sys/shm.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// shared memory variables
int shm_id_vars,
    shm_id_sems;

// struct containing semaphores
typedef struct{
    sem_t mutex,
          noJudge,
          confirmed,
          starts,
          allSignedIn,
          glob_count_mutex;
}struct_Sems;
struct_Sems *sems;

// struct containing all other variables
typedef struct{
    int global_count,
        PI,
        IG,
        JG,
        IT,
        JT,
        entered,
        checked,
        judge,
        PI_count,
        in_building,
        imm_num;
    FILE *output;
}struct_Vars;
struct_Vars *vars;
 
// function that creates shared memory & initializes semaphores
int load_resources()
{
    sems = mmap(NULL, sizeof(struct_Sems), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
    vars = mmap(NULL, sizeof(struct_Vars), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
    
    if ((shm_id_vars = shmget(IPC_PRIVATE, sizeof(struct_Vars), IPC_CREAT | IPC_EXCL | 0666)) == -1) return -1;
    if ((shm_id_sems = shmget(IPC_PRIVATE, sizeof(struct_Vars), IPC_CREAT | IPC_EXCL | 0666)) == -1) return -1;
    
    if ((vars = (struct_Vars *)shmat(shm_id_vars, NULL, 0)) == (void *)-1) return -1;
    if ((sems = (struct_Sems *)shmat(shm_id_sems, NULL, 0)) == (void *)-1) return -1;
    
    if (sem_init(&sems->mutex, 1, 1) == -1) return -1;
    if (sem_init(&sems->noJudge, 1, 1) == -1) return -1;
    if (sem_init(&sems->confirmed, 1, 0) == -1) return -1;
    if (sem_init(&sems->starts, 1, 1) == -1) return -1;
    if (sem_init(&sems->allSignedIn, 1, 0) == -1) return -1;
    if (sem_init(&sems->glob_count_mutex, 1, 1) == -1) return -1;
    
    return 0;
}

// function that uninitializes semaphores & frees the alocated shared memory
int free_resources()
{
    if (sem_destroy(&sems->mutex) == -1) return -1;
    if (sem_destroy(&sems->noJudge) == -1) return -1;
    if (sem_destroy(&sems->confirmed) == -1) return -1;
    if (sem_destroy(&sems->starts) == -1) return -1;
    if (sem_destroy(&sems->allSignedIn) == -1) return -1;
    if (sem_destroy(&sems->glob_count_mutex) == -1) return -1;
   
    if (shmctl(shm_id_vars, IPC_RMID, NULL) == -1) return -1;
    if (shmctl(shm_id_sems, IPC_RMID, NULL) == -1) return -1;
    if (shmdt(vars) == -1) return -1;
    if (shmdt(sems) == -1) return -1;
    return 0;
}

// function containing path followed by immigrant processes
void immigrant(int I)
{
    // following sequence of semaphores is taken from "Little Books of Semaphores", but with some additional semaphores
    sem_wait(&sems->starts);
    vars->imm_num++;
    I = vars->imm_num;
    sem_wait(&sems->glob_count_mutex);
    fflush(vars->output);
    fprintf(vars->output, "%d: IMM %d: starts\n", ++vars->global_count, I);
    fflush(vars->output);
    sem_post(&sems->glob_count_mutex);
    sem_post(&sems->starts);
    
    sem_wait(&sems->noJudge);
    sem_wait(&sems->glob_count_mutex);
    fflush(vars->output);
    fprintf(vars->output, "%d: IMM %d: enters: %d: %d: %d\n", ++vars->global_count, I, ++vars->entered, vars->checked, ++vars->in_building);
    fflush(vars->output);
    sem_post(&sems->glob_count_mutex);
    sem_post(&sems->noJudge);
    
    sem_wait(&sems->mutex);
    sem_wait(&sems->glob_count_mutex);
    fflush(vars->output);
    fprintf(vars->output, "%d: IMM %d: checks: %d: %d: %d\n", ++vars->global_count, I, vars->entered, ++vars->checked, vars->in_building);
    fflush(vars->output);
    sem_post(&sems->glob_count_mutex);
    
    if (vars->judge == 1 && vars->entered == vars->checked){
        sem_post(&sems->allSignedIn);
    }
    else{
        sem_post(&sems->mutex);
    }
    
    sem_wait(&sems->confirmed);
    sem_wait(&sems->glob_count_mutex);
    fflush(vars->output);
    fprintf(vars->output, "%d: IMM %d: wants certificate: %d: %d: %d\n", ++vars->global_count, I, vars->entered, vars->checked, vars->in_building);
    fflush(vars->output);
    sem_post(&sems->glob_count_mutex);
    sem_wait(&sems->glob_count_mutex);
    fflush(vars->output);
    if (vars->IT != 0){
        // process sleeps for random time of interval <0; IT> miliseconds
        usleep((rand() % vars->IT*1000 + 1));
    }
    fprintf(vars->output, "%d: IMM %d: got certificate: %d: %d: %d\n", ++vars->global_count, I, vars->entered, vars->checked, vars->in_building);
    fflush(vars->output);
    sem_post(&sems->glob_count_mutex);
    
    sem_wait(&sems->noJudge);
    sem_wait(&sems->glob_count_mutex);
    fflush(vars->output);
    fprintf(vars->output, "%d: IMM %d: leaves: %d: %d: %d\n", ++vars->global_count, I, vars->entered, vars->checked, --vars->in_building);
    fflush(vars->output);
    sem_post(&sems->glob_count_mutex);
    sem_post(&sems->noJudge);
    
    free_resources();
    _Exit(0);
}

// function containing path followed by the judge
void judge()
{
    // judge repeats his work, while there is at least 1 immigrant which hasn't been confirmed yet.
    while(vars->PI_count != vars->PI){
        if (vars->JG != 0){
            // process sleeps for random time of interval <0; JG> miliseconds
            usleep((rand() % vars->JG*1000 + 1));
        }
        
        // following sequence of semaphores is taken from "Little Books of Semaphores", but with some additional semaphores
        sem_wait(&sems->glob_count_mutex);
        fflush(vars->output);
        fprintf(vars->output, "%d: JUDGE : wants to enter\n", ++vars->global_count);
        fflush(vars->output);
        sem_post(&sems->glob_count_mutex);
        
        sem_wait(&sems->noJudge);
        sem_wait(&sems->mutex);
        
        sem_wait(&sems->glob_count_mutex);
        fflush(vars->output);
        fprintf(vars->output, "%d: JUDGE : enters: %d: %d: %d\n", ++vars->global_count, vars->entered, vars->checked, vars->in_building);
        fflush(vars->output);
        sem_post(&sems->glob_count_mutex);
        vars->judge = 1;
        
        if (vars->entered > vars->checked){
            sem_wait(&sems->glob_count_mutex);
            fflush(vars->output);
            fprintf(vars->output, "%d: JUDGE : waits for imm: %d: %d: %d\n", ++vars->global_count, vars->entered, vars->checked, vars->in_building);
            fflush(vars->output);
            sem_post(&sems->glob_count_mutex);
            
            sem_post(&sems->mutex);
            sem_wait(&sems->allSignedIn);
        }
        sem_wait(&sems->glob_count_mutex);
        fflush(vars->output);
        fprintf(vars->output, "%d: JUDGE : starts confirmation: %d: %d: %d\n", ++vars->global_count, vars->entered, vars->checked, vars->in_building);
        fflush(vars->output);
        sem_post(&sems->glob_count_mutex);
        
        if (vars->JT != 0){
            // process sleeps for random time of interval <0; JT> miliseconds
            usleep((rand() % vars->JT*1000 + 1));
        }
        
        sem_wait(&sems->glob_count_mutex);
        fflush(vars->output);
        fprintf(vars->output, "%d: JUDGE : ends confirmation: 0: 0: %d\n", ++vars->global_count, vars->in_building);
        fflush(vars->output);
        sem_post(&sems->glob_count_mutex);
        
        for(int i = 0; i < vars->checked; i++){
            sem_post(&sems->confirmed);
            vars->PI_count++;
        }
        
        vars->checked = 0;
        vars->entered = 0;
        
        if (vars->JT != 0){
            // process sleeps for random time of interval <0; JT> miliseconds
            usleep((rand() % vars->JT*1000 + 1));
        }
        
        sem_wait(&sems->glob_count_mutex);
        fflush(vars->output);
        fprintf(vars->output, "%d: JUDGE : leaves: %d: %d: %d\n", ++vars->global_count, vars->entered, vars->checked, vars->in_building);
        fflush(vars->output);
        sem_post(&sems->glob_count_mutex);
        vars->judge = 0;
        
        sem_post(&sems->mutex);
        sem_post(&sems->noJudge);
    }
    sem_wait(&sems->glob_count_mutex);
    fflush(vars->output);
    fprintf(vars->output, "%d: JUDGE : finishes\n", ++vars->global_count);
    fflush(vars->output);
    sem_post(&sems->glob_count_mutex);
    
    free_resources();
    _Exit(0);
}
 
// function creates one judge process
void judge_create()
{
    int judge_pid;
    
    // process forking
    judge_pid = fork();
    if (judge_pid == 0) // child process proceeds to the judge() function, where it ends
    {
        judge();
    }
    // parent process ends here
    free_resources();
    _Exit(0);
}

// function creates PI immigrant processes
void immigrant_maker()
{
    int immigrant_pid;

    // process keeps forking itself in the loop
    for (int i = 0; i < vars->PI; i++)
    {
        // process forking
        immigrant_pid = fork();
        if (immigrant_pid == 0) // child process proceeds to the immigrant() function, where it ends
        {
            if (vars->IG != 0){
                // process sleeps for random time of interval <0; IG> miliseconds
                usleep((rand() % vars->IG * 1000 + 1));
            }
            immigrant(i+1);
        }
    }
    // after the loop, parent process ends here
    free_resources();
    _Exit(0);
}

// function parses all arguments
int parse(int argc, const char *argv[])
{
    if (argc != 6) return -1;
    char *ptr;
 
    vars->PI = (int)strtol(argv[1], &ptr, 10);
    vars->IG = (int)strtol(argv[2], &ptr, 10);
    vars->JG = (int)strtol(argv[3], &ptr, 10);
    vars->IT = (int)strtol(argv[4], &ptr, 10);
    vars->JT = (int)strtol(argv[5], &ptr, 10);
 
    if (vars->PI < 1) return -1;
    if (vars->IG < 0 || vars->IG > 2000) return -1;
    if (vars->JG < 0 || vars->JG > 2000) return -1;
    if (vars->IT < 0 || vars->IT > 2000) return -1;
    if (vars->JT < 0 || vars->JT > 2000) return -1;
    return 0;
}
 
 
int main(int argc, const char * argv[])
{
    int exclude = 0;

    // initializing semaphores and allocating shared memory
    if (load_resources() == -1) {fprintf(stderr, "System call error.\n"); free_resources(); exit(2);}
 
    // parsing arguments
    if (parse(argc, argv) == -1) {fprintf(stderr,"Wrong arguments.\n"); free_resources(); exit(1);}

    // following command makes use of the computer's internal clock to control the choice of the seed.
    // Since time is continually changing, the seed is forever changing.
    srand((int)time(NULL));
    vars->PI_count = 0;
    vars->entered = 0;
    vars->checked = 0;
    vars->global_count = 0;
    vars->judge = 0;
    vars->in_building = 0;
    vars->imm_num = 0;

    pid_t childPID;

    
    vars->output = fopen("proj2.out", "w+");

    // process forking
    childPID = fork();
    if (childPID == 0) // child process proceeds to judge_create() function, where it ends
    {
        exclude = 1;
        judge_create();
    }
    else if (childPID == -1)
    {
    fprintf(stderr, "System call error.\n");
    free_resources();
    exit(2);
    }
    
    // parent process continues
 
    // process forking
    childPID = fork();
    if (childPID == 0 && exclude != 1) // child process proceeds to immigrant_maker() function, where it ends
    {
        immigrant_maker();
    }
    else if (childPID == -1)
    {
    fprintf(stderr, "System call error.\n");
    free_resources();
    exit(2);
    }
    
    // parent process continues here
    
    //closing output file
    fclose(vars->output);
    // uninitializg semaphores and freeing allocated shared memory
    free_resources();

    // process ends here
    exit(EXIT_SUCCESS);
}
