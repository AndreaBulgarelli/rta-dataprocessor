import threading
import queue
import json
import time

class WorkerThread(threading.Thread):
    def __init__(self, thread_id, low_priority_queue, high_priority_queue, monitoringpoint, processname):
        super().__init__()
        self.processname = processname
        self.thread_id = thread_id
        self.low_priority_queue = low_priority_queue
        self.high_priority_queue = high_priority_queue
        self.monitoringpoint = monitoringpoint
        self._stop_event = threading.Event()  # Usato per segnalare l'arresto

    def stop(self):
        self._stop_event.set()  # Imposta l'evento di arresto

    def run(self):
        while not self._stop_event.is_set():
            time.sleep(1)
            #print("data processing")
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

    def process_data(self, data, priority):
        # Process the data based on the priority
        print(f"Thread-{self.thread_id} Priority-{priority} processing data: {data}")

        # Update monitoringpoint data
        self.monitoringpoint.update({"parameter1": 1, "parameter2": 2})  # Update with your data

