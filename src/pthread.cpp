#ifndef _REENTRANT
#define _REENTRANT
#endif
#include <cmath>
#include <unistd.h>
#include "pthread.h"
struct particle_args {
    int arg1;
    bool arg2;
};

particle_t *particles;             /* main particle array */
particle_vector_t *particle_matrix;
int nof_slices;                     /* number of slices */
int nof_particles;                  /* number of particles */
double size;
pthread_t threads[NOF_THREADS];
pthread_mutex_t index_lock;
pthread_mutex_t work_lock;
pthread_mutex_t barrier_lock;
pthread_mutex_t print_lock;

pthread_cond_t barrier_cond;
int thread_counter = 0;

void Barrier() {
    pthread_mutex_lock(&barrier_lock);
    thread_counter++;
    if (thread_counter == NOF_THREADS) {
        thread_counter = 0;
        pthread_cond_broadcast(&barrier_cond);
    } else {
        pthread_cond_wait(&barrier_cond, &barrier_lock);
    }
    pthread_mutex_unlock(&barrier_lock);
}

void *Worker(void *arg) {
    struct particle_args *args = (struct particle_args *) arg;
    int slice = (nof_slices + NOF_THREADS - 1) / NOF_THREADS;
    int start = args->arg1 * slice;
    int end = (args->arg1 == NOF_THREADS - 1) ? (nof_slices) : start + slice;

    for (int step = 0; step < NSTEPS; step++) {

        if (args->arg1 == 0) {
            //här problemet ligger. Hur kan vi syncha detta bättre?
            //fixat, gjort av med kommunikationen mellan funktionerna med condition variables, slöade ner allt extremt.
            for (int i = 0; i < nof_slices; i++) {
                particle_matrix[i].clear();
            }
            for (int i = 0; i < nof_particles; i++) {
                particle_matrix[(int) (particles[i].x * floor(nof_slices / size))].emplace_back(&particles[i]);
            }
        }
        //indexering klar av mastertråden, vänta in den.
        Barrier();
        //parallellisering
        for (int i = start; i < end; i++) {
            int comp_start = ((i > 0) ? (i - 1) : i);
            int comp_end = ((i < nof_slices - 1) ? (i + 2) : (i + 1));

            for (particle_t *curr_particle : particle_matrix[i]) {
                (*curr_particle).ax = (*curr_particle).ay = 0;

                for (int j = comp_start; j < comp_end; j++) {
                    for (particle_t *neigh_particle: particle_matrix[j]) {
                        apply_force((*curr_particle), (*neigh_particle));
                    }
                }
            }
        }

        for (int i = start * 10; i < end * 10; i++) {
            move(particles[i]);
        }

        Barrier();
        if (args->arg1 == 0) {
            //mastertråden sparar positionerna
            if (args->arg2) {
                save(particles);
            }
        }
        //vänta in mastertråden
        Barrier();
    }
}

void perform_steps(int n, bool perform_save) {
    nof_particles = n;
    nof_slices = (n < 10) ? 1 : n / 10;
    size = sqrt(0.0005 * n);

    particle_matrix = new particle_vector_t[nof_slices];
    particles = new particle_t[nof_particles];
    init_particles(nof_particles, particles);
    pthread_cond_init(&barrier_cond, NULL);

    pthread_mutex_init(&work_lock, NULL);
    pthread_mutex_init(&index_lock, NULL);
    pthread_mutex_init(&barrier_lock, NULL);
    pthread_mutex_init(&print_lock, NULL);

    struct particle_args args[NOF_THREADS];
    for (int i = 0; i < NOF_THREADS; i++) {
        args[i].arg1 = i;
        args[i].arg2 = perform_save;
        pthread_create(&threads[i], NULL, Worker, (void *)&args[i]);
    }
    for (int i = 0; i < NOF_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
}