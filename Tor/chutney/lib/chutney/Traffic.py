#!/usr/bin/env python2
#
# Copyright 2013 The Tor Project
#
# You may do anything with this work that copyright law would normally
# restrict, so long as you retain the above notice(s) and this license
# in all redistributed copies and derived works.  There is no warranty.

# Do select/read/write for binding to a port, connecting to it and
# write, read what's written and verify it. You can connect over a
# SOCKS proxy (like Tor).
#
# You can create a TrafficTester and give it an IP address/host and
# port to bind to. If a Source is created and added to the
# TrafficTester, it will connect to the address/port it was given at
# instantiation and send its data. A Source can be configured to
# connect over a SOCKS proxy. When everything is set up, you can
# invoke TrafficTester.run() to start running. The TrafficTester will
# accept the incoming connection and read from it, verifying the data.
#
# For example code, see main() below.

from __future__ import print_function

import sys
import socket
import select
import struct
import errno

# Set debug_flag=True in order to debug this program or to get hints
# about what's going wrong in your system.
debug_flag = False


def debug(s):
    "Print a debug message on stdout if debug_flag is True."
    if debug_flag:
        print("DEBUG: %s" % s)


def socks_cmd(addr_port):
    """
    Return a SOCKS command for connecting to addr_port.

    SOCKSv4: https://en.wikipedia.org/wiki/SOCKS#Protocol
    SOCKSv5: RFC1928, RFC1929
    """
    ver = 4                     # Only SOCKSv4 for now.
    cmd = 1                     # Stream connection.
    user = '\x00'
    dnsname = ''
    host, port = addr_port
    try:
        addr = socket.inet_aton(host)
    except socket.error:
        addr = '\x00\x00\x00\x01'
        dnsname = '%s\x00' % host
    return struct.pack('!BBH', ver, cmd, port) + addr + user + dnsname


class TestSuite(object):

    """Keep a tab on how many tests are pending, how many have failed
    and how many have succeeded."""

    def __init__(self):
        self.not_done = 0
        self.successes = 0
        self.failures = 0

    def add(self):
        self.not_done += 1

    def success(self):
        self.not_done -= 1
        self.successes += 1

    def failure(self):
        self.not_done -= 1
        self.failures += 1

    def failure_count(self):
        return self.failures

    def all_done(self):
        return self.not_done == 0

    def status(self):
        return('%d/%d/%d' % (self.not_done, self.successes, self.failures))


class Peer(object):

    "Base class for Listener, Source and Sink."
    LISTENER = 1
    SOURCE = 2
    SINK = 3

    def __init__(self, ptype, tt, s=None):
        self.type = ptype
        self.tt = tt            # TrafficTester
        if s is not None:
            self.s = s
        else:
            self.s = socket.socket()
            self.s.setblocking(False)

    def fd(self):
        return self.s.fileno()

    def is_source(self):
        return self.type == self.SOURCE

    def is_sink(self):
        return self.type == self.SINK


class Listener(Peer):

    "A TCP listener, binding, listening and accepting new connections."

    def __init__(self, tt, endpoint):
        super(Listener, self).__init__(Peer.LISTENER, tt)
        self.s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.s.bind(endpoint)
        self.s.listen(0)

    def accept(self):
        newsock, endpoint = self.s.accept()
        debug("new client from %s:%s (fd=%d)" %
              (endpoint[0], endpoint[1], newsock.fileno()))
        self.tt.add(Sink(self.tt, newsock))


class Sink(Peer):

    "A data sink, reading from its peer and verifying the data."

    def __init__(self, tt, s):
        super(Sink, self).__init__(Peer.SINK, tt, s)
        self.inbuf = ''

    def on_readable(self):
        """Invoked when the socket becomes readable.
        Return 0 on finished, successful verification.
               -1 on failed verification
               >0 if more data needs to be read
        """
        return self.verify(self.tt.data)

    def verify(self, data):
        self.inbuf += self.s.recv(len(data) - len(self.inbuf))
        assert(len(self.inbuf) <= len(data))
        if len(self.inbuf) == len(data):
            if self.inbuf != data:
                return -1       # Failed verification.
            debug("successful verification")
        return len(data) - len(self.inbuf)


class Source(Peer):

    """A data source, connecting to a TCP server, optionally over a
    SOCKS proxy, sending data."""
    NOT_CONNECTED = 0
    CONNECTING = 1
    CONNECTING_THROUGH_PROXY = 2
    CONNECTED = 5

    def __init__(self, tt, server, buf, proxy=None):
        super(Source, self).__init__(Peer.SOURCE, tt)
        self.state = self.NOT_CONNECTED
        self.data = buf
        self.outbuf = ''
        self.inbuf = ''
        self.proxy = proxy
        self.connect(server)

    def connect(self, endpoint):
        self.dest = endpoint
        self.state = self.CONNECTING
        dest = self.proxy or self.dest
        try:
            self.s.connect(dest)
        except socket.error as e:
            if e[0] != errno.EINPROGRESS:
                raise

    def on_readable(self):
        """Invoked when the socket becomes readable.
        Return -1 on failure
               >0 if more data needs to be read or written
        """
        if self.state == self.CONNECTING_THROUGH_PROXY:
            self.inbuf += self.s.recv(8 - len(self.inbuf))
            if len(self.inbuf) == 8:
                if ord(self.inbuf[0]) == 0 and ord(self.inbuf[1]) == 0x5a:
                    debug("proxy handshake successful (fd=%d)" % self.fd())
                    self.state = self.CONNECTED
                    self.inbuf = ''
                    self.outbuf = self.data
                    debug("successfully connected (fd=%d)" % self.fd())
                    return 1    # Keep us around for writing.
                else:
                    debug("proxy handshake failed (0x%x)! (fd=%d)" %
                          (ord(self.inbuf[1]), self.fd()))
                    self.state = self.NOT_CONNECTED
                    return -1
            assert(8 - len(self.inbuf) > 0)
            return 8 - len(self.inbuf)
        return 1                # Keep us around for writing.

    def want_to_write(self):
        return self.state == self.CONNECTING or len(self.outbuf) > 0

    def on_writable(self):
        """Invoked when the socket becomes writable.
        Return 0 when done writing
               -1 on failure (like connection refused)
               >0 if more data needs to be written
        """
        if self.state == self.CONNECTING:
            if self.proxy is None:
                self.state = self.CONNECTED
                self.outbuf = self.data
                debug("successfully connected (fd=%d)" % self.fd())
            else:
                self.state = self.CONNECTING_THROUGH_PROXY
                self.outbuf = socks_cmd(self.dest)
        try:
            n = self.s.send(self.outbuf)
        except socket.error as e:
            if e[0] == errno.ECONNREFUSED:
                debug("connection refused (fd=%d)" % self.fd())
                return -1
            raise
        self.outbuf = self.outbuf[n:]
        if self.state == self.CONNECTING_THROUGH_PROXY:
            return 1            # Keep us around.
        return len(self.outbuf)  # When 0, we're being removed.


class TrafficTester():

    """
    Hang on select.select() and dispatch to Sources and Sinks.
    Time out after self.timeout seconds.
    Keep track of successful and failed data verification using a
    TestSuite.
    Return True if all tests succeed, else False.
    """

    def __init__(self, endpoint, data={}, timeout=3):
        self.listener = Listener(self, endpoint)
        self.pending_close = []
        self.timeout = timeout
        self.tests = TestSuite()
        self.data = data
        debug("listener fd=%d" % self.listener.fd())
        self.peers = {}         # fd:Peer

    def sinks(self):
        return self.get_by_ptype(Peer.SINK)

    def sources(self):
        return self.get_by_ptype(Peer.SOURCE)

    def get_by_ptype(self, ptype):
        return filter(lambda p: p.type == ptype, self.peers.itervalues())

    def add(self, peer):
        self.peers[peer.fd()] = peer
        if peer.is_source():
            self.tests.add()

    def remove(self, peer):
        self.peers.pop(peer.fd())
        self.pending_close.append(peer.s)

    def run(self):
        while not self.tests.all_done() and self.timeout > 0:
            rset = [self.listener.fd()] + list(self.peers)
            wset = [p.fd() for p in
                    filter(lambda x: x.want_to_write(), self.sources())]
            # debug("rset %s wset %s" % (rset, wset))
            sets = select.select(rset, wset, [], 1)
            if all(len(s) == 0 for s in sets):
                self.timeout -= 1
                continue

            for fd in sets[0]:  # readable fd's
                if fd == self.listener.fd():
                    self.listener.accept()
                    continue
                p = self.peers[fd]
                n = p.on_readable()
                if n > 0:
                    # debug("need %d more octets from fd %d" % (n, fd))
                    pass
                elif n == 0:  # Success.
                    self.tests.success()
                    self.remove(p)
                else:       # Failure.
                    self.tests.failure()
                    if p.is_sink():
                        print("verification failed!")
                    self.remove(p)

            for fd in sets[1]:             # writable fd's
                p = self.peers.get(fd)
                if p is not None:  # Might have been removed above.
                    n = p.on_writable()
                    if n == 0:
                        self.remove(p)
                    elif n < 0:
                        self.tests.failure()
                        self.remove(p)

        self.listener.s.close()
        for s in self.pending_close:
            s.close()
        return self.tests.all_done() and self.tests.failure_count() == 0


def main():
    """Test the TrafficTester by sending and receiving some data."""
    DATA = "a foo is a bar" * 1000
    proxy = ('localhost', 9008)
    bind_to = ('localhost', int(sys.argv[1]))

    tt = TrafficTester(bind_to, DATA)
    tt.add(Source(tt, bind_to, DATA, proxy))
    success = tt.run()

    if success:
        return 0
    return 255

if __name__ == '__main__':
    sys.exit(main())
