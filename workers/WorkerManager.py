from MonitoringPoint import MonitoringPoint
from WorkerThread import WorkerThread
from MonitoringThread import MonitoringThread
import json
import zmq
import queue
import threading
import time
import sys
import psutil
import threading

class WorkerManager(threading.Thread):
    def __init__(self, supervisor, name = "None"):
        super().__init__()
        self.supervisor = supervisor
        self.config_data = self.supervisor.config_data
        self.name = name
        self.globalname = "WorkerManager-"+self.supervisor.name + "-" + name
        self.continueall = True

        self.pid = psutil.Process().pid

        self.context = self.supervisor.context
                
        self.socket_monitoring = self.supervisor.socket_monitoring

        self.socket_result = self.context.socket(zmq.PUSH)
        self.socket_result.connect(self.config_data["result_socket_push"])

        self.low_priority_queue = queue.Queue()
        self.high_priority_queue = queue.Queue()

        self.monitoringpoint = None
        self.monitoring_thread = None

        self.worker_threads = []
        self.status = "Initialised"
        #process data based on Supervisor state
        self.processdata = False
        self.suspenddata = False

        self._stop_event = threading.Event()  # Usato per segnalare l'arresto

        print(f"{self.globalname} started")

    def start_service_threads(self):
        #Monitoring thread
        self.monitoringpoint = MonitoringPoint(self)
        self.monitoring_thread = MonitoringThread(self.socket_monitoring, self.monitoringpoint)
        self.monitoring_thread.start()

    #to be reimplemented
    def start_worker_threads(self, num_threads=5):
        #Worker threads
        for i in range(num_threads):
            thread = WorkerThread(i, self)
            self.worker_threads.append(thread)
            thread.start()

    def run(self):
        self.start_service_threads()

        self.status = "Waiting"

        try:
            while not self._stop_event.is_set():
                time.sleep(1)  # Puoi aggiungere una breve pausa per evitare il loop infinito senza utilizzo elevato della CPU

            print(f"Manager stop {self.globalname}")
        except KeyboardInterrupt:
            print("Keyboard interrupt received. Terminating.")
            self.stop_threads()
            self.continueall = False

    def stop(self):
        self.stop_threads()
        self._stop_event.set()  # Imposta l'evento di arresto

    def stop_threads(self):
        print("Stopping Manager threads...")
        # Stop monitoring thread
        self.monitoring_thread.stop()
        self.monitoring_thread.join()

        # Stop worker threads
        # for thread in self.worker_threads:
        #     thread.stop()
        #     thread.join()

        print("All Manager threads terminated.")


