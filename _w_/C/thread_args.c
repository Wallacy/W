#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <assert.h>

struct args {
  int a;
  int b;
};

struct results {
  int sum;
  int difference;
  int product;
  int quotient;
  int modulus;
};

void* calculator (void *_args)
{
  /* Cast the args to the usable struct type */
  struct args *args = (struct args *) _args;

  /* Allocate heap space for this thread's results */
  struct results *results = calloc (sizeof (struct results), 1);
  results->sum        = args->a + args->b;
  results->difference = args->a - args->b;
  results->product    = args->a * args->b;
  results->quotient   = args->a / args->b;
  results->modulus    = args->a % args->b;
  /* De-allocate the input instance and return the pointer to
     results on heap */
  free (args);
  pthread_exit (results);
}

int main(){
    /* Create 5 threads, each calling calculator() */
pthread_t child[5];

/* Allocate arguments and create the threads */
struct args *args[5] = { NULL, NULL, NULL, NULL, NULL };
for (int i = 0; i < 5; i++)
  {
    /* args[i] is a pointer to the arguments for thread i */
    args[i] = calloc (sizeof (struct args), 1);

    /* thread 0 calls calculator(1,1)
       thread 1 calls calculator(2,4)
       thread 2 calls calculator(3,9)
       and so on... */
    args[i]->a = i + 1;
    args[i]->b = (i + 1) * (i + 1);
    assert (pthread_create (&child[i], NULL, calculator, args[i]) 
            == 0);
  }

/* Allocate an array of pointers to result structs */
struct results *results[5];
for (int i = 0; i < 5; i++)
  {
    /* Passing results[i] by reference creates (void **) */
    pthread_join (child[i], (void **)&results[i]);

    /* Print each of the results and free the struct */
    printf ("Calculator (%d, %2d) ==> ", i+1, (i+1) * (i+1));
    printf ("+:%3d;   ", results[i]->sum);
    printf ("-:%3d;   ", results[i]->difference);
    printf ("*:%3d;   ", results[i]->product);
    printf ("/:%3d;   ", results[i]->quotient);
    printf ("%%:%3d\n", results[i]->modulus);
    free (results[i]);
  }
}

// Aqui está no modo "normal"; Para W o correto seria quem chama alocar tudo.
// Depois troco, até porque quero pensar na questão do pthread_exit ((void*)code); onde code é um inteiro com possivel codigo de erro para dar cast no enum de erros.
// O retorno real deve vir junto com os args, já que quem alocou e deve alocar é o caller...

// Como quem chama aloca, isso deve simplificar o gerenciamento de threads, até porque posso evitar atomic e outras coisas em varios objetos já que sei que não vai ter acesso multi thread.