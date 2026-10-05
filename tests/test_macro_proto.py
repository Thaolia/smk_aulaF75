"""Tests hôte du protocole macro du GM610 (aucun matériel requis).

Vérifie le round-trip encode/decode, la couverture du blob par les trames HID
(reconstruction identique à celle du firmware), le checksum et les gardes."""

import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "utils"))

from gm610_macro_gui import keycodes, proto  # noqa: E402


def reconstruct(pkts):
    """Rejoue les trames comme le firmware : BEGIN fixe la longueur, DATA écrit à
    l'offset (index*5), COMMIT rend le checksum annoncé. Retourne (blob, sum)."""
    stg = bytearray(proto.MACRO_BLOB_SIZE)
    length = 0
    announced_sum = None
    for p in pkts:
        assert p[0] == proto.REPORT_ID_MACRO
        op = p[1]
        if op == proto.OP_BEGIN:
            length = p[2] | (p[3] << 8)
        elif op == proto.OP_DATA:
            base = p[2] * 5
            for i in range(5):
                pos = base + i
                if pos < length:
                    stg[pos] = p[3 + i]
        elif op == proto.OP_COMMIT:
            announced_sum = p[2]
    return bytes(stg[:length]), announced_sum


class TestMacroProto(unittest.TestCase):
    def sample_table(self):
        return {
            proto.key_index(2, 1): [("tap", keycodes.KEYS["C"], keycodes.MODS["LCtrl"])],
            proto.key_index(1, 1): [
                ("tap", keycodes.KEYS["H"], keycodes.MODS["LShift"]),
                ("tap", keycodes.KEYS["I"], 0),
                ("delay", 100),
                ("tap", keycodes.KEYS["Enter"], 0),
            ],
        }

    def test_round_trip(self):
        hi = (10, 200, 30)
        table = self.sample_table()
        blob = proto.encode(table, hi)
        hi2, table2 = proto.decode(blob)
        self.assertEqual(hi2, hi)
        self.assertEqual(table2, table)

    def test_frames_reconstruct_blob(self):
        blob = proto.encode(self.sample_table(), proto.DEFAULT_HI_COLOR)
        rebuilt, announced = reconstruct(proto.frames(blob))
        self.assertEqual(rebuilt, blob)
        self.assertEqual(announced, proto.checksum(blob))

    def test_frames_are_single_packet(self):
        blob = proto.encode(self.sample_table(), proto.DEFAULT_HI_COLOR)
        for p in proto.frames(blob):
            # 1 octet d'ID + 7 octets de données = 1 paquet EP0 de 8 octets.
            self.assertEqual(len(p), 1 + proto.FEATURE_REPORT_SIZE)

    def test_empty_table(self):
        blob = proto.encode({}, proto.DEFAULT_HI_COLOR)
        hi, table = proto.decode(blob)
        self.assertEqual(table, {})
        self.assertEqual(blob[proto.BLOB_COUNT_OFF], 0)

    def test_overflow_raises(self):
        # 16 touches de longues macros dépassent le budget de 240 octets.
        big = {proto.key_index(0, c): [("tap", keycodes.KEYS["A"], 0)] * 30 for c in range(14)}
        with self.assertRaises(ValueError):
            proto.encode(big, proto.DEFAULT_HI_COLOR)

    def test_too_many_keys_raises(self):
        many = {ki: [("tap", keycodes.KEYS["A"], 0)] for ki in range(proto.MACRO_MAX_KEYS + 1)}
        with self.assertRaises(ValueError):
            proto.encode(many, proto.DEFAULT_HI_COLOR)

    def test_kc_no_tap_raises(self):
        with self.assertRaises(ValueError):
            proto.encode({proto.key_index(0, 0): [("tap", 0, 0)]}, proto.DEFAULT_HI_COLOR)

    def test_text_to_steps(self):
        steps = keycodes.text_to_steps("Hi!")
        self.assertEqual(
            steps,
            [
                ("tap", keycodes.KEYS["H"], keycodes.MODS["LShift"]),
                ("tap", keycodes.KEYS["I"], 0),
                ("tap", keycodes.KEYS["1"], keycodes.MODS["LShift"]),
            ],
        )


if __name__ == "__main__":
    unittest.main()
