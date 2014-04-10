#!/usr/bin/env python2.7

import tachyon
import lousy
import time
import itertools

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

		self.assertVtyCursorPos(23, 0)

	def test_csiCursorPosition(self):
		self.pipe.write('adfasdfasdfasdf\r\n;lkjh')

		self.assertVtyCursorPos(23, 5)

		self.sendCsi('6;6f')

		self.assertVtyCursorPos(5, 5)

		self.pipe.write('z')
		self.assertVtyCharIs(5, 5, 'z')

	def test_csiCursorPosition_default(self):
		self.pipe.write('adfasdfasdfasdf\r\n;lkjh')

		self.assertVtyCursorPos(23, 5)

		self.sendCsi('f')

		self.assertVtyCursorPos(0, 0)

		self.pipe.write('z')
		self.assertVtyCharIs(0, 0, 'z')

	def test_csiCursorPosition_empty(self):
		self.pipe.write('adfasdfasdfasdf\r\n;lkjh')

		self.assertVtyCursorPos(23, 5)

		self.sendCsi(';f')

		self.assertVtyCursorPos(0, 0)

		self.pipe.write('z')
		self.assertVtyCharIs(0, 0, 'z')

	def test_csiCursorUp_default(self):
		self.pipe.write('adsfasdfadsf\r\nhjklhkjl')

		self.assertVtyCursorPos(row=23)

		self.sendCsi('A')
		self.sendCsi('A')
		self.sendCsi('A')

		self.pipe.write('z')

		self.assertVtyCursorPos(row=20)
		self.assertVtyCharIs(20, 8, 'z')

	def test_csiCursorUp_one(self):
		self.pipe.write('adsfasdfadsf\r\nhjklhkjl')

		self.assertVtyCursorPos(row=23)

		self.sendCsi('1A')
		self.sendCsi('1A')
		self.sendCsi('1A')

		self.pipe.write('z')

		self.assertVtyCursorPos(row=20)
		self.assertVtyCharIs(20, 8, 'z')

	def test_csiCursorUp_zero(self):
		self.pipe.write('adsfasdfadsf\r\nhjklhkjl')

		self.assertVtyCursorPos(row=23)

		self.sendCsi('0A')
		self.sendCsi('0A')
		self.sendCsi('0A')

		self.pipe.write('z')

		self.assertVtyCursorPos(row=20)
		self.assertVtyCharIs(20, 8, 'z')

	def test_csiCursorUp_arg(self):
		self.pipe.write('adsfasdfadsf\r\nhjklhkjl')

		self.assertVtyCursorPos(row=23)

		self.sendCsi('3A')

		self.pipe.write('z')

		self.assertVtyCursorPos(row=20)
		self.assertVtyCharIs(20, 8, 'z')

	def test_csiCursorUp_pastMargin(self):
		self.pipe.write('adsfasdfadsf\r\nhjklhkjl')

		self.assertVtyCursorPos(row=23)

		self.sendCsi('300A')

		self.pipe.write('z')

		self.assertVtyCursorPos(row=0)
		self.assertVtyCharIs(0, 8, 'z')

	def test_csiCursorDown_default(self):
		self.setCursorPos(0, 0)
		self.pipe.write('adsfasdfadsf\r\nhjklhkjl')

		self.assertVtyCursorPos(row=1)

		self.sendCsi('B')
		self.sendCsi('B')
		self.sendCsi('B')

		self.pipe.write('z')

		self.assertVtyCursorPos(row=4)
		self.assertVtyCharIs(4, 8, 'z')

	def test_csiCursorDown_one(self):
		self.setCursorPos(0, 0)
		self.pipe.write('adsfasdfadsf\r\nhjklhkjl')

		self.assertVtyCursorPos(row=1)

		self.sendCsi('1B')
		self.sendCsi('1B')
		self.sendCsi('1B')

		self.pipe.write('z')

		self.assertVtyCursorPos(row=4)
		self.assertVtyCharIs(4, 8, 'z')

	def test_csiCursorDown_zero(self):
		self.setCursorPos(0, 0)
		self.pipe.write('adsfasdfadsf\r\nhjklhkjl')

		self.assertVtyCursorPos(row=1)

		self.sendCsi('0B')
		self.sendCsi('0B')
		self.sendCsi('0B')

		self.pipe.write('z')

		self.assertVtyCursorPos(row=4)
		self.assertVtyCharIs(4, 8, 'z')

	def test_csiCursorDown_arg(self):
		self.setCursorPos(0, 0)
		self.pipe.write('adsfasdfadsf\r\nhjklhkjl')

		self.assertVtyCursorPos(row=1)

		self.sendCsi('3B')

		self.pipe.write('z')

		self.assertVtyCursorPos(row=4)
		self.assertVtyCharIs(4, 8, 'z')

	def test_csiCursorDown_pastMargin(self):
		self.setCursorPos(0, 0)
		self.pipe.write('adsfasdfadsf\r\nhjklhkjl')

		self.assertVtyCursorPos(row=1)

		self.sendCsi('300B')

		self.pipe.write('z')

		self.assertVtyCursorPos(row=23)
		self.assertVtyCharIs(23, 8, 'z')

	def test_csiCursorLeft_default(self):
		self.setCursorPos(0, 0)
		self.pipe.write('a' * 50)

		self.assertVtyCursorPos(col=50)

		self.sendCsi('D')
		self.sendCsi('D')
		self.sendCsi('D')

		self.pipe.write('b')

		self.assertVtyCursorPos(col=48)

		self.assertVtyCharIs(0, 46, 'a')
		self.assertVtyCharIs(0, 47, 'b')
		self.assertVtyCharIs(0, 48, 'a')

	def test_csiCursorLeft_one(self):
		self.setCursorPos(0, 0)
		self.pipe.write('a' * 50)

		self.assertVtyCursorPos(col=50)

		self.sendCsi('1D')
		self.sendCsi('1D')
		self.sendCsi('1D')

		self.pipe.write('b')

		self.assertVtyCursorPos(col=48)

		self.assertVtyCharIs(0, 46, 'a')
		self.assertVtyCharIs(0, 47, 'b')
		self.assertVtyCharIs(0, 48, 'a')

	def test_csiCursorLeft_zero(self):
		self.setCursorPos(0, 0)
		self.pipe.write('a' * 50)

		self.assertVtyCursorPos(col=50)

		self.sendCsi('0D')
		self.sendCsi('0D')
		self.sendCsi('0D')

		self.pipe.write('b')

		self.assertVtyCursorPos(col=48)

		self.assertVtyCharIs(0, 46, 'a')
		self.assertVtyCharIs(0, 47, 'b')
		self.assertVtyCharIs(0, 48, 'a')

	def test_csiCursorLeft_arg(self):
		self.setCursorPos(0, 0)
		self.pipe.write('a' * 50)

		self.assertVtyCursorPos(col=50)

		self.sendCsi('3D')

		self.pipe.write('b')

		self.assertVtyCursorPos(col=48)

		self.assertVtyCharIs(0, 46, 'a')
		self.assertVtyCharIs(0, 47, 'b')
		self.assertVtyCharIs(0, 48, 'a')

	def test_csiCursorLeft_pastMargin(self):
		self.setCursorPos(0, 0)
		self.pipe.write('a' * 50)

		self.assertVtyCursorPos(col=50)

		self.sendCsi('300D')

		self.pipe.write('b')

		self.assertVtyCursorPos(col=1)

		self.assertVtyCharIs(0, 0, 'b')
		self.assertVtyCharIs(0, 1, 'a')

	def test_csiCursorRight_default(self):
		self.setCursorPos(0, 0)
		self.pipe.write('a' * 50)

		self.assertVtyCursorPos(col=50)

		self.setCursorPos(0, 44)

		self.sendCsi('C')
		self.sendCsi('C')
		self.sendCsi('C')

		self.pipe.write('b')

		self.assertVtyCursorPos(col=48)

		self.assertVtyCharIs(0, 46, 'a')
		self.assertVtyCharIs(0, 47, 'b')
		self.assertVtyCharIs(0, 48, 'a')

	def test_csiCursorRight_one(self):
		self.setCursorPos(0, 0)
		self.pipe.write('a' * 50)

		self.assertVtyCursorPos(col=50)

		self.setCursorPos(0, 44)

		self.sendCsi('1C')
		self.sendCsi('1C')
		self.sendCsi('1C')

		self.pipe.write('b')

		self.assertVtyCursorPos(col=48)

		self.setCursorPos(0, 44)

		self.assertVtyCharIs(0, 46, 'a')
		self.assertVtyCharIs(0, 47, 'b')
		self.assertVtyCharIs(0, 48, 'a')

	def test_csiCursorRight_zero(self):
		self.setCursorPos(0, 0)
		self.pipe.write('a' * 50)

		self.assertVtyCursorPos(col=50)

		self.setCursorPos(0, 44)

		self.sendCsi('0C')
		self.sendCsi('0C')
		self.sendCsi('0C')

		self.pipe.write('b')

		self.assertVtyCursorPos(col=48)

		self.setCursorPos(0, 44)

		self.assertVtyCharIs(0, 46, 'a')
		self.assertVtyCharIs(0, 47, 'b')
		self.assertVtyCharIs(0, 48, 'a')

	def test_csiCursorRight_arg(self):
		self.setCursorPos(0, 0)
		self.pipe.write('a' * 50)

		self.assertVtyCursorPos(col=50)

		self.setCursorPos(0, 44)

		self.sendCsi('3C')

		self.pipe.write('b')

		self.assertVtyCursorPos(col=48)

		self.assertVtyCharIs(0, 46, 'a')
		self.assertVtyCharIs(0, 47, 'b')
		self.assertVtyCharIs(0, 48, 'a')

	def test_csiCursorRight_pastMargin(self):
		self.setCursorPos(0, 0)
		self.pipe.write('a' * self.vtyMaxCol())

		self.setCursorPos(0, 20)

		self.sendCsi('300C')

		self.pipe.write('b')

		self.assertVtyCursorPos(col=self.vtyMaxCol())

		self.assertVtyCharIs(0, self.vtyMaxCol(), 'b')
		self.assertVtyCharIs(0, self.vtyMaxCol() - 1, 'a')

	def test_escapeSaveRestoreCursore(self):
		self.setCursorPos(10, 10)
		self.sendEsc('7')

		self.pipe.write('a\n' * 10)

		self.assertVtyCursorPos(20, 0)

		self.sendEsc('8')

		self.assertVtyCursorPos(10, 10)

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

		self.assertVtyCursorPos(row=1)

		self.sendEsc('D')
		self.sendEsc('D')
		self.sendEsc('D')

		self.pipe.write('z')

		self.assertVtyCursorPos(row=4)
		self.assertVtyCharIs(4, 8, 'z')

	def test_escapeCursorDown_pastMargin_noScrollBack(self):
		self.setCursorPos(0, 0)
		self.pipe.write('adsfasdfadsf\r\nhjklhkjl')

		self.assertVtyCursorPos(row=1)

		for i in range(self.vtyMaxRow() - 1):
			# Move to bottom of screen
			self.sendEsc('D')

		self.pipe.write('z')

		self.assertVtyCursorPos(row=23)
		self.assertVtyCharIs(23, 8, 'z')
		self.assertVtyCharIs(0, 0, 'a')

		self.sendEsc('D')

		self.assertVtyCursorPos(row=23)
		self.assertVtyCharIs(22, 8, 'z')
		self.assertVtyCharIs(0, 0, 'h')

	def test_escapeCursorDown_pastMargin_withScrollBack(self):
		for i in range(self.vtyRows() + 5):
			self.pipe.write('Row %d\r\n' % i)

		# Scroll some text off the bottom to be retrieved
		for i in range(self.vtyRows() + 5):
			self.syncOutput()
			self.sendEsc('M')

		self.setCursorPos(0, 0)
		self.pipe.write('adsfasdfadsf\r\nhjklhkjl')

		self.assertVtyCursorPos(row=1)

		for i in range(self.vtyMaxRow() - 1):
			# Move to bottom of screen
			self.sendEsc('D')

		self.pipe.write('z')

		self.assertVtyCursorPos(23, 9)
		self.assertVtyCharIs(23, 8, 'z')
		self.assertVtyCharIs(0, 0, 'a')

		# Scroll past bottom margin
		self.sendEsc('D')

		self.assertVtyString(self.vtyMaxRow(), 0, 'Row 24')
		self.assertVtyCursorPos(23, 9)
		
		self.pipe.write('arstarstarst')

		self.assertVtyString(23, 9, 'arstarstarst')
		self.assertVtyCharIs(22, 8, 'z')
		self.assertVtyCharIs(0, 0, 'h')

	def test_escapeNextLine(self):
		self.setCursorPos(self.vtyMaxRow() - 1, 5)
		self.pipe.write('arstarstarst')

		self.sendEsc('E')

		self.assertVtyCursorPos(self.vtyMaxRow(), 0)
		self.assertVtyString(self.vtyMaxRow() - 1, 5, 'arstarstarst')

		self.sendEsc('E')

		self.assertVtyCursorPos(self.vtyMaxRow(), 0)
		self.assertVtyString(self.vtyMaxRow() - 2, 5, 'arstarstarst')

	def test_escapeCursorUp(self):
		self.setCursorPos(10, 0)
		self.pipe.write('adsfasdfadsf\r\nhjklhkjl')

		self.assertVtyCursorPos(row=11)

		self.sendEsc('M')
		self.sendEsc('M')
		self.sendEsc('M')

		self.pipe.write('z')

		self.assertVtyCursorPos(row=8)
		self.assertVtyCharIs(8, 8, 'z')

	def test_escapeCursorUp_pastMargin_noScrollBack(self):
		self.pipe.write('This will scroll off the bottom')
		self.setCursorPos(10, 0)
		self.pipe.write('adsfasdfadsf\r\nhjklhkjl')

		self.assertVtyCursorPos(row=11)

		for i in range(11):
			# Move to top of screen
			self.sendEsc('M')

		self.pipe.write('z')

		self.assertVtyCursorPos(row=0)
		self.assertVtyCharIs(0, 8, 'z')
		self.assertVtyCharIs(10, 0, 'a')

		# Scroll past upper margin
		self.sendEsc('M')
		
		self.pipe.write('arstarstarst')

		self.assertVtyCursorPos(row=0)
		self.assertVtyString(0, 9, 'arstarstarst')
		self.assertVtyCharIs(1, 8, 'z')
		self.assertVtyCharIs(11, 0, 'a')

	def test_escapeCursorUp_pastMargin_withScrollBack(self):
		for i in range(self.vtyRows() + 10):
			self.pipe.write('Row %d\r\n' % i)

		self.pipe.write('This will scroll off the bottom')
		self.setCursorPos(10, 0)
		self.pipe.write('adsfasdfadsf\r\nhjklhkjl')

		self.assertVtyCursorPos(row=11)

		for i in range(11):
			# Move to top of screen
			self.sendEsc('M')

		self.pipe.write('z')

		self.assertVtyCursorPos(row=0)
		self.assertVtyCharIs(0, 8, 'z')
		self.assertVtyCharIs(10, 0, 'a')

		# Scroll past upper margin
		self.sendEsc('M')

		self.assertVtyString(0, 0, 'Row 10')
		
		self.pipe.write('arstarstarst')

		self.assertVtyCursorPos(row=0)
		self.assertVtyString(0, 9, 'arstarstarst')
		self.assertVtyCharIs(1, 8, 'z')
		self.assertVtyCharIs(11, 0, 'a')

	def test_escapeResetState(self):
		self.setCursorPos(10, 10);
		self.pipe.write('a')

		self.sendEsc('c')

		self.pipe.write('z')

		self.assertVtyCharIs(0, 0, 'z')
		self.assertVtyCharIs(10, 10, 'a')

	def test_defaultTabStops(self):
		self.setCursorPos(0, 0)
		self.pipe.write('a\tb\tc\td\te\tf\tg\th\ti\tj\tk\tl')

		self.assertVtyCharIs(0, 0, 'a')
		self.assertVtyCharIs(0, 8, 'b')
		self.assertVtyCharIs(0, 16, 'c')
		self.assertVtyCharIs(0, 24, 'd')
		self.assertVtyCharIs(0, 32, 'e')
		self.assertVtyCharIs(0, 40, 'f')
		self.assertVtyCharIs(0, 48, 'g')
		self.assertVtyCharIs(0, 56, 'h')
		self.assertVtyCharIs(0, 64, 'i')
		self.assertVtyCharIs(0, 72, 'j')
		self.assertVtyCharIs(0, 79, 'l')
	
	def test_clearTabStop_default(self):
		self.setCursorPos(0, 8)
		self.sendCsi('g')

		self.setCursorPos(0, 0)

		self.pipe.write('a\tb\tc')

		# 16 is the second tabstop now that we've erased the second tabstop
		# 24 is the third tabstop
		self.assertVtyCursorPos(0, 25)
		self.assertVtyCharIs(0, 0, 'a')
		self.assertVtyCharIs(0, 16, 'b')
		self.assertVtyCharIs(0, 24, 'c')

	def test_clearTabStop_arg(self):
		self.setCursorPos(0, 8)
		self.sendCsi('0g')

		self.setCursorPos(0, 0)

		self.pipe.write('a\tb\tc')

		# 16 is the second tabstop now that we've erased the second tabstop
		# 24 is the third tabstop
		self.assertVtyCursorPos(0, 25)
		self.assertVtyCharIs(0, 0, 'a')
		self.assertVtyCharIs(0, 16, 'b')
		self.assertVtyCharIs(0, 24, 'c')

	def test_clearTabStop_all(self):
		self.setCursorPos(0, 8)
		self.sendCsi('3g')

		self.setCursorPos(0, 0)

		self.pipe.write('a\tb\tc')

		# vtyMaxCol is the second tabstop now that we've erased the second tabstop
		self.assertVtyCursorPos(0, self.vtyMaxCol())
		self.assertVtyCharIs(0, 0, 'a')
		self.assertVtyCharIs(0, self.vtyMaxCol(), 'c')

	def test_setTabStop(self):
		self.setCursorPos(0, 4)
		self.sendEsc('H')

		self.setCursorPos(0, 0)

		self.pipe.write('a\tb\tc')

		# 4 is the new second tabstop
		# 8 is the third tabstop
		self.assertVtyCursorPos(0, 9)
		self.assertVtyCharIs(0, 0, 'a')
		self.assertVtyCharIs(0, 4, 'b')
		self.assertVtyCharIs(0, 8, 'c')

	def test_attributeBold(self):
		self.setCursorPos(0, 0)
		self.sendCsi('1m')

		self.pipe.write('a')

		self.sendCsi('0m')

		self.assertVtyCharIs(0, 0, 'a')
		self.assertVtyCharAttrIs(0, 0, [lousy.FrameBufferCell.BOLD])

	def test_attributeUnderline(self):
		self.setCursorPos(0, 0)
		self.sendCsi('4m')

		self.pipe.write('a')

		self.sendCsi('0m')

		self.assertVtyCharIs(0, 0, 'a')
		self.assertVtyCharAttrIs(0, 0, [lousy.FrameBufferCell.UNDERSCORE])

	def test_attributeBlink(self):
		self.setCursorPos(0, 0)
		self.sendCsi('5m')

		self.pipe.write('a')

		self.sendCsi('0m')

		self.assertVtyCharIs(0, 0, 'a')
		self.assertVtyCharAttrIs(0, 0, [lousy.FrameBufferCell.BLINK])

	def test_attributeReverse(self):
		self.setCursorPos(0, 0)
		self.sendCsi('7m')

		self.pipe.write('a')

		self.sendCsi('0m')

		self.assertVtyCharIs(0, 0, 'a')
		self.assertVtyCharAttrIs(0, 0, [lousy.FrameBufferCell.REVERSE])

	def test_attributeCombinationsCumulative(self):
		attributes = [1, 4, 5, 7]
		map = {1 : lousy.FrameBufferCell.BOLD,
			4: lousy.FrameBufferCell.UNDERSCORE,
			5: lousy.FrameBufferCell.BLINK,
			7: lousy.FrameBufferCell.REVERSE
			}
		row = 0

		self.setCursorPos(0, 0)

		for l in range(1, len(attributes) + 1):
			for combo in itertools.combinations(attributes, l):
				for permutation in itertools.permutations(combo):
					for attr in permutation:
						self.sendCsi('%sm' % str(attr))
						self.pipe.write('a')
					self.sendCsi('0m')

					self.pipe.write('\t')
					for attr in permutation:
						self.pipe.write(str(attr))

					for i in range(len(permutation)):
						attrs = [map[a] for a in permutation[:i + 1]]
						self.assertVtyCharAttrIs(row, i, attrs)

					self.pipe.write('\n')
					self.pipe.write('\r')
					row += 1
					if row == self.vtyMaxRow() + 1:
						row = self.vtyMaxRow()
