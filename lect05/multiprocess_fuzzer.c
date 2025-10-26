#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>
#include <signal.h>

void mutate_and_write(const char *seedfile, const char *outpath, int seed){
    FILE *in = fopen(seedfile,"r");
    FILE *out = fopen(outpath,"w");
    if(!in || !out){ perror("mutate fopen"); exit(1); }
    int terms; double x; char mode[16];
    fscanf(in,"terms=%d\n",&terms);
    fscanf(in,"x=%lf\n",&x);
    if(fscanf(in,"mode=%15s\n",mode) < 0) strcpy(mode,"safe");
    fclose(in);

    srand(seed);
    int delta = (rand()%7) - 3; // -3..+3
    terms = terms + delta;
    if(terms < 0) terms = 0;
    if(rand()%50==0) strcpy(mode,"crash");

    fprintf(out,"terms=%d\n", terms);
    fprintf(out,"x=%g\n", x + ((rand()%100)-50)/100.0);
    fprintf(out,"mode=%s\n", mode);
    fclose(out);
}

int main(int argc, char **argv){
    if(argc < 4){
        fprintf(stderr,"Usage: %s seedfile n_workers rounds\n", argv[0]);
        return 1;
    }
    const char *seedfile = argv[1];
    int n_workers = atoi(argv[2]);
    int rounds = atoi(argv[3]);
    pid_t *pids = calloc(n_workers, sizeof(pid_t));
    srand(time(NULL));

    for(int r=0;r<rounds;r++){
        for(int w=0; w<n_workers; w++){
            int seed = rand();
            char tmpfile[256];
            snprintf(tmpfile, sizeof(tmpfile), "/tmp/fuzz_in_%d_%d.txt", r, w);
            mutate_and_write(seedfile, tmpfile, seed);

            pid_t pid = fork();
            if(pid == 0){
                execl("./taylor_program", "taylor_program", tmpfile, (char*)NULL);
                perror("execl");
                _exit(127);
            } else if(pid > 0){
                pids[w] = pid;
            } else {
                perror("fork");
            }
        }

        for(int w=0; w<n_workers; w++){
            int status;
            pid_t c = waitpid(pids[w], &status, 0);
            if(c > 0){
                if(WIFSIGNALED(status)){
                    int sig = WTERMSIG(status);
                    printf("Worker %d crashed with signal %d! Saving input.\n", w, sig);
                    char src[256], dst[256];
                    snprintf(src,sizeof(src),"/tmp/fuzz_in_%d_%d.txt", r, w);
                    snprintf(dst,sizeof(dst),"interesting/interesting_r%d_w%d_sig%d.txt", r,w,sig);
                    mkdir("interesting",0755);
                    char cmd[512];
                    snprintf(cmd,sizeof(cmd),"cp %s %s", src, dst);
                    system(cmd);
                } else if(WIFEXITED(status)){
                    int code = WEXITSTATUS(status);
                    printf("Worker %d exited normally code=%d\n", w, code);
                }
            }
        }
    }
    free(pids);
    return 0;
}
