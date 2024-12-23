#ifndef WORKER1_H
#define WORKER1_H

#include "WorkerBase.h"
#include "avro/Generic.hh"
#include "avro/Schema.hh"
#include "avro/ValidSchema.hh"
#include "avro/Compiler.hh"
#include "avro/GenericDatum.hh"
#include "avro/DataFile.hh"
#include "avro/Decoder.hh"
#include "avro/Specific.hh"
#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <sstream>
#include <string>
#include <vector>
#include "Supervisor.h"

class Worker1 : public WorkerBase {
private:
    avro::ValidSchema avro_schema; // Store schema

    // Helper function to generate random duration between 0 and 100 milliseconds
    double random_duration();

public:
    // Constructor
    Worker1();

    // Override the config method
    void config(const nlohmann::json& configuration);

    // void printGenericDatum(const avro::GenericDatum& datum, int indent);

    // Override the process_data method
    // std::string process_data(const std::string& data);
    std::vector<uint8_t> processData(const std::vector<uint8_t>& data, int priority);
};

#endif // WORKER1_H
