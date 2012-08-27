PANDOC=$(shell which pandoc)
MD2PDF=$(shell which markdown2pdf)
OPTS=-s --smart

all: pdf html

pdf: manual.pdf

html: manual.html

clean:
	rm -f manual.pdf manual.html

manual.pdf: manual.md
	$(MD2PDF) -o manual.pdf	manual.md

manual.html: manual.md
	$(PANDOC) $(OPTS) manual.md -o manual.html

ifeq ($(PANDOC),)
	$(warning Pandoc not found!)
endif
