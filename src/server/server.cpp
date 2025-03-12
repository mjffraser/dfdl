//////declare stuff std::
#include <iostream>
#include <thread>

//imports from project dfd::
#include "sourceInfo.hpp"
#include "networking/socket.hpp"
#include "server/internal/database/db.hpp"
#include "server/internal/database/internal/types.hpp"
#include "server/internal/database/internal/queries.hpp"
#include "server/internal/database/internal/tableInfo.hpp"
#include "networking/messageFormatting.hpp"

//other imports std::
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace dfd{

//mutex for db
std::mutex dbMutex;
Database* db = nullptr;  //global shared database instance

//structure to store each job
//used to make offshoot threads to handle a given job
struct Job {
    //file descriptor of the client
    int cfd;
    //message sent by the client
    std::vector<uint8_t> message;
};

//queue to hold pending jobs and its mutex
std::queue<Job> jobQ;
std::mutex jobMutex;

//signals worker threads that a job is avalable
std::condition_variable jobReady;
//atomic flag to control main server loop (atomic prevents need of mutex)
std::atomic<bool> serverRunning(true);



//these threads are spawned on each accepted connection and handle said connection
void handleConnectionThread(int client_fd) {
    //2.5 sec time out (changable in future) timeval is a structure explained here to set to differing times: http://www.ccplusplus.com/2011/09/struct-timeval-in-c.html (website is unsecure btw)
    struct timeval timeout;
    timeout.tv_sec  = 5;
    timeout.tv_usec = 0;
    //buffer
    std::vector<uint8_t> buffer;

    //store the recved msg by calling it with the above time out n buffer
    ssize_t readToCheck = recvMessage(client_fd, buffer, timeout);
    //check if recv failed
    if (readToCheck <= 0) {
        std::cerr << "no message or timeout";
        closeSocket(client_fd);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(jobMutex);
        //create a job on the worker queue with this connections file descriptor and buffer containing msg
        jobQ.push({client_fd, buffer});
    }
    
    //notify workers that a job is avalable
    jobReady.notify_one();
}


//worker thread, needs message handelling
void workerThread() {
    while (serverRunning) {
        //the job this thread is working on
        Job job;
        //mutex lock block (mutex for job q)
        {
            std::unique_lock<std::mutex> lock(jobMutex);
            
            //wait for job q to have a job or shutdown
            jobReady.wait(lock, [] { return !jobQ.empty() || !serverRunning; });
            //check if shutdown case broke the wait (aka server ! running) and break
            if (!serverRunning) {
                break;
            } else {
                //store job in from job Q
                job = jobQ.front();
                //remove job in from job Q
                jobQ.pop();
            }
        }


        /////////////////////MESSAGE HANDELLING/////////////////
        //get the message to handle
        std::vector<uint8_t> initial = job.message;
        std::uint8_t& decisionByte = initial.front();
        //create responce (change default to failure)
        std::vector<uint8_t> response;
        //lock db mutex (delete if read/write leader election happens)
        {
            std::lock_guard<std::mutex> lock(dbMutex);
            if (decisionByte == INDEX_REQUEST){
                auto fileID = parseIndexRequest(initial);
                //temp to hold exit code
                auto tempHold = db->indexFile(fileID.uuid, fileID.indexer, fileID.f_size);
                //check if operation had an EXIT_FAILURE and if not make responce the proper OK
                if (tempHold == EXIT_FAILURE) {
                    response = createFailMessage(db->sqliteError());
                } else {
                    response.push_back(INDEX_OK);
                }
       
            } else if (decisionByte == DROP_REQUEST){
                auto uidPair = parseDropRequest(initial);
                //fit the secound thing in pair into the structure
                SourceInfo structForDropIndex; 
                structForDropIndex.peer_id = uidPair.second;

                //temp to hold exit code
                auto tempHold = db->dropIndex(uidPair.first, structForDropIndex);
                //check if operation had an EXIT_FAILURE and if not make responce the proper OK
                if (tempHold == EXIT_FAILURE){
                    response = createFailMessage(db->sqliteError());
                }else{
                    response.push_back(DROP_OK);
                }
       
            } else if (decisionByte == REREGISTER_REQUEST){
                auto sourceInfo = parseReregisterRequest(initial);
                //temp to hold exit code
                auto tempHold = db->updateClient(sourceInfo);
                //check if operation had an EXIT_FAILURE and if not make responce the proper OK
                if (tempHold == EXIT_FAILURE){
                    response = createFailMessage(db->sqliteError());
                }else{
                    response.push_back(REREGISTER_OK);
                }

            } else if (decisionByte == SOURCE_REQUEST){
                auto fileUuid = parseSourceRequest(initial);
                std::vector<dfd::SourceInfo> buffer; 
                //temp to hold exit code
                auto tempHold = db->grabSources(fileUuid, buffer);

                if (tempHold == EXIT_FAILURE) {
                    response = createFailMessage(db->sqliteError());
                } else {
                    response = createSourceList(buffer);
                }

            } else {
                //should never happen in standard run
                response = createFailMessage("invalid message sent");
            }
        }
        //send responce to client (response is wrong meessage handling will implement)
        sendMessage(job.cfd, response);

        //close the jobs socket (with client file directory)
        closeSocket(job.cfd);
    }
}

//main server front end that loks for connections and handles proccess
void socketThread(const uint16_t port) {
    auto socket = openSocket(true, port);
    if (!socket) {
        //handle fail to open
        std::cerr << "no socket for socket thread";
        return;
    }

    //server file descriptor
    int sfd = socket->first;
    std::cout << "listening on port " << socket->second << "\n\n";

    //start listening with backlog of 10 (number is unimportant)
    if (listen(sfd, 10) == EXIT_FAILURE) {
        std::cerr << "listen failed\n";
        closeSocket(sfd);
        return;
    }

    while (serverRunning) {
        SourceInfo clientInfo;
        
        //accept new client connection
        int cfd = accept(sfd, clientInfo);
        
        if (cfd < 0) {
            std::cerr << "client unables to be accepted\n";
        } else {
            std::cout << "served client: " << clientInfo.ip_addr << ":" << clientInfo.port << "\n";
            //spawn a thread that handles the connection
            //keep in mind detached threads can be bad we may want a threadpool
            std::thread(handleConnectionThread, cfd).detach();
        }
        
    }
    
    //close up socket with api
    closeSocket(sfd);
}



///////main function
//setup db -> start server and worker threads -> close up
int mainServer(const uint16_t port) {
    //mutex lock db (may be unneeded in future)
    {
        std::lock_guard<std::mutex> lock(dbMutex);
        //create new db w api to global pointer
        db = new Database("name.db");
    }
    //output msg
    std::cout << "DB setup complete\n";

    //start the socket thread (will spawn handleConnectionThreads on each connection)
    std::thread socketT(socketThread, port);
    //vector of threads containing our workers
    std::vector<std::thread> workers;
    
    //create worker threads (currently 4 as example did, but number has no meaning)
    for (int i = 0; i < 4; ++i) {
        //push a new worker thread onto vetor i times
        workers.push_back(std::thread(workerThread));
    }

    ///////////CLEANUP///////////
    //wait for socket thread to finish and join it
    socketT.join();
    std::cout << "cleanup start\n";
    //signal workers to be done
    serverRunning = false;
    //notify every worker (since server running is ! this will break them all out of loop)
    jobReady.notify_all();
    
    // iterate through vector end all workers (after waiting for em to finish via join)
    for (int i = 0; i < workers.size(); i++) {
        workers[i].join();
    }

    //once again db mutex lock may be unneeded
    {
        std::lock_guard<std::mutex> lock(dbMutex);
        //clear db instance
        delete db;
        db = nullptr;
    }
    
    std::cout << "cleanup comple server shutdown\n";
    return 0;
}
}//dfd
