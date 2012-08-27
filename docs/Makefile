PANDOC=$(shell which pandoc)
OPTS=-s --smart

all: pdf html

pdf: manual.pdf

html: manual.html

clean:
	rm -f manual.pdf manual.html

manual.pdf: manual.md
	$(PANDOC) manual.md -o manual.pdf

manual.html: manual.md
	$(PANDOC) $(OPTS) manual.md -o manual.html

ifeq ($(PANDOC),)
	$(warning Pandoc not found!)
endif
