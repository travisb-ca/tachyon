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

	# Assert the terminal cell is the given character
	def assertVtyCharIs(self, row, col, char):
		self.syncOutput()
		cell = self.tachyon.vty.cell(row, col)
		self.assertEqual(char, cell.char)

	# Assert the terminal cell has the given set of attributes, such as bold
	def assertVtyCharAttrIs(self, row, col, attr_list):
		self.syncOutput()
		cell = self.tachyon.vty.cell(row, col)
		self.assertIsNotNone(cell, 'cell at (%d, %d)' % (row, col))
		attributes = set(attr_list)
		msg = 'Attributes "%s" != "%s" at cell (%d, %d)' % (attributes, cell.attributes, row, col)
		self.assertEqual(cell.attributes, attributes, msg)

	# Assert an entire string starting at the given coordinates
	def assertVtyString(self, row, col, string):
		self.syncOutput()
		vtyString = self.tachyon.vty.string(row, col, len(string))

		if string != vtyString:
			self.tachyon.vty.snapShotScreen(forcePrint=True)

		self.assertEqual(string, vtyString)

	# Return the Vty cursor position as a 2-tuple
	def vtyCursorPosition(self):
		self.syncOutput()
		return self.tachyon.vty.cursorPosition()

	# Get the emulated VT width
	def vtyCols(self):
		self.syncOutput()
		return self.tachyon.vty.cols()

	# Maximum column number, to save from writing vtyCols() - 1 all the time
	def vtyMaxCol(self):
		return self.vtyCols() - 1

	# Get the emulated VT height
	def vtyRows(self):
		self.syncOutput()
		return self.tachyon.vty.rows()

	# Maximum row number, to save from writing vtyRows() - 1 all the time
	def vtyMaxRow(self):
		return self.vtyRows() - 1

	# Ensure the cursor position is at the specified position
	# A value of -1 means don't test the row/column
	def assertVtyCursorPos(self, row=-1, col=-1):
		vty_row, vty_col = self.vtyCursorPosition()
		if row != -1:
			self.assertEqual(vty_row, row)
		if col != -1:
			self.assertEqual(vty_col, col)

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
		self.syncOutput()
		self.sendLine('') # Reshow prompt
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

	# Goto the previous buffer ^tn, handles special synchronization
	def bufferPrev(self):
		self.sendMeta('p')
		self.sendLine('') # Reshow prompt

class PipeStub(lousy.Stub):
	# This is stub which just punts strings back and forth.
	# tests/stubs/PipeStub.py is the script counterpart to this stub.
	
	type = 'PipeStub'
