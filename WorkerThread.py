import threading
import queue
import json
import time

class WorkerThread(threading.Thread):
    def __init__(self, thread_id, low_priority_queue, high_priority_queue, monitoringpoint, process):
        super().__init__()
        self.process = process
        self.start_time = time.time()
        self.next_time = time.time()
        self.processname = self.process.processname
        self.thread_id = thread_id
        self.low_priority_queue = low_priority_queue
        self.high_priority_queue = high_priority_queue
        self.monitoringpoint = monitoringpoint
        self.processed_data_count = 0
        self.processing_rate = 0
        self._stop_event = threading.Event()  # Usato per segnalare l'arresto

    def stop(self):
        self._stop_event.set()  # Imposta l'evento di arresto

    def run(self):
        while not self._stop_event.is_set():
            time.sleep(0.01) #must be 0

            if self.process.status == "Processing":
                try:
                    # Check and process high-priority queue first
                    high_priority_data = self.high_priority_queue.get_nowait()
                    self.monitoringpoint.update("queue_hp_size", self.high_priority_queue.qsize())
                    self.process_data(high_priority_data, priority="High")
                except queue.Empty:
                    try:
                        # Process low-priority queue if high-priority queue is empty
                        low_priority_data = self.low_priority_queue.get(timeout=1)
                        self.monitoringpoint.update("queue_lp_size", self.low_priority_queue.qsize())
                        self.process_data(low_priority_data, priority="Low")
                    except queue.Empty:
                        pass  # Continue if both queues are empty

    def process_data(self, data, priority):
        # Process the data based on the priority
        print(f"Thread-{self.thread_id} Priority-{priority} processing data. Queues size: {self.low_priority_queue.qsize()} {self.high_priority_queue.qsize()}")
        # Increment the processed data count and calculate the rate
        self.processed_data_count += 1
        elapsed_time = time.time() - self.next_time
        if elapsed_time > 60:
            self.process.next_time = time.time()
            self.processing_rate = self.processed_data_count / elapsed_time
            self.processed_data_count = 0
        
        # Update monitoringpoint data
        #self.monitoringpoint.update({"parameter1": 1, "parameter2": 2})  # Update with your data

