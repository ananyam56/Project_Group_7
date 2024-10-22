#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <algorithm>
#include <numeric>
#include <math.h>
#include <caliper/cali.h>
#include <caliper/cali-manager.h>
#include <adiak.hpp>
#include <string>

bool correctness_check(double* data, int size) {
    for (int i = 1; i < size; i++) {
        if (data[i - 1] > data[i]) {
            return false;
        }
    }
    return true;
}

int main(int argc, char *argv[]) {
    // Initialize MPI
    CALI_MARK_BEGIN("main");
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // // Time variables
    // double start_time, end_time, local_time, global_min_time, global_max_time, total_time, avg_time, variance, global_sum_time, local_sum_sq, global_sum_sq;

    // // Start timer
    // start_time = MPI_Wtime();  // Record start time for each rank

    if (argc != 4) {
        if (rank == 0) {
            printf("\nPlease provide the algorithm, input size, and input type as command line arguments.\n");
        }
        MPI_Abort(MPI_COMM_WORLD, 1);
        return 0;
    }

    std::string algorithm = argv[1];
    int sizeOfInput =  pow(2, atoi(argv[2]));
    std::string input_type = argv[3];

    // Set up Adiak 
    adiak::init(NULL);
    adiak::launchdate();
    adiak::libraries();
    adiak::cmdline();
    adiak::clustername();
    adiak::value("algorithm", algorithm);
    adiak::value("programming_model", "mpi");
    adiak::value("data_type", "double");
    adiak::value("size_of_data_type", sizeof(double));
    adiak::value("input_size", sizeOfInput);
    adiak::value("input_type", input_type);
    adiak::value("num_procs", size);
    adiak::value("scalability", "strong");  // or "weak" depending on the experiment
    adiak::value("group_num", 7); 
    adiak::value("implementation_source", "online");

    // Data initialization
    int local_size = sizeOfInput / size;
    int remainder = sizeOfInput % size;
    if (rank < remainder) {
        local_size++;
    }
    double* local_data = new double[local_size];

    if (rank == 0) {
        CALI_MARK_BEGIN("data_init_runtime");
        double* full_data = new double[sizeOfInput];
        for (int i = 0; i < sizeOfInput; i++) {
            if (input_type == "random") {
                full_data[i] = rand() % 10000;
            } else if (input_type == "sorted" || input_type == "perturbed" ) {
                full_data[i] = i;
            } else if (input_type == "reverse") {
                full_data[i] = sizeOfInput - i;
            }
        }
        if (input_type == "perturbed") {
            int num_perturbed = sizeOfInput / 100;  // 1% of the elements
            for (int i = 0; i < num_perturbed; i++) {
                // Choose two random indices to swap
                int idx1 = rand() % sizeOfInput;
                int idx2 = rand() % sizeOfInput;
                std::swap(full_data[idx1], full_data[idx2]);
            }
        }

        int* send_counts = new int[size];
        int* displs = new int[size];
        int offset = 0;
        for (int i = 0; i < size; i++) {
            send_counts[i] = sizeOfInput / size + (i < remainder ? 1 : 0);
            displs[i] = offset;
            offset += send_counts[i];
        }

        CALI_MARK_END("data_init_runtime");

        CALI_MARK_BEGIN("comm");
        CALI_MARK_BEGIN("comm_large");
        MPI_Scatterv(full_data, send_counts, displs, MPI_DOUBLE, local_data, local_size, MPI_DOUBLE, 0, MPI_COMM_WORLD);
        CALI_MARK_END("comm_large");
        CALI_MARK_END("comm");

        delete[] full_data;
        delete[] send_counts;
        delete[] displs;

    } else {
        CALI_MARK_BEGIN("comm");
        CALI_MARK_BEGIN("comm_large");
        MPI_Scatterv(NULL, NULL, NULL, MPI_DOUBLE, local_data, local_size, MPI_DOUBLE, 0, MPI_COMM_WORLD);
        CALI_MARK_END("comm_large");
        CALI_MARK_END("comm");
    }

    //Local sorting
    CALI_MARK_BEGIN("comp");
    CALI_MARK_BEGIN("comp_large");  
    std::sort(local_data, local_data + local_size);
    CALI_MARK_END("comp_large");
    CALI_MARK_END("comp");

    //Local splitters and communication
    CALI_MARK_BEGIN("comm");  
    CALI_MARK_BEGIN("comm_small");
    int num_samples = size - 1;
    std::vector<double> local_splitters(num_samples);
    for (int i = 0; i < num_samples; i++) {
        local_splitters[i] = local_data[(i + 1) * local_size / size];
    }

    // Gather local splitters at root
    std::vector<double> all_splitters;
    if (rank == 0) {
        all_splitters.resize(num_samples * size);
    }

    MPI_Gather(local_splitters.data(), num_samples, MPI_DOUBLE, all_splitters.data(), num_samples, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    CALI_MARK_END("comm_small");  
    CALI_MARK_END("comm");

    // Get global splitters at root and broadcast
    CALI_MARK_BEGIN("comp");
    std::vector<double> global_splitters(num_samples);
    if (rank == 0) {
        CALI_MARK_BEGIN("comp_small");  // sorting splitters
        std::sort(all_splitters.begin(), all_splitters.end());
        for (int i = 0; i < num_samples; i++) {
            global_splitters[i] = all_splitters[(i + 1) * size - 1];
        }
        CALI_MARK_END("comp_small");
    }
    CALI_MARK_END("comp");

    CALI_MARK_BEGIN("comm");
    CALI_MARK_BEGIN("comm_small"); 
    MPI_Bcast(global_splitters.data(), num_samples, MPI_DOUBLE, 0, MPI_COMM_WORLD); // broadcasting splitters
    CALI_MARK_END("comm_small");

    // Redistribute data based on global splitters
    std::vector<int> send_counts(size, 0);
    std::vector<int> send_offsets(size, 0);
    std::vector<int> recv_counts(size, 0);
    std::vector<int> recv_offsets(size, 0);

    //determine target process 
    for (int i = 0; i < local_size; i++) {
        int target_proc = std::upper_bound(global_splitters.begin(), global_splitters.end(), local_data[i]) - global_splitters.begin();
        send_counts[target_proc]++;
    }

    // Getting send counts and receive counts
    CALI_MARK_BEGIN("comm_small");
    MPI_Alltoall(send_counts.data(), 1, MPI_INT, recv_counts.data(), 1, MPI_INT, MPI_COMM_WORLD);
    CALI_MARK_END("comm_small");

    int total_recv_size = std::accumulate(recv_counts.begin(), recv_counts.end(), 0);
    std::vector<double> recv_data(total_recv_size);

    for (int i = 1; i < size; i++) {
        send_offsets[i] = send_offsets[i - 1] + send_counts[i - 1];
        recv_offsets[i] = recv_offsets[i - 1] + recv_counts[i - 1];
    }

    CALI_MARK_BEGIN("comm_large");
    MPI_Alltoallv(local_data, send_counts.data(), send_offsets.data(), MPI_DOUBLE,
                recv_data.data(), recv_counts.data(), recv_offsets.data(), MPI_DOUBLE, MPI_COMM_WORLD);
    CALI_MARK_END("comm_large");
    CALI_MARK_END("comm");

    // Sort received data
    CALI_MARK_BEGIN("comp");
    CALI_MARK_BEGIN("comp_large");
    std::sort(recv_data.begin(), recv_data.end());
    CALI_MARK_END("comp_large");
    CALI_MARK_END("comp");

    // Gather final data at root for correctness check
    if (rank == 0) {
        std::vector<int> recv_counts_root(size);
        std::vector<int> displs_root(size);
        int total_elements = 0;
        int offset = 0;

        CALI_MARK_BEGIN("comm");
        CALI_MARK_BEGIN("comm_small");
        MPI_Gather(&total_recv_size, 1, MPI_INT, recv_counts_root.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);
        CALI_MARK_END("comm_small");

        for (int i = 0; i < size; i++) {
            displs_root[i] = offset;
            offset += recv_counts_root[i];
            total_elements += recv_counts_root[i];
        }

        std::vector<double> full_data(total_elements);

        CALI_MARK_BEGIN("comm_large");
        MPI_Gatherv(recv_data.data(), total_recv_size, MPI_DOUBLE, full_data.data(), recv_counts_root.data(), displs_root.data(), MPI_DOUBLE, 0, MPI_COMM_WORLD);
        CALI_MARK_END("comm_large");
        CALI_MARK_END("comm");

        CALI_MARK_BEGIN("correctness_check");
        if (correctness_check(full_data.data(), total_elements)) {
            printf("Data is sorted correctly.\n");
        } else {
            printf("Data is not sorted correctly.\n");
        }
        CALI_MARK_END("correctness_check");
    } else {
        CALI_MARK_BEGIN("comm");
        CALI_MARK_BEGIN("comm_small");
        MPI_Gather(&total_recv_size, 1, MPI_INT, nullptr, 1, MPI_INT, 0, MPI_COMM_WORLD);
        CALI_MARK_END("comm_small");

        CALI_MARK_BEGIN("comm_large");
        MPI_Gatherv(recv_data.data(), total_recv_size, MPI_DOUBLE, nullptr, nullptr, nullptr, MPI_DOUBLE, 0, MPI_COMM_WORLD);
        CALI_MARK_END("comm_large");
        CALI_MARK_END("comm");
    }

    // // End timer and calculate local time
    // end_time = MPI_Wtime();  // Record end time
    // local_time = end_time - start_time;  // Time spent by this rank

    // // Record the min time/rank across all ranks
    // MPI_Reduce(&local_time, &global_min_time, 1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);

    // // Record the max time/rank across all ranks
    // MPI_Reduce(&local_time, &global_max_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    // // Sum all local times to calculate the average time/rank and total time
    // MPI_Reduce(&local_time, &global_sum_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    // avg_time = global_sum_time / size;  // Only root will calculate the average

    // // Calculate variance: sum of squares of differences from the average
    // local_sum_sq = (local_time - avg_time) * (local_time - avg_time);
    // MPI_Reduce(&local_sum_sq, &global_sum_sq, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    // variance = global_sum_sq / size;  // Only root will calculate variance

    // // Root process prints the performance metrics
    // if (rank == 0) {
    //     printf("Min time/rank: %f\n", global_min_time);
    //     printf("Max time/rank: %f\n", global_max_time);
    //     printf("Avg time/rank: %f\n", avg_time);
    //     printf("Total time: %f\n", global_sum_time);
    //     printf("Variance in time/rank: %f\n", variance);
    // }

    // Finalize MPI and clean up
    delete[] local_data;
    MPI_Finalize();
    CALI_MARK_END("main");
    return 0;
}
