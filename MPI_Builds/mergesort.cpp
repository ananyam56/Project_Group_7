#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <algorithm>
#include <numeric>
#include <math.h>
#include <caliper/cali.h>
#include <adiak.hpp>
#include <string>
#include <climits>

// Function to check if the array is sorted correctly
bool correctness_check(int* data, int size) {
    for (int i = 1; i < size; i++) {
        if (data[i - 1] > data[i]) {
            return false;
        }
    }
    return true;
}

// Function to merge multiple sorted chunks into a single sorted array
std::vector<int> global_merge(int* full_data, int* recv_counts, int* displs, int size, int total_elements) {
    std::vector<int> merged_data(total_elements);
    std::vector<int*> chunk_ptrs(size);
    std::vector<int> chunk_sizes(size);

    // Set pointers and sizes for each chunk
    for (int i = 0; i < size; ++i) {
        chunk_ptrs[i] = &full_data[displs[i]];
        chunk_sizes[i] = recv_counts[i];
    }

    // Merge the chunks using a k-way merge algorithm
    for (int i = 0; i < total_elements; ++i) {
        int min_value = INT_MAX;
        int min_index = -1;
        for (int j = 0; j < size; ++j) {
            if (chunk_sizes[j] > 0 && *chunk_ptrs[j] < min_value) {
                min_value = *chunk_ptrs[j];
                min_index = j;
            }
        }
        if (min_index != -1) {
            merged_data[i] = *chunk_ptrs[min_index];
            chunk_ptrs[min_index]++;
            chunk_sizes[min_index]--;
        }
    }

    return merged_data;
}

int main(int argc, char *argv[]) {
    // Initialize MPI and Caliper
    CALI_MARK_BEGIN("main");
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Ensure proper number of arguments
    if (argc != 4) {
        if (rank == 0) {
            printf("\nUsage: mpirun -np <processes> ./mergesort <algorithm> <array_size> <input_type>\n");
        }
        MPI_Abort(MPI_COMM_WORLD, 1);
        return 0;
    }

    // Parse command-line arguments
    std::string algorithm = argv[1];
    int array_size = atoi(argv[2]);
    std::string input_type = argv[3];

    // Adiak metadata collection
    adiak::init(NULL);
    adiak::value("algorithm", algorithm);
    adiak::value("programming_model", "mpi");
    adiak::value("data_type", "int");
    adiak::value("input_size", array_size);
    adiak::value("input_type", input_type);
    adiak::value("num_procs", size);

    // Determine local array size for each process
    int local_size = array_size / size;
    int remainder = array_size % size;
    if (rank < remainder) {
        local_size++;
    }
    int* local_data = new int[local_size];

    // Root process initializes data
    if (rank == 0) {
        CALI_MARK_BEGIN("data_init_runtime");
        double* full_data = new double[array_size];
        for (int i = 0; i < array_size; i++) {
            if (input_type == "random") {
                full_data[i] = rand() % 10000;
            } else if (input_type == "sorted" || input_type == "perturbed" ) {
                full_data[i] = i;
            } else if (input_type == "reverse") {
                full_data[i] = array_size - i;
            }
        }
        if (input_type == "perturbed") {
            int num_perturbed = array_size / 100;  // 1% of the elements
            for (int i = 0; i < num_perturbed; i++) {
                // Choose two random indices to swap
                int idx1 = rand() % array_size;
                int idx2 = rand() % array_size;
                std::swap(full_data[idx1], full_data[idx2]);
            }
        }

        // Prepare data distribution (scatter)
        int* send_counts = new int[size];
        int* displs = new int[size];
        int offset = 0;
        for (int i = 0; i < size; i++) {
            send_counts[i] = array_size / size + (i < remainder ? 1 : 0);
            displs[i] = offset;
            offset += send_counts[i];
        }

        // Scatter the data
        CALI_MARK_BEGIN("comm");
        CALI_MARK_BEGIN("comm_large");  // Mark communication of large data
        MPI_Scatterv(full_data, send_counts, displs, MPI_INT, local_data, local_size, MPI_INT, 0, MPI_COMM_WORLD);
        CALI_MARK_END("comm_large");
        CALI_MARK_END("comm");

        delete[] full_data;
        delete[] send_counts;
        delete[] displs;

        CALI_MARK_END("data_init_runtime");
    } else {
        // Other processes just receive data
        CALI_MARK_BEGIN("comm");
        CALI_MARK_BEGIN("comm_large");  // Mark communication of large data
        MPI_Scatterv(NULL, NULL, NULL, MPI_INT, local_data, local_size, MPI_INT, 0, MPI_COMM_WORLD);
        CALI_MARK_END("comm_large");
        CALI_MARK_END("comm");
    }

    // Local sorting
    CALI_MARK_BEGIN("comp");
    CALI_MARK_BEGIN("comp_large");  // Mark computation on large data
    std::sort(local_data, local_data + local_size);
    CALI_MARK_END("comp_large");
    CALI_MARK_END("comp");

    // Gather sorted data at root
    if (rank == 0) {
        int* recv_counts = new int[size];
        int* displs = new int[size];
        int total_elements = 0;
        int offset = 0;

        // Gather counts and displacements for root process
        MPI_Gather(&local_size, 1, MPI_INT, recv_counts, 1, MPI_INT, 0, MPI_COMM_WORLD);

        for (int i = 0; i < size; i++) {
            displs[i] = offset;
            offset += recv_counts[i];
            total_elements += recv_counts[i];
        }

        int* full_data = new int[total_elements];

        // Gather sorted data
        CALI_MARK_BEGIN("comm");
        CALI_MARK_BEGIN("comm_large");  // Mark communication of large data
        MPI_Gatherv(local_data, local_size, MPI_INT, full_data, recv_counts, displs, MPI_INT, 0, MPI_COMM_WORLD);
        CALI_MARK_END("comm_large");
        CALI_MARK_END("comm");

        // Perform global merge of sorted chunks
        std::vector<int> merged_data = global_merge(full_data, recv_counts, displs, size, total_elements);

        // Check correctness
        CALI_MARK_BEGIN("correctness_check");
        if (correctness_check(merged_data.data(), total_elements)) {
            printf("Data is sorted correctly.\n");
        } else {
            printf("Data is not sorted correctly.\n");
        }
        CALI_MARK_END("correctness_check");

        delete[] full_data;
        delete[] recv_counts;
        delete[] displs;
    } else {
        // Non-root processes still perform communication
        MPI_Gather(&local_size, 1, MPI_INT, nullptr, 1, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Gatherv(local_data, local_size, MPI_INT, nullptr, nullptr, nullptr, MPI_INT, 0, MPI_COMM_WORLD);
    }

    // Clean up and finalize
    delete[] local_data;
    MPI_Finalize();
    CALI_MARK_END("main");

    return 0;
}
