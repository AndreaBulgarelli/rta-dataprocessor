#ifndef WORKERBASE_H
#define WORKERBASE_H

#include <string>
#include <iostream>
#include "json.hpp" 
#include <zmq.hpp>     
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/fmt/fmt.h"


class WorkerManager;
class Supervisor;


class WorkerBase {

    WorkerManager* manager = nullptr;
    Supervisor* supervisor = nullptr;
    std::shared_ptr<spdlog::logger> logger; 
    std::string fullname;

public:
    std::string workersname;

    WorkerBase();
    virtual ~WorkerBase();

    // Initialize the worker with manager, supervisor, and names
    void init(WorkerManager* manager, Supervisor* supervisor, const std::string& workersname, const std::string& fullname);

    virtual void config(const nlohmann::json& configuration);

    // virtual std::string 
    // const std::string& data);
    virtual std::vector<uint8_t> processData(const std::vector<uint8_t>& data, int priority) = 0;

    Supervisor* get_supervisor() const{{
        return supervisor;
    }}


};

#endif // WORKERBASE_H
