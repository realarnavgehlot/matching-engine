# The quick example from README.md. If this test fails, the README's example output is out of date.
SELL 1 5 10000
SELL 2 5 10000
SELL 3 4 10100
BUY 4 7 10050
BUY 5 3 9900 GTT 100
PRINT
BUY 6 10 10100 FOK
TIME 100
CANCEL 5
BUY 7 0 10000
PRINT
