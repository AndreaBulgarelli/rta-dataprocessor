import zmq
import json
import time

def publish_data():
    context = zmq.Context()
    socket = context.socket(zmq.PUB)
    socket.bind("tcp://127.0.0.1:5556")

    data_sequence = [
        {"parameter1": 1, "parameter2": 2, "status": "Running"},
        {"parameter1": 3, "parameter2": 4, "status": "Paused"},
        {"parameter1": 5, "parameter2": 6, "status": "Stopped"}
    ]
    time.sleep(1)
    for data in data_sequence:
        message = json.dumps(data)
        socket.send_string(message)
        print(f"Sent: {message}")
        time.sleep(1)

if __name__ == "__main__":
    publish_data()
