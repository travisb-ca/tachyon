#!/usr/bin/env python2.7

import subprocess
import time

process = subprocess.Popen('../../tachyon -z', shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
time.sleep(1)
if process.poll() and process.returncode == 1:
	print 'pass'
else:
	print 'fail'

