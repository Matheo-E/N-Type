"""Black-box checks for the two-player demo (Python standard library only)."""
import socket
import struct
import subprocess
import sys
import time

HEADER = struct.Struct('<2sBBI')
STATE = struct.Struct('<2sBBIIBBHHHH')


class Peer:
    def __init__(self, port, nonce):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.socket.connect(('127.0.0.1', port))
        self.socket.settimeout(0.1)
        self.nonce = nonce

    def send(self, kind, extra=b''):
        self.socket.send(HEADER.pack(b'NT', 1, kind, self.nonce) + extra)

    def input(self, sequence, bits):
        self.send(2, struct.pack('<IB', sequence, bits))

    def wait(self, predicate, timeout=2):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                data = self.socket.recv(256)
            except (socket.timeout, ConnectionRefusedError, ConnectionResetError):
                continue
            if len(data) == 22:
                value = STATE.unpack(data)
                if value[:3] != (b'NT', 1, 3) or value[3] != self.nonce:
                    continue
                state = dict(tick=value[4], slot=value[5], mask=value[6],
                             positions=((value[7], value[8]), (value[9], value[10])))
                if predicate(state):
                    return state
            elif len(data) == 8 and HEADER.unpack(data) == (b'NT', 1, 5, self.nonce):
                if predicate(None):
                    return None
        raise AssertionError('Expected server state not received')

    def close(self):
        self.socket.close()


def check(binary):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        probe.bind(('127.0.0.1', 0))
        port = probe.getsockname()[1]
    server = subprocess.Popen([binary, '-p', str(port)], stdout=subprocess.DEVNULL)
    peers = []
    try:
        first, second, third = [Peer(port, nonce) for nonce in (101, 202, 303)]
        peers = [first, second, third]
        # Allow startup without assuming a fixed process startup duration.
        deadline = time.monotonic() + 5
        while True:
            assert server.poll() is None, 'Server failed to start'
            first.send(1)
            try:
                alone = first.wait(lambda s: s is not None and s['mask'] == 1, 0.2)
                break
            except AssertionError:
                if time.monotonic() >= deadline:
                    raise
        assert alone['slot'] == 0
        assert alone['positions'][0] == (200, 280)

        # A malformed or server-only packet from a stranger must not create a player.
        third.socket.send(b'garbage')
        third.socket.send(HEADER.pack(b'NT', 9, 1, third.nonce))
        unchanged = first.wait(lambda s: s is not None and s['tick'] > alone['tick'])
        assert unchanged['mask'] == 1

        second.send(1)
        both = first.wait(lambda s: s is not None and s['mask'] == 3)
        other = second.wait(lambda s: s is not None and s['mask'] == 3)
        assert other['slot'] == 1
        assert both['positions'] == ((200, 280), (560, 280))
        third.send(1)
        third.wait(lambda s: s is None)  # Server full.

        # Each endpoint controls only its player; client coordinates are never sent.
        first.input(10, 2)  # Right.
        moved = first.wait(lambda s: s is not None and s['positions'][0][0] >= 220)
        assert moved['positions'][1] == (560, 280)
        first.input(11, 0)
        stopped = first.wait(lambda s: s is not None and s['tick'] > moved['tick'] + 4)
        first.input(9, 1)  # Old input must be ignored.
        later = first.wait(lambda s: s is not None and s['tick'] > stopped['tick'] + 6)
        assert later['positions'][0] == stopped['positions'][0]

        second.input(1, 4)  # Up.
        moved = second.wait(lambda s: s is not None and s['positions'][1][1] <= 260)
        assert moved['positions'][0] == stopped['positions'][0]
        second.input(2, 0)
        second.send(4)
        first.wait(lambda s: s is not None and s['mask'] == 1)

        # A freed slot can be used again; a crashed peer disappears after timeout.
        second.send(1)
        first.wait(lambda s: s is not None and s['mask'] == 3)
        sequence = 20
        deadline = time.monotonic() + 5
        timed_out = False
        while time.monotonic() < deadline:
            first.input(sequence, 0)
            sequence += 1
            state = first.wait(lambda s: s is not None)
            if state['mask'] == 1:
                timed_out = True
                break
            time.sleep(0.03)
        assert timed_out, 'Crashed peer did not disappear'
        assert server.poll() is None
        print('PASS: one/two players, full server, malformed packets, independent movement,')
        print('      stale input, disconnect, slot reuse and crash timeout.')
    finally:
        for peer in peers:
            peer.close()
        server.terminate()
        try:
            server.wait(timeout=5)
        except subprocess.TimeoutExpired:
            server.kill()
            server.wait()


if __name__ == '__main__':
    check(sys.argv[1])
