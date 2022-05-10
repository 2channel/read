apacheinc     = `apxs -q INCLUDEDIR`

CC            = gcc
DEFS          = -DHAVE_READ2CH_H
# DEFS        = -DCUTRESLINK -DRELOADLINK -DLASTMOD -DNEWBA -DGSTR2 \
# 		-DTYPE_TERI -DCOOKIE -DPREVENTRELOAD
CFLAGS        = -I$(apacheinc) -g -O2 -Wall -funsigned-char -fPIC
LIBS          = 
OBJS          = datindex.o read.o

SRCS          = datindex.c digest.c read.c
HDRS          = r2chhtml.h r2chhtml_en.h read2ch.h datindex.h digest.h \
		read.h
DOCS          = Makefile config.txt ChangeLog

.SUFFIXES: .c .o .so

# phony targets
.PHONY:all clean test dat strip tags dist

# targets
read.so: $(OBJS)

all: read.so TAGS

clean:
	$(RM) -f read.so *.o

# バイナリのあるファイルの下にtech/dat/998845501.datを置いてから実行
test:
	( export HTTP_COOKIE='NAME=abcde%21;MAIL=hoge@huga.net' \
	export QUERY_STRING='bbs=tech&key=998845501'; \
	export HTTP_USER_AGENT='console'; \
	cd tech; \
	../read.so )

dat:
	( mkdir tech; mkdir tech/dat; \
	wget -O tech/dat/998845501.dat \
		http://piza2.2ch.net/tech/dat/998845501.dat )

strip: read.so
	strip -v $^ $>

dist: read.tgz

read.tgz: $(SRCS) $(HDRS) $(DOCS)
	tar cf - $(SRCS) $(HDRS) $(DOCS) \
	| gzip -9 > read.tgz

tags: TAGS

TAGS: *.c *.h
	etags $^ $>

# implicit rules
.o.so:
	$(CC) -shared $^ $> $(LIBS) -Wl,-S,-soname,$@ -o $@

.c.o:
	$(CC) -c $< $(DEFS) $(CFLAGS) -o $@

# dependencies
datindex.o: datindex.c datindex.h read.h read2ch.h
digest.o: digest.c digest.h read.h read2ch.h r2chhtml.h
read.o: read.c read2ch.h datindex.h digest.h read.h r2chhtml.h
