#Replace the language with the one you want to test
GRAMMARDIR=grammar
LAN=php

all: template/* Generator.py
	@if [ ! -d "src" ]; then \
	    mkdir src; \
	    mkdir include; \
	    mkdir parser; \
	fi
	cd ${GRAMMARDIR}/${LAN}_grammar && ./replace.sh
	python Generator.py -i ${GRAMMARDIR}/${LAN}_grammar/replaced_grammar -t ${GRAMMARDIR}/${LAN}_grammar/tokens -d data/destructor -D data/datatype -e data/extra_flex_rule_${LAN} -s ${GRAMMARDIR}/${LAN}_grammar/semantic.json
	cd parser && flex flex.l && bison bison.y --output=bison_parser.cpp --defines=bison_parser.h --verbose -Wconflicts-rr
	cd AFL_replace_mutate && make

clean:
	rm include/* parser/* src/*
	rm grammar/*/replaced_grammar grammar/*/tokens
	rm *.pyc

FORCE:
