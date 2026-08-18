# Gate-Level Tic-Tac-Toe / Sequential Logic in Verilog
# Requires Icarus Verilog (iverilog + vvp).

IVERILOG := iverilog
VVP      := vvp
INC      := -I src
BUILD    := build

TESTS := ms ed counter tcell tbox

.PHONY: all test clean $(TESTS)

all: test

test: $(TESTS)

$(BUILD):
	mkdir -p $(BUILD)

# Q1a - master-slave D flip-flop
ms: $(BUILD)
	$(IVERILOG) $(INC) -o $(BUILD)/ms.vvp tb/Q1a_tb.v
	cd $(BUILD) && $(VVP) ms.vvp

# Q1b - edge-triggered D flip-flop
ed: $(BUILD)
	$(IVERILOG) $(INC) -o $(BUILD)/ed.vvp tb/Q1b_tb.v
	cd $(BUILD) && $(VVP) ed.vvp

# Q2 - 4-bit asynchronous ripple counter
counter: $(BUILD)
	$(IVERILOG) $(INC) -o $(BUILD)/counter.vvp tb/Q2_tb.v
	cd $(BUILD) && $(VVP) counter.vvp

# Bonus - single tic-tac-toe cell
tcell: $(BUILD)
	$(IVERILOG) $(INC) -o $(BUILD)/tcell.vvp tb/TCell_tb.v
	cd $(BUILD) && $(VVP) tcell.vvp

# Bonus - full 3x3 game engine
tbox: $(BUILD)
	$(IVERILOG) $(INC) -o $(BUILD)/tbox.vvp tb/TBox_tb.v
	cd $(BUILD) && $(VVP) tbox.vvp

clean:
	rm -rf $(BUILD)
