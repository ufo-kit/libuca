PANDOC=$(shell which pandoc)
OPTS=-s --smart --toc

all: pdf html

pdf: manual.pdf

html: manual.html

clean:
	rm -f manual.pdf manual.html

manual.pdf: manual.md
	$(PANDOC) $(OPTS) manual.md z-changes-*.md -o manual.pdf

manual.html: manual.md style.css
	$(PANDOC) $(OPTS) manual.md z-changes-*.md -H webfonts.html -c style.css -o manual.html

ifeq ($(PANDOC),)
	$(warning Pandoc not found!)
endif
