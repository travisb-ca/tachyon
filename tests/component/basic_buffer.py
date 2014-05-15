#!/usr/bin/env python2.7

import tachyon

class TestBasicWindows(tachyon.TachyonTestCase):
	def setUp1(self):
		self.startTachyon()

	def tearDown1(self):
		self.waitForTermination()

	def test_exitImmediately(self):
		self.sendLine('exit')

	def test_bufferShellResponds(self):
		self.sendCmd('export BUFNUM=1')
		self.sendCmd('echo $BUFNUM')
		self.expectOnly('^1$')
		self.sendCmd('exit')

	def test_createNewBuffer(self):
		self.sendCmd('export BUFNUM=1')
		self.sendMeta('c')
		self.sendCmd('export BUFNUM=2')
		self.sendCmd('echo $BUFNUM')
		self.expectOnly('^2$')
		self.sendCmdExit()
		self.sendCmd('echo $BUFNUM')
		self.expectOnly('^1$')
		self.sendCmd('exit')

	def test_nextBuffers(self):
		self.sendCmd('export BUFNUM=1')
		self.bufferCreate()
		self.sendCmd('export BUFNUM=2')
		self.bufferCreate()
		self.sendCmd('export BUFNUM=3')
		self.bufferNext()
		self.sendCmd('echo $BUFNUM')
		self.expectOnly('^1$')
		self.bufferNext()
		self.sendCmd('echo $BUFNUM')
		self.expectOnly('^2$')
		self.sendCmdExit()
		self.sendCmd('echo $BUFNUM')
		self.expectOnly('^3$')
		self.sendCmdExit()
		self.sendCmd('echo $BUFNUM')
		self.expectOnly('^1$')
		self.sendCmd('exit')

	def test_prevBuffers(self):
		self.sendCmd('export BUFNUM=1')
		self.bufferCreate()
		self.sendCmd('export BUFNUM=2')
		self.bufferCreate()
		self.sendCmd('export BUFNUM=3')
		self.bufferPrev()
		self.sendCmd('echo $BUFNUM')
		self.expectOnly('^2$')
		self.bufferPrev()
		self.sendCmd('echo $BUFNUM')
		self.expectOnly('^1$')
		self.sendCmdExit()
		self.sendCmd('echo $BUFNUM')
		self.expectOnly('^2$')
		self.sendCmdExit()
		self.sendCmd('echo $BUFNUM')
		self.expectOnly('^3$')
		self.sendCmd('exit')

	def test_lastBuffers(self):
		self.bufferCreate()
		self.bufferCreate()

		self.sendCmd('echo $TACHYON_BUFNUM')
		self.expectOnly('^2')

		self.bufferPrev()

		self.sendCmd('echo $TACHYON_BUFNUM')
		self.expectOnly('^1')

		self.bufferLast()

		self.sendCmd('echo $TACHYON_BUFNUM')
		self.expectOnly('^2')

		self.bufferNext()

		self.sendCmd('echo $TACHYON_BUFNUM')
		self.expectOnly('^0')

		self.sendCmdExit()

		self.sendCmd('echo $TACHYON_BUFNUM')
		self.expectOnly('^2')

		self.sendCmdExit()

		self.sendCmd('echo $TACHYON_BUFNUM')
		self.expectOnly('^1')

		self.sendCmd('exit')

	def test_tachyonVariables(self):
		self.sendCmd('echo $TACHYON_BUFNUM')
		self.expectOnly('^0')

		self.bufferCreate()
		self.sendCmd('echo $TACHYON_BUFNUM')
		self.expectOnly('^1')

		self.bufferPrev()
		self.sendCmd('echo $TACHYON_BUFNUM')
		self.expectOnly('^0$')

		self.sendCmdExit()
		
		self.sendCmd('echo $TACHYON_BUFNUM')
		self.expectOnly('^1')

		self.bufferCreate()
		self.sendCmd('echo $TACHYON_BUFNUM')
		self.expectOnly('^0$')

		self.sendCmdExit()
		self.sendCmdExit()

	def test_gotoBuffer(self):
		for i in range(9):
			self.bufferCreate()

		for i in range(10):
			self.sendMeta('%d' % i)
			self.sendCmd('echo $TACHYON_BUFNUM')
			self.expectOnly('^%d' % i)

		for i in range(9):
			self.sendCmdExit()

		self.sendCmd('echo $TACHYON_BUFNUM')
		self.expectOnly('^0')

		self.sendCmd('exit')
