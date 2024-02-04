import threading
import queue
import json
import time
from threading import Timer

class WorkerThread(threading.Thread):
    def __init__(self, thread_id, manager):
        super().__init__()
        self.manager = manager
        self.thread_id = thread_id
        self.name = "WorkerThread"+str(self.thread_id)

        self.low_priority_queue = self.manager.low_priority_queue
        self.high_priority_queue = self.manager.high_priority_queue
        self.monitoringpoint = self.manager.monitoringpoint

        #monitoring
        self.start_time = time.time()
        self.next_time = self.start_time
        self.processed_data_count = 0
        self.total_processed_data_count = 0
        self.processing_rate = 0

        self._stop_event = threading.Event()  # Usato per segnalare l'arresto

        print(f"Worker {self.name} started")

    def stop(self):
        self._stop_event.set()  # Imposta l'evento di arresto
        self.timer.cancel()


    def run(self):
        self.start_timer(10)

        while not self._stop_event.is_set():
            time.sleep(0.00001) #must be 0

            if self.manager.processdata == True:
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
        
        print(f"Worker stop {self.name}")

    def start_timer(self, interval):
        self.timer = Timer(interval, self.calcdatarate)
        self.timer.start()

    def calcdatarate(self):    
        elapsed_time = time.time() - self.next_time
        self.next_time = time.time()
        self.processing_rate = self.processed_data_count / elapsed_time
        self.total_processed_data_count += self.processed_data_count
        print(f"Thread-{self.thread_id} rate Hz {self.processing_rate} total events {self.total_processed_data_count}")
        self.processed_data_count = 0
        
        self.start_timer(10)

    def process_data(self, data, priority):

        #print(f"Thread-{self.thread_id} Priority-{priority} processing data. Queues size: {self.low_priority_queue.qsize()} {self.high_priority_queue.qsize()}")
        # Increment the processed data count and calculate the rate
        self.processed_data_count += 1

        #Derive a class and put the code of analysis in this method
