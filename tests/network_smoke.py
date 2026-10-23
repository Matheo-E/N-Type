"""Black-box checks for the server (Python standard library only)."""
import socket
import struct
import subprocess
import sys
import time

# Message types from docs/PROTOCOL_v2.md: one type byte, then the payload.
CONNECT, ACCEPT, REFUSE, DISCONNECT, INPUT = 1, 2, 3, 4, 5
SNAPSHOT, CLIENT_LEFT = 9, 11
UP, DOWN, LEFT, RIGHT = 1, 2, 4, 8
PLAYER_KIND = 1
SPAWNS = {1: (100, 150), 2: (100, 270), 3: (100, 390), 4: (100, 510)}


def parse(kind, payload):
    if kind in (ACCEPT, REFUSE, CLIENT_LEFT):
        return payload[0]
    if kind == SNAPSHOT:
        players = {}
        for index in range(payload[0]):
            entity, entity_kind, x, y = struct.unpack_from('<HBhh', payload, 1 + 7 * index)
            assert entity_kind == PLAYER_KIND
            players[entity] = (x, y)
        return dict(players=players)
    return None


class Peer:
    def __init__(self, port):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.socket.connect(('127.0.0.1', port))
        self.socket.settimeout(0.1)

    def send(self, kind, payload=b''):
        self.socket.send(bytes([kind]) + payload)

    def input(self, bits):
        self.send(INPUT, bytes([bits]))

    def wait(self, kind, predicate=lambda value: True, timeout=2):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                data = self.socket.recv(1500)
            except (socket.timeout, ConnectionRefusedError, ConnectionResetError):
                continue
            if not data or data[0] != kind:
                continue
            value = parse(kind, data[1:])
            if predicate(value):
                return value
        raise AssertionError(f'Expected message {kind} not received')

    def skip(self, count):
        """Returns the snapshot `count` snapshots from now (the server sends one per tick)."""
        seen = []
        return self.wait(SNAPSHOT, lambda s: seen.append(s) or len(seen) == count)

    def close(self):
        self.socket.close()


def check(binary):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        probe.bind(('127.0.0.1', 0))
        port = probe.getsockname()[1]
    server = subprocess.Popen([binary, '-p', str(port)], stdout=subprocess.DEVNULL)
    peers = []
    try:
        peers = [Peer(port) for _ in range(5)]
        first, second, third, fourth, fifth = peers
        # Allow startup without assuming a fixed process startup duration.
        deadline = time.monotonic() + 5
        while True:
            assert server.poll() is None, 'Server failed to start'
            first.send(CONNECT)
            try:
                assert first.wait(ACCEPT, timeout=0.2) == 1
                break
            except AssertionError:
                if time.monotonic() >= deadline:
                    raise
        alone = first.wait(SNAPSHOT)
        assert alone['players'] == {1: SPAWNS[1]}

        # Malformed packets, or a server-only message from a stranger, must not create a player.
        fifth.socket.send(b'garbage')
        fifth.socket.send(bytes([CONNECT, 0]))
        fifth.send(SNAPSHOT, bytes([0]))
        fifth.send(INPUT, bytes([RIGHT]))
        unchanged = first.skip(6)
        assert unchanged['players'] == {1: SPAWNS[1]}

        # Up to four players, each with the lowest free id; a fifth is refused.
        for expected, peer in enumerate((second, third, fourth), start=2):
            peer.send(CONNECT)
            assert peer.wait(ACCEPT) == expected
        full = first.wait(SNAPSHOT, lambda s: len(s['players']) == 4)
        assert full['players'] == SPAWNS
        fifth.send(CONNECT)
        assert fifth.wait(REFUSE) == 1  # ServerFull

        # A repeated Connect (lost Accept) gets the same id back.
        first.send(CONNECT)
        assert first.wait(ACCEPT) == 1

        # Each endpoint controls only its player; client coordinates are never sent.
        first.input(RIGHT)
        moved = first.wait(SNAPSHOT, lambda s: s['players'][1][0] >= 150)
        assert moved['players'][2] == SPAWNS[2]
        first.input(0)
        stopped = first.skip(5)
        later = first.skip(6)
        assert later['players'][1] == stopped['players'][1]

        second.input(UP)
        moved = first.wait(SNAPSHOT, lambda s: s['players'][2][1] <= 250)
        assert moved['players'][1] == stopped['players'][1]
        second.input(0)

        # A disconnect is announced to the others, and the freed id is reused.
        second.send(DISCONNECT)
        assert first.wait(CLIENT_LEFT, lambda player: player == 2) == 2
        first.wait(SNAPSHOT, lambda s: 2 not in s['players'])
        second.send(CONNECT)
        assert second.wait(ACCEPT) == 2

        # Silent (crashed) peers disappear after the timeout; the active one stays.
        deadline = time.monotonic() + 8
        timed_out = False
        while time.monotonic() < deadline:
            first.input(0)
            try:
                first.wait(SNAPSHOT, lambda s: set(s['players']) == {1}, timeout=0.3)
                timed_out = True
                break
            except AssertionError:
                continue
        assert timed_out, 'Crashed peers did not disappear'
        assert server.poll() is None
        print('PASS: connect, four players, full server, malformed packets, independent movement,')
        print('      repeated connect, disconnect, id reuse and crash timeout.')
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
