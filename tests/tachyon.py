# This testcase extends LousyTestCase to add numerous useful utility functions

import lousy
import time
import subprocess

class TachyonTestCase(lousy.TestCase):

	def setUp(self):
		self.tachyon = None
		self.tachyonTerminated = False

	def tearDown(self):
		if self.tachyon and not self.tachyonTerminated:
			self.tachyon.kill()

	# Start a tachyon process with the given arguments
	def startTachyon(self, args):
		self.tachyon = subprocess.Popen('./tachyon ' + args, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		self.tachyonTerminated = False

	# Wait for the tachyon process to terminate. Fail if the timeout is exceeded
	def waitForTermination(self, timeout=5):
		startTime = time.time()
		while not self.tachyon.poll():
			if time.time() - startTime > timeout:
				self.fail('Timed out waiting to process to terminate')
			time.sleep(0.001)
		self.tachyonTerminated = True

