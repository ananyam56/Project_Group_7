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
#include <cstring>

// Helper function to get the digit value for radix sort
int get_digit(int value, int exp) {
    return ((int)value / exp) % 10;
}

// Check if the array is sorted
bool correctness_check(int* data, int size) {
    for (int i = 1; i < size; i++) {
        if (data[i - 1] > data[i]) {
            return false;
        }
    }
    return true;
}

// Local radix sort using counting sort for each digit
void radix_sort(int* data, int size) {
    int max_val = *std::max_element(data, data + size);

    // Perform counting sort for each digit
    for (int exp = 1; max_val / exp > 0; exp *= 10) {
        std::vector<int> count(10, 0);
        std::vector<int> output(size);

        for (int i = 0; i < size; i++) {
            int digit = get_digit(data[i], exp);
            count[digit]++;
        }

        for (int i = 1; i < 10; i++) {
            count[i] += count[i - 1];
        }

        for (int i = size - 1; i >= 0; i--) {
            int digit = get_digit(data[i], exp);
            output[count[digit] - 1] = data[i];
            count[digit]--;
        }

        std::memcpy(data, output.data(), size * sizeof(int));
    }
}

// Merge two sorted arrays into one sorted array
void merge(int* left, int left_size, int* right, int right_size, int* result) {
    int i = 0, j = 0, k = 0;

    // Merge the two sorted arrays
    while (i < left_size && j < right_size) {
        if (left[i] < right[j]) {
            result[k++] = left[i++];
        } else {
            result[k++] = right[j++];
        }
    }

    // Copy remaining elements of left[]
    while (i < left_size) {
        result[k++] = left[i++];
    }

    // Copy remaining elements of right[]
    while (j < right_size) {
        result[k++] = right[j++];
    }
}


int main(int argc, char *argv[]) {
    CALI_MARK_BEGIN("main");
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc != 4) {
        if (rank == 0) {
            printf("Please provide the algorithm, input size (power of 2), and input type as command line arguments.\n");
        }
        MPI_Abort(MPI_COMM_WORLD, 1);
        return 0;
    }

    std::string algorithm = argv[1];
    int sizeOfInput = atoi(argv[2]);
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
    int* local_data = new int[local_size];

    if (rank == 0) {
        // Root process initializes data
        CALI_MARK_BEGIN("data_init_runtime");
        int* full_data = new int[sizeOfInput];
        for (int i = 0; i < sizeOfInput; i++) {
            if (input_type == "random") {
                full_data[i] = rand() % 10000;
            } else if (input_type == "sorted") {
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
        int* displacement = new int[size];
        int offset = 0;
        for (int i = 0; i < size; i++) {
            send_counts[i] = sizeOfInput / size + (i < remainder ? 1 : 0);
            displacement[i] = offset;
            offset += send_counts[i];
        }

        CALI_MARK_END("data_init_runtime");

        // Scatter data
        CALI_MARK_BEGIN("comm");
        MPI_Scatterv(full_data, send_counts, displacement, MPI_INT, local_data, local_size, MPI_INT, 0, MPI_COMM_WORLD);
        CALI_MARK_END("comm");

        delete[] full_data;
        delete[] send_counts;
        delete[] displacement;

    } else {
        // Non-root processes receive data
        CALI_MARK_BEGIN("comm");
        MPI_Scatterv(NULL, NULL, NULL, MPI_INT, local_data, local_size, MPI_INT, 0, MPI_COMM_WORLD);
        CALI_MARK_END("comm");
    }

    // Local radix sort
    CALI_MARK_BEGIN("comp");
    CALI_MARK_BEGIN("comp_large"); 
    radix_sort(local_data, local_size);
    CALI_MARK_END("comp_large");
    CALI_MARK_END("comp");

    // Gather the sorted segments back to the root
    CALI_MARK_BEGIN("comm");
    int* sorted_data = nullptr;
    if (rank == 0) {
        sorted_data = new int[sizeOfInput];
    }

    // Gather sorted local arrays to root
    int* recv_counts = new int[size];
    MPI_Gather(&local_size, 1, MPI_INT, recv_counts, 1, MPI_INT, 0, MPI_COMM_WORLD);
    CALI_MARK_END("comm");

    // Calculate displacements for gatherv
    CALI_MARK_BEGIN("comp");
    CALI_MARK_BEGIN("comp_small");
    int* recv_displacement = new int[size];
    if (rank == 0) {
        recv_displacement[0] = 0;
        for (int i = 1; i < size; i++) {
            recv_displacement[i] = recv_displacement[i - 1] + recv_counts[i - 1];
        }
    }
    CALI_MARK_END("comp_small");
    CALI_MARK_END("comp");

    CALI_MARK_BEGIN("comm");
    // Gather sorted local arrays to root
    MPI_Gatherv(local_data, local_size, MPI_INT, sorted_data, recv_counts, recv_displacement, MPI_INT, 0, MPI_COMM_WORLD);
    CALI_MARK_END("comm");

    // Merge sorted data in the root process
    if (rank == 0) {
        int* final_sorted_data = new int[sizeOfInput];

        // sorted_data contains sorted segments from all processes
        int current_size = 0; // Size of the current merged array

        CALI_MARK_BEGIN("comp");
        CALI_MARK_BEGIN("comp_large");
        // Merge each segment into final_sorted_data
        for (int i = 0; i < size; i++) {
            int segment_size = recv_counts[i]; // Size of the i-th segment
            int* segment_data = sorted_data + recv_displacement[i]; // Pointer to the i-th segment
            
            // Merge the current segment with the already merged data
            merge(final_sorted_data, current_size, segment_data, segment_size, final_sorted_data);
            current_size += segment_size; // Update the size of the merged array
        }
        CALI_MARK_END("comp_large");
        CALI_MARK_END("comp");

        // Verify correctness
        CALI_MARK_BEGIN("correctness_check");
        if (correctness_check(final_sorted_data, sizeOfInput)) {
            printf("Data is sorted correctly!\n");
        } else {
            printf("Data is NOT sorted correctly!\n");
        }
        CALI_MARK_END("correctness_check");

        delete[] final_sorted_data;
        delete[] sorted_data;
    }

    // Clean up
    delete[] local_data;
    delete[] recv_counts;
    delete[] recv_displacement;

    // Finalize MPI
    MPI_Finalize();
    CALI_MARK_END("main");
    return 0;
}
