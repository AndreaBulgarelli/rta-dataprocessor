#include "Worker1.h"
#include "Supervisor.h"
#include "avro/Generic.hh"
#include "avro/Schema.hh"
#include "avro/ValidSchema.hh"
#include "avro/Compiler.hh"
#include "avro/GenericDatum.hh"
#include "avro/DataFile.hh"
#include "avro/Decoder.hh"
#include "avro/Specific.hh"
#include <iostream>

// Constructor
Worker1::Worker1() : WorkerBase() {
    // Load Avro schema from the provided schema string
    std::string avro_schema_str = R"({
        "type": "record",
        "name": "AvroMonitoringPoint",
        "namespace": "astri.mon.kafka",
        "fields": [
            {"name": "assembly", "type": "string"},
            {"name": "name", "type": "string"},
            {"name": "serial_number", "type": "string"},
            {"name": "timestamp", "type": "double"},
            {"name": "source_timestamp", "type": ["null", "long"]},
            {"name": "units", "type": "string"},
            {"name": "archive_suppress", "type": "boolean"},
            {"name": "env_id", "type": "string"},
            {"name": "eng_gui", "type": "boolean"},
            {"name": "op_gui", "type": "boolean"},
            {"name": "data", "type": {"type": "array", "items": ["double", "int", "long", "string", "boolean"]}}
        ]
    })";

    std::istringstream schema_stream(avro_schema_str);
    avro::compileJsonSchema(schema_stream, avro_schema);

    this->avro_schema = avro_schema;
}

// Override the config method
void Worker1::config(const nlohmann::json& configuration) {
    WorkerBase::config(configuration);
}


void Worker1::printGenericDatum(const avro::GenericDatum& datum, int indent = 0) {
    // Funzione per aggiungere spazi per l'indentazione
    auto indentSpaces = [](int level) -> std::string {
        return std::string(level * 4, ' ');
        };

    // Aggiungi indentazione
    std::cout << indentSpaces(indent);

    // Controlla il tipo del dato e stampalo
    switch (datum.type()) {
    case avro::AVRO_NULL:
        std::cout << "null" << std::endl;
        break;

    case avro::AVRO_BOOL:
        std::cout << "bool: " << datum.value<bool>() << std::endl;
        break;

    case avro::AVRO_INT:
        std::cout << "int: " << datum.value<int32_t>() << std::endl;
        break;

    case avro::AVRO_LONG:
        std::cout << "long: " << datum.value<int64_t>() << std::endl;
        break;

    case avro::AVRO_FLOAT:
        std::cout << "float: " << datum.value<float>() << std::endl;
        break;

    case avro::AVRO_DOUBLE:
        std::cout << "double: " << datum.value<double>() << std::endl;
        break;

    case avro::AVRO_STRING:
        std::cout << "string: " << datum.value<std::string>() << std::endl;
        break;

    case avro::AVRO_BYTES: {
        std::cout << "bytes: ";
        const std::vector<uint8_t>& bytes = datum.value<std::vector<uint8_t>>();
        for (uint8_t byte : bytes) {
            std::cout << static_cast<int>(byte) << " ";
        }
        std::cout << std::endl;
        break;
    }

    case avro::AVRO_ARRAY: {
        std::cout << "array: [" << std::endl;
        const auto& array = datum.value<std::vector<avro::GenericDatum>>();
        for (const auto& elem : array) {
            printGenericDatum(elem, indent + 1);
        }
        std::cout << indentSpaces(indent) << "]" << std::endl;
        break;
    }

    case avro::AVRO_MAP: {
        std::cout << "map: {" << std::endl;
        const auto& map = datum.value<std::map<std::string, avro::GenericDatum>>();
        for (const auto& [key, value] : map) {
            std::cout << indentSpaces(indent + 1) << key << ": ";
            printGenericDatum(value, indent + 1);
        }
        std::cout << indentSpaces(indent) << "}" << std::endl;
        break;
    }

    case avro::AVRO_RECORD: {
        std::cout << "record: {" << std::endl;
        try {
            const auto& record = datum.value<avro::GenericRecord>();
            for (size_t i = 0; i < record.schema()->leaves(); ++i) {
                const auto& field = record.fieldAt(i);
                const auto& fieldName = record.schema()->nameAt(i);
                std::cout << indentSpaces(indent + 1) << fieldName << ": ";
                printGenericDatum(field, indent + 1);
            }
            std::cout << indentSpaces(indent) << "}" << std::endl;
            break;
        }
        catch (const std::bad_any_cast& e) {
            spdlog::error("std::any_cast fallito: {}", e.what());
        }

    }

    case avro::AVRO_ENUM: {
        const auto& enumValue = datum.value<avro::GenericEnum>();
        std::cout << "enum: " << enumValue.symbol() << std::endl;
        break;
    }

    case avro::AVRO_FIXED: {
        const auto& fixed = datum.value<avro::GenericFixed>().value();
        std::cout << "fixed: ";
        for (uint8_t byte : fixed) {
            std::cout << static_cast<int>(byte) << " ";
        }
        std::cout << std::endl;
        break;
    }

    default:
        std::cout << "unknown type" << std::endl;
        break;
    }
}


////////////////////////////////////////////
nlohmann::json Worker1::processData(const std::string data, int priority) { 
    nlohmann::json result;
    result["data"] = "stringa_vuota";
    //result["dataType"] = "binary";

    /* std::cout << "CCCCCCCCCCC: " << std::endl;

    std::cout << "AOAOAOOAOAOAOOA: " << std::endl;

    spdlog::warn("Received data: {}", data);

    std::cout << "AOAOAOOAOAOAOOA2: " << std::endl;


    std::string dataflow_type = get_supervisor()->dataflowtype;

    if (dataflow_type == "binary") {
        std::vector<uint8_t> binary_data;

            try {
                // Decodifica base64 se applicabile, altrimenti copia come binario
                binary_data.assign(data.begin(), data.end());
            }
            catch (const std::exception& e) {
                std::cerr << "Errore nel decodificare i dati: " << e.what() << std::endl;
                return result;
            }

        // Passa i dati binari al decoder Avro
        std::cout << "\nUUUUUUUUUUUUUUUUUUUUU: " << binary_data.data() << std::endl;

        auto in = avro::memoryInputStream(binary_data.data(), binary_data.size());
        auto decoder = avro::binaryDecoder();
        decoder->init(*in);

        avro::GenericDatum datum(avro_schema);
        avro::decode(*decoder, datum);

        if (datum.type() == avro::AVRO_RECORD) {
            spdlog::warn("STAMPOOOO DATUMMMMMM");

            // printGenericDatum(datum);

            spdlog::warn("FINITO DI STAMPARE DATUM");

        }

        result["data"] = data;


        // Simulate processing
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(random_duration())));
    } */
    /*
    else if (dataflow_type == "filename") {
        std::string filename = data.get<std::string>();
        // Simulate processing
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(random_duration())));
        result["filename"] = filename;
        std::cout << "Processed file: " << filename << std::endl;
    }
    else if (dataflow_type == "string") {
        std::string str_data = data.get<std::string>();
        result["data"] = str_data;
        std::cout << "\nProcessed string data: " << str_data << std::endl;
    }
    */

    result["priority"] = priority;
    return result;
}
////////////////////////////////////////////

// Helper function to generate random duration between 0 and 100 milliseconds
double Worker1::random_duration() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 100.0);
    return dis(gen);
}
