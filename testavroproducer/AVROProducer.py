import avro.schema
from avro.io import DatumWriter
import zmq
import time
import random
import struct
import sys
import json
import io

def generate_avro_data():
    # Generate an array of 16384 double values for the 'data' field
    avro_data = [random.uniform(0, 100) for _ in range(16)]
    return avro_data

def create_avro_message():
    # Create an Avro message with the specified schema
    # Create an Avro message with the specified schema
    avro_schema_str = '''
        {
            "type": "record",
            "name": "AvroMonitoringPoint",
            "namespace": "astri.mon.kafka",
            "fields": [
                {"name": "assembly", "type": "string"},
                {"name": "name", "type": "string"},
                {"name": "serial_number", "type": "string"},
                {"name": "timestamp", "type": "long"},
                {"name": "source_timestamp", "type": ["null", "long"]},
                {"name": "units", "type": "string"},
                {"name": "archive_suppress", "type": "boolean"},
                {"name": "env_id", "type": "string"},
                {"name": "eng_gui", "type": "boolean"},
                {"name": "op_gui", "type": "boolean"},
                {"name": "data", "type": {"type": "array", "items": ["double", "int", "long", "string", "boolean"]}}
            ]
        }
    '''


    avro_schema = avro.schema.parse(avro_schema_str)

    # Fill in other fields based on your use case
    avro_message = {
        "assembly": "YourAssemblyName",
        "name": "YourDisplayName",
        "serial_number": "YourSerialNumber",
        "timestamp": int(time.time()),  
        "source_timestamp": None,
        "units": "YourUnits",
        "archive_suppress": False,
        "env_id": "YourEnvID",
        "eng_gui": True,
        "op_gui": True,
        "data": generate_avro_data(),
    }

    return avro_message, avro_schema

def main(config_file_path, queue):
    # Read configuration from the provided file
    config = read_config(config_file_path)

    # Connect to the ZeroMQ push-type endpoint
    context = zmq.Context()
    socket = context.socket(zmq.PUSH)
    print(queue)
    if queue == "hp":
        socket.connect(config["data_hp_socket_push"])
    else:
        socket.connect(config["data_lp_socket_push"])

    # Generate and send Avro messages continuously
    while True:
        avro_message, avro_schema = create_avro_message()
        print(avro_message)

        # Serialize the Avro message using avro library
        writer = avro.io.DatumWriter(avro_schema)
        bytes_io = io.BytesIO()  # Use Python's io module
        encoder = avro.io.BinaryEncoder(bytes_io)
        writer.write(avro_message, encoder)
        avro_binary_data = bytes_io.getvalue()

        # Send the serialized Avro message via ZeroMQ
        socket.send(avro_binary_data)

        # Simulate a delay (adjust as needed)
        time.sleep(0.01)

def read_config(file_path="config.json"):
    with open(file_path, "r") as file:
        config = json.load(file)
    return config

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python script.py <config_file> <queue (lp|hp)>")
        sys.exit(1)

    config_file_path = sys.argv[1]
    queue = sys.argv[2]

    main(config_file_path, queue)

