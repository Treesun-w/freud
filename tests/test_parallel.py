import freud
import unittest
import warnings


class TestParallel(unittest.TestCase):
    """Ensure that setting threads is appropriately reflected in Python."""
    # The setUp and tearDown ensure that these tests don't affect others.
    def setUp(self):
        freud.parallel.setNumThreads(0)

    def tearDown(self):
        freud.parallel.setNumThreads(0)

    def test_set(self):
        self.assertEqual(freud.parallel._numThreads, 0)
        freud.parallel.setNumThreads(10)
        self.assertEqual(freud.parallel._numThreads, 10)

    def test_NumThreads(self):
        """Test the NumThreads context manager."""
        self.assertEqual(freud.parallel._numThreads, 0)
        with freud.parallel.NumThreads(10):
            self.assertEqual(freud.parallel._numThreads, 10)
        self.assertEqual(freud.parallel._numThreads, 0)


if __name__ == '__main__':
    unittest.main()