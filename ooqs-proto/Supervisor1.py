from Supervisor import Supervisor
from WorkerThread1 import WorkerThread1

class Supervisor1(Supervisor):
    def __init__(self, config_file="config.json", name="None"):
        super().__init__(config_file, name)

    def start_threads(self, num_threads=5):
        #Worker threads
        for i in range(num_threads):
            thread = WorkerThread1(i, self)
            self.worker_threads.append(thread)
            thread.start()
            print("Started ooqs threads")

