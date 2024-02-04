from WorkerManager import WorkerManager
from WorkerThread1 import WorkerThread1
import io
import threading

class WorkerManager1(WorkerManager):
	def __init__(self, supervisor, name = "S22"):
		super().__init__(supervisor, name)

	def start_worker_threads(self, num_threads=5):
		#Worker threads
		for i in range(num_threads):
			thread = WorkerThread1(i, self, "Rate")
			self.worker_threads.append(thread)
			thread.start()


			