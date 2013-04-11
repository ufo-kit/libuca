PANDOC=$(shell which pandoc)
OPTS=-s --smart --toc
MANUAL=manual.md
NEWS=../NEWS
INPUTS=$(MANUAL) $(NEWS)

all: pdf html

pdf: manual.pdf

html: manual.html

clean:
	rm -f manual.pdf manual.html

manual.pdf: $(INPUTS)
	$(PANDOC) $(OPTS) $(INPUTS) -o $@

manual.html: $(INPUTS) style.css
	$(PANDOC) $(OPTS) $(INPUTS) -H webfonts.html -c style.css -o $@

ifeq ($(PANDOC),)
	$(warning Pandoc not found!)
endif
