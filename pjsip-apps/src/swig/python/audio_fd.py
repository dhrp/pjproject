import pjsua2 as pj
import sys
import io
import traceback
try:
	# On Windows, pass handles rather than descriptors
	import msvcrt
except:
	pass

# File name to read 8kHz PCM from
FN_IN = "in.pcm"
# File name to write 8kHz PCM to
FN_OUT = "out.pcm"


#
# Custom log writer
#
class MyLogWriter(pj.LogWriter):
	def write(self, entry):
		print entry.msg,


#
# Class to receive call events
#
class MyCall(pj.Call):
	def onCallState(self, prm):
		info = self.getInfo()
		if info.state == pj.PJSIP_INV_STATE_DISCONNECTED:
			global g_current_call
			g_current_call = None
			print "Call hung up"

	def onCallMediaState(self, prm):
		if not self.hasMedia(): return
		info = self.getInfo()

		# Do not use iterators, may cause crash!
		#for mi in info.media:

		for i in range(0, len(info.media)):
			mi = info.media[i]
			if mi.type != pj.PJMEDIA_TYPE_AUDIO or \
					mi.status != pj.PJSUA_CALL_MEDIA_ACTIVE:
				continue
			m = self.getMedia(mi.index)
			if not m: continue
			global g_fdp
			am = pj.AudioMedia.typecastFromMedia(m)
			g_fdp.startTransmit(am)
			am.startTransmit(g_fdp)


#
# Class to receive account events
#
class MyAccount(pj.Account):
    def onIncomingCall(self, prm):
	global g_current_call
	call = MyCall(self, prm.callId)
	op = pj.CallOpParam(True)
	if g_current_call:
		op.statusCode = pj.PJSIP_SC_BUSY_HERE
		call.hangup(op)
		return
	info = call.getInfo()
	print "Incoming call from", info.remoteUri
	op.statusCode = pj.PJSIP_SC_OK
	g_current_call = call
	call.answer(op)


#
# Write PJ Error info
#
def handleError(error):
	print
	print 'Exception:'
	print '  ', error.info()
	print 'Traceback:'
	print traceback.print_stack()
	print


ep = pj.Endpoint()
try:
	ep.libCreate()
except pj.Error, error:
	handleError(error)
	sys.exit(1)

acc = None
g_current_call = None
g_fdp = None
try:
	logger = MyLogWriter()
	cfg = pj.EpConfig()
	cfg.logConfig.msgLogging = False
	cfg.logConfig.writer = logger
	cfg.medConfig.clockRate = 8000
	cfg.medConfig.noVad = True
	ep.libInit(cfg)
	ep.audDevManager().setNullDev()

	tcfg = pj.TransportConfig()
	tcfg.port = 0  # Any available
	tid = ep.transportCreate(pj.PJSIP_TRANSPORT_UDP, tcfg)
	tinfo = ep.transportGetInfo(tid)

	acfg = pj.AccountConfig()
	acfg.idUri = "sip:" + tinfo.localName
	acc = MyAccount()
	acc.create(acfg)

	ep.libStart()

	try:
		f_in = io.open(FN_IN, "rb")
		fd_in = f_in.fileno()
	except:
		fd_in = -1
		print "\nInput file", FN_IN, "cannot be opened for reading\n"

	try:
		f_out = io.open(FN_OUT, "wb")
		fd_out = f_out.fileno()
	except:
		fd_out = -1
		print "\nOutput file", FN_OUT, "cannot be opened for writing\n"

	# Try to pass Windows handles instead of descriptors
	flags = 0
	if 'msvcrt' in sys.modules:
		if fd_in >= 0: fd_in = msvcrt.get_osfhandle(fd_in)
		if fd_out >= 0: fd_out = msvcrt.get_osfhandle(fd_out)
		flags |= pj.PJMEDIA_FD_HANDLES

	g_fdp = pj.AudioMediaFD()
	g_fdp.createFD(fd_in, fd_out, flags)
	print "Audio file descriptor port ID:", g_fdp.getPortId()

	print "\nWaiting for incoming call"
	print "My SIP URI is", acfg.idUri
	print "\nPress ENTER to quit"
	sys.stdin.readline()
except pj.Error, error:
	handleError(error)

# Shutdown the library -- destroy objects manually, do not let GC
if g_fdp: del g_fdp
if acc: del acc
ep.libDestroy()
del ep
