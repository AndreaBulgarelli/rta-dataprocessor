from Supervisor import Supervisor
from WorkerManager1 import WorkerManager1

class Supervisor1(Supervisor):
	def __init__(self, config_file="config.json", name="OOQS1"):
		super().__init__(config_file, name)

	def start_manager_threads(self):
		manager = WorkerManager1(self, "S22")
		manager.start()
		self.manager_threads.append(manager)

