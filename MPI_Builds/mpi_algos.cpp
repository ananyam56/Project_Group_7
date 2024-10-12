/******************************************************************************
* FILE: mpi_algos.cpp
* DESCRIPTION:  
*   MPI Algorithms
*   In this code, the master task distributes a matrix multiply
*   operation to numtasks-1 worker tasks.
* AUTHORS: Abigail Hunt, Ananya Maddali, Jordyn Hamstra, Veda Javalagi
* LAST REVISED: 10/12/2024
******************************************************************************/
#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string>

#include <caliper/cali.h>
#include <caliper/cali-manager.h>
#include <adiak.hpp>

// parameters: 1) algorithm, 2) input size, 3) input type, 4) number of processes
int main (int argc, char *argv[])
{
    int sizeOfInput;
    if (argc == 4) {
        // get algorithm
        std::string algorithm = argv[0];
        if (algorithm != "bitonic") && (algorithm != "sample") && (algorithm != "merge") && (algorithm != "radix") {
            printf("\n The algorithm must be \'bitonic\', \'sample\', \'merge\', or \'radix\'.");
            return 0;
        }

        // get size of the input array
        std::string sizeInput = argv[1];
        if (sizeInput.substr(0, 2) == "2^") {
            sizeOfInput = pow(2, stoi(sizeInput.substr(3)));
        } else {
            printf("\n The size of the input array must be in the format \'2^x\'.");
            return 0;
        }

        // get type of input array
        std::string inputType = argv[0];
        if (algorithm != "sorted") && (algorithm != "random") && (algorithm != "reverse") && (algorithm != "perturbed") {
            printf("\n The input type must be \'sorted\', \'random\', \'reverse\', or \'perturbed\'.");
            return 0;
        }
    } else {
        printf("\n Please provide the the algorithm to use, the size of the input array, the type of input array and the number of processes all as command line arguments.");
        return 0;
    }

    double input[sizeOfInput];
}
