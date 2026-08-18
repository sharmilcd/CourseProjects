# Starter baseline

These two files shipped with the assignment and describe the *unweighted* baseline that the
simulator was extended from: `sampleinput.txt` has no cost column, and `sampleoutput.txt` is the
hop-count routing table the original code produced for it.

They are kept for provenance only and are excluded from `scripts/run_tests.sh`. The extended parser
requires a cost on every link line; given a four-field line it fails the `cin >> cost` read, leaving
the stream in a fail state that the parse loop never clears, so the program hangs rather than
erroring out. Use the weighted topologies in `tests/input/` instead.
