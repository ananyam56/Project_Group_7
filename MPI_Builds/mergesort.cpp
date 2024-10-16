#include <mpi.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <string>
#include "generate_data.h"
#include <ctime>
#include <caliper/cali.h>
#include <adiak.hpp>

// Function to perform sequential merge sort
std::vector<int> merge_sort(const std::vector<int>& data) {
    if (data.size() <= 1)
        return data;

    size_t mid = data.size() / 2;
    std::vector<int> left(data.begin(), data.begin() + mid);
    std::vector<int> right(data.begin() + mid, data.end());

    left = merge_sort(left);
    right = merge_sort(right);

    std::vector<int> merged;
    size_t i = 0, j = 0;

    while (i < left.size() && j < right.size()) {
        if (left[i] <= right[j]) {
            merged.push_back(left[i]);
            i++;
        } else {
            merged.push_back(right[j]);
            j++;
        }
    }

    while (i < left.size()) {
        merged.push_back(left[i]);
        i++;
    }
    while (j < right.size()) {
        merged.push_back(right[j]);
        j++;
    }

    return merged;
}

// Parallel merge function on the root process (rank 0)
std::vector<int> parallel_merge(std::vector<int>& data, int num_procs) {
    int step = 1;
    while (step < num_procs) {
        for (int i = 0; i < num_procs; i += 2 * step) {
            if (i + step < num_procs) {
                // Merge two adjacent chunks
                std::vector<int> left(data.begin() + i * data.size() / num_procs,
                                      data.begin() + (i + step) * data.size() / num_procs);
                std::vector<int> right(data.begin() + (i + step) * data.size() / num_procs,
                                       data.begin() + std::min((i + 2 * step) * data.size() / num_procs, data.size()));
                std::vector<int> merged;
                std::merge(left.begin(), left.end(), right.begin(), right.end(), std::back_inserter(merged));

                // Copy merged result back into the data array
                std::copy(merged.begin(), merged.end(), data.begin() + i * data.size() / num_procs);
            }
        }
        step *= 2;
    }
    return data;
}

int main(int argc, char** argv) {
    CALI_MARK_BEGIN("main");
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Adiak metadata collection
    adiak::init(NULL);
    std::string algorithm = "merge";
    std::string programming_model = "mpi";
    std::string data_type = "int";
    int size_of_data_type = sizeof(int);
    std::string input_type = "random";
    int num_procs = size;

    adiak::value("algorithm", algorithm);
    adiak::value("programming_model", programming_model);
    adiak::value("data_type", data_type);
    adiak::value("size_of_data_type", size_of_data_type);
    adiak::value("input_type", input_type);
    adiak::value("num_procs", num_procs);

    int total_size = 1 << 20;

    // Root handles argument parsing
    if (rank == 0) {
        if (argc >= 2) total_size = std::stoi(argv[1]);
        if (argc >= 3) input_type = argv[2];
    }

    // Broadcast total size and input type
    MPI_Bcast(&total_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
    int input_type_length = 0;
    if (rank == 0) input_type_length = input_type.length();
    MPI_Bcast(&input_type_length, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (rank != 0) input_type.resize(input_type_length);
    MPI_Bcast(&input_type[0], input_type_length, MPI_CHAR, 0, MPI_COMM_WORLD);

    int chunk_size = total_size / size;
    std::vector<int> local_chunk(chunk_size);

    // Data initialization at root
    std::vector<int> data;
    if (rank == 0) {
        CALI_MARK_BEGIN("data_init_runtime");
        if (total_size % size != 0) {
            total_size = (total_size / size) * size;
        }

        if (input_type == "random") {
            data = generate_random_data(total_size);
        } else if (input_type == "sorted") {
            data = generate_sorted_data(total_size);
        } else if (input_type == "reverse") {
            data = generate_reverse_sorted_data(total_size);
        } else if (input_type == "perturbed") {
            data = generate_perturbed_sorted_data(total_size);
        }
        CALI_MARK_END("data_init_runtime");
    }

    // ** Adding MPI_Comm_dup to better match the example **
    MPI_Comm new_comm;
    CALI_MARK_BEGIN("comm");
    MPI_Comm_dup(MPI_COMM_WORLD, &new_comm);
    CALI_MARK_END("comm");

    // Barrier before starting communication
    CALI_MARK_BEGIN("comm");
    MPI_Barrier(new_comm);  // Synchronize all processes
    CALI_MARK_END("comm");

    // Start timing after data generation
    double start_time = MPI_Wtime();

    // Scatter the data
    CALI_MARK_BEGIN("comm");
    CALI_MARK_BEGIN("comm_large");
    MPI_Scatter(data.data(), chunk_size, MPI_INT, local_chunk.data(), chunk_size, MPI_INT, 0, MPI_COMM_WORLD);
    CALI_MARK_END("comm_large");
    CALI_MARK_END("comm");

    // Perform local sort
    CALI_MARK_BEGIN("comp");
    CALI_MARK_BEGIN("comp_large");
    local_chunk = merge_sort(local_chunk);
    CALI_MARK_END("comp_large");
    CALI_MARK_END("comp");

    // Gather sorted chunks back
    std::vector<int> sorted_data(total_size);
    CALI_MARK_BEGIN("comm");
    CALI_MARK_BEGIN("comm_large");
    MPI_Gather(local_chunk.data(), chunk_size, MPI_INT, sorted_data.data(), chunk_size, MPI_INT, 0, MPI_COMM_WORLD);
    CALI_MARK_END("comm_large");
    CALI_MARK_END("comm");

    // Parallel merge on root process
    if (rank == 0) {
        CALI_MARK_BEGIN("comp");
        CALI_MARK_BEGIN("comp_large");
        sorted_data = parallel_merge(sorted_data, size);  // Use parallel_merge instead of sequential merge_sort
        CALI_MARK_END("comp_large");
        CALI_MARK_END("comp");

        double end_time = MPI_Wtime();
        double execution_time = end_time - start_time;
        std::cout << "Execution Time: " << execution_time << " seconds." << std::endl;

        // Verify sorting correctness
        CALI_MARK_BEGIN("correctness_check");
        bool is_sorted_flag = std::is_sorted(sorted_data.begin(), sorted_data.end());
        if (is_sorted_flag) {
            std::cout << "Array is sorted correctly." << std::endl;
        } else {
            std::cout << "Array is NOT sorted correctly." << std::endl;
        }
        CALI_MARK_END("correctness_check");
    }

    MPI_Finalize();
    CALI_MARK_END("main");
    return 0;
}

