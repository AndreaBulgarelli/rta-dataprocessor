from MonitoringPoint import MonitoringPoint
from WorkerThread import WorkerThread
from MonitoringThread import MonitoringThread
from WorkerManager import WorkerManager
import json
import zmq
import queue
import threading
import time
import sys
import psutil

class Supervisor:
    def __init__(self, config_file="config.json", name = "None"):
        self.load_configuration(config_file)
        self.name = name
        self.globalname = "Supervisor-"+name
        self.continueall = True


        self.pid = psutil.Process().pid

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
        # self.monitoringpoint = MonitoringPoint(self)
        # self.monitoring_thread = None

        self.manager_threads = []
        self.status = "Initialised"
        #process data based on Supervisor state
        self.processdata = False
        self.suspenddata = False
        print(f"{self.globalname} started")

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

    def start_service_threads(self):
        #Monitoring thread
        # self.monitoring_thread = MonitoringThread(self.socket_monitoring, self.monitoringpoint)
        # self.monitoring_thread.start()
        #Command receiving thread
        #self.command_thread = threading.Thread(target=self.listen_for_commands, daemon=True)
        #self.command_thread.start()

        #Data receiving on two queues: high and low priority
        self.lp_data_thread = threading.Thread(target=self.listen_for_lp_data, daemon=True)
        self.lp_data_thread.start()

        self.hp_data_thread = threading.Thread(target=self.listen_for_hp_data, daemon=True)
        self.hp_data_thread.start()

    #to be reimplemented ####
    def start_manager_threads(self):
        manager = WorkerManager(self, "Generic")
        manager.start()
        self.manager_threads.append(manager)

    def start_worker_threads(self, num_threads=5):
        #Worker threads
        for manager in self.manager_threads: 
            manager.start_worker_threads(num_threads)


    def start(self):
        self.start_service_threads()
        self.start_manager_threads()
        self.start_worker_threads(self.num_threads)

        self.status = "Waiting"

        try:
            while self.continueall:
                self.listen_for_commands()
                time.sleep(1)  # Puoi aggiungere una breve pausa per evitare il loop infinito senza utilizzo elevato della CPU
                print("wait")
        except KeyboardInterrupt:
            print("Keyboard interrupt received. Terminating.")
            self.stop_threads()
            self.continueall = False

    def listen_for_lp_data(self):
        while True:
            if not self.suspenddata:
                data = self.socket_lp_data.recv()
                for manager in self.manager_threads: 
                    manager.low_priority_queue.put(data) 

    def listen_for_hp_data(self):
        while True:
            if not self.suspenddata:
                data = self.socket_hp_data.recv()
                for manager in self.manager_threads: 
                    self.high_priority_queue.put(data) 


    def listen_for_commands(self):
        while True:
            print("Waiting for commands")
            command = json.loads(self.socket_command.recv_string())
            self.process_command(command)

    def process_command(self, command):
        print(f"Received command: {command}")
        subtype_value = command['header']['subtype']
        pidtarget = command['header']['pidtarget']
        pidsource = command['header']['pidsource']
        if pidtarget == self.name or pidtarget == "all".lower() or pidtarget == "*":
            if subtype_value == "shutdown":
                self.status = "Shutdown"
                self.stop_threads()
                self.continueall = False
            if subtype_value == "cleanedshutdown":
                self.status = "EndingProcessing"
                self.suspenddata = True
                for manager in self.manager_threads:
                    manager.status = "EndingProcessing"
                    manager.suspenddata = True
                    while manager.low_priority_queue.qsize() != 0 and manager.low_priority_queue.qsize() != 0:
                        time.sleep(0.1)
                    print(f"Queues of manager {manager.globalname} are empty {manager.low_priority_queue.qsize()} {manager.low_priority_queue.qsize() }")
                    manager.status = "Shutdown"
                    
                self.status = "Shutdown"
                self.stop_threads()
                self.continueall = False
            if subtype_value == "getstatus":
                for manager in self.manager_threads:
                    manager.monitoring_thread.sendto(pidsource)
            if subtype_value == "start": #data processing
                self.status = "Processing"
                for manager in self.manager_threads:
                    manager.status = "Processing"
                    manager.processdata = True
                pass
            if subtype_value == "suspend": #data processing
                self.status = "Suspend"
                for manager in self.manager_threads:
                    manager.status = "Suspend"
                    manager.processdata = False
                pass
            if subtype_value == "suspenddata": #data acquisition
                for manager in self.manager_threads:
                    manager.suspenddata = True
                pass
            if subtype_value == "startdata": #data acquisition
                for manager in self.manager_threads:
                    manager.suspenddata = False
                pass
            if subtype_value == "restart": #data processing
                self.status = "Processing"
                for manager in self.manager_threads:
                    manager.status = "Processing"
                    manager.processdata = True
                pass       
            if subtype_value == "stop": #data processing
                self.status = "Waiting"
                for manager in self.manager_threads:
                    manager.status = "Waiting"
                    manager.processdata = False
                pass    
        # monitoringpoint_data = self.monitoringpoint.get_data()
        # print(f"MonitoringPoint data: {monitoringpoint_data}")

    def stop_threads(self):
        print("Stopping all threads...")
        # Stop monitoring thread
        # self.monitoring_thread.stop()
        # self.monitoring_thread.join()

        # Stop worker threads
        for manager in self.manager_threads: 
            for thread in manager.worker_threads:
                thread.stop()
                thread.join()
            manager.stop()
            manager.join()

        print("All Supervisor threads terminated.")
        sys.exit(1)

