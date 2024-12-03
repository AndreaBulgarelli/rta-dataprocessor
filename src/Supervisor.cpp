// Copyright (C) 2024 INAF
// This software is distributed under the terms of the BSD-3-Clause license
//
// Authors:
//
//    Andrea Bulgarelli <andrea.bulgarelli@inaf.it>
//

#include "Supervisor.h"

Supervisor* Supervisor::instance = nullptr;

Supervisor::Supervisor(std::string config_file, std::string name)
    : name(name), continueall(true), config_manager(nullptr), manager_num_workers(0) {
    Supervisor::set_instance(this);  // Set the current instance
    load_configuration(config_file, name);
    fullname = name;
    globalname = "Supervisor-" + name;

    // Set up logging
    std::string log_file = config["logs_path"].get<std::string>() + "/" + globalname + ".log";
    logger = new WorkerLogger("worker_logger", log_file, spdlog::level::debug);

    pid = getpid();
    context = zmq::context_t(1);

    try {


        int timeout = 20000; // Timeout di 1 secondo


        // Retrieve and log configuration
        processingtype = config["processing_type"].get<std::string>();
        dataflowtype = config["dataflow_type"].get<std::string>();
        std::cout << "\n\n\n dataflowtype: " << dataflowtype << std::endl;
        datasockettype = config["datasocket_type"].get<std::string>();

        std::cout << "Supervisor: " << globalname << " / " << dataflowtype << " / " 
                  << processingtype << " / " << datasockettype << std::endl;
        logger->system("Supervisor: " + globalname + " / " + dataflowtype + " / " 
                       + processingtype + " / " + datasockettype, globalname);

        // Set up data sockets based on configuration
        if (datasockettype == "pushpull") {
            socket_lp_data = new zmq::socket_t(context, ZMQ_PULL);
            socket_lp_data->bind(config["data_lp_socket"].get<std::string>());

            socket_hp_data = new zmq::socket_t(context, ZMQ_PULL);
            socket_hp_data->bind(config["data_hp_socket"].get<std::string>());
        } 
        else if (datasockettype == "pubsub") {
            socket_lp_data = new zmq::socket_t(context, ZMQ_SUB);
            socket_lp_data->connect(config["data_lp_socket"].get<std::string>());
            socket_lp_data->setsockopt(ZMQ_SUBSCRIBE, "", 0);

            socket_hp_data = new zmq::socket_t(context, ZMQ_SUB);
            socket_hp_data->connect(config["data_hp_socket"].get<std::string>());
            socket_hp_data->setsockopt(ZMQ_SUBSCRIBE, "", 0);

            /////////////////////////////////
            socket_lp_data->setsockopt(ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
            socket_hp_data->setsockopt(ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
            /////////////////////////////////

        } 
        else if (datasockettype == "custom") {
            logger->system("Supervisor started with custom data receiver", globalname);
        } 
        else {
            throw std::invalid_argument("Config file: datasockettype must be pushpull or pubsub");
        }

        // Set up command and monitoring sockets
        socket_command = new zmq::socket_t(context, ZMQ_SUB);
        socket_command->connect(config["command_socket"].get<std::string>());
        socket_command->setsockopt(ZMQ_SUBSCRIBE, "", 0);

        /////////////////////////////////
        socket_command->setsockopt(ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
        /////////////////////////////////

        socket_monitoring = new zmq::socket_t(context, ZMQ_PUSH);
        socket_monitoring->connect(config["monitoring_socket"].get<std::string>());

        socket_lp_result.resize(100, nullptr);
        socket_hp_result.resize(100, nullptr);
    } 
    catch (const std::exception &e) {
        // Handle any other unexpected exceptions
        std::cerr << "ERROR: An unexpected error occurred: " << e.what() << std::endl;
        logger->warning("ERROR: An unexpected error occurred: " + std::string(e.what()), globalname);
        exit(1);
    }

    manager_workers = std::vector<WorkerManager*>();
    processdata = 0;
    stopdata = true;

    sendresultslock = std::make_shared<std::mutex>();


    // Set up signal handlers
    try {
        if (std::signal(SIGTERM, handle_signals) == SIG_ERR) {
            throw std::runtime_error("Failed to set SIGTERM handler");
        }
        if (std::signal(SIGINT, handle_signals) == SIG_ERR) {
            throw std::runtime_error("Failed to set SIGINT handler");
        }
    } catch (const std::exception &e) {
        std::cerr << "WARNING! Signal only works in main thread. It is not possible to set up signal handlers!" << std::endl;
        logger->warning("WARNING! Signal only works in main thread. It is not possible to set up signal handlers!", globalname);
    }

    status = "Initialised";
    send_info(1, status, fullname, 1, "Low");

    std::cout << globalname << " started" << std::endl;
    logger->system(globalname + " started", globalname);
}

//////////////////////////////////
// Destructor to clean up resources
Supervisor::~Supervisor() {
    while (!shutdown_over) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    spdlog::error("ENTRO NEL DISTRUTTORE DEL SUPERVISOR");

    if (lp_data_thread.joinable()) {
        spdlog::error("JOIN DI lp_data_thread");

        lp_data_thread.join();
    }

    if (hp_data_thread.joinable()) {
        spdlog::error("JOIN DI hp_data_thread");

        hp_data_thread.join();
    }

    if (result_thread.joinable()) {
        spdlog::error("JOIN DI result_thread");

        result_thread.join();
    }

    if (socket_command) {
        spdlog::error("AAAAA Closing ZMQ socket (socket_command)...");

        try {
            socket_command->close(); // Chiude il socket
        }
        catch (const zmq::error_t& e) {
            spdlog::error("Error while closing socket_command: {}", e.what());
        }
        delete socket_command;     // Libera la memoria
        socket_command = nullptr;  // Evita puntatori dangling
    }
    if (socket_lp_data) {
        spdlog::error("AAAAA Closing socket_lp_data...");

        try {
            socket_lp_data->close(); 
        }
        catch (const zmq::error_t& e) {
            spdlog::error("Error while closing socket_lp_data: {}", e.what());
        }
        delete socket_lp_data;
        socket_lp_data = nullptr;
    }
    if (socket_hp_data) {
        spdlog::error("AAAAA Closing socket_hp_data...");

        try {
            socket_hp_data->close();
        }
        catch (const zmq::error_t& e) {
            spdlog::error("Error while closing socket_hp_data: {}", e.what());
        }
        delete socket_hp_data;
        socket_hp_data = nullptr;
    }
    if (!socket_lp_result.empty()) {
        spdlog::error("AAAAA Closing socket_lp_result...");
        for (auto* socket : socket_lp_result) {
            if (socket) {
                try {
                    socket->close();  // Chiudi il socket
                }
                catch (const zmq::error_t& e) {
                    spdlog::error("Error while closing socket: {}", e.what());
                }
                delete socket; // Dealloca il puntatore
                socket = nullptr;
            }
        }
        socket_lp_result.clear(); // Svuota il vettore
    }
    if (!socket_hp_result.empty()) {
        spdlog::error("AAAAA Closing socket_hp_result...");
        for (auto* socket : socket_hp_result) {
            if (socket) {
                try {
                    socket->close();  // Chiudi il socket
                }
                catch (const zmq::error_t& e) {
                    spdlog::error("Error while closing socket: {}", e.what());
                }
                delete socket; // Dealloca il puntatore
                socket = nullptr;
            }
        }
        socket_hp_result.clear(); // Svuota il vettore
    }
    if (socket_monitoring) {
        spdlog::error("AAAAA Closing socket_monitoring...");

        try {
            socket_monitoring->close();
        }
        catch (const zmq::error_t& e) {
            spdlog::error("Error while closing socket_monitoring: {}", e.what());
        }
        delete socket_monitoring;
        socket_monitoring = nullptr;
    }

    // Chiudi il contesto ZMQ (prima devono essere chiusi tutti i zmq::socket)
    spdlog::error("AAAAA zmq_ctx_shutdown...");
    zmq_ctx_shutdown(context.handle()); // Fermare i thread associati al contesto

    try {
        spdlog::error("AAAAA Closing ZMQ context...");

        context.close(); // Chiude esplicitamente il contesto
    }
    catch (const zmq::error_t& e) {
        spdlog::error("Error while closing ZMQ context: {}", e.what());
    }

    if (logger) {
        spdlog::error("AAAAA Deleting logger...");

        // logger->close()
        delete logger;
        logger = nullptr;
    }

    spdlog::error("DISTRUTTORE SUPERVISOR CONCLUSO");
}
//////////////////////////////////

// Static method to set the current instance
void Supervisor::set_instance(Supervisor *instance) {
    Supervisor::instance = instance;
}

// Static method to get the current instance
Supervisor* Supervisor::get_instance() {
    return Supervisor::instance;
}

std::vector<std::string> Supervisor::getNameWorkers() const {
    return worker_names;
}

//////////////////////////////////////////////////
// Load configuration from the specified file and name
void Supervisor::load_configuration(const std::string &config_file, const std::string &name) {
    config_manager = new ConfigurationManager(config_file);
    config = config_manager->get_configuration(name);
    std::cout << config << std::endl;

    // Extract values from the tuple returned by get_workers_config
    auto workers_config = config_manager->get_workers_config(name);
    manager_result_sockets_type = std::get<0>(workers_config)[0]; // assuming single value
    manager_result_dataflow_type = std::get<1>(workers_config)[0]; // assuming single value
    manager_result_lp_sockets = std::get<2>(workers_config);
    manager_result_hp_sockets = std::get<3>(workers_config);
    manager_num_workers = std::get<4>(workers_config)[0]; // assuming single value
    workername = std::get<5>(workers_config)[0]; // assuming single value
    name_workers = std::get<6>(workers_config);

    // Log the contents of the tuple
    // DEBUG aggiunto per stampare i campi
    spdlog::info("manager_result_sockets_type: {}", manager_result_sockets_type);
    spdlog::info("manager_result_dataflow_type: {}", manager_result_dataflow_type);

    spdlog::info("manager_result_lp_sockets:");
    for (const auto& socket : manager_result_lp_sockets) {
        spdlog::info("  {}", socket);
    }

    spdlog::info("manager_result_hp_sockets:");
    for (const auto& socket : manager_result_hp_sockets) {
        spdlog::info("  {}", socket);
    }

    spdlog::info("manager_num_workers: {}", manager_num_workers);
    spdlog::info("workername: {}", workername);

    spdlog::info("name_workers:");
    for (const auto& worker : name_workers) {
        spdlog::info("  {}", worker);
    }

    // Additional information about workers
    spdlog::info("Number of workers: {}", name_workers.size());
}
//////////////////////////////////////////////////

// Start service threads for data handling
void Supervisor::start_service_threads() {
    if (dataflowtype == "binary") {
        lp_data_thread = std::thread(&Supervisor::listen_for_lp_data, this);
        hp_data_thread = std::thread(&Supervisor::listen_for_hp_data, this);
    } else if (dataflowtype == "filename") {
        lp_data_thread = std::thread(&Supervisor::listen_for_lp_file, this);
        hp_data_thread = std::thread(&Supervisor::listen_for_hp_file, this);
    } else if (dataflowtype == "string") {
        lp_data_thread = std::thread(&Supervisor::listen_for_lp_string, this);
        hp_data_thread = std::thread(&Supervisor::listen_for_hp_string, this);
    }

    result_thread = std::thread(&Supervisor::listen_for_result, this);
}

// Set up result channel for a given WorkerManager
void Supervisor::setup_result_channel(WorkerManager *manager, int indexmanager) {
    socket_lp_result[indexmanager] = nullptr;
    socket_hp_result[indexmanager] = nullptr;
    //context = zmq::context_t(1);

    if (manager->get_result_lp_socket() != "none") {
        if (manager->get_result_socket_type() == "pushpull") {
            socket_lp_result[indexmanager] = new zmq::socket_t(context, ZMQ_PUSH);
            socket_lp_result[indexmanager]->connect(manager->get_result_lp_socket());
            std::cout << "---result lp socket pushpull " << manager->get_globalname() << " " << manager->get_result_lp_socket() << std::endl;
            logger->system("---result lp socket pushpull " + manager->get_globalname() + " " + manager->get_result_lp_socket(), globalname);
        } else if (manager->get_result_socket_type() == "pubsub") {
            socket_lp_result[indexmanager] = new zmq::socket_t(context, ZMQ_PUB);
            socket_lp_result[indexmanager]->bind(manager->get_result_lp_socket());
            std::cout << "---result lp socket pushpull " << manager->get_globalname() << " " << manager->get_result_lp_socket() << std::endl;
            logger->system("---result lp socket pushpull " + manager->get_globalname() + " " + manager->get_result_lp_socket(), globalname);
        }
    }

    if (manager->get_result_hp_socket() != "none") {
        if (manager->get_result_socket_type() == "pushpull") {
            socket_hp_result[indexmanager] = new zmq::socket_t(context, ZMQ_PUSH);
            socket_hp_result[indexmanager]->connect(manager->get_result_hp_socket());
            std::cout << "---result hp socket pushpull " << manager->get_globalname() << " " << manager->get_result_hp_socket() << std::endl;
            logger->system("---result hp socket pushpull " + manager->get_globalname() + " " + manager->get_result_hp_socket(), globalname);
        } else if (manager->get_result_socket_type() == "pubsub") {
            socket_hp_result[indexmanager] = new zmq::socket_t(context, ZMQ_PUB);
            socket_hp_result[indexmanager]->bind(manager->get_result_hp_socket());
            std::cout << "---result hp socket pushpull " << manager->get_globalname() << " " << manager->get_result_hp_socket() << std::endl;
            logger->system("---result hp socket pushpull " + manager->get_globalname() + " " + manager->get_result_hp_socket(), globalname);
        }
    }
}

// Start managers
void Supervisor::start_managers() {
    int indexmanager = 0;
    WorkerManager *manager = new WorkerManager(indexmanager, this, "Generic");
    setup_result_channel(manager, indexmanager);
    manager->run();
    manager_workers.push_back(manager);
    std::cout << "BASE SUP manager started. man lenght: " << manager_workers.size() << std::endl;
}

// Start workers
void Supervisor::start_workers() {
    int indexmanager = 0;

    for (auto &manager : manager_workers) {
        manager->start_worker_threads(manager_num_workers);
        std::cout << "SUP start_worker_threads" << std::endl;
        indexmanager++;
    }
}

///////////////////////////////////////
// Start Supervisor operation
void Supervisor::start() {
    start_managers();
    start_workers();
    start_service_threads();

    // Era:
    // start_service_threads();
    // start_managers();
    // start_workers();

    status = "Waiting";
    send_info(1, status, fullname, 1, "Low");

    while (continueall) {
        listen_for_commands();
        std::this_thread::sleep_for(std::chrono::seconds(1)); // To avoid 100% CPU
    }

    /*try {
        while (continueall) {
            listen_for_commands();
            std::this_thread::sleep_for(std::chrono::seconds(1)); // To avoid 100% CPU
        }
    } catch (const std::exception &e) {
        std::cerr << "BBBBBBBBBBBBBBB Exception caught: " << e.what() << std::endl;
        command_shutdown();
    }*/
}
//////////////////////////////////////

// Static function to handle signals
void Supervisor::handle_signals(int signum) {
    Supervisor* instance = Supervisor::get_instance();

    if (instance) {
        if (signum == SIGTERM) {
            std::cerr << "SIGTERM received in main thread. Terminating with cleaned shutdown." << std::endl;
            instance->logger->system("SIGTERM received in main thread. Terminating with cleaned shutdown", instance->globalname);
            instance->command_cleanedshutdown();
        } 
        else if (signum == SIGINT) {
            std::cerr << "SIGINT received in main thread. Terminating with shutdown." << std::endl;
            instance->logger->system("SIGINT received in main thread. Terminating with shutdown", instance->globalname);
            instance->command_shutdown();
        } 
        else {
            std::cerr << "Received signal " << signum << "in main thread. Terminating." << std::endl;
            instance->logger->system("Received signal " + std::to_string(signum) + "in main thread. Terminating", instance->globalname);
            instance->command_shutdown();
        }
    }
}

///////////////////////////////////////////////////////////////////
// Listen for result data
void Supervisor::listen_for_result() {
    try {
        while (continueall) {
            int indexmanager = 0;

            for (auto& manager : manager_workers) {
                int attempt = 0;  // Contatore per i tentativi

                while (manager == nullptr && attempt < 10) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));  // Sleep for 1 second
                    spdlog::warn("Waiting for manager workers for listening results, attempt {}", attempt + 1);
                    attempt++;
                }

                if (manager == nullptr) {
                    spdlog::error("Manager worker not initialized after maximum attempts, skipping index {}", indexmanager);
                    continue;  // Salta l'invio dei risultati se `manager` è ancora nullo
                }

                try {
                    send_result(manager, indexmanager);
                }
                catch (const std::exception& e) {
                    spdlog::error("Exception while sending results for manager at index {}: {}", indexmanager, e.what());
                }
                catch (...) {
                    spdlog::error("Unknown exception while sending results for manager at index {}", indexmanager);
                }

                indexmanager++;
            }
        }
    }
    catch (const std::exception& e) {
        spdlog::critical("Exception in listen_for_result: {}", e.what());
        continueall = false;  // Interrompi il ciclo per evitare ulteriori errori
    }
    catch (...) {
        spdlog::critical("Unknown exception in listen_for_result, terminating thread");
        continueall = false;
    }

    spdlog::info("End listen_for_result");
    logger->system("End listen_for_result", globalname);
}
///////////////////////////////////////////////////////////////////

/*
void Supervisor::listen_for_result() {
    while (continueall) {
        int indexmanager = 0;
    
        for (auto &manager : manager_workers) {
            while(manager == nullptr){
            std::this_thread::sleep_for(std::chrono::seconds(1));  // Sleep for 1 second
            std::cout << "waiting for manager workers for listening results" << std::endl;
        }
            send_result(manager, indexmanager);
            indexmanager++;
        }
    }
    std::cout << "End listen_for_result" << std::endl;
    logger->system("End listen_for_result", globalname);
}
*/

///////////////////////////////////////////////////////////////////
// Send result data
void Supervisor::send_result(WorkerManager *manager, int indexmanager) {
    // std::lock_guard<std::mutex> lock(*sendresultslock);

  if (manager->getResultLpQueue()->empty() && manager->getResultHpQueue()->empty()) {
        return;
    }

    json data;
    int channel = -1;

    try {
        // Prova a prelevare un elemento dalla HP queue
        channel = 1;
        spdlog::warn("Supervisor::send_result: HPQUEUE SIZE: {}", manager->getResultHpQueue()->size());
        data = manager->getResultHpQueue()->get();  // Preleva dalla HP queue
    } catch (const std::exception &e) {
        try {
            // Se fallisce, passa alla LP queue
            channel = 0;
            spdlog::warn("Supervisor::send_result: LPQUEUE SIZE: {}", manager->getResultLpQueue()->size());
            data = manager->getResultLpQueue()->get();  // Preleva dalla LP queue
        } catch (const std::exception &e) {
            // Entrambe le code sono vuote
            return;
        }
    }

    /*
        // Gestione della coda ad alta priorità
    if (!manager->getResultHpQueue()->empty()) {
        channel = 1;
        spdlog::warn("Supervisor::send_result: HPQUEUE SIZE: {}", manager->getResultHpQueue()->size());
        data = manager->getResultHpQueue()->front();
        manager->getResultHpQueue()->pop();
    }
    // Gestione della coda a bassa priorità
    else if (!manager->getResultLpQueue()->empty()) {
        channel = 0;
        spdlog::warn("Supervisor::send_result: LPQUEUE SIZE: {}", manager->getResultLpQueue()->size());
        data = manager->getResultLpQueue()->front();
        manager->getResultLpQueue()->pop();
    }
    // Code vuote
    else {
        spdlog::warn("Both queues are empty. Nothing to send.");
        return;
    }
    */

    if (channel == 0) {
        if (manager->get_result_lp_socket() == "none") {
            return;
        }
        if (manager->get_result_dataflow_type() == "string" || manager->get_result_dataflow_type() == "filename") {
            try {
                std::string data_str = data.get<std::string>();
                socket_lp_result[indexmanager]->send(zmq::buffer(data_str));
            } catch (const std::exception &e) {
                std::cerr << "ERROR: data not in string format to be sent to: " << e.what() << std::endl;
                logger->error("ERROR: data not in string format to be sent to: " + std::string(e.what()), globalname);
            }
        } else if (manager->get_result_dataflow_type() == "binary") {
            try {
                socket_lp_result[indexmanager]->send(zmq::buffer(data.dump()));
            } catch (const std::exception &e) {
                std::cerr << "ERROR: data not in binary format to be sent to socket_result: " << e.what() << std::endl;
                logger->error("ERROR: data not in binary format to be sent to socket_result: " + std::string(e.what()), globalname);
            }
        }
    }

    if (channel == 1) {
        if (manager->get_result_hp_socket() == "none") {
            return;
        }
        if (manager->get_result_dataflow_type() == "string" || manager->get_result_dataflow_type() == "filename") {
            try {
                std::string data_str = data.get<std::string>();
                socket_hp_result[indexmanager]->send(zmq::buffer(data_str));
            } catch (const std::exception &e) {
                std::cerr << "ERROR: data not in string format to be sent to: " << e.what() << std::endl;
                logger->error("ERROR: data not in string format to be sent to: " + std::string(e.what()), globalname);
            }
        } else if (manager->get_result_dataflow_type() == "binary") {
            try {
                socket_hp_result[indexmanager]->send(zmq::buffer(data.dump()));
            } catch (const std::exception &e) {
                std::cerr << "ERROR: data not in binary format to be sent to socket_result: " << e.what() << std::endl;
                logger->error("ERROR: data not in binary format to be sent to socket_result: " + std::string(e.what()), globalname);
            }
        }
    }
}
///////////////////////////////////////////////////////////////////

// Listen for low priority data
void Supervisor::listen_for_lp_data() {
    while (continueall) {
        if (!stopdata) {
            zmq::message_t data;
            socket_lp_data->recv(data);

            for (auto &manager : manager_workers) {
                json decodeddata = json::parse(data.to_string());
                manager->getLowPriorityQueue()->push(decodeddata);
            }
        }
    }

    std::cout << "End listen_for_lp_data" << std::endl;
    logger->system("End listen_for_lp_data", globalname);
}

// Listen for high priority data
void Supervisor::listen_for_hp_data() {
    while (continueall) {
        if (!stopdata) {
            zmq::message_t data;
            socket_hp_data->recv(data);

            for (auto &manager : manager_workers) {
                json decodeddata = json::parse(data.to_string());
                manager->getHighPriorityQueue()->push(decodeddata);
            }
        }
    }

    std::cout << "End listen_for_hp_data" << std::endl;
    logger->system("End listen_for_hp_data", globalname);
}

// Listen for low priority strings
void Supervisor::listen_for_lp_string() {
    while (continueall) {
        if (!stopdata) {
            zmq::message_t data;
            socket_lp_data->recv(data);
            std::string data_str(static_cast<char*>(data.data()), data.size());

            for (auto &manager : manager_workers) {
                manager->getLowPriorityQueue()->push(data_str);
            }
        }
    }

    std::cout << "End listen_for_lp_string" << std::endl;
    logger->system("End listen_for_lp_string", globalname);
}

// Listen for high priority strings
void Supervisor::listen_for_hp_string() {
    while (continueall) {
        if (!stopdata) {
            zmq::message_t data;
            socket_hp_data->recv(data);
            std::string data_str(static_cast<char*>(data.data()), data.size());

            for (auto &manager : manager_workers) {
                manager->getHighPriorityQueue()->push(data_str);
            }
        }
    }

    std::cout << "End listen_for_hp_string" << std::endl;
    logger->system("End listen_for_hp_string", globalname);
}

// Listen for low priority files
void Supervisor::listen_for_lp_file() {
    while (continueall) {
        if (!stopdata) {
            zmq::message_t filename_msg;
            socket_lp_data->recv(filename_msg);
            std::string filename(static_cast<char*>(filename_msg.data()), filename_msg.size());

            for (auto &manager : manager_workers) {
                auto [data, size] = open_file(filename);
                for (int i = 0; i < size; i++) {
                    manager->getLowPriorityQueue()->push(data[i]);
                }
            }
        }
    }

    std::cout << "End listen_for_lp_file" << std::endl;
    logger->system("End listen_for_lp_file", globalname);
}

std::pair<std::vector<json>, int> Supervisor::open_file(const std::string &filename) {
    std::vector<json> data;  // Vector to store parsed JSON objects
    int size = 0;

    std::ifstream file(filename); // Open the file for reading
    if (!file.is_open()) {
        std::cerr << "Unable to open file: " << filename << std::endl;
        logger->error("Unable to open file: " + filename, globalname);
        return {data, size};  // Return empty vector and size 0 if the file cannot be opened
    }

    try {
        std::string line;
        while (std::getline(file, line)) { // Read the file line-by-line
            if (!line.empty()) { // Only attempt to parse non-empty lines
                json jsonData = json::parse(line); // Parse the line as JSON
                data.push_back(jsonData); // Add the parsed JSON object to the vector
                size++;
            }
        }
    } catch (const std::exception &e) {
        std::cerr << "Error while reading file: " << e.what() << std::endl;
        logger->error("Error while reading file: " + std::string(e.what()), globalname);
    }

    file.close(); // Close the file after reading
    return {data, size}; // Return the vector and the size of the data read
}

// Listen for high priority files
void Supervisor::listen_for_hp_file() {
    while (continueall) {
        if (!stopdata) {
            zmq::message_t filename_msg;
            socket_hp_data->recv(filename_msg);
            std::string filename(static_cast<char*>(filename_msg.data()), filename_msg.size());

            for (auto &manager : manager_workers) {
                auto [data, size] = open_file(filename);
                for (int i = 0; i < size; i++) {
                    manager->getHighPriorityQueue()->push(data[i]);
                }
            }
        }
    }

    std::cout << "End listen_for_hp_file" << std::endl;
    logger->system("End listen_for_hp_file", globalname);
}

///////////////////////////////////
void Supervisor::listen_for_commands() {
    while (continueall) {
        try {
            std::cout << "Waiting for commands..." << std::endl;
            logger->system("Waiting for commands...", globalname);

            zmq::message_t command_msg;

            // Controlla se il socket è valido
            if (!socket_command) {
                spdlog::error("Socket is null or invalid in listen_for_commands");
                break; // Esci dal ciclo se il socket è invalido
            }

            // Usa un timeout per evitare blocchi infiniti
            zmq::recv_flags flags = zmq::recv_flags::none;

            // Riceve il messaggio e controlla se c'è un errore
            auto result = socket_command->recv(command_msg, flags);

            if (!result) {
                // Se non è stato ricevuto nulla, gestisci l'errore (timeout o nessun messaggio)
                int err_code = zmq_errno();

                if (err_code == EAGAIN) {
                    // Non ci sono messaggi da ricevere, procedi senza errore
                    spdlog::warn("No command received within the timeout period (EAGAIN)");
                }
                else {
                    // Gestisci altri errori (se presenti)
                    spdlog::error("ZMQ recv error: {}", zmq_strerror(err_code));
                    // break; // Esci dal ciclo se c'è un errore grave
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Dorme per evitare carico CPU
                continue; // Continua a cercare comandi
            }

            spdlog::warn("Supervisor::listen_for_command: COMMAND RECEIVED");

            std::string command_str(static_cast<char*>(command_msg.data()), command_msg.size());

            try {
                json command = json::parse(command_str);
                process_command(command);
            }
            catch (const json::parse_error& e) {
                spdlog::error("JSON parse error: {}", e.what());
                // Puoi decidere se ignorare questo errore o interrompere
            }
        }
        catch (const zmq::error_t& e) {
            spdlog::error("ZMQ exception in listen_for_commands: {}", e.what());
            // break; // Esci dal ciclo su errore di ZMQ
        }
        /* catch (const std::exception& e) {
            spdlog::error("Exception in Supervisor::listen_for_commands: {}", e.what());
            throw;
            //break; // Esci dal ciclo su altre eccezioni
        }*/

        // std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Evita un uso intensivo della CPU
    }

    spdlog::error("Supervisor::listen_for_command: FUORI DAL WHILE (continueall = FALSE)");

    std::cout << "End listen_for_commands" << std::endl;
    logger->system("End listen_for_commands", globalname);
}
///////////////////////////////////

// Listen for commands
/* 
void Supervisor::listen_for_commands() {
    while (continueall) {
        std::cout << "Waiting for commands..." << std::endl;
        logger->system("Waiting for commands...", globalname);

        zmq::message_t command_msg;
        socket_command->recv(command_msg);
        std::string command_str(static_cast<char*>(command_msg.data()), command_msg.size());
        json command = json::parse(command_str);
        process_command(command);
    }

    std::cout << "End listen_for_commands" << std::endl;
    logger->system("End listen_for_commands", globalname);
}
*/

// Shutdown command
void Supervisor::command_shutdown() {
    status = "Shutdown";
    stop_all(false);
}

// Cleaned shutdown command
void Supervisor::command_cleanedshutdown() {
    if (status == "Processing") {
        status = "EndingProcessing";
        command_stopdata();

        for (auto &manager : manager_workers) {
            std::cout << "Trying to stop " << manager->get_globalname() << "..." << std::endl;
            logger->system("Trying to stop " + manager->get_globalname() + "...", globalname);

            while (manager->getLowPriorityQueue()->size() != 0 || manager->getHighPriorityQueue()->size() != 0) {
                std::cout << "Queues data of manager " << manager->get_globalname() << " have size " 
                          << manager->getLowPriorityQueue()->size() << " " << manager->getHighPriorityQueue()->size() << std::endl;
                logger->system("Queues data of manager " + manager->get_globalname() + " have size " 
                               + std::to_string(manager->getLowPriorityQueue()->size()) + " " 
                               + std::to_string(manager->getHighPriorityQueue()->size()), globalname);
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }

            while (manager->getResultLpQueue()->size() != 0 || manager->getResultHpQueue()->size() != 0) {
                std::cout << "Queues result of manager " << manager->get_globalname() << " have size " 
                          << manager->getResultLpQueue()->size() << " " << manager->getResultHpQueue()->size() << std::endl;
                logger->system("Queues result of manager " + manager->get_globalname() + " have size " 
                               + std::to_string(manager->getResultLpQueue()->size()) + " " 
                               + std::to_string(manager->getResultHpQueue()->size()), globalname);
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
        }
    } 
    else {
        std::cerr << "WARNING! Not in Processing state for a clean shutdown. Force the shutdown." << std::endl;
        logger->warning("WARNING! Not in Processing state for a clean shutdown. Force the shutdown.", globalname);
    }

    status = "Shutdown";
    stop_all(false);
}

// Reset command
void Supervisor::command_reset() {
    if (status == "Processing" || status == "Waiting") {
        command_stop();

        for (auto &manager : manager_workers) {
            std::cout << "Trying to reset " << manager->get_globalname() << "..." << std::endl;
            logger->system("Trying to reset " + manager->get_globalname() + "...", globalname);
            manager->clean_queue();
            std::cout << "Queues of manager " << manager->get_globalname() << " have size " 
                      << manager->getLowPriorityQueue()->size() << " " << manager->getHighPriorityQueue()->size() << " " 
                      << manager->getResultLpQueue()->size() << " " << manager->getResultHpQueue()->size() << std::endl;
            logger->system("Queues of manager " + manager->get_globalname() + " have size " 
                           + std::to_string(manager->getLowPriorityQueue()->size()) + " " 
                           + std::to_string(manager->getHighPriorityQueue()->size()) + " " 
                           + std::to_string(manager->getResultLpQueue()->size()) + " " 
                           + std::to_string(manager->getResultHpQueue()->size()), globalname);
        }

        status = "Waiting";
        send_info(1, status, fullname, 1, "Low");
    }
}

// Start command
void Supervisor::command_start() {
    std::cout << "COMMAND START" << std::endl;
    command_startprocessing();
    command_startdata();
}

// Stop command
void Supervisor::command_stop() {
    command_stopdata();
    command_stopprocessing();
}

// Start processing command
void Supervisor::command_startprocessing() {
    status = "Processing";
    std::cout << "CCCCC" << std::endl;
    send_info(1, status, fullname, 1, "Low");

    for (auto &manager : manager_workers) {
        manager->set_processdata(1);
    }
}

// Stop processing command
void Supervisor::command_stopprocessing() {
    status = "Waiting";
    send_info(1, status, fullname, 1, "Low");

    for (auto &manager : manager_workers) {
        manager->set_processdata(0);
    }
}

// Start data command
void Supervisor::command_startdata() {
    stopdata = false;
    for (auto &manager : manager_workers) {
        manager->set_stopdata(false);
    }
}

// Stop data command
void Supervisor::command_stopdata() {
    stopdata = true;
    for (auto &manager : manager_workers) {
        manager->set_stopdata(true);
    }
}

// Process received command
void Supervisor::process_command(const json &command) {
    int type_value = command["header"]["type"].get<int>();
    std::string subtype_value = command["header"]["subtype"].get<std::string>();
    std::string pidtarget = command["header"]["pidtarget"].get<std::string>();
    std::string pidsource = command["header"]["pidsource"].get<std::string>();

    if (type_value == 0) { // command
        if (pidtarget == name || pidtarget == "all" || pidtarget == "*") {
            std::cout << "\nReceived command: " << command << std::endl;
            if (subtype_value == "shutdown") {
                command_shutdown();
            } else if (subtype_value == "cleanedshutdown") {
                command_cleanedshutdown();
            } else if (subtype_value == "getstatus") {
                for (auto &manager : manager_workers) {
                    manager->getMonitoringThread()->sendto(pidsource);
                }
            } else if (subtype_value == "start") {
                command_start();
            } else if (subtype_value == "stop") {
                command_stop();
            } else if (subtype_value == "startprocessing") {
                command_startprocessing();
            } else if (subtype_value == "stopprocessing") {
                command_stopprocessing();
            } else if (subtype_value == "reset") {
                command_reset();
            } else if (subtype_value == "stopdata") {
                command_stopdata();
            } else if (subtype_value == "startdata") {
                command_startdata();
            }
        }
    } else if (type_value == 3) { // config
        for (auto &manager : manager_workers) {
            manager->configworkers(command);
        }
    }
}

// Send alarm message
void Supervisor::send_alarm(int level, const std::string &message, const std::string &pidsource, int code, const std::string &priority) {
    json msg;
    msg["header"]["type"] = 2;
    msg["header"]["subtype"] = "alarm";
    msg["header"]["time"] = static_cast<double>(time(nullptr));
    msg["header"]["pidsource"] = pidsource;
    msg["header"]["pidtarget"] = "*";
    msg["header"]["priority"] = priority;
    msg["body"]["level"] = level;
    msg["body"]["code"] = code;
    msg["body"]["message"] = message;
    socket_monitoring->send(zmq::buffer(msg.dump()));
}

// Send log message
void Supervisor::send_log(int level, const std::string &message, const std::string &pidsource, int code, const std::string &priority) {
    json msg;
    msg["header"]["type"] = 4;
    msg["header"]["subtype"] = "log";
    msg["header"]["time"] = static_cast<double>(time(nullptr));
    msg["header"]["pidsource"] = pidsource;
    msg["header"]["pidtarget"] = "*";
    msg["header"]["priority"] = priority;
    msg["body"]["level"] = level;
    msg["body"]["code"] = code;
    msg["body"]["message"] = message;
    socket_monitoring->send(zmq::buffer(msg.dump()));
}

// Send info message
void Supervisor::send_info(int level, const std::string &message, const std::string &pidsource, int code, const std::string &priority) {
    json msg;
    msg["header"]["type"] = 5;
    msg["header"]["subtype"] = "info";
    msg["header"]["time"] = static_cast<double>(time(nullptr));
    msg["header"]["pidsource"] = pidsource;
    msg["header"]["pidtarget"] = "*";
    msg["header"]["priority"] = priority;
    msg["body"]["level"] = level;
    msg["body"]["code"] = code;
    msg["body"]["message"] = message;
    socket_monitoring->send(zmq::buffer(msg.dump()));
}

//////////////////////////////////////////////////
// Stop all threads and processes
void Supervisor::stop_all(bool fast) {
    spdlog::error("Supervisor::stop_all: ENTRO NELLA FUNZIONE");

    continueall = false;

    std::cout << "Stopping all workers and managers..." << std::endl;
    logger->system("Stopping all workers and managers...", globalname);

    command_stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    for (auto &manager : manager_workers) {
        manager->stop(fast);
    }

    // continueall = false;

    shutdown_over = true;

    std::cout << "All Supervisor workers and managers and internal threads terminated." << std::endl;
    logger->system("All Supervisor workers and managers and internal threads terminated.", globalname);
}
//////////////////////////////////////////////////