# This testcase extends LousyTestCase to add numerous useful utility functions

import lousy
import time
import subprocess
import re

class TachyonTestCase(lousy.TestCase):
	verbose_output = True
	META = '\x14'

	def setUp2(self):
		self.tachyon = None

	def tearDown2(self):
		if self.tachyon:
			self.tachyon.terminate()

	# Start a tachyon process with the given arguments
	def startTachyon(self, args=[], sync=True):
		cmd = ['./tachyon', '--shell="/bin/bash --noprofile --norc"']
		cmd.extend(args)
		self.tachyon = lousy.Process(cmd, shell=True, pty='vt100')
		if sync:
			self.expectPrompt('bash.*\$ ')
			self.sendLine('') # Reshow prompt

	# Wait for the tachyon process to terminate. Fail if the timeout is exceeded
	def waitForTermination(self, timeout=5):
		response = self.tachyon.waitForTermination(timeout)
		self.assertTrue(response, 'Timed out waiting to process to terminate')

	# Return a snapshot of the terminal
	def snapShot(self):
		self.syncOutput()
		return self.tachyon.vty.snapShotScreen()

	# Send the given string to tachyon as given
	def send(self, string):
		self.tachyon.send(string)

	# Send the given characters to tachyon, adding newline
	def sendLine(self, string):
		self.tachyon.sendLine(string)

	# Send a meta command with the metacharacter prepended
	def sendMeta(self, cmd):
		self.tachyon.flushOutput()
		self.send(self.META + cmd)

	def sendCmd(self, cmd):
		self.expectPrompt('bash.*\$ ')
		self.sendLine(cmd)
		self.expectOnly('.*' + re.escape(cmd) + '.*')

	# Ensure that there is no more waiting output
	def syncOutput(self):
		self.tachyon.flushOutput()

	# Wait until the timeout is seen or the given prompt is that last output on the last line
	def expectPrompt(self, prompt, timeout=5):
		result = self.tachyon.expectPrompt([prompt], timeout)
		self.assertEqual(result, 0)

	# Retrieve the n'th last line of the terminal output as if the test was a terminal emulator
	def terminalLine(self, lineFromEnd):
		output = self.tachyon.read()
		return output.split('\n')[-1 - lineFromEnd]

	def expect(self, regexes, timeout=5):
		return self.tachyon.expect(regexes, timeout)

	def expectOnly(self, regex, timeout=5):
		result = self.tachyon.expect([regex], timeout)
		self.assertEqual(result, 0)

	# Like sendCmd('exit') but handles the necessary extra synchronization
	def sendCmdExit(self):
		self.sendCmd('exit')
		time.sleep(0.5)
		self.sendLine('') # Reshow prompt

	# Create a new buffer ^tc
	def bufferCreate(self):
		self.sendMeta('c')

	# Goto the next buffer ^tn, handles special synchronization
	def bufferNext(self):
		self.sendMeta('n')
		self.sendLine('') # Reshow prompt

	# Goto the previous buffer ^tn, handles special synchronization
	def bufferPrev(self):
		self.sendMeta('p')
		self.sendLine('') # Reshow prompt
