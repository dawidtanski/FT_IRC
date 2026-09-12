#!/usr/bin/env python3
"""Run with: python3 tests/test_channel_commands.py /path/to/ircserv.

Commands are sent separately because TCP input buffering is outside this fix.
Each test stops its server before closing sockets to isolate disconnect bugs.
"""
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time
import unittest

BINARY = str(Path(sys.argv.pop(1) if len(sys.argv) > 1 else './ircserv').resolve())


class ChannelCommands(unittest.TestCase):
    def setUp(self):
        self.sockets = {}
        self.log = tempfile.TemporaryFile()
        self.addCleanup(self.log.close)
        with socket.socket() as reserve:
            reserve.bind(('127.0.0.1', 0))
            port = reserve.getsockname()[1]
        self.process = subprocess.Popen(
            [BINARY, str(port), 'secret'], stdout=self.log, stderr=self.log,
            env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
        self.addCleanup(self.stop)
        for nick in ('alice', 'bob', 'carol'):
            for _ in range(100):
                try:
                    conn = socket.create_connection(('127.0.0.1', port), timeout=.2)
                    break
                except OSError:
                    if self.process.poll() is not None:
                        self.fail('Server exited during startup')
                    time.sleep(.01)
            else:
                self.fail('Server did not start')
            conn.settimeout(.05)
            self.sockets[nick] = conn
            for command in ('PASS secret', 'NICK ' + nick, 'USER ' + nick + ' 0 * :Test'):
                self.send(nick, command)
        self.send('alice', 'JOIN #test')
        self.send('bob', 'JOIN #test')
        self.drain()

    def stop(self):
        if self.process.poll() is None:
            self.process.terminate()
        try:
            self.process.wait(timeout=2)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait()
        for conn in self.sockets.values():
            conn.close()
        self.log.seek(0)
        diagnostics = self.log.read().decode(errors='replace')
        self.assertNotIn('ERROR: AddressSanitizer', diagnostics)
        self.assertNotIn('runtime error:', diagnostics)

    def send(self, nick, command):
        self.sockets[nick].sendall((command + '\r\n').encode())
        time.sleep(.03)
        self.assertIsNone(self.process.poll(), 'Server crashed after ' + command)

    def read(self, nick):
        result = b''
        while True:
            try:
                chunk = self.sockets[nick].recv(65536)
                if not chunk:
                    self.fail('Server closed client connection')
                result += chunk
            except socket.timeout:
                return result.decode()

    def drain(self):
        for nick in self.sockets:
            self.read(nick)

    def error(self, nick, command, code):
        self.drain()
        self.send(nick, command)
        self.assertIn(':server ' + str(code) + ' ' + nick + ' ', self.read(nick))

    def test_missing_parameters_and_channels(self):
        for command in ('KICK', 'MODE', 'TOPIC'):
            self.error('alice', command, 461)
        for command in ('KICK #missing bob', 'MODE #missing +t', 'TOPIC #missing'):
            self.error('alice', command, 403)

    def test_kick_permissions_and_broadcast(self):
        self.error('carol', 'KICK #test bob', 442)
        self.error('bob', 'KICK #test alice', 482)
        self.error('alice', 'KICK #test nobody', 401)
        self.error('alice', 'KICK #test carol', 441)
        self.send('alice', 'KICK #test bob :test reason')
        for nick in ('alice', 'bob'):
            self.assertIn(':alice!alice@127.0.0.1 KICK #test bob :test reason\r\n', self.read(nick))
        self.error('bob', 'TOPIC #test', 442)
        self.send('bob', 'JOIN #test')
        self.send('bob', 'TOPIC #test')
        self.assertIn(' 331 bob #test ', self.read('bob'))

    def test_kick_lists_and_self_kick(self):
        self.send('carol', 'JOIN #test')
        self.send('alice', 'KICK #test bob,carol')
        self.assertIn(' KICK #test bob :alice\r\n', self.read('bob'))
        self.assertIn(' KICK #test carol :alice\r\n', self.read('carol'))
        self.drain()
        self.send('alice', 'KICK #test alice')
        self.assertIn(' KICK #test alice :alice\r\n', self.read('alice'))
        self.error('alice', 'MODE #test', 403)

    def test_topic_read_set_clear_and_restriction(self):
        self.error('carol', 'TOPIC #test', 442)
        self.send('bob', 'TOPIC #test')
        self.assertIn(' 331 bob #test ', self.read('bob'))
        self.send('bob', 'TOPIC #test :hello world')
        for nick in ('alice', 'bob'):
            self.assertIn(' TOPIC #test :hello world\r\n', self.read(nick))
        self.send('alice', 'MODE #test +t')
        self.error('bob', 'TOPIC #test :forbidden', 482)
        self.send('bob', 'TOPIC #test')
        self.assertIn(' 332 bob #test :hello world\r\n', self.read('bob'))
        self.send('alice', 'TOPIC #test :')
        self.drain()
        self.send('alice', 'TOPIC #test')
        self.assertIn(' 331 alice #test ', self.read('alice'))
        self.send('alice', 'MODE #test -t')
        self.drain()
        self.send('bob', 'TOPIC #test singleword')
        self.assertIn(' TOPIC #test :singleword\r\n', self.read('alice'))

    def test_mode_permissions_and_operator_changes(self):
        self.error('carol', 'MODE #test +t', 442)
        self.error('bob', 'MODE #test +t', 482)
        self.error('alice', 'MODE #test +o nobody', 401)
        self.error('alice', 'MODE #test +o carol', 441)
        self.send('alice', 'MODE #test +o bob')
        for nick in ('alice', 'bob'):
            self.assertIn(' MODE #test +o bob\r\n', self.read(nick))
        self.send('bob', 'MODE #test +t')
        self.assertIn(' MODE #test +t\r\n', self.read('alice'))
        self.send('alice', 'MODE #test -o bob')
        self.error('bob', 'MODE #test -t', 482)

    def test_mode_key_invite_and_limit_enforced_by_join(self):
        self.send('alice', 'MODE #test +ikl secret 2')
        self.drain()
        self.send('alice', 'MODE #test')
        self.assertIn(' 324 alice #test +ikl secret 2\r\n', self.read('alice'))
        self.send('carol', 'MODE #test')
        self.assertIn(' 324 carol #test +ikl * 2\r\n', self.read('carol'))
        self.error('carol', 'JOIN #test secret', 471)
        self.send('alice', 'MODE #test -l')
        self.error('carol', 'JOIN #test secret', 473)
        self.send('alice', 'MODE #test -i')
        self.error('carol', 'JOIN #test wrong', 475)
        self.send('carol', 'JOIN #test secret')
        self.send('carol', 'TOPIC #test')
        self.assertIn(' 331 carol #test ', self.read('carol'))
        self.send('alice', 'MODE #test -k secret')
        self.drain()
        self.send('alice', 'MODE #test')
        self.assertIn(' 324 alice #test +\r\n', self.read('alice'))

    def test_mode_validation_and_multiple_groups(self):
        for mode in ('+o', '+k', '+l'):
            self.error('alice', 'MODE #test ' + mode, 461)
        self.error('alice', 'MODE #test +z', 472)
        for limit in ('0', '-1', 'abc', '12x', '999999999999999999999999999999999999'):
            self.error('alice', 'MODE #test +l ' + limit, 696)
        self.error('alice', 'MODE #test +k :bad key', 696)
        self.send('alice', 'MODE #test +it -i +l 2')
        self.drain()
        self.send('alice', 'MODE #test')
        self.assertIn(' 324 alice #test +tl 2\r\n', self.read('alice'))
        self.send('bob', 'JOIN #test')  # Existing members are not rejected by +l.
        self.assertEqual('', self.read('bob'))

    def test_user_modes_target_combined_flags_and_operator(self):
        self.error('alice', 'MODE bob +i', 502)
        self.send('alice', 'MODE alice +iws')
        self.drain()
        self.send('alice', 'MODE alice')
        self.assertIn(' 221 alice +iws\r\n', self.read('alice'))
        self.send('alice', 'MODE alice +o')
        self.send('alice', 'MODE alice -iw')
        self.drain()
        self.send('alice', 'MODE alice')
        self.assertIn(' 221 alice +s\r\n', self.read('alice'))
        self.error('alice', 'MODE alice +z', 501)


if __name__ == '__main__':
    unittest.main(verbosity=2)
