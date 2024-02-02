import threading
import time
import psutil

class MonitoringPoint:
    def __init__(self, process):
        self.process = process
        self.processOS = psutil.Process(self.process.pid)
        self.lock = threading.Lock()
        self.data = {}
        header = {
                "type": 1,
                "time": 0,  # Replace with actual timestamp if needed
                "pidsource": self.process.processname,
                "pidtarget": "*"
        }
        self.data["header"] = header
        self.data["status"] = "Initialised"  # Append the "status" key to the data dictionary
        procinfo = {
            "cpu_percent": 0,
            "memory_usage": 0
        }
        self.data["procinfo"] = procinfo
        #self.data = {"parameter1": 0, "parameter2": 0}  # Initialize other parameters
        print("MonitoringPoint initialised")

    def update(self, key, value):
        with self.lock:
            if key in self.data:
                self.data[key] = value
            else:
                print(f"Error: Key '{key}' not found in the data dictionary.")

    def get_data(self):
        #with self.lock:
            self.resource_monitor()
             # Create a copy of the data
            data_copy = self.data.copy()
            data_copy["header"]["time"] = time.time()
            self.set_status(self.process.status)
            
            return data_copy
 
    def set_status(self, new_status):
        with self.lock:
            self.data["status"] = new_status

    def get_status(self):
        with self.lock:
            return self.data["status"]

    def resource_monitor(self):
        
        # Monitoraggio CPU
        self.data["procinfo"]["cpu_percent"] = self.processOS.cpu_percent(interval=1)

        # Monitoraggio occupazione di memoria
        self.data["procinfo"]["memory_usage"] = self.processOS.memory_info()

        #print(f"CPU: {cpu_percent}% | Memroy: {memory_usage}% | I/O Disco: {disk_io} | I/O Rete: {network_io}")


