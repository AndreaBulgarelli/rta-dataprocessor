import zmq
import json
import time


if __name__ == "__main__":
    context = zmq.Context()
    socket = context.socket(zmq.PULL)
    socket.bind("tcp://*:5557")

    while True:
        data = socket.recv_json()
        # Esegui l'aggregazione o altre operazioni sui dati ricevuti
        print("Dati ricevuti:", data)

