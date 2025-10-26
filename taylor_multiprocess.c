#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <math.h>
#include <string.h>
#include <errno.h>

typedef struct {
    int idx;
    double val;
} packet_t;

static double sin_taylor(double x, int terms) {
    if (terms <= 0) return 0.0;

    double value = x;         
    if (terms == 1) return value;

    double numer = x * x * x;  
    double denom = 6.0;         
    int sign = -1;             

    for (int j = 2; j <= terms; ++j) {
        value += (double)sign * (numer / denom);
        
        numer *= x * x;
       
        denom *= (2.0*(double)j) * (2.0*(double)j + 1.0);
        sign *= -1;
    }
    return value;
}

static void child_work(int fd_write, const double *x, int s, int e, int terms) {
    for (int i = s; i < e; ++i) {
        packet_t pkt;
        pkt.idx = i;
        pkt.val = sin_taylor(x[i], terms);
        ssize_t w = write(fd_write, &pkt, sizeof(pkt));
        if (w != (ssize_t)sizeof(pkt)) {
            break;
        }
    }
    close(fd_write);
}

static inline int min_int(int a, int b){ return a < b ? a : b; }

int main(int argc, char **argv) {
    int terms = 6;     
    int procs = 4;     

    if (argc >= 2) terms = atoi(argv[1]);
    if (argc >= 3) procs = atoi(argv[2]);
    if (terms < 1) terms = 1;
    if (procs < 1) procs = 1;

    double x[] = {
        0.0,
        M_PI/12,  M_PI/8,   M_PI/6,   M_PI/4,
        M_PI/3,   M_PI/2,   0.134,   -0.25,
        -M_PI/3
    };
    const int N = (int)(sizeof(x)/sizeof(x[0]));


    int (*pipes)[2] = malloc(sizeof(int[2]) * procs);
    if (!pipes) { perror("malloc"); return 1; }
    for (int p = 0; p < procs; ++p) {
        if (pipe(pipes[p]) == -1) { perror("pipe"); return 1; }
    }


    int chunk = (N + procs - 1) / procs;
    if (chunk < 1) chunk = 1;

 
    for (int p = 0; p < procs; ++p) {
        int s = p * chunk;
        int e = min_int(N, s + chunk);
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return 1;
        }
        if (pid == 0) {
            for (int q = 0; q < procs; ++q) {
                close(pipes[q][0]); // read end
                if (q != p) close(pipes[q][1]);
            }
            child_work(pipes[p][1], x, s, e, terms);
            _exit(0);
        }
        close(pipes[p][1]);
    }

    double *result = calloc(N, sizeof(double));
    if (!result) { perror("calloc"); return 1; }

    for (int p = 0; p < procs; ++p) {
        for (;;) {
            packet_t pkt;
            ssize_t r = read(pipes[p][0], &pkt, sizeof(pkt));
            if (r == 0) break;            
            if (r < 0) {
                if (errno == EINTR) continue;
                perror("read");
                break;
            }
            if (r != (ssize_t)sizeof(pkt)) {
                fprintf(stderr, "partial packet read\n");
                break;
            }
            if (0 <= pkt.idx && pkt.idx < N) {
                result[pkt.idx] = pkt.val;
            }
        }
        close(pipes[p][0]);
    }

    for (int p = 0; p < procs; ++p) wait(NULL);

    printf("# terms=%d, processes=%d\n", terms, procs);
    for (int i = 0; i < N; ++i) {
        printf("x=% .6f | taylor= % .10f | math.h sin= % .10f | abs err= %.3e\n",
               x[i], result[i], sin(x[i]), fabs(result[i] - sin(x[i])));
    }

    free(result);
    free(pipes);
    return 0;
}
