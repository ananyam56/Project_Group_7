/******************************************************************************
* FILE: bitonic.cpp
* DESCRIPTION:  
*  This program implements the Bitonic Sort algorithm using MPI for parallelization.
*  The data is distribtued across multiple processes, where each process sorts its local        portion of the array using the bitonic sort algorithm. Then, the processes communicate to    perform parallel bitonic merging, resulting in a fully sorted array. The master process      initiates the sorting, distributes data, and gathers the sorted portions from all            processes to produce the final sorted array.
* AUTHOR: Abigail Hunt
* LAST REVISED: 10/15/2024
******************************************************************************/

#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include <algorithm> // Included for std::is_sorted

#include <caliper/cali.h>
#include <caliper/cali-manager.h>
#include <adiak.hpp>

//Define sorting directions
#define ASCENDING 1
#define DESCENDING 0

/* Define Caliper region names */
  const char* data_init_runtime = "data_init_runtime";
  const char* correctness_check = "correctness_check";
  const char* comm = "comm";
  const char* comm_small = "comm_small";
  const char* comm_large = "comm_large";
  const char* comp = "comp";
  const char* comp_small = "comp_small";
  const char* comp_large = "comp_large";

// Function to compare and swap elements based on the direction
void swap(int *array, int i, int j, int dir) {
    if ((dir == ASCENDING && array[i] > array[j]) || 
        (dir == DESCENDING && array[i] < array[j])) {
        int temp = array[i];
        array[i] = array[j];
        array[j] = temp;
    }
}

//Function to perform bitonic merge, merging two halves such that
  // In ASCENDING order, elements in the first half are smaller than or equal to those in the     second half
  // In DESCENDING order, elements in the first half are larger than or equal to those in the     second half
void bitonicMerge(int *array, int low, int count, int dir) {
    if (count > 1) {
    
        //Find midpoint to divide the array into two halves
        int mid = count / 2;
        
        //Iterate through first half of current section and compare with second half
        for (int i = low; i < low + mid; i++) {
          swap(array, i, i+mid, dir);
        }
        // Recursively merge first half of current section
        bitonicMerge(array, low, mid, dir);
        
        // Recursively merge second half of the current section
        bitonicMerge(array, low + mid, mid, dir);
    }
}

//Function to recursively sort an array using the bitonic sort algorithm
void bitonicSort(int *array, int low, int count, int dir) {
    if (count > 1) {
        int mid = count / 2;
        
        //Sorts the first half in ASCENDING order
        bitonicSort(array, low, mid, ASCENDING);
        
        //Sorts the second half in descending order
        bitonicSort(array, low + mid, mid, DESCENDING);
        
        //Merge the two halves in the specified direction (dir)
        bitonicMerge(array, low, count, dir);
    }
}

// Function to perform the parallel merge after local sorting
void parallelBitonicMerge(int *array, int count, int dir, int rank, int num_procs) {

    //stepSize determines the number of processes involved in each merge
    int stepSize;
    
    // stepSize = 2 initially because two processes will exchange and merge their data
    // stepSize doubles at each iteration in order to group more processes together for             merging
    for (stepSize = 2; stepSize <= num_procs; stepSize *= 2) {
        // XOR operation used to find the process that each current process will exchange               data with
        // using XOR ensures that the pairing follows the hierarchical structure for merging
        int partner = (rank ^ (stepSize >> 1));

        if (partner < num_procs) {
            // Allocate memory for receiving data from the partner process
            int *recv_data = (int *)malloc((count) * sizeof(int));
            
            CALI_MARK_END(comp_large);
            CALI_MARK_END(comp);
            CALI_MARK_BEGIN(comm);
            {
              CALI_MARK_BEGIN(comm_small);
              // Send local data to the partner and receive data from the partner
              MPI_Sendrecv(array, count, MPI_INT, partner, 0,
                         recv_data, count, MPI_INT, partner, 0,
                         MPI_COMM_WORLD, MPI_STATUS_IGNORE);
              CALI_MARK_END(comm_small);
            }
            CALI_MARK_END(comm);
            
            CALI_MARK_BEGIN(comp);
            CALI_MARK_BEGIN(comp_large);
                         
            // Merge received data with local data
            for (int i = 0; i < count; ++i) {
                array[i] = recv_data[i];
            }
            
            // Perform bitonic merge on the combined data
            bitonicMerge(array, 0, count, dir);
            
            free(recv_data);
        }
    }
}


int main(int argc, char *argv[]) {

    //Adiak metadata
    adiak::init(NULL);
    adiak::launchdate();    // launch date of the job
    adiak::libraries();     // Libraries used
    adiak::cmdline();       // Command line used to launch the job
    adiak::clustername();   // Name of the cluster
    adiak::value("algorithm", "bitonic"); // The name of the algorithm you are using (e.g.,      "merge", "bitonic")
    adiak::value("programming_model", "mpi"); // e.g. "mpi"
    adiak::value("data_type", "int"); // The datatype of input elements (e.g., double, int,      float)
    adiak::value("size_of_data_type", sizeof(int)); // sizeof(datatype) of input elements in     bytes (e.g., 1, 2, 4)
    adiak::value("input_type", "Random"); // For sorting, this would be choices: ("Sorted",      "ReverseSorted", "Random", "1_perc_perturbed")
    //adiak::value("scalability", scalability); // The scalability of your algorithm. choices    : ("strong", "weak")
    adiak::value("gtoup_number", 7); // The number of your group (integer, e.g., 1, 10)
    adiak::value("implementation_source", "handwritten"); // Where you got the source code of    your algorithm. choices: ("online", "ai", "handwritten").
    
    CALI_CXX_MARK_FUNCTION;  
    
    // Create caliper ConfigManager object
    cali::ConfigManager mgr;
    mgr.start();
    
    int sizeOfArray;
    if (argc == 2) {
        sizeOfArray = atoi(argv[1]);
    } else {
        printf("\n Please provide the size of the array");
        return 0;
    }

    int num_procs, rank;
    int *data = NULL; //pointer to hold the entire array
    int local_size; //size of local section of array for each process
    int *local_data; //pointer to hold local section of array
    
    adiak::value("input_size",sizeOfArray); // The number of elements in input dataset (1000)
    adiak::value("num_procs", num_procs); // The number of processors (MPI ranks)

    // Initialize MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    // Rank 0 initializes random data and scatters to other processes
    CALI_MARK_BEGIN(data_init_runtime);
    if (rank == 0) {
        data = (int *)malloc(sizeOfArray * sizeof(int));
        srand(time(NULL));

        // Generating random data
        for (int i = 0; i < sizeOfArray; i++) {
            data[i] = rand() % sizeOfArray;  
        }
    }
    CALI_MARK_END(data_init_runtime);

    // Determine local array size
    local_size = sizeOfArray / num_procs;
    local_data = (int *)malloc(local_size * sizeof(int));

    // Scatter the data to all processes
    CALI_MARK_BEGIN(comm);
    {
        CALI_MARK_BEGIN(comm_large);
        
        MPI_Scatter(data, local_size, MPI_INT, local_data, local_size, MPI_INT, 0,                  MPI_COMM_WORLD);
        
        CALI_MARK_END(comm_large);
    }
    CALI_MARK_END(comm);

    // Perform bitonic sort on the local data
    CALI_MARK_BEGIN(comp);
    {
        CALI_MARK_BEGIN(comp_large);
        bitonicSort(local_data, 0, local_size, ASCENDING);
        
        // Perform parallel merging of locally sorted arrays
        parallelBitonicMerge(local_data, local_size, sizeOfArray, rank, num_procs);
        
        CALI_MARK_END(comp_large);
    }
    CALI_MARK_END(comp);

   
    CALI_MARK_BEGIN(comm);
    {
      CALI_MARK_BEGIN(comm_large);
      // Gather the sorted local arrays to the root process 
      MPI_Gather(local_data, local_size, MPI_INT, data, local_size, MPI_INT, 0,   MPI_COMM_WORLD);
      CALI_MARK_END(comm_large);
    }
    CALI_MARK_END(comm);

    // Ir root process, check if final data is sorted
    if (rank == 0) {
        
        CALI_MARK_BEGIN(comp);
        {
          CALI_MARK_BEGIN(comp_large);
          bitonicSort(data, 0, sizeOfArray, ASCENDING);
          CALI_MARK_END(comp_large);
        }
        CALI_MARK_END(comp);

        // Check if the final sorted array is correct
        CALI_MARK_BEGIN(correctness_check);
        if (std::is_sorted(data, data + sizeOfArray)) {
            printf("Process %d: The data is correctly sorted\n", rank);
        } else {
            printf("Process %d: The data is NOT sorted correctly\n", rank);
        }
        CALI_MARK_END(correctness_check);

        // Free the data array on the root process
        free(data);
    }

    // Free local data array
    free(local_data);
    
    mgr.stop();
    mgr.flush();

    // Finalize MPI
    MPI_Finalize();
    return 0;
}
