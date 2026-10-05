"""Focused checks for the reusable profiling wrapper without requiring perf privileges."""

from contextlib import redirect_stderr
import io
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import profile_game


class ProfileGameTests(unittest.TestCase):
    def test_attach_is_bounded_and_does_not_launch_game(self):
        with tempfile.TemporaryDirectory() as directory:
            output = str(Path(directory) / "profile.data")
            args = ["profile_game.py", "--pid", "123", "--warmup", "2", "--seconds", "5", "--output", output]
            with patch("sys.argv", args), patch("profile_game.validate_target") as validate, \
                    patch("profile_game.shutil.which", return_value="/usr/bin/perf"), \
                    patch("profile_game.subprocess.Popen") as launch:
                launch.return_value.wait.return_value = 0
                self.assertEqual(profile_game.main(), 0)
            validate.assert_called_once_with(123)
            command = launch.call_args.args[0]
            self.assertIn("cycles:u", command)
            self.assertIn("dwarf,16384", command)
            self.assertEqual(command[-6:], ["--delay=2000", "-p", "123", "--", "sleep", "7.0"])

    def test_zero_warmup_omits_invalid_perf_zero_delay(self):
        with tempfile.TemporaryDirectory() as directory:
            args = ["profile_game.py", "--pid", "123", "--warmup", "0", "--output", directory + "/profile.data"]
            with patch("sys.argv", args), patch("profile_game.validate_target"), \
                    patch("profile_game.shutil.which", return_value="/usr/bin/perf"), \
                    patch("profile_game.subprocess.Popen") as launch:
                launch.return_value.wait.return_value = 0
                self.assertEqual(profile_game.main(), 0)
            self.assertFalse(any(arg.startswith("--delay=") for arg in launch.call_args.args[0]))

    def test_invalid_attach_arguments_never_start_perf(self):
        for args in (["--pid", "0"], ["--pid", "123", "--seconds", "nan"],
                     ["--pid", "123", "--seconds", "0"], ["--pid", "123", "--seconds", "3601"],
                     ["--pid", "123", "--include-loading"], ["--pid", "123", "--", "--world", "mm6"]):
            with self.subTest(args=args), patch("sys.argv", ["profile_game.py", *args]), \
                    patch("profile_game.subprocess.Popen") as launch, redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit) as error:
                    profile_game.main()
                self.assertEqual(error.exception.code, 2)
                launch.assert_not_called()

    def test_attach_rejects_other_executables_and_missing_processes(self):
        with self.assertRaisesRegex(ValueError, "owned by the current user"):
            profile_game.validate_target(os.getpid())  # This is Python, not OpenYAMM.
        with self.assertRaises(OSError):
            profile_game.validate_target(2147483647)


if __name__ == "__main__":
    unittest.main()
