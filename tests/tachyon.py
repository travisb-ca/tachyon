# This testcase extends LousyTestCase to add numerous useful utility functions

import lousy
import time
import subprocess

class TachyonTestCase(lousy.TestCase):
	META = '\x14'

	def setUp(self):
		self.tachyon = None

	def tearDown(self):
		if self.tachyon:
			self.tachyon.terminate()

	# Start a tachyon process with the given arguments
	def startTachyon(self, args):
		self.tachyon = lousy.Process('./tachyon ' + args, shell=True)

	# Wait for the tachyon process to terminate. Fail if the timeout is exceeded
	def waitForTermination(self, timeout=5):
		response = self.tachyon.waitForTermination(timeout)
		self.assertTrue(response, 'Timed out waiting to process to terminate')

	# Send the given string to tachyon as given
	def sendString(self, string):
		self.tachyon.stdin.write(string)

	# Send a meta command with the metacharacter prepended
	def sendMeta(self, cmd):
		self.sendString(self.META + cmd)

	# Retrieve the n'th last line of the terminal output as if the test was a terminal emulator
	def terminalLine(self, lineFromEnd):
		return 'not implemented'

