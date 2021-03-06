#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <time.h>
#include <vector>

#include "rdmc/rdmc.h"
#include "rdmc/util.h"
#include "block_size.h"
#include "aggregate_bandwidth.h"
#include "log_results.h"

#include "rdmc/rdmc.h"

using std::vector;

int main(int argc, char *argv[]) {
    uint32_t node_rank;
    uint32_t num_nodes;

    std::map<uint32_t, std::string> node_addresses;

    rdmc::query_addresses(node_addresses, node_rank);
    num_nodes = node_addresses.size();

    // initialize RDMA resources, input number of nodes, node rank and ip addresses and create TCP connections
    bool success = rdmc::initialize(node_addresses, node_rank);
    if(!success) {
        std::cout << "Failed RDMC initialization" << std::endl;
        std::cout << "Exiting" << std::endl;
    }
    // size of one message
    long long int msg_size = atoll(argv[1]);
    // set block size
    const size_t block_size = get_block_size(msg_size);
    // size of the buffer
    const size_t buffer_size = msg_size;

    // create the vector of members - node 0 is the sender
    vector<uint32_t> members(num_nodes);
    for(uint32_t i = 0; i < num_nodes; i++) {
        members[i] = i;
    }

    int count = 0;
    int num_messages = 1000;
    if(msg_size == 100000000) {
        std::cout << "Setting number of messages to 100 for this case"
                  << std::endl;
        num_messages = 100;
    }
    // type of send algorithm
    rdmc::send_algorithm type = rdmc::BINOMIAL_SEND;

    // buffer for the message - received here by the receivers and generated
    // here by the sender
    std::unique_ptr<char[]> buffer(new char[buffer_size]);
    auto mr = std::make_shared<rdma::memory_region>(buffer.get(), buffer_size);

    // create the group
    success = rdmc::create_group(0, members, block_size, type,
                                 [&mr](size_t length) -> rdmc::receive_destination {
                           return {mr, 0};
                                 },
                                 [&count](char *data, size_t size) { ++count; },
                                 [](std::experimental::optional<uint32_t>) {});
    if(!success) {
        std::cout << "Failed RDMC group creation" << std::endl;
        std::cout << "Exiting" << std::endl;
    }

    struct timespec start_time, end_time;
    clock_gettime(CLOCK_REALTIME, &start_time);
    if(node_rank == 0) {
        for(int i = 0; i < num_messages; ++i) {
            // send the message
            success = rdmc::send(node_rank, mr, 0, msg_size);
            while(count <= i) {
            }
        }
    } else {
        while(count <= num_messages - 1) {
        }
    }
    clock_gettime(CLOCK_REALTIME, &end_time);
    long long int nanoseconds_elapsed =
        (end_time.tv_sec - start_time.tv_sec) * (long long int)1e9 +
        (end_time.tv_nsec - start_time.tv_nsec);
    double bw = (msg_size * num_messages + 0.0) / nanoseconds_elapsed;
    double avg_bw = aggregate_bandwidth(members, node_rank, bw);
    log_results(num_nodes, 2, msg_size, avg_bw, "data_rdmc_bw");
}
