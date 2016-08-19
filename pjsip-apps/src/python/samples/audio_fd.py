# $Id: audio_cb.py 2171 2008-07-24 09:01:33Z bennylp $
#
# SIP account and registration sample. In this sample, the program
# will block to wait until registration is complete
#
# Copyright (C) 2003-2008 Benny Prijono <benny at prijono.org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
import sys
import pjsua as pj
import io
try:
    # On Windows, pass handles rather than descriptors
    import msvcrt
except:
    pass

# File name to read 8kHz PCM from
FN_IN = "in.pcm"
# File name to write 8kHz PCM to
FN_OUT = "out.pcm"
# Flags passed to FD port
FD_NONBLOCK = 1
FD_HANDLES  = 2


def log_cb(level, str, len):
    print str,


class MyCallCallback(pj.CallCallback):

    def __init__(self, call=None):
        pj.CallCallback.__init__(self, call)

    def on_state(self):
        if self.call.info().state == pj.CallState.DISCONNECTED:
            global g_current_call
            g_current_call = None
            print "Call hung up"

    def on_media_state(self):
        info = self.call.info()
        call_slot = info.conf_slot
        if (info.media_state == pj.MediaState.ACTIVE) and (call_slot >= 0):
            print "Call slot:", call_slot
            global g_fdp_id
            fdp_slot = lib.audio_fd_get_slot(g_fdp_id)
            print "Audio FD:", g_fdp_id, " slot:", fdp_slot
            lib.conf_connect(call_slot, fdp_slot)
            lib.conf_connect(fdp_slot, call_slot)


class MyAccountCallback(pj.AccountCallback):

    def __init__(self, account=None):
        pj.AccountCallback.__init__(self, account)

    # Notification on incoming call
    def on_incoming_call(self, call):
        global g_current_call
        if g_current_call:
            call.answer(486, "Busy")
            return

        call.set_callback(MyCallCallback(call))
        info = call.info()
        print "Incoming call from", info.remote_uri
        call.answer()
        g_current_call = call


lib = pj.Lib()

try:
    mcfg = pj.MediaConfig()
    mcfg.clock_rate = 8000
    mcfg.no_vad = True
    lib.init(log_cfg = pj.LogConfig(level=4, callback=log_cb),
             media_cfg = mcfg)

    # This is a MUST if not using a HW sound
    lib.set_null_snd_dev()

    # Create UDP transport which listens to any available port
    transport = lib.create_transport(pj.TransportType.UDP,
                                     pj.TransportConfig(0))
    print "\nListening on", transport.info().host,
    print "port", transport.info().port, "\n"

    lib.start(True)

    # Create local account
    acc = lib.create_account_for_transport(transport, cb=MyAccountCallback())

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
        flags |= FD_HANDLES

    g_current_call = None
    g_fdp_id = lib.create_audio_fd(fd_in, fd_out, flags)
    print "Audio file descriptor port ID:", g_fdp_id

    print "\nWaiting for incoming call"
    my_sip_uri = "sip:" + transport.info().host + \
                 ":" + str(transport.info().port)
    print "My SIP URI is", my_sip_uri
    print "\nPress ENTER to quit"
    sys.stdin.readline()

    # Shutdown the library
    lib.audio_fd_destroy(g_fdp_id)
    transport = None
    acc.delete()
    acc = None
    lib.destroy()
    lib = None

except pj.Error, e:
    print "Exception: " + str(e)
    lib.destroy()

