import unittest
from server import Bench


class ProtocolTests(unittest.TestCase):
    def setUp(self):
        self.bench = Bench()

    def test_ports_do_not_get_swapped(self):
        self.bench.parse('COUNT encoder=1 delta=-4 total=-12 invalid=2')
        self.bench.parse('COUNT encoder=2 delta=8 total=16 invalid=0')
        first, second, third = [self.bench.state['encoders'][str(e)] for e in (1, 2, 3)]
        self.assertEqual((first['port'], first['count'], first['invalid']), (3, -12, 2))
        self.assertEqual((second['port'], second['count']), (2, 16))
        self.bench.parse('COUNT encoder=3 delta=4 total=24 invalid=1')
        self.assertEqual((third['port'], third['count'], third['invalid']), (1, 24, 1))

    def test_fourth_encoder_count_button_and_reboot(self):
        self.bench.parse('COUNT encoder=4 delta=-8 total=-8 invalid=0')
        fourth = self.bench.state['encoders']['4']
        self.assertEqual((fourth['port'], fourth['count'], fourth['invalid']), (4, -8, 0))
        self.bench.parse('BUTTON ms=42 encoder=4 event=PRESS raw=1')
        self.bench.parse('BUTTON ms=84 encoder=4 event=RELEASE raw=0')
        self.assertEqual((fourth['presses'], fourth['pressed']), (1, False))
        self.bench.parse('Genesis quad encoder bench v3; 115200 baud')
        self.assertTrue(self.bench.state['verified'])
        self.assertFalse(self.bench.state['encoders']['4']['observed'])
        self.assertEqual(self.bench.state['encoders']['4']['count'], 0)

    def test_third_encoder_button(self):
        self.bench.parse('BUTTON ms=42 encoder=3 event=PRESS raw=1')
        self.assertTrue(self.bench.state['encoders']['3']['pressed'])
        self.bench.parse('BUTTON ms=84 encoder=3 event=RELEASE raw=0')
        self.assertFalse(self.bench.state['encoders']['3']['pressed'])
        self.assertEqual(self.bench.state['encoders']['3']['presses'], 1)

    def test_triple_firmware_reboot(self):
        self.bench.parse('COUNT encoder=3 delta=4 total=40 invalid=0')
        self.bench.parse('Genesis triple encoder bench v2; 115200 baud')
        self.assertTrue(self.bench.state['verified'])
        self.assertFalse(self.bench.state['encoders']['3']['observed'])

    def test_press_release_and_reset(self):
        self.bench.parse('BUTTON ms=42 encoder=2 event=PRESS raw=1')
        self.assertTrue(self.bench.state['encoders']['2']['pressed'])
        self.bench.parse('BUTTON ms=84 encoder=2 event=RELEASE raw=0')
        self.assertFalse(self.bench.state['encoders']['2']['pressed'])
        self.assertEqual(self.bench.state['encoders']['2']['presses'], 1)
        self.bench.reset()
        self.assertIsNone(self.bench.state['encoders']['2']['pressed'])

    def test_overflow_invalidates_capture(self):
        self.bench.parse('OVERFLOW dropped=123 TRIAL INVALID: repeat slower')
        self.assertEqual(self.bench.state['overflow'], 123)

    def test_reboot_clears_old_counts(self):
        self.bench.parse('COUNT encoder=1 delta=4 total=40 invalid=0')
        self.bench.parse('Genesis dual encoder bench v1; 115200 baud')
        self.assertTrue(self.bench.state['verified'])
        self.assertFalse(self.bench.state['encoders']['1']['observed'])
        self.assertEqual(self.bench.state['encoders']['1']['count'], 0)

    def test_noise_does_not_invent_movement(self):
        for line in ['garbage', 'COUNT encoder=5 delta=1 total=1 invalid=0',
                     'COUNT encoder=1 delta=x total=1 invalid=0']:
            self.bench.parse(line)
        self.assertFalse(self.bench.state['verified'])
        self.assertTrue(all(not e['observed'] for e in self.bench.state['encoders'].values()))


if __name__ == '__main__':
    unittest.main()
