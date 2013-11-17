# This testcase extends LousyTestCase to add numerous useful utility functions

import lousy
import time

class TachyonTestCase(lousy.LousyTestCase):

	# Wait for the tachyon process to terminate. Fail if the timeout is exceeded
	def waitForTermination(self, timeout=5):
		startTime = time.time()
		while not self.tachyon.poll():
			if time.time() - startTime > timeout:
				self.fail('Timed out waiting to process to terminate')
			time.sleep(0.001)
		self.tachyonTerminated = True

