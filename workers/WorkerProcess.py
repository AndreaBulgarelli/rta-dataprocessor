import queue
import multiprocessing
import json
import time
from multiprocessing import Event, Queue, Process
from threading import Timer
import psutil

class WorkerProcess(Process):
    def __init__(self, worker_id, manager, processdata_shared, name="None"):
        super().__init__()
        self.manager = manager
        self.worker_id = worker_id
        self.name = name
        self.pidprocess = psutil.Process().pid
        self.globalname = f"WorkerProcess-{self.manager.supervisor.name}-{self.manager.name}-{self.name}-{self.worker_id}"

        self.low_priority_queue = self.manager.low_priority_queue
        self.high_priority_queue = self.manager.high_priority_queue
        self.monitoringpoint = self.manager.monitoringpoint

        #monitoring
        self.start_time = time.time()
        self.next_time = self.start_time
        self.processed_data_count = 0
        self.total_processed_data_count = 0
        self.processing_rate = 0

        self._stop_event = Event()  # Usato per segnalare l'arresto

        self.processdata = 0
        self.processdata_shared = processdata_shared

        self.testvar = "hello"

        print(f"{self.globalname} started {self.pidprocess}")

    def stop(self):
        self._stop_event.set()  # Imposta l'evento di arresto
        #self.timer.cancel()

    def set_processdata(self, processdata1):
        print(f"Worker {self.globalname} set_processdata {processdata1}")
        self.processdata=processdata1
        print(f"Worker {self.globalname} set_processdata {self.processdata}")

    def run(self):
        print(f"{self.globalname} run entering...")
        self.start_timer(10)

        while not self._stop_event.is_set():
            time.sleep(0.01) #must be 0
            #print(f"wait. {self.globalname} / {self.processdata} Queues size: {self.manager.low_priority_queue.qsize()} {self.manager.high_priority_queue.qsize()}")  

            if self.processdata_shared.value == 1:
                #print(f"wait2. {self.globalname} / {self.processdata} Queues size: {self.manager.low_priority_queue.qsize()} {self.manager.high_priority_queue.qsize()}")

                try:
                    # Check and process high-priority queue first
                    high_priority_data = self.high_priority_queue.get_nowait()
                    self.process_data(high_priority_data, priority="High")
                except queue.Empty:
                    try:
                        # Process low-priority queue if high-priority queue is empty
                        low_priority_data = self.low_priority_queue.get(timeout=1)
                        self.process_data(low_priority_data, priority="Low")
                    except queue.Empty:
                        pass  # Continue if both queues are empty

        self.timer.cancel()
        print(f"WorkerProcess stop {self.globalname}")

    def start_timer(self, interval):
        self.timer = Timer(interval, self.calcdatarate)
        self.timer.start()

    def calcdatarate(self):    
        elapsed_time = time.time() - self.next_time
        self.next_time = time.time()
        self.processing_rate = self.processed_data_count / elapsed_time
        self.total_processed_data_count += self.processed_data_count
        print(f"{self.globalname} rate Hz {self.processing_rate:.1f} total events {self.total_processed_data_count}. State {self.processdata_shared.value}")
        self.processed_data_count = 0
        if not self._stop_event.is_set():
            self.start_timer(10)

    def process_data(self, data, priority):

        #print(f"Thread-{self.worker_id} Priority-{priority} processing data. Queues size: {self.low_priority_queue.qsize()} {self.high_priority_queue.qsize()}")
        # Increment the processed data count and calculate the rate
        self.processed_data_count += 1

        #Derive a class and put the code of analysis in this method