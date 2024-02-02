import threading
import time

class MonitoringPoint:
    def __init__(self, process):
        self.process = process
        self.lock = threading.Lock()
        self.data = {"parameter1": 0, "parameter2": 0}  # Initialize other parameters
        self.data["status"] = "Initialised"  # Append the "status" key to the data dictionary
        print("MonitoringPoint initialised")

    def update(self, key, value):
        with self.lock:
            if key in self.data:
                self.data[key] = value
            else:
                print(f"Error: Key '{key}' not found in the data dictionary.")

    def get_data(self):
        #with self.lock:
             # Create a copy of the data
            data_copy = self.data.copy()

            # Add the header to the copy
            header = {
                "type": 1,
                "time": time.time(),  # Replace with actual timestamp if needed
                "pidsource": self.process.processname,
                "pidtarget": "*"
            }
            data_copy["header"] = header
            self.set_status(self.process.status)
            return data_copy
            # Return a copy of the housekeeping data within the critical section

    def set_status(self, new_status):
        with self.lock:
            self.data["status"] = new_status

    def get_status(self):
        with self.lock:
            return self.data["status"]
