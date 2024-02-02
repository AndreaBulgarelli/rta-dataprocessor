from MonitoringPoint import MonitoringPoint
from WorkerThread import WorkerThread
from MonitoringThread import MonitoringThread
import json
import zmq
import queue
import threading
import time
import sys

class Supervisor:
    def __init__(self, config_file="config.json", name = "None", pid="0"):
        self.load_configuration(config_file)
        self.processname = name
        self.continueall = True
        self.pid = pid

        self.context = zmq.Context()
        
        #low priority data connection
        self.socket_lp_data = self.context.socket(zmq.PULL)
        self.socket_lp_data.bind(self.config_data["data_lp_socket_pull"])
        #high priority data connection
        self.socket_hp_data = self.context.socket(zmq.PULL)
        self.socket_hp_data.bind(self.config_data["data_hp_socket_pull"])
        
        self.socket_command = self.context.socket(zmq.SUB)
        self.socket_command.connect(self.config_data["command_socket_pubsub"])
        self.socket_command.setsockopt_string(zmq.SUBSCRIBE, "")  # Subscribe to all topics
        
        self.socket_monitoring = self.context.socket(zmq.PUSH)
        self.socket_monitoring.connect(self.config_data["monitoring_socket_push"])

        self.socket_result = self.context.socket(zmq.PUSH)
        self.socket_result.connect(self.config_data["result_socket_push"])

        self.low_priority_queue = queue.Queue()
        self.high_priority_queue = queue.Queue()
        self.command_queue = queue.Queue()

        self.monitoringpoint = MonitoringPoint(self)
        self.monitoring_thread = None

        self.worker_threads = []
        self.status = "Initialised"
        

    def load_configuration(self, config_file):
        try:
            with open(config_file, "r") as file:
                self.config_data = json.load(file)
                print(self.config_data)
        except FileNotFoundError:
            print(f"Error: File '{config_file}' not found.")
            return
        except json.JSONDecodeError:
            print(f"Error: Invalid JSON format in file '{config_file}'.")
            return

        self.num_threads = self.config_data.get("num_threads", 5)


    def start_threads(self, num_threads=5):
        
        #Monitoring thread
        self.monitoring_thread = MonitoringThread(self.socket_monitoring, self.monitoringpoint)
        self.monitoring_thread.start()

        self.command_thread = threading.Thread(target=self.listen_for_commands, daemon=True)
        self.command_thread.start()

        self.lp_data_thread = threading.Thread(target=self.listen_for_lp_data, daemon=True)
        self.lp_data_thread.start()

        self.hp_data_thread = threading.Thread(target=self.listen_for_hp_data, daemon=True)
        self.hp_data_thread.start()

        #Worker threads
        for i in range(num_threads):
            thread = WorkerThread(i, self.low_priority_queue, self.high_priority_queue, self.monitoringpoint, self)
            self.worker_threads.append(thread)
            thread.start()



    def start(self):
        self.start_threads(self.num_threads)

        self.status = "Waiting"

        try:
            while self.continueall:
                time.sleep(1)  # Puoi aggiungere una breve pausa per evitare il loop infinito senza utilizzo elevato della CPU
        except KeyboardInterrupt:
            print("Ricevuta interruzione da tastiera. Terminazione in corso.")
            self.stop_threads()
            self.continueall = False

    def listen_for_lp_data(self):
        while True:
            data = self.socket_lp_data.recv()
            self.low_priority_queue.put(data) 
            self.monitoringpoint.update("queue_lp_size", self.low_priority_queue.qsize())
            #print("low_priority_queue")

    def listen_for_hp_data(self):
        while True:
            data = self.socket_hp_data.recv()
            self.high_priority_queue.put(data) 
            self.monitoringpoint.update("queue_hp_size", self.high_priority_queue.qsize())
            #print("high_priority_queue")

    def listen_for_commands(self):
        while True:
            print("Pulling commands")
            command = json.loads(self.socket_command.recv_string())
            self.process_command(command)

    def process_command(self, command):
        print(f"Received command: {command}")
        subtype_value = command['header']['subtype']
        pidtarget = command['header']['pidtarget']
        pidsource = command['header']['pidsource']
        if pidtarget == self.processname or pidtarget == "all".lower() or pidtarget == "*":
            if subtype_value == "shutdown":
                self.status = "Shutdown"
                self.stop_threads()
                self.continueall = False
            if subtype_value == "getstatus":
                self.monitoring_thread.sendto(pidsource)
            if subtype_value == "start": #data processing
                self.status = "Processing"
                pass
            if subtype_value == "suspend": #data processing
                self.status = "Suspend"
                pass
            if subtype_value == "restart": #data processing
                self.status = "Processing"
                pass       
            if subtype_value == "stop": #data processing
                self.status = "Waiting"
                pass    
        monitoringpoint_data = self.monitoringpoint.get_data()
        print(f"MonitoringPoint data: {monitoringpoint_data}")

    def stop_threads(self):
        print("Stopping threads...")
        # Stop monitoring thread
        self.monitoring_thread.stop()
        self.monitoring_thread.join()

        # Stop worker threads
        for thread in self.worker_threads:
            thread.stop()
            thread.join()

        print("All threads terminated.")


