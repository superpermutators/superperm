all: tsp/5.tsp tsp/6.tsp tsp/7.tsp

.PHONY: all

tsp/%.tsp: atsp/%.atsp
	bin/symmetrise.py "$<" > "$@"

atsp/%.atsp:
	bin/mkatsp.py $* > "$@"
