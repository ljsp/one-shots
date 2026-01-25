all:
	g++ -std=c++23 -Wextra -Wpedantic main.cpp -o main

clean:
	rm -f main listing_*_mov

LISTING ?= listing_0039_more_movs
verify:
	@echo "Verifying output against $(LISTING).asm"
	g++ -std=c++23 -Wextra -Wpedantic main.cpp -o main
	nasm $(LISTING).asm
	./main $(LISTING) > output.asm
	nasm output.asm
	diff $(LISTING) output
	rm -f main output output.asm $(LISTING)