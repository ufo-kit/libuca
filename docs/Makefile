PANDOC=$(shell which pandoc)
OPTS=-s --smart --toc

all: pdf html

pdf: manual.pdf

html: manual.html

clean:
	rm -f manual.pdf manual.html

manual.pdf: manual.md gen-changelog
	./gen-changelog
	$(PANDOC) $(OPTS) manual.md z-changes-*.md -o manual.pdf
	rm z-changes-*.md

manual.html: manual.md style.css gen-changelog
	./gen-changelog
	$(PANDOC) $(OPTS) manual.md z-changes-*.md -H webfonts.html -c style.css -o manual.html
	rm z-changes-*.md

ifeq ($(PANDOC),)
	$(warning Pandoc not found!)
endif
