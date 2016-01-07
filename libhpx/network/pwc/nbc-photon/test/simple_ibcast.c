#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <unistd.h>
#include <nbc.h>

static inline void  log_fail(char* msg){
  printf( "[Error! Test failed with msg : %s ]\n", msg);
  fflush(stdout);
}

static inline void  log_pass(char* msg){
  printf( "[Test passed with msg : %s ]\n", msg);
}

char* my_msg =  "Helloabcdefghijklmno";

#define I 20
/*#define I 1*/

int main(int argc, char **argv)
{
	char hostname[25];
	char message[50];
	NBC_Comminfo comm  ;

	memset(message, '\0', sizeof(message));
	int  i, rank, wsize ;
	MPI_Status status;
	MPI_Request request = MPI_REQUEST_NULL;
	int root = 0;
	gethostname(hostname, 25);
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &wsize);
        int active_ranks[wsize];
	for (i = 0; i < wsize; ++i) {
	  active_ranks[i] = i;	
	}

	printf("my hostname is : %s my rank is : %d \n", hostname, rank);
	if (rank == root)
	{
		strcpy(message, my_msg); 
	}
        int res = NBC_Init_comm(rank, active_ranks, wsize, wsize, &comm);
	if(res != NBC_OK){
	  printf( "Error NBC Initialization() from process %d \n", rank);
	  goto end;
	}

	for (i = 0; i < I; ++i) {
	  NBC_Handle handle;
	  //broadcast
	  NBC_Ibcast(message, 21, MPI_CHAR, root, &comm, &handle); 
	  NBC_Wait(&handle);
	  int ret = strcmp(my_msg, message);
	  if(ret != 0){
	    log_fail(message);	
	    goto end;  
	  }
	}
        
	printf( "Message from process %d : %s\n", rank, message);
	if(rank == root){
	  sleep(1);	
	  log_pass(" All simple Ibcast tests were succesfull");
	}  
end:
	MPI_Finalize();
}

