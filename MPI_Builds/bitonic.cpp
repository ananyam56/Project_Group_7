/****************************************************************************** 
* FILE: bitonic.cpp

* DESCRIPTION:  
*  This program implements the Bitonic Sort algorithm using MPI for parallelization.
*  The data is distribtued across multiple processes, where each process sorts its local    
*  portion of the array using the bitonic sort algorithm. Then, the processes communicate to
*  perform parallel bitonic merging, resulting in a fully sorted array. The master process  
*  initiates the sorting, distributes data, and gathers the sorted portions from all        
*  processes to produce the final sorted array.

* AUTHOR: Abigail Hunt
* LAST REVISED: 10/21/2024
******************************************************************************/

#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include <vector>       
#include <algorithm>

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

void parallelBitonicSort(std::vector<int>& local_array, int partner, int rank, int local_size, bool direction) {
    //MPI_Status status;
    std::vector<int> partner_array(local_size);

    CALI_MARK_BEGIN(comm);
    CALI_MARK_BEGIN(comm_large);
    MPI_Sendrecv(local_array.data(), local_size, MPI_INT, partner, 0,partner_array.data(), local_size, MPI_INT, partner, 0,MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    CALI_MARK_END(comm_large);
    CALI_MARK_END(comm);

    CALI_MARK_BEGIN(comp);
    CALI_MARK_BEGIN(comp_large);
    std::vector<int> merged_array(local_size * 2);
    std::merge(local_array.begin(), local_array.end(), partner_array.begin(), partner_array.end(), merged_array.begin());
    
    if (direction) {
        if (rank < partner) {
            std::copy(merged_array.begin(), merged_array.begin() + local_size, local_array.begin());
        } else {
            std::copy(merged_array.begin() + local_size, merged_array.end(), local_array.begin());
        }
    } else {
        if (rank < partner) {
            std::copy(merged_array.begin() + local_size, merged_array.end(), local_array.begin());
        } else {
            std::copy(merged_array.begin(), merged_array.begin() + local_size, local_array.begin());
        }
    }
    CALI_MARK_END(comp_large);
    CALI_MARK_END(comp);
  
}


int main(int argc, char *argv[]) {
 
    
    CALI_CXX_MARK_FUNCTION;
    
    // Create caliper ConfigManager object
    cali::ConfigManager mgr;
    mgr.start();
    
    int array_size;
    std::string algorithm;
    std::string input_type;
    if (argc == 4) {
        algorithm = argv[1];
        array_size = atoi(argv[2]);
        input_type = argv[3];
    } else {
        printf("\n Please provide the size of the array");
        return 0;
    }
    
    int num_procs, rank;
    std::vector<int> data;
    int local_size;
    std::vector<int> local_data;
    
    
    // Initialize MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    
    // Adiak metadata
    adiak::init(NULL);
    adiak::launchdate();
    adiak::libraries();
    adiak::cmdline();
    adiak::clustername();
    adiak::value("algorithm", "bitonic");
    adiak::value("programming_model", "mpi");
    adiak::value("data_type", "int");
    adiak::value("size_of_data_type", sizeof(int));
    adiak::value("input_size", array_size);
    adiak::value("input_type", input_type);
    adiak::value("num_procs", num_procs);
    adiak::value("scalability", "strong");  // or "weak" depending on the experiment
    adiak::value("group_num", 7); 
    adiak::value("implementation_source", "online");


    
    if (rank == 0) {
      CALI_MARK_BEGIN(data_init_runtime);
      data.resize(array_size);
      for (int i = 0; i < array_size; i++) {
        if (input_type == "random") {
            data[i] = rand() % 10000;
        } else if (input_type == "sorted" || input_type == "perturbed") {
            data[i] = i;
        } else if (input_type == "reverse") {
            data[i] = array_size - i;
        }
      }
        
      if (input_type == "perturbed") {
        int num_perturbed = array_size / 100;  // 1% of the elements
        for (int i = 0; i < num_perturbed; i++) {
          int idx1 = rand() % array_size;
          int idx2 = rand() % array_size;
          std::swap(data[idx1], data[idx2]);
        }
      }
      
      /*std::cout<<"Initial Array: "<<std::endl;
      for(int i = 0; i < array_size; ++i){
        std::cout<<data[i]<<" ";
        }
        std::cout<<"\n";*/
      CALI_MARK_END(data_init_runtime);
    }
    

    // Determine local array size
    CALI_MARK_BEGIN(comp);
    CALI_MARK_BEGIN(comp_small);
    
    local_size = array_size / num_procs;
    local_data.resize(local_size);
    
    CALI_MARK_END(comp_small);
    CALI_MARK_END(comp);


    CALI_MARK_BEGIN(comm);
    CALI_MARK_BEGIN(comm_small);
    MPI_Bcast(&num_procs, 1, MPI_INT, 0, MPI_COMM_WORLD);
    CALI_MARK_END(comm_small);
    CALI_MARK_END(comm);

    CALI_MARK_BEGIN(comm);
    CALI_MARK_BEGIN(comm_large);

    MPI_Scatter(data.data(), local_size, MPI_INT, local_data.data(), local_size, MPI_INT, 0, MPI_COMM_WORLD);

    CALI_MARK_END(comm_large);
    CALI_MARK_END(comm);

   // Local sort
    CALI_MARK_BEGIN(comp);
    CALI_MARK_BEGIN(comp_large);
    std::sort(local_data.begin(), local_data.end());
    CALI_MARK_END(comp_large);
    CALI_MARK_END(comp);



    // Exchange with partner
    for (int i = 2; i <= num_procs; i *= 2) {
        for (int j = i / 2; j > 0; j /= 2) {
            int partner = rank ^ j;
            bool direction = ((rank & i) == 0);
            if (partner < num_procs) {
                parallelBitonicSort(local_data, partner, rank, local_size, direction);
            }
            MPI_Barrier(MPI_COMM_WORLD);
        }
    }

    // Gather sorted arrays to root process
    CALI_MARK_BEGIN(comm);
    CALI_MARK_BEGIN(comm_large);

    MPI_Gather(local_data.data(), local_size, MPI_INT, data.data(), local_size, MPI_INT, 0, MPI_COMM_WORLD);
    
    CALI_MARK_END(comm_large);
    CALI_MARK_END(comm);

    if (rank == 0)
    {
        /*std::cout << "Sorted array: ";
        for (int i = 0; i < data.size(); ++i)
        {
            std::cout << data.at(i) << ", ";
        }*/
        
        // Check if the final sorted array is correct
        CALI_MARK_BEGIN(correctness_check);
        if (std::is_sorted(data.begin(), data.end())) {
            std::cout<<"The data is correctly sorted"<<std::endl;
        } else {
            std::cout<<"The data is NOT sorted correctly"<<std::endl;
        }
        CALI_MARK_END(correctness_check);
    }

    mgr.stop();
    mgr.flush();

    // Finalize MPI
    MPI_Finalize();
    return 0;
}
