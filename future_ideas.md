# Ideas

- Operand B determines which IO unit should be used instead of opcode bit.

- Load2 uses a new address layout: The sixteen 8-bit registers are paired up to eight 16-bit registers. the output address now references one of these eight 16-bit registers. Besides preserving previous behaviour, this would allow swapping two registers with a single instruction (R0, R1) = (R1, R0).
