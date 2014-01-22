#!/usr/bin/env python2.7

import tachyon
import lousy

class TestBasicTerminal(tachyon.TachyonTestCase):
	def setUp1(self):
		self.startTachyon()

	def tearDown1(self):
		self.waitForTermination()

	def test_bufferContentsRestoredOnSwitch(self):
		self.sendCmd('ls')
		before = self.snapShot()

		self.bufferCreate()
		self.sendCmd('printenv')

		self.bufferNext()

		after = self.snapShot()
		self.assertEqual(before, after)

		self.sendCmdExit()
		self.sendCmdExit()

class TestTerminalEscapeCodes(tachyon.TachyonTestCase):
	# Start a tests/stubs/PipeStub.py client and return the matching stub
	def startPipeStub(self):
		lousy.stubs.add_class('PipeStub', tachyon.PipeStub)

		port = lousy.stubs.port()
		self.sendCmd('tests/stubs/PipeStub.py %d' % port)
		stub = lousy.stubs.waitForStub('PipeStub')
		self.assertIsNotNone(stub)

		return stub

	def setUp1(self):
		self._FrameBufferLooseEquality = True
		self.startTachyon()
		self.pipe = self.startPipeStub()

		self.bufferCreate()
		self.bufferNext()

		# Clear the screen the slow way to ensure that each test has a clean slate to start with
		msg = '\r\n' * (self.tachyon.vty.rows() + 1)
		self.pipe.write(msg)

	def tearDown1(self):
		self.pipe.disconnect()

		self.sendCmdExit()
		self.sendCmdExit()
		self.waitForTermination()

	def sendCsi(self, string):
		self.pipe.write('\033[' + string)

	def setCursorPos(self, row, col):
		self.sendCsi('%d;%df' % (int(row) + 1, int(col) + 1))

	def test_PipeStub(self):
		# Basic test to ensure the PipeSstub is operating correctly

		a = self.snapShot()

		self.pipe.write('\n\n\rasdfasdfasdfasdf')

		b = self.snapShot()

		self.assertNotEqual(a, b)

		self.assertVtyCharIs(22, 0, 'a')
		self.assertVtyString(22, 0, 'asdfasdfasdfasdf')

	def test_csiClearScreen_toEnd_default(self):
		self.setCursorPos(0, 0)

		self.pipe.write('asdfasdfasdf\r\n')
		self.pipe.write('qewrqwerqwer\r\n')

		self.setCursorPos(0, 4)

		self.sendCsi('J')

		a = self.snapShot()

		self.assertVtyString(0, 0, 'asdf')
		for col in range(4, 20):
			self.assertVtyCharIs(0, col, '')
		for col in range(20):
			self.assertVtyCharIs(1, col, '')
		
		row, col = self.tachyon.vty.cursorPosition()

		self.bufferNext()
		self.bufferNext()

		b = self.snapShot()
		self.assertEqual(a, b)

		row, col = self.vtyCursorPosition()
		self.assertEqual(row, 0)
		self.assertEqual(col, 4)

	def test_csiClearScreen_toEnd(self):
		self.setCursorPos(0, 0)

		self.pipe.write('asdfasdfasdf\r\n')
		self.pipe.write('qewrqwerqwer\r\n')

		self.setCursorPos(0, 4)

		self.sendCsi('0J')

		a = self.snapShot()

		self.assertVtyString(0, 0, 'asdf')
		for col in range(4, 20):
			self.assertVtyCharIs(0, col, '')
		for col in range(20):
			self.assertVtyCharIs(1, col, '')
		
		row, col = self.tachyon.vty.cursorPosition()

		self.bufferNext()
		self.bufferNext()

		b = self.snapShot()
		self.assertEqual(a, b)

		row, col = self.vtyCursorPosition()
		self.assertEqual(row, 0)
		self.assertEqual(col, 4)

	def test_csiClearScreen_fromStart(self):
		self.setCursorPos(0, 0)

		self.pipe.write('asdfasdfasdf\r\n')
		self.pipe.write('qewrqwerqwer\r\n')

		self.setCursorPos(1, 6)

		self.sendCsi('1J')

		a = self.snapShot()

		for col in range(20):
			self.assertVtyCharIs(0, col, '')
		for col in range(6):
			self.assertVtyCharIs(1, col, '')
		self.assertVtyString(1, 7, 'rqwer')
		
		row, col = self.vtyCursorPosition()

		self.bufferNext()
		self.bufferNext()

		b = self.snapShot()
		self.assertEqual(a, b)

		row, col = self.vtyCursorPosition()
		self.assertEqual(row, 1)
		self.assertEqual(col, 6)

	def test_csiClearScreen_all(self):
		a = self.snapShot()

		self.pipe.write('asdfasdfasdf\r\n')
		self.pipe.write('qewrqwerqwer\r\n')

		self.sendCsi('2J')
		
		b = self.snapShot()
		self.assertEqual(a, b)

		row, col = self.tachyon.vty.cursorPosition()
		self.assertEqual(row, 23)
		self.assertEqual(col, 0)

		self.bufferNext()
		self.bufferNext()

		b = self.snapShot()
		self.assertEqual(a, b)

		row, col = self.tachyon.vty.cursorPosition()
		self.assertEqual(row, 23)
		self.assertEqual(col, 0)

	def test_csiCursorPosition(self):
		self.pipe.write('adfasdfasdfasdf\r\n;lkjh')

		row, col = self.vtyCursorPosition()
		self.assertEqual(row, 23)
		self.assertEqual(col, 5)

		self.sendCsi('6;6f')

		self.bufferNext()
		self.bufferNext()

		row, col = self.vtyCursorPosition()
		self.assertEqual(row, 5)
		self.assertEqual(col, 5)

		self.pipe.write('z')

		self.assertVtyCharIs(5, 5, 'z')

	def test_csiCursorPosition_default(self):
		self.pipe.write('adfasdfasdfasdf\r\n;lkjh')

		row, col = self.vtyCursorPosition()
		self.assertEqual(row, 23)
		self.assertEqual(col, 5)

		self.sendCsi('f')

		self.bufferNext()
		self.bufferNext()

		row, col = self.vtyCursorPosition()
		self.assertEqual(row, 0)
		self.assertEqual(col, 0)

		self.pipe.write('z')

		self.assertVtyCharIs(0, 0, 'z')

	def test_csiCursorPosition_empty(self):
		self.pipe.write('adfasdfasdfasdf\r\n;lkjh')

		row, col = self.vtyCursorPosition()
		self.assertEqual(row, 23)
		self.assertEqual(col, 5)

		self.sendCsi(';f')

		self.bufferNext()
		self.bufferNext()

		row, col = self.vtyCursorPosition()
		self.assertEqual(row, 0)
		self.assertEqual(col, 0)

		self.pipe.write('z')

		self.assertVtyCharIs(0, 0, 'z')
