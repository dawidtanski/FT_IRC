#!/usr/bin/env python3
"""Black-box TCP regression tests. Run: python3 tests/integration.py [server-binary]."""
import os
import shutil
import select
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import time
import unittest

BINARY = os.path.abspath(sys.argv.pop(1) if len(sys.argv) > 1 else './ircserv')


class Peer:
    def __init__(self, port):
        self.socket = socket.create_connection(('127.0.0.1', port), timeout=2)
        self.buffer = b''

    def send(self, line):
        self.socket.sendall((line + '\r\n').encode())

    def until(self, token, timeout=2):
        deadline = time.monotonic() + timeout
        lines = []
        while time.monotonic() < deadline:
            while b'\r\n' in self.buffer:
                raw, self.buffer = self.buffer.split(b'\r\n', 1)
                line = raw.decode(errors='replace')
                lines.append(line)
                if token in line:
                    return '\n'.join(lines)
            remaining = max(0, deadline - time.monotonic())
            if not select.select([self.socket], [], [], remaining)[0]:
                break
            data = self.socket.recv(65536)
            if not data:
                raise AssertionError('Connection closed waiting for ' + token)
            self.buffer += data
        raise AssertionError('Missing %r; received %r' % (token, lines))

    def barrier(self):
        self.send('PING :barrier')
        return self.until(' PONG server :barrier')

    def close(self):
        self.socket.close()


class IRC(unittest.TestCase):
    def setUp(self):
        with socket.socket() as probe:
            probe.bind(('127.0.0.1', 0))
            self.port = probe.getsockname()[1]
        self.log = tempfile.TemporaryFile()
        self.proc = subprocess.Popen([BINARY, str(self.port), 'secret'],
                                     stdout=self.log, stderr=self.log)
        self.peers = []
        deadline = time.monotonic() + 3
        while True:
            try:
                p = self.peer()
                p.close()
                break
            except OSError:
                if self.proc.poll() is not None or time.monotonic() > deadline:
                    self.log.seek(0)
                    self.fail('Server startup failed: ' + self.log.read().decode())
                time.sleep(.02)

    def tearDown(self):
        for peer in self.peers:
            peer.close()
        if self.proc.poll() is None:
            self.proc.send_signal(signal.SIGTERM)
        try:
            code = self.proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            self.proc.kill()
            self.proc.wait()
            self.fail('Server failed to stop')
        self.log.seek(0)
        log = self.log.read().decode(errors='replace')
        self.log.close()
        self.assertEqual(code, 0, log)
        self.assertNotIn('Sanitizer', log)
        self.assertNotIn('runtime error:', log)
        self.assertNotIn('secret', log)

    def peer(self, nick=None):
        peer = Peer(self.port)
        self.peers.append(peer)
        if nick:
            peer.send('PASS secret\r\nNICK ' + nick + '\r\nUSER user 0 * :Real Name')
            peer.until(' 376 ')
        return peer

    def joined(self):
        a, b = self.peer('alice'), self.peer('bob')
        a.send('JOIN #room'); a.until(' 366 ')
        b.send('JOIN #room'); b.until(' 366 ')
        a.barrier()
        return a, b

    def test_registration_fragmented_cap_and_auth(self):
        p = self.peer()
        p.send('CAP LS 302'); p.until(' CAP * LS :')
        p.socket.sendall(b'PA')
        p.socket.sendall(b'SS :secret\r')
        p.socket.sendall(b'\nnick alice\r\nUSER user 0 * :Name\r\n')
        self.assertNotIn(' 001 ', p.barrier())
        p.send('CAP END'); p.until(' 001 alice '); p.until(' 376 ')
        p.send('PASS secret'); p.until(' 462 ')
        q = self.peer(); q.send('JOIN #room'); q.until(' 451 ')
        q.send('PASS wrong'); q.until(' 464 ')
        self.assertEqual(q.socket.recv(1), b'')

    def test_missing_params_and_parser_recovery(self):
        p = self.peer('alice')
        for cmd in ['PRIVMSG', 'PRIVMSG :hello', 'PRIVMSG bob', 'JOIN', 'PART', 'KICK', 'INVITE', 'TOPIC', 'MODE', 'NICK', 'USER']:
            p.send(cmd)
            self.assertIn(':server ', p.barrier())
        p.socket.sendall(b'PRIVMSG bob :bad\x00text\r\n')
        p.until(' 417 ')
        p.send('UNKNOWN'); p.until(' 421 ')
        p.send('   pInG   :spaced token'); p.until(' PONG server :spaced token')

    def test_direct_and_channel_messages(self):
        a, b = self.joined()
        c = self.peer('carol')
        a.send('PRIVMSG #ROOM :hello channel'); b.until('PRIVMSG #ROOM :hello channel')
        self.assertNotIn('PRIVMSG', a.barrier())
        a.send('PRIVMSG BOB,carol hello'); b.until('PRIVMSG BOB :hello'); c.until('PRIVMSG carol :hello')
        c.send('PRIVMSG #room :outside'); c.until(' 404 ')
        a.send('PRIVMSG #missing :x'); a.until(' 403 ')
        a.send('NOTICE bob :notice'); b.until('NOTICE bob :notice')
        a.send('NOTICE nobody :x'); self.assertNotIn(' 401 ', a.barrier())

    def test_nick_validation_and_broadcast(self):
        a, b = self.joined()
        a.send('JOIN #second'); a.until(' 366 ')
        b.send('JOIN #second'); b.until(' 366 '); a.barrier()
        a.send('NICK Alicia'); a.until(' NICK :Alicia')
        received = b.barrier()
        self.assertEqual(received.count(' NICK :Alicia'), 1)
        b.send('PRIVMSG alicia :new nick'); a.until('PRIVMSG alicia :new nick')
        b.send('NICK ALICIA'); b.until(' 433 ')
        for nick in ['#bad', '1bad', 'bad!nick', 'verylongnickname']:
            b.send('NICK ' + nick); b.until(' 432 ')

    def test_part_empty_channel_and_join_zero(self):
        a = self.peer('alice')
        a.send('JOIN #One,#Two'); a.until(' 366 alice #Two ')
        a.send('PART #ONE,#two :bye'); a.until(' PART #One :bye'); a.until(' PART #Two :bye')
        a.send('JOIN #One'); self.assertIn('@alice', a.until(' 366 '))
        a.send('MODE #one +i'); a.until(' MODE #one +i')
        a.send('JOIN #two'); a.until(' 366 ')
        a.send('JOIN 0'); received = a.barrier(); self.assertEqual(received.count(' PART '), 2)
        a.send('PART #one'); a.until(' 403 ')

    def test_channel_modes_and_operator_rights(self):
        a, b = self.joined()
        b.send('MODE #room +i'); b.until(' 482 ')
        a.send('MODE #room +itkl key 2'); a.until(' MODE #room +l 2')
        c = self.peer('carol'); c.send('JOIN #room key'); c.until(' 471 ')
        a.send('MODE #room -l'); a.until(' MODE #room -l')
        c.send('JOIN #room key'); c.until(' 473 ')
        b.send('INVITE carol #room'); b.until(' 482 ')
        a.send('INVITE carol #room'); a.until(' 341 alice carol #room'); c.until(' INVITE carol :#room')
        c.send('JOIN #room wrong'); c.until(' 475 ')
        c.send('JOIN #room key'); c.until(' 366 ')
        b.send('TOPIC #room :no'); b.until(' 482 ')
        a.send('MODE #room +o bob'); a.until(' MODE #room +o bob')
        b.send('TOPIC #room :new topic'); b.until(' TOPIC #room :new topic')
        a.send('MODE #room -o bob'); a.until(' MODE #room -o bob')
        b.send('KICK #room alice'); b.until(' 482 ')
        a.send('MODE #room -itk'); a.until(' MODE #room -k')
        b.send('TOPIC #room :'); b.until(' TOPIC #room :')
        b.send('TOPIC #room'); b.until(' 331 ')
        for limit in ['0', '-1', 'abc', '999999999999999999999999999999999']:
            a.send('MODE #room +l ' + limit); a.until(' 696 ')
        a.send('MODE #room'); a.until(' 324 alice #room +')

    def test_kick_and_rejoin(self):
        a, b = self.joined()
        a.send('KICK #ROOM bob :bye'); a.until(' KICK #ROOM bob :bye'); b.until(' KICK #ROOM bob :bye')
        b.send('PRIVMSG #room :outside'); b.until(' 404 ')
        b.send('JOIN #room'); b.until(' 366 ')
        a.send('KICK #room alice,bob :both'); a.until(' KICK #room alice :both'); a.until(' 442 ')
        b.send('QUIT :done')

    def test_invite_survives_rename_but_not_disconnect(self):
        a = self.peer('alice'); b = self.peer('bob')
        a.send('JOIN #room'); a.until(' 366 ')
        a.send('MODE #room +i'); a.until(' MODE ')
        a.send('INVITE bob #room'); a.until(' 341 '); b.until(' INVITE ')
        b.send('NICK robert'); b.until(' NICK ')
        b.send('JOIN #room'); b.until(' 366 ')
        b.send('PART #room'); b.until(' PART ')
        b.send('JOIN #room'); b.until(' 473 ')
        a.send('INVITE robert #room'); a.until(' 341 '); b.until(' INVITE ')
        b.send('QUIT'); self.assertEqual(b.socket.recv(1), b'')
        c = self.peer('robert'); c.send('JOIN #room'); c.until(' 473 ')

    def test_quit_dedup_and_reset(self):
        a, b = self.joined()
        a.send('JOIN #two'); a.until(' 366 ')
        b.send('JOIN #two'); b.until(' 366 '); a.barrier()
        b.send('QUIT :bye')
        self.assertEqual(b.socket.recv(1), b'')
        self.assertEqual(a.barrier().count(' QUIT :bye'), 1)
        c = self.peer('bob'); c.send('JOIN #room'); c.until(' 366 ')
        c.socket.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack('ii', 1, 0))
        c.close()
        a.send('PRIVMSG #room :after reset'); a.barrier()
        d = self.peer('dave'); d.barrier()

    def test_queries_and_line_limits(self):
        a, b = self.joined()
        for cmd, reply in [('NAMES #room', ' 366 '), ('WHO #room', ' 315 '), ('WHOIS bob', ' 318 '), ('LIST', ' 323 '), ('MODE #room +b', ' 368 ')]:
            a.send(cmd); a.until(reply)
        a.send('PRIVMSG bob :' + 'x' * 497)
        response = b.until('PRIVMSG bob :')
        self.assertLessEqual(len(response.encode()) + 2, 512)
        bad = self.peer('bad'); bad.socket.sendall(b'x' * 512)
        self.assertEqual(bad.socket.recv(1), b'')
        a.barrier()

    def test_channel_names(self):
        a = self.peer('alice')
        for name in ['room', '#' + 'a' * 50, '#a:b', '#a\x07b']:
            a.send('JOIN ' + name); a.until(' 403 ')
        a.send('JOIN #Good'); a.until(' 366 ')
        b = self.peer('bob'); b.send('JOIN #good')
        self.assertIn('@alice', b.until(' 366 '))

    def test_large_names_reply_is_split(self):
        owner = self.peer('owner')
        owner.send('JOIN #many'); owner.until(' 366 ')
        for i in range(60):
            p = self.peer('user%05d' % i)
            p.send('JOIN #many'); p.until(' 366 ')
        owner.barrier()
        owner.send('NAMES #many')
        reply = owner.until(' 366 ')
        names = []
        for line in reply.splitlines():
            self.assertLessEqual(len(line.encode()) + 2, 512)
            if ' 353 ' in line:
                names.extend(line.split(' :', 1)[1].split())
        self.assertEqual(len(names), 61)
        self.assertIn('@owner', names)
        self.assertIn('user00059', names)

    def test_mode_key_removal_and_topic_limit(self):
        a, b = self.joined()
        a.send('MODE #room +ko key bob'); a.until(' MODE #room +o bob')
        a.send('MODE #room -ko ignored bob'); a.until(' MODE #room -o bob')
        b.send('MODE #room +i'); b.until(' 482 ')
        a.send('MODE #room +k key'); a.until(' MODE #room +k key')
        a.send('MODE #room -k'); a.until(' MODE #room -k')
        a.send('TOPIC #room :' + 'x' * 400)
        response = a.until(' TOPIC #room :')
        self.assertEqual(len(response.split(' TOPIC #room :')[1]), 300)
        a.send('TOPIC #room'); self.assertIn('x' * 300, a.until(' 332 '))

    @unittest.skipUnless(shutil.which('irssi'), 'Irssi is not installed')
    def test_reference_client_irssi(self):
        observer = self.peer('observer')
        observer.send('JOIN #reference'); observer.until(' 366 ')
        master, slave = os.openpty()
        with tempfile.TemporaryDirectory(prefix='ft-irc-irssi-') as home:
            ref = subprocess.Popen(['irssi', '--home=' + home, '--noconnect',
                                    '--nick=refcli', '--connect=127.0.0.1',
                                    '--port=' + str(self.port), '--password=secret'],
                                   stdin=slave, stdout=slave, stderr=slave,
                                   env=dict(os.environ, TERM='xterm'))
            os.close(slave)
            try:
                screen = b''
                deadline = time.monotonic() + 5
                while b'Welcome to the IRC network' not in screen and time.monotonic() < deadline:
                    if select.select([master], [], [], .2)[0]:
                        screen += os.read(master, 65536)
                self.assertIn(b'Welcome to the IRC network', screen, repr(screen))
                os.write(master, b'/join #reference\n')
                observer.until(':refcli!')
                os.write(master, b'/msg observer hello-from-irssi\n')
                observer.until('PRIVMSG observer :hello-from-irssi', timeout=12)
                os.write(master, b'/msg #reference channel-from-irssi\n')
                observer.until('PRIVMSG #reference :channel-from-irssi', timeout=12)
                observer.send('MODE #reference +o refcli'); observer.until(' MODE ')
                os.write(master, b'/topic #reference topic-from-irssi\n')
                observer.until(' TOPIC #reference :topic-from-irssi', timeout=12)
                os.write(master, b'/query observer\n')
                observer.send('PRIVMSG refcli :hello-to-irssi')
                deadline = time.monotonic() + 3
                while b'hello-to-irssi' not in screen and time.monotonic() < deadline:
                    if select.select([master], [], [], .2)[0]:
                        screen += os.read(master, 65536)
                self.assertIn(b'hello-to-irssi', screen)
                os.write(master, b'/quit done\n')
                observer.until(' QUIT :done')
                ref.wait(timeout=3)
            except Exception:
                while select.select([master], [], [], .1)[0]:
                    try:
                        screen += os.read(master, 65536)
                    except OSError:
                        break
                print('IRSSI SCREEN (tail):', repr(screen[-2000:]))
                raise
            finally:
                if ref.poll() is None:
                    ref.terminate()
                    ref.wait(timeout=3)
                os.close(master)

    def test_slow_reader_does_not_block_other_clients(self):
        slow = self.peer('slow')
        slow.socket.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1024)
        sender, observer = self.peer('sender'), self.peer('observer')
        # More than the kernel send buffer; a blocking sendall stalls here.
        batch = ('PRIVMSG slow :' + 'x' * 450 + '\r\n') * 20
        sender.socket.setblocking(False)
        payload = (batch * 800).encode()
        offset = 0
        deadline = time.monotonic() + 8
        while offset < len(payload) and time.monotonic() < deadline:
            readable, writable, _ = select.select([sender.socket], [sender.socket], [], .1)
            if readable:
                self.assertTrue(sender.socket.recv(65536))
            if writable:
                try:
                    offset += sender.socket.send(payload[offset:offset + 65536])
                except BlockingIOError:
                    pass
        self.assertEqual(offset, len(payload), 'Server stopped reading while a peer was slow')
        observer.barrier()
        self.assertIsNone(self.proc.poll())


if __name__ == '__main__':
    unittest.main(verbosity=2)
