#pragma once

//declares
#include "networking/socket.hpp"
#include "server/internal/database/db.hpp"
#include "networking/messageFormatting.hpp"
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace dfd {

//db mutex
extern std::mutex dbMutex;
extern Database* db; // Global shared database instance

//job struct
struct Job {
    int cfd;                     // Client file descriptor
    std::vector<uint8_t> message; // Message received from client
};

//job q and stuff
extern std::queue<Job> jobQ;
extern std::mutex jobMutex;
extern std::condition_variable jobReady;
extern std::atomic<bool> serverRunning;

//functions
void handleConnectionThread(int client_fd);
void workerThread();
void socketThread();
int mainServer(const uint16_t port);

} //namespace dfd
