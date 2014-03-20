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
		self.assertSurvivesBufferChange()

		self.pipe.disconnect()

		self.sendCmdExit()
		self.sendCmdExit()
		self.waitForTermination()

	def assertSurvivesBufferChange(self):
		if lousy._debug:
			print 'Ensuring framebuffer survived buffer switch'
		a = self.snapShot()
		a_row, a_col = self.vtyCursorPosition()
		if lousy._debug:
			print 'Saved sample a'

		self.bufferNext()
		self.bufferNext()

		b = self.snapShot()
		b_row, b_col = self.vtyCursorPosition()
		if lousy._debug:
			print 'Saved sample b'

		self.assertEqual(a, b)
		self.assertEqual(a_row, b_row)
		self.assertEqual(a_col, b_col)

	def sendEsc(self, string):
		self.pipe.write('\033' + string)

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

		self.assertVtyCharIs(23, 0, 'a')
		self.assertVtyString(23, 0, 'asdfasdfasdfasdf')

	def test_csiClearScreen_toEnd_default(self):
		self.setCursorPos(0, 0)

		self.pipe.write('asdfasdfasdf\r\n')
		self.pipe.write('qewrqwerqwer\r\n')

		self.setCursorPos(0, 4)

		self.sendCsi('J')

		self.assertVtyString(0, 0, 'asdf')
		for col in range(4, 20):
			self.assertVtyCharIs(0, col, '')
		for col in range(20):
			self.assertVtyCharIs(1, col, '')

	def test_csiClearScreen_toEnd(self):
		self.setCursorPos(0, 0)

		self.pipe.write('asdfasdfasdf\r\n')
		self.pipe.write('qewrqwerqwer\r\n')

		self.setCursorPos(0, 4)

		self.sendCsi('0J')

		self.assertVtyString(0, 0, 'asdf')
		for col in range(4, 20):
			self.assertVtyCharIs(0, col, '')
		for col in range(20):
			self.assertVtyCharIs(1, col, '')

	def test_csiClearScreen_fromStart(self):
		self.setCursorPos(0, 0)

		self.pipe.write('asdfasdfasdf\r\n')
		self.pipe.write('qewrqwerqwer\r\n')

		self.setCursorPos(1, 6)

		self.sendCsi('1J')

		for col in range(20):
			self.assertVtyCharIs(0, col, '')
		for col in range(6):
			self.assertVtyCharIs(1, col, '')
		self.assertVtyString(1, 7, 'rqwer')

	def test_csiClearScreen_all(self):
		a = self.snapShot()

		self.pipe.write('asdfasdfasdf\r\n')
		self.pipe.write('qewrqwerqwer\r\n')

		self.sendCsi('2J')
		
		b = self.snapShot()
		self.assertEqual(a, b)

		row, col = self.vtyCursorPosition()
		self.assertEqual(row, 23)
		self.assertEqual(col, 0)

	def test_csiCursorPosition(self):
		self.pipe.write('adfasdfasdfasdf\r\n;lkjh')

		row, col = self.vtyCursorPosition()
		self.assertEqual(row, 23)
		self.assertEqual(col, 5)

		self.sendCsi('6;6f')

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

		row, col = self.vtyCursorPosition()
		self.assertEqual(row, 0)
		self.assertEqual(col, 0)

		self.pipe.write('z')
		self.assertVtyCharIs(0, 0, 'z')

	def test_csiCursorUp_default(self):
		self.pipe.write('adsfasdfadsf\r\nhjklhkjl')

		row, col = self.vtyCursorPosition()
		self.assertEqual(row, 23)

		self.sendCsi('A')
		self.sendCsi('A')
		self.sendCsi('A')

		self.pipe.write('z')

		row, col = self.vtyCursorPosition()
		self.assertEqual(row, 20)
		self.assertVtyCharIs(20, 8, 'z')

	def test_csiCursorUp_one(self):
		self.pipe.write('adsfasdfadsf\r\nhjklhkjl')

		row, col = self.vtyCursorPosition()
		self.assertEqual(row, 23)

		self.sendCsi('1A')
		self.sendCsi('1A')
		self.sendCsi('1A')

		self.pipe.write('z')

		row, col = self.vtyCursorPosition()
		self.assertEqual(row, 20)
		self.assertVtyCharIs(20, 8, 'z')

	def test_csiCursorUp_zero(self):
		self.pipe.write('adsfasdfadsf\r\nhjklhkjl')

		row, col = self.vtyCursorPosition()
		self.assertEqual(row, 23)

		self.sendCsi('0A')
		self.sendCsi('0A')
		self.sendCsi('0A')

		self.pipe.write('z')

		row, col = self.vtyCursorPosition()
		self.assertEqual(row, 20)
		self.assertVtyCharIs(20, 8, 'z')

	def test_csiCursorUp_arg(self):
		self.pipe.write('adsfasdfadsf\r\nhjklhkjl')

		row, col = self.vtyCursorPosition()
		self.assertEqual(row, 23)

		self.sendCsi('3A')

		self.pipe.write('z')

		row, col = self.vtyCursorPosition()
		self.assertEqual(row, 20)
		self.assertVtyCharIs(20, 8, 'z')

	def test_csiCursorUp_pastMargin(self):
		self.pipe.write('adsfasdfadsf\r\nhjklhkjl')

		row, col = self.vtyCursorPosition()
		self.assertEqual(row, 23)

		self.sendCsi('300A')

		self.pipe.write('z')

		row, col = self.vtyCursorPosition()
		self.assertEqual(row, 0)
		self.assertVtyCharIs(0, 8, 'z')

	def test_csiCursorDown_default(self):
		self.setCursorPos(0, 0)
		self.pipe.write('adsfasdfadsf\r\nhjklhkjl')

		row, col = self.vtyCursorPosition()
		self.assertEqual(row, 1)

		self.sendCsi('B')
		self.sendCsi('B')
		self.sendCsi('B')

		self.pipe.write('z')

		row, col = self.vtyCursorPosition()
		self.assertEqual(row, 4)
		self.assertVtyCharIs(4, 8, 'z')

	def test_csiCursorDown_one(self):
		self.setCursorPos(0, 0)
		self.pipe.write('adsfasdfadsf\r\nhjklhkjl')

		row, col = self.vtyCursorPosition()
		self.assertEqual(row, 1)

		self.sendCsi('1B')
		self.sendCsi('1B')
		self.sendCsi('1B')

		self.pipe.write('z')

		row, col = self.vtyCursorPosition()
		self.assertEqual(row, 4)
		self.assertVtyCharIs(4, 8, 'z')

	def test_csiCursorDown_zero(self):
		self.setCursorPos(0, 0)
		self.pipe.write('adsfasdfadsf\r\nhjklhkjl')

		row, col = self.vtyCursorPosition()
		self.assertEqual(row, 1)

		self.sendCsi('0B')
		self.sendCsi('0B')
		self.sendCsi('0B')

		self.pipe.write('z')

		row, col = self.vtyCursorPosition()
		self.assertEqual(row, 4)
		self.assertVtyCharIs(4, 8, 'z')

	def test_csiCursorDown_arg(self):
		self.setCursorPos(0, 0)
		self.pipe.write('adsfasdfadsf\r\nhjklhkjl')

		row, col = self.vtyCursorPosition()
		self.assertEqual(row, 1)

		self.sendCsi('3B')

		self.pipe.write('z')

		row, col = self.vtyCursorPosition()
		self.assertEqual(row, 4)
		self.assertVtyCharIs(4, 8, 'z')

	def test_csiCursorDown_pastMargin(self):
		self.setCursorPos(0, 0)
		self.pipe.write('adsfasdfadsf\r\nhjklhkjl')

		row, col = self.vtyCursorPosition()
		self.assertEqual(row, 1)

		self.sendCsi('300B')

		self.pipe.write('z')

		row, col = self.vtyCursorPosition()
		self.assertEqual(row, 23)
		self.assertVtyCharIs(23, 8, 'z')

	def test_csiCursorLeft_default(self):
		self.setCursorPos(0, 0)
		self.pipe.write('a' * 50)

		row, col = self.vtyCursorPosition()
		self.assertEqual(col, 50)

		self.sendCsi('D')
		self.sendCsi('D')
		self.sendCsi('D')

		self.pipe.write('b')

		row, col = self.vtyCursorPosition()
		self.assertEqual(col, 48)

		self.assertVtyCharIs(0, 46, 'a')
		self.assertVtyCharIs(0, 47, 'b')
		self.assertVtyCharIs(0, 48, 'a')

	def test_csiCursorLeft_one(self):
		self.setCursorPos(0, 0)
		self.pipe.write('a' * 50)

		row, col = self.vtyCursorPosition()
		self.assertEqual(col, 50)

		self.sendCsi('1D')
		self.sendCsi('1D')
		self.sendCsi('1D')

		self.pipe.write('b')

		row, col = self.vtyCursorPosition()
		self.assertEqual(col, 48)

		self.assertVtyCharIs(0, 46, 'a')
		self.assertVtyCharIs(0, 47, 'b')
		self.assertVtyCharIs(0, 48, 'a')

	def test_csiCursorLeft_zero(self):
		self.setCursorPos(0, 0)
		self.pipe.write('a' * 50)

		row, col = self.vtyCursorPosition()
		self.assertEqual(col, 50)

		self.sendCsi('0D')
		self.sendCsi('0D')
		self.sendCsi('0D')

		self.pipe.write('b')

		row, col = self.vtyCursorPosition()
		self.assertEqual(col, 48)

		self.assertVtyCharIs(0, 46, 'a')
		self.assertVtyCharIs(0, 47, 'b')
		self.assertVtyCharIs(0, 48, 'a')

	def test_csiCursorLeft_arg(self):
		self.setCursorPos(0, 0)
		self.pipe.write('a' * 50)

		row, col = self.vtyCursorPosition()
		self.assertEqual(col, 50)

		self.sendCsi('3D')

		self.pipe.write('b')

		row, col = self.vtyCursorPosition()
		self.assertEqual(col, 48)

		self.assertVtyCharIs(0, 46, 'a')
		self.assertVtyCharIs(0, 47, 'b')
		self.assertVtyCharIs(0, 48, 'a')

	def test_csiCursorLeft_pastMargin(self):
		self.setCursorPos(0, 0)
		self.pipe.write('a' * 50)

		row, col = self.vtyCursorPosition()
		self.assertEqual(col, 50)

		self.sendCsi('300D')

		self.pipe.write('b')

		row, col = self.vtyCursorPosition()
		self.assertEqual(col, 1)

		self.assertVtyCharIs(0, 0, 'b')
		self.assertVtyCharIs(0, 1, 'a')

	def test_csiCursorRight_default(self):
		self.setCursorPos(0, 0)
		self.pipe.write('a' * 50)

		row, col = self.vtyCursorPosition()
		self.assertEqual(col, 50)

		self.setCursorPos(0, 44)

		self.sendCsi('C')
		self.sendCsi('C')
		self.sendCsi('C')

		self.pipe.write('b')

		row, col = self.vtyCursorPosition()
		self.assertEqual(col, 48)

		self.assertVtyCharIs(0, 46, 'a')
		self.assertVtyCharIs(0, 47, 'b')
		self.assertVtyCharIs(0, 48, 'a')

	def test_csiCursorRight_one(self):
		self.setCursorPos(0, 0)
		self.pipe.write('a' * 50)

		row, col = self.vtyCursorPosition()
		self.assertEqual(col, 50)

		self.setCursorPos(0, 44)

		self.sendCsi('1C')
		self.sendCsi('1C')
		self.sendCsi('1C')

		self.pipe.write('b')

		row, col = self.vtyCursorPosition()
		self.assertEqual(col, 48)

		self.setCursorPos(0, 44)

		self.assertVtyCharIs(0, 46, 'a')
		self.assertVtyCharIs(0, 47, 'b')
		self.assertVtyCharIs(0, 48, 'a')

	def test_csiCursorRight_zero(self):
		self.setCursorPos(0, 0)
		self.pipe.write('a' * 50)

		row, col = self.vtyCursorPosition()
		self.assertEqual(col, 50)

		self.setCursorPos(0, 44)

		self.sendCsi('0C')
		self.sendCsi('0C')
		self.sendCsi('0C')

		self.pipe.write('b')

		row, col = self.vtyCursorPosition()
		self.assertEqual(col, 48)

		self.setCursorPos(0, 44)

		self.assertVtyCharIs(0, 46, 'a')
		self.assertVtyCharIs(0, 47, 'b')
		self.assertVtyCharIs(0, 48, 'a')

	def test_csiCursorRight_arg(self):
		self.setCursorPos(0, 0)
		self.pipe.write('a' * 50)

		row, col = self.vtyCursorPosition()
		self.assertEqual(col, 50)

		self.setCursorPos(0, 44)

		self.sendCsi('3C')

		self.pipe.write('b')

		row, col = self.vtyCursorPosition()
		self.assertEqual(col, 48)

		self.assertVtyCharIs(0, 46, 'a')
		self.assertVtyCharIs(0, 47, 'b')
		self.assertVtyCharIs(0, 48, 'a')

	def test_csiCursorRight_pastMargin(self):
		self.setCursorPos(0, 0)
		self.pipe.write('a' * self.vtyMaxCol())

		self.setCursorPos(0, 20)

		self.sendCsi('300C')

		self.pipe.write('b')

		row, col = self.vtyCursorPosition()
		self.assertEqual(col, self.vtyMaxCol())

		self.assertVtyCharIs(0, self.vtyMaxCol(), 'b')
		self.assertVtyCharIs(0, self.vtyMaxCol() - 1, 'a')

	def test_escapeSaveRestoreCursore(self):
		self.setCursorPos(10, 10)
		self.sendEsc('7')

		self.pipe.write('a\n' * 10)

		row, col = self.vtyCursorPosition()
		self.assertEqual(row, 20)
		self.assertEqual(col, 0)

		self.sendEsc('8')

		row, col = self.vtyCursorPosition()
		self.assertEqual(row, 10)
		self.assertEqual(col, 10)

	def test_csiClearLine_default(self):
		self.setCursorPos(10, 0)
		self.pipe.write('a' * self.vtyMaxCol())
		self.setCursorPos(10, 40)

		self.sendCsi('K')

		for i in range(self.vtyMaxCol()):
			if i < 40:
				c = 'a'
			else:
				c = ''
			self.assertVtyCharIs(10, i, c)

	def test_csiClearLine_toEnd(self):
		self.setCursorPos(10, 0)
		self.pipe.write('a' * self.vtyMaxCol())
		self.setCursorPos(10, 40)

		self.sendCsi('0K')

		for i in range(self.vtyMaxCol()):
			if i < 40:
				c = 'a'
			else:
				c = ''
			self.assertVtyCharIs(10, i, c)

	def test_csiClearLine_fromStart(self):
		self.setCursorPos(10, 0)
		self.pipe.write('a' * self.vtyMaxCol())
		self.setCursorPos(10, 40)

		self.sendCsi('1K')

		for i in range(self.vtyMaxCol()):
			if i > 40:
				c = 'a'
			else:
				c = ''
			self.assertVtyCharIs(10, i, c)

	def test_csiClearLine_all(self):
		self.setCursorPos(10, 0)
		self.pipe.write('a' * self.vtyMaxCol())
		self.setCursorPos(10, 40)

		self.sendCsi('2K')

		for i in range(self.vtyMaxCol()):
			self.assertVtyCharIs(10, i, '')

	def test_escapeCursorDown(self):
		self.setCursorPos(0, 0)
		self.pipe.write('adsfasdfadsf\r\nhjklhkjl')

		row, col = self.vtyCursorPosition()
		self.assertEqual(row, 1)

		self.sendEsc('D')
		self.sendEsc('D')
		self.sendEsc('D')

		self.pipe.write('z')

		row, col = self.vtyCursorPosition()
		self.assertEqual(row, 4)
		self.assertVtyCharIs(4, 8, 'z')

	def test_escapeCursorDown_pastMargin(self):
		self.setCursorPos(0, 0)
		self.pipe.write('adsfasdfadsf\r\nhjklhkjl')

		row, col = self.vtyCursorPosition()
		self.assertEqual(row, 1)

		for i in range(self.vtyMaxRow() - 1):
			# Move to bottom of screen
			self.sendEsc('D')

		self.pipe.write('z')

		row, col = self.vtyCursorPosition()
		self.assertEqual(row, 23)
		self.assertVtyCharIs(23, 8, 'z')
		self.assertVtyCharIs(0, 0, 'a')

		self.sendEsc('D')

		row, col = self.vtyCursorPosition()
		self.assertEqual(row, 23)
		self.assertVtyCharIs(22, 8, 'z')
		self.assertVtyCharIs(0, 0, 'h')
