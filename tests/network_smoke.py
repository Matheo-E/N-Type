"""Black-box checks for the server (Python standard library only)."""
import socket
import subprocess
import struct
import sys
import threading
import time

# Message types from docs/PROTOCOL_v2.md: one type byte, then the payload.
CONNECT, ACCEPT, REFUSE, DISCONNECT, INPUT = 1, 2, 3, 4, 5
SNAPSHOT, CLIENT_LEFT = 9, 11
LIST_REQUEST, LIST, CREATE, JOIN, LOBBY_REFUSE, LEAVE = 13, 14, 15, 16, 17, 18
STATE, RENAME, START = 19, 20, 21
UP, DOWN, LEFT, RIGHT = 1, 2, 4, 8
PLAYER_KIND = 1
WAITING, PLAYING = 1, 2
PLAYER, SPECTATOR = 1, 2
MAX_CLIENTS = 128


def string(text):
    data = text.encode()
    return bytes([len(data)]) + data


def read_string(payload, pos):
    size = payload[pos]
    return payload[pos + 1:pos + 1 + size].decode(), pos + 1 + size


def parse(kind, payload):
    if kind in (REFUSE, CLIENT_LEFT, LOBBY_REFUSE):
        return payload[0]
    if kind == SNAPSHOT:
        # frame, part, parts, count, then 14-byte entities: id, kind, type, hp, hits, x, y.
        # Without enemies a frame is one part holding only the ships.
        frame, part, parts, count = struct.unpack_from('<IBBB', payload, 0)
        assert (part, parts) == (0, 1)
        ships = {}
        for index in range(count):
            entity, entity_kind, _, _, _, x, y = struct.unpack_from('<HBBBBii', payload, 7 + 14 * index)
            assert entity_kind == PLAYER_KIND
            ships[entity] = (x, y)
        return ships
    if kind == LIST:
        lobbies, pos = [], 1
        for _ in range(payload[0]):
            code = payload[pos:pos + 4].decode()
            name, pos = read_string(payload, pos + 4)
            lobbies.append(dict(code=code, name=name, members=payload[pos], state=payload[pos + 1]))
            pos += 2
        return lobbies
    if kind == STATE:
        code = payload[0:4].decode()
        name, pos = read_string(payload, 4)
        state, level, result, your, leader, count = payload[pos:pos + 6]
        pos += 6
        members, scores = [], {}
        for _ in range(count):
            slot, role, score = struct.unpack_from('<BBI', payload, pos)
            member_name, pos = read_string(payload, pos + 6)
            members.append((slot, role, member_name))
            scores[slot] = score
        return dict(code=code, name=name, state=state, level=level, result=result, your=your, leader=leader, members=members,
                    scores=scores)
    return None


def role_of(state, slot):
    return next(role for member_slot, role, _ in state['members'] if member_slot == slot)


class Peer:
    def __init__(self, port):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.socket.connect(('127.0.0.1', port))
        self.socket.settimeout(0.05)
        self.connected = False
        self.silent = False  # True: stop the heartbeat, like a crashed client
        self.keys = 0

    def send(self, kind, payload=b''):
        self.socket.send(bytes([kind]) + payload)

    def input(self, keys):
        self.keys = keys
        self.send(INPUT, bytes([keys]))

    def connect(self, name, timeout=5):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            self.send(CONNECT, string(name))
            try:
                self.wait(ACCEPT, timeout=0.2)
                self.connected = True
                return
            except AssertionError:
                continue
        raise AssertionError(f'{name} could not connect')

    def receive(self, deadline):
        """Yields (kind, value) until the deadline."""
        while time.monotonic() < deadline:
            try:
                data = self.socket.recv(1500)
            except (socket.timeout, ConnectionRefusedError, ConnectionResetError):
                continue
            if data:
                yield data[0], parse(data[0], data[1:])

    def wait(self, kind, predicate=lambda value: True, timeout=2):
        for got, value in self.receive(time.monotonic() + timeout):
            if got == kind and predicate(value):
                return value
        raise AssertionError(f'Expected message {kind} not received')

    def collect(self, kind, duration):
        """Every message of `kind` received during `duration` seconds."""
        return [value for got, value in self.receive(time.monotonic() + duration) if got == kind]

    def close(self):
        self.socket.close()


def check(binary):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        probe.bind(('127.0.0.1', 0))
        port = probe.getsockname()[1]
    # No enemies: these checks are about the network and the lobbies, not gameplay.
    server = subprocess.Popen([binary, '-p', str(port), '--no-enemies'], stdin=subprocess.PIPE,
                              stdout=subprocess.DEVNULL)
    peers = []
    stop = threading.Event()

    def command(text):
        server.stdin.write((text + '\n').encode())
        server.stdin.flush()

    def heartbeat():
        # Input is the heartbeat: keep every live peer connected while others are checked.
        while not stop.wait(0.3):
            for peer in list(peers):
                if peer.connected and not peer.silent:
                    try:
                        peer.input(peer.keys)
                    except OSError:
                        pass

    threading.Thread(target=heartbeat, daemon=True).start()
    try:
        a, b, c, d, e, stranger = (Peer(port) for _ in range(6))
        peers.extend([a, b, c, d, e, stranger])
        assert server.poll() is None, 'Server failed to start'
        a.connect('Ann')

        # 1. Malformed packets and requests from unknown endpoints create nothing.
        stranger.socket.send(b'garbage')
        stranger.send(CONNECT, bytes([0]))  # empty name
        stranger.send(CREATE, string('Ghost'))
        time.sleep(0.2)
        a.send(LIST_REQUEST)
        assert a.wait(LIST) == []

        # 2. Creating a lobby makes you its leader.
        a.send(CREATE, string('Alpha'))
        alpha = a.wait(STATE)
        code = alpha['code']
        assert len(code) == 4 and alpha['name'] == 'Alpha'
        assert alpha['state'] == WAITING and alpha['members'] == [(1, PLAYER, 'Ann')]
        assert alpha['your'] == 1 and alpha['leader'] == 1

        # 3. The lobby is listed, and can be joined by code.
        b.connect('Bo')
        b.send(LIST_REQUEST)
        assert b.wait(LIST) == [dict(code=code, name='Alpha', members=1, state=WAITING)]
        b.send(JOIN, code.encode())
        joined = b.wait(STATE)
        assert joined['your'] == 2 and joined['leader'] == 1 and len(joined['members']) == 2

        # 4. A member asking for another lobby stays where it is; unknown codes are refused.
        b.send(JOIN, b'ZZZZ')
        assert b.wait(STATE)['code'] == code
        b.send(CREATE, string('Other'))
        assert b.wait(STATE)['code'] == code
        b.send(LIST_REQUEST)
        assert len(b.wait(LIST)) == 1
        c.connect('Cy')
        c.send(JOIN, b'ZZZZ')
        assert c.wait(LOBBY_REFUSE) == 1  # NotFound

        # 5. Only the leader starts; then the lobby's members get snapshots, others do not.
        b.send(START)
        states = b.collect(STATE, 0.4)
        assert states and all(state['state'] == WAITING for state in states)
        a.send(RENAME, string('Beta'))
        a.send(START)
        beta = b.wait(STATE, lambda s: s['state'] == PLAYING)
        assert beta['name'] == 'Beta'
        assert set(a.wait(SNAPSHOT)) == {1, 2}
        assert c.collect(SNAPSHOT, 0.5) == []
        a.input(RIGHT)
        moved = b.wait(SNAPSHOT, lambda s: s[1][0] >= 150)
        assert moved[2] == (100, 270)
        a.input(0)

        # 6. Joining a running game makes a spectator, promoted when a level starts.
        c.send(JOIN, code.encode())
        spectating = c.wait(STATE, lambda s: s['your'] == 3)
        assert role_of(spectating, 3) == SPECTATOR
        c.input(RIGHT)
        snapshots = c.collect(SNAPSHOT, 0.5)
        assert snapshots and all(set(ships) == {1, 2} for ships in snapshots)
        command('level ' + code.lower())
        assert 3 in c.wait(SNAPSHOT, lambda s: 3 in s)
        assert role_of(c.wait(STATE, lambda s: role_of(s, 3) == PLAYER), 3) == PLAYER
        c.input(0)

        # Scores: the console gives points; every member sees them.
        command('score ' + code + ' 2 150')
        command('score ' + code + ' 2 50')
        assert c.wait(STATE, lambda s: s['scores'].get(2) == 200)['scores'][1] == 0

        # 7. Game over sends everyone back to the waiting room as players, scores kept.
        command('gameover ' + code)
        over = a.wait(STATE, lambda s: s['state'] == WAITING)
        assert over['result'] == 1  # GameResult::Defeat, shown on the results screen
        assert all(role == PLAYER for _, role, _ in over['members'])
        assert over['scores'][2] == 200

        # 8. When the leader leaves, the longest-present member leads.
        a.send(DISCONNECT)
        a.connected = False
        assert b.wait(STATE, lambda s: s['leader'] == 2)['leader'] == 2

        # 9. Leaving is answered with the list, and the lobby state stops.
        b.send(LEAVE)
        b.wait(LIST)
        assert b.collect(STATE, 0.5) == []

        # 10. Two lobbies play at once; each receives only its own snapshots.
        c.send(START)  # C is now alone in Beta, with slot 3
        c.wait(STATE, lambda s: s['state'] == PLAYING and s['leader'] == 3)
        d.connect('Di')
        d.send(CREATE, string('Delta'))
        delta = d.wait(STATE)['code']
        e.connect('Ed')
        e.send(JOIN, delta.encode())
        e.wait(STATE, lambda s: len(s['members']) == 2)
        d.send(START)
        d.wait(STATE, lambda s: s['state'] == PLAYING)
        for peer, slots in ((c, {3}), (d, {1, 2}), (e, {1, 2})):
            snapshots = peer.collect(SNAPSHOT, 0.5)
            assert snapshots and all(set(ships) == slots for ships in snapshots)

        # Players reach every screen edge, and held input cannot push them out.
        d.input(RIGHT | DOWN)
        d.wait(SNAPSHOT, lambda s: s.get(1) == (1240, 680), timeout=6)
        assert d.collect(SNAPSHOT, 0.3)[-1][1] == (1240, 680)
        d.input(LEFT | UP)
        d.wait(SNAPSHOT, lambda s: s.get(1) == (0, 0), timeout=6)
        assert d.collect(SNAPSHOT, 0.3)[-1][1] == (0, 0)
        d.input(0)

        # 11. The server holds 128 clients and refuses the next one.
        connected = sum(peer.connected for peer in peers)
        extra = [Peer(port) for _ in range(MAX_CLIENTS - connected)]
        for index, peer in enumerate(extra):
            peer.connect(f'x{index}')
        last = Peer(port)
        last.send(CONNECT, string('late'))
        assert last.wait(REFUSE) == 1  # ServerFull
        for peer in extra:
            peer.send(DISCONNECT)
            peer.close()
        last.close()

        # 12. A silent (crashed) member disappears from the others' lobby state.
        e.silent = True
        assert len(d.wait(STATE, lambda s: len(s['members']) == 1, timeout=8)['members']) == 1
        assert server.poll() is None
        print('PASS: lobby protocol, list, join by code, leader, spectators, level/gameover, scores, leave,')
        print('      two lobbies, four screen edges, full server and timeout.')
    finally:
        stop.set()
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
