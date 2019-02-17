all: tsp/5.tsp tsp/6.tsp tsp/7.tsp demutator/demutator

.PHONY: all

tsp/%.tsp: atsp/%.atsp
	bin/symmetrise.py "$<" > "$@"

atsp/%.atsp:
	bin/mkatsp.py $* > "$@"

necklace/%.gtsp:
	bin/mkatsp.py --necklace $* > "$@"

bracelet/%.gtsp:
	bin/mkatsp.py --bracelet $* > "$@"
