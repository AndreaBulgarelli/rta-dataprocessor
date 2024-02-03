import threading
import queue
import json
import time

class WorkerThread(threading.Thread):
    def __init__(self, thread_id, supervisor):
        super().__init__()
        self.supervisor = supervisor
        self.thread_id = thread_id

        self.low_priority_queue = self.supervisor.low_priority_queue
        self.high_priority_queue = self.supervisor.high_priority_queue
        self.monitoringpoint = self.supervisor.monitoringpoint

        #monitoring
        self.start_time = time.time()
        self.next_time = self.start_time
        self.processed_data_count = 0
        self.total_processed_data_count = 0
        self.processing_rate = 0

        self._stop_event = threading.Event()  # Usato per segnalare l'arresto

        print(f"Worker Thread-{self.thread_id} started")

    def stop(self):
        self._stop_event.set()  # Imposta l'evento di arresto

    def run(self):
        self.perf_thread = threading.Thread(target=self.calcdatarate, daemon=True)
        self.perf_thread.start()

        while not self._stop_event.is_set():
            #time.sleep(0.00001) #must be 0

            if self.supervisor.processdata == True:
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

    def calcdatarate(self):
        while True:
            time.sleep(10)        
            elapsed_time = time.time() - self.next_time
            if elapsed_time > 10:
                self.next_time = time.time()
                self.processing_rate = self.processed_data_count / elapsed_time
                self.total_processed_data_count += self.processed_data_count
                print(f"Thread-{self.thread_id} rate Hz {self.processing_rate} total events {self.total_processed_data_count}")

                self.processed_data_count = 0

    def process_data(self, data, priority):

        #print(f"Thread-{self.thread_id} Priority-{priority} processing data. Queues size: {self.low_priority_queue.qsize()} {self.high_priority_queue.qsize()}")
        # Increment the processed data count and calculate the rate
        self.processed_data_count += 1
        # elapsed_time = time.time() - self.next_time
        # if elapsed_time > 60:
        #     self.next_time = time.time()
        #     self.processing_rate = self.processed_data_count / elapsed_time
        #     self.processed_data_count = 0

        #Derive a class and put the code of analysis in this method
