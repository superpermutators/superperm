all: lower-bound-notes.pdf

clean:
	@rm -f lower-bound-notes.pdf

.PHONY: all clean

%.pdf: %.tex refs.bib
	latexmk -pdf $<
	latexmk -c -pdf $<
