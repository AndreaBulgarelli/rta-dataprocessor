from Supervisor import Supervisor
from WorkerManager1 import WorkerManager1

class Supervisor1(Supervisor):
	def __init__(self, config_file="config.json", name="OOQS1"):
		super().__init__(config_file, name)

	def start_manager_threads(self):
		manager1 = WorkerManager1(self, "S22Rate")
		manager1.start()
		self.manager_threads.append(manager1)

		# manager2 = WorkerManager1(self, "S22Mean")
		# manager2.start()
		# self.manager_threads.append(manager2)

