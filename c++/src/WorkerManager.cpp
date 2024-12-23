// Copyright (C) 2024 INAF
// This software is distributed under the terms of the BSD-3-Clause license
//
// Authors:
//
//    Andrea Bulgarelli <andrea.bulgarelli@inaf.it>
//
#include <atomic>
#include "WorkerManager.h"

// Constructor
WorkerManager::WorkerManager(int manager_id, Supervisor* supervisor, const std::string& name)
    : manager_id(manager_id), supervisor(supervisor), name(name), 
      status("Initialising"), continueall(true), processdata(0), stopdata(true), 
      _stop_event(false), context(supervisor->context) {
    
    // Initialize member variables from supervisor
    workersname = supervisor->name_workers[manager_id];
    config = std::make_shared<json>(supervisor-> config);
    logger = supervisor->logger;
    fullname = supervisor->name + "-" + name;
    globalname = "WorkerManager-" + fullname;
    processingtype = supervisor->processingtype;
    max_workers = 100;
    result_socket_type = supervisor->manager_result_sockets_type[manager_id];
    result_lp_socket = supervisor->manager_result_lp_sockets[manager_id];
    result_hp_socket = supervisor->manager_result_hp_sockets[manager_id];
    result_dataflow_type = supervisor->manager_result_dataflow_type[manager_id];
    socket_lp_result = supervisor->socket_lp_result;
    socket_hp_result = supervisor->socket_hp_result;
    pid = getpid();
    socket_monitoring = supervisor->socket_monitoring;
       
    /////////////////////////////////////////////
    low_priority_queue = std::make_shared<ThreadSafeQueue<std::vector<uint8_t>>>();
    high_priority_queue = std::make_shared<ThreadSafeQueue<std::vector<uint8_t>>>();
    result_lp_queue = std::make_shared<ThreadSafeQueue<std::vector<uint8_t>>>();
    result_hp_queue = std::make_shared<ThreadSafeQueue<std::vector<uint8_t>>>();
    
    // Initialize monitoring
    monitoringpoint = nullptr;
    monitoringthread = nullptr;
    //////////////////////////////////
    num_workers = supervisor->manager_num_workers;  // num_workers = 0;
    //////////////////////////////////
    workersstatus = 0;
    workersstatusinit = 0;

    tokenresultslock = std::make_shared<std::mutex>();
    tokenreadinglock = std::make_shared<std::mutex>();

    // Log the start of WorkerManager
    logger->info("Started", globalname);
    logger->info(fmt::format("Socket result parameters: {} / {} / {} / {}", result_socket_type, result_lp_socket, result_hp_socket, result_dataflow_type), globalname);

    status = "Initialised";
    supervisor->send_info(1, status, fullname, 1, "Low");
}

Supervisor* WorkerManager::getSupervisor() const {
    return supervisor;
}

std::string WorkerManager::getName() const {
    return name;
}

std::string WorkerManager::getFullname() const {
    return fullname;
}

std::string WorkerManager::getStatus() const {
    return status;
}

bool WorkerManager::getStopData() const {
    return stopdata;
}

int WorkerManager::getWorkersStatusInit() const {
    return workersstatusinit;
}

int WorkerManager::getWorkersStatus() const {
    return workersstatus;
}

int WorkerManager::getProcessDataSharedValue() const {
    return processdata_shared.load();  // Use .load() to get the value of the atomic variable
}

std::string WorkerManager::getWorkersName() const {
    return workersname;
}

std::string WorkerManager::getProcessingType() const {
    return processingtype;
}

 std::vector<std::shared_ptr<WorkerThread>> WorkerManager::getWorkerProcesses() {
    return workerprocesses;
}

std::vector<std::shared_ptr<WorkerProcess>> WorkerManager::getWorker_Processes() {
	return worker_processes;
}

std::vector<std::atomic<double>>& WorkerManager::getProcessingRatesShared() {
    return processing_rates_shared;
}

std::vector<std::atomic<int>>& WorkerManager::getTotalProcessedDataCountShared() {
    return total_processed_data_count_shared;
}

std::vector<std::atomic<int>>& WorkerManager::getWorkerStatusShared() {
    return worker_status_shared;
}

std::vector<std::shared_ptr<WorkerThread>> WorkerManager::getWorkerThreads() {
    return worker_threads;
}


/////////////////////////////////////////////
std::shared_ptr<ThreadSafeQueue<std::vector<uint8_t>>> WorkerManager::getLowPriorityQueue() const {
    return low_priority_queue;
}

std::shared_ptr<ThreadSafeQueue<std::vector<uint8_t>>> WorkerManager::getHighPriorityQueue() const {
    return high_priority_queue;
}

std::shared_ptr<ThreadSafeQueue<std::vector<uint8_t>>> WorkerManager::getResultLpQueue() const {
    return result_lp_queue;
}

std::shared_ptr<ThreadSafeQueue<std::vector<uint8_t>>> WorkerManager::getResultHpQueue() const {
    return result_hp_queue;
}
/////////////////////////////////////////////


MonitoringPoint* WorkerManager::getMonitoringPoint() const {
    return monitoringpoint;
}

MonitoringThread* WorkerManager::getMonitoringThread() const {
    return monitoringthread;
}

// Function to change token results
void WorkerManager::change_token_results() {
    std::lock_guard<std::mutex> lock(*tokenresultslock);

    for (auto& worker : worker_threads) {
        int token_result = worker->get_tokenresult();
        token_result = (token_result - 1 + num_workers) % num_workers; // Fix the circular decrement
        worker->set_tokenresult(token_result);
    }
}

////////////////////////////////////////
void WorkerManager::change_token_reading() {
    if (!tokenreadinglock) {
        logger->error("tokenreadinglock is null");
        return;
    }

    std::lock_guard<std::mutex> lock(*tokenreadinglock);

    if (num_workers == 0) {
        logger->error("No workers available to change token reading (num_workers = 0).");
        return;
    }

    for (auto& worker : worker_threads) {
        int token_reading = worker->get_tokenreading();
        token_reading = (token_reading - 1 + num_workers) % num_workers; // Fix the circular decrement
        worker->set_tokenreading(token_reading);
    }
}
////////////////////////////////////////

void WorkerManager::set_stopdata(bool stopdata) {
    this->stopdata = stopdata;
    change_status();
}

void WorkerManager::setProcessingRate(int worker_id, double rate) {
    if (worker_id >= 0 && worker_id < processing_rates_shared.size()) {
        processing_rates_shared[worker_id] = rate;
    } else {
        std::cerr << "Invalid worker_id: " << worker_id << std::endl;
    }
}

void WorkerManager::setTotalProcessedDataCount(int worker_id, int count) {
    if (worker_id >= 0 && worker_id < total_processed_data_count_shared.size()) {
        total_processed_data_count_shared[worker_id] = count;
    } else {
        std::cerr << "Invalid worker_id: " << worker_id << std::endl;
    }
}

void WorkerManager::set_processdata(int processdata) {
    this->processdata = processdata;
    change_status();

    for (auto& worker : worker_threads) {
        worker->set_processdata(this->processdata);
    }
}

void WorkerManager::setWorkerStatus(int worker_id, int status) {
    if (worker_id >= 0 && worker_id < worker_status_shared.size()) {
        worker_status_shared[worker_id] = status;
    } else {
        // Handle invalid worker_id (e.g., out of bounds)
        std::cerr << "Invalid worker_id: " << worker_id << std::endl;
    }
}

// Function to change the status based on flags
void WorkerManager::change_status() {
    if (stopdata && processdata == 0) {
        status = "Initialised";
    } else if (stopdata && processdata == 1) {
        status = "Wait for data";
    } else if (!stopdata && processdata == 1) {
        status = "Processing";
    } else if (!stopdata && processdata == 0) {
        status = "Wait for processing";
    }
    supervisor->send_info(1, status, fullname, 1, "Low");
}

void WorkerManager::start_service_threads() {
    monitoringpoint = new MonitoringPoint(this);
    monitoring_thread = new MonitoringThread(*socket_monitoring, *monitoringpoint);  // Create MonitoringThread instance
    // monitoring_thread = std::thread(&MonitoringThread::run, monitoringthread);  // Start the thread with run method
    monitoring_thread->start();
    logger->info(fmt::format("Service thread started"));
}

// Function to start worker threads 
void WorkerManager::start_worker_threads(int num_threads) {
}

// Function to start worker processes
void WorkerManager::start_worker_processes(int num_processes) {
    if (num_processes > max_workers) {
        // spdlog::warn("WARNING! It is not possible to create more than {} processes", max_workers);
        logger->warning(fmt::format("WARNING! It is not possible to create more than {} processes", max_workers), globalname);
    }
    num_workers = num_processes;    
}

void WorkerManager::start() {
    // Start the thread with the run() method
    worker_thread = std::thread(&WorkerManager::run, this);
}

// Main run function
void WorkerManager::run() {   
	std::cout << "Start WorkerManager run" << std::endl;
    start_service_threads();

    status = "Initialised";
    supervisor->send_info(1, status, fullname, 1, "Low");

    try {
        while (!continueall) {      
            // std::this_thread::sleep_for(std::chrono::seconds(1)); // To avoid 100% CPU consumption

            // Check the status of the workers
            workersstatus = 0;
            workersstatusinit = 0;
            int worker_id = 0;

            for (auto& thread : worker_threads) {
                if (thread->get_status() == 0) {
                    workersstatusinit++;
                } 
                else {
                    workersstatus += thread->get_status();
                }
            }

            if (num_workers != workersstatusinit) {
                workersstatus = workersstatus / (num_workers - workersstatusinit);
            }
        }
    } 
    catch (const std::exception& e) {
        logger->error("Exception caught: {}", e.what());
        stop_internalthreads();
        continueall = false;
    }
}

// Function to clean the queues
void WorkerManager::clean_queue() {
    logger->info("Cleaning queues...", globalname);

    clean_single_queue(low_priority_queue, "low_priority_queue");
    clean_single_queue(high_priority_queue, "high_priority_queue");
    clean_single_queue(result_lp_queue, "result_lp_queue");
    clean_single_queue(result_hp_queue, "result_hp_queue");

    logger->info("End cleaning queues", globalname);
}

//////////////////////////////////////////
// Function to stop the manager
void WorkerManager::stop(bool fast) {

    _stop_event = true;

    // Notifica tutti i thread che stanno aspettando sulle code
    low_priority_queue->notify_all();
    high_priority_queue->notify_all();
    result_lp_queue->notify_all();
    result_hp_queue->notify_all();

    if (worker_thread.joinable()) {
        worker_thread.join();
    }

    for (auto& t : worker_threads) {
        if (t) {
            t->stop();
        }
    }

    // _stop_event = true;
    stop_internalthreads();
    status = "End";
}

void WorkerManager::stop_internalthreads() {
    _stop_event = true;

    logger->info("Stopping Manager internal threads...", globalname);

    if (monitoring_thread) {
        delete monitoring_thread;  
        monitoring_thread = nullptr;  
    }
    
    logger->info("All Manager internal threads terminated.", globalname);
}
//////////////////////////////////////////

// Function to configure workers
void WorkerManager::configworkers(const json& configuration) {
    if (processingtype == "thread") {
        for (auto& worker : worker_threads) {
            worker->config(configuration);
        }
    }
}

void WorkerManager::clean_single_queue(std::shared_ptr<ThreadSafeQueue<std::vector<uint8_t>>>& queue, const std::string& queue_name) {
    if (!queue->empty()) {
        logger->info(fmt::format("   - {} size {}", queue_name, queue->size()), globalname);

        while (!queue->empty()) {
            queue->pop();
        }

        logger->info(fmt::format("   - {} empty", queue_name), globalname);
    }
}

// Not used
void WorkerManager::close_queue(std::shared_ptr<std::queue<std::string>>& queue, const std::string& queue_name) {
    try {
        logger->info(fmt::format("   - {} size {}", queue_name, queue->size()), globalname);

        while (!queue->empty()) {
            queue->pop();
        }

        queue.reset();
        logger->info(fmt::format("   - {} empty", queue_name), globalname);
    } 
    catch (const std::exception& e) {
        logger->error(fmt::format("ERROR in worker stop {} cleaning: {}", queue_name, e.what()), globalname);
    }
}