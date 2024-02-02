import zmq
import json
import time
def subscribe_data():
    context = zmq.Context()
    socket = context.socket(zmq.SUB)
    socket.connect("tcp://127.0.0.1:5556")  # Subscriber connects to the same address

    # Subscribe to all topics (empty string)
    socket.setsockopt_string(zmq.SUBSCRIBE, "")
    time.sleep(0.1)

    try:
        while True:
            message = socket.recv_string()
            data = json.loads(message)
            print(f"Received: {data}")

    except KeyboardInterrupt:
        print("Subscriber stopped.")

if __name__ == "__main__":
    subscribe_data()

