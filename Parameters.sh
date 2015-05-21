# Run parameters
# UWFD64 unit number. Hexadecimal !!!
UNIT_NUMBER=5
# Geographical address to use. Should be in the range 2..1F. Hexadecimal !!!
UNIT_GA=2
# Connectors to be used. Count from top. Range 0..3
RUN_LIST="0 1 2 3"
# Running time in seconds. Hexadecimal !!!
RUN_TIME=20
# Self trigger threshold. Hexadecimal !!!
THRESHOLD=A
# ADC digital outputs phase. . Hexadecimal !!! Range 0..B

export UNIT_NUMBER UNIT_GA THRESHOLD RUN_TIME RUN_LIST ADC_PHASE
