SOURCES = \
	src/hash.c \
	src/http.c \
	src/http_server.c \
	src/jabber_bind.c \
	src/jabber.c \
	src/list.c \
	src/log.c \
	src/main.c \
	src/socket_monitor.c \
	src/socket_util.c \
	src/time.c
		  
SRCDIR = src
OBJDIR = obj
DEPSDIR = .deps
CFLAGS = -Wall -D_GNU_SOURCE $(shell pkg-config iksemel --cflags)
#CFLAGS += -O3 -fomit-frame-pointer
CFLAGS += -ggdb3
CXXFLAGS = ${CFLAGS}
LDLIBS = -I${HOME}/.usr/lib -lrt $(shell pkg-config iksemel --libs)
TARGET = bosh
CC=gcc
CXX=g++

OBJECTS = $(patsubst ${SRCDIR}/%.c,${OBJDIR}/%.o,${SOURCES})
DEPS = $(patsubst ${SRCDIR}/%.c,${DEPSDIR}/%.d,${SOURCES})

all: ${TARGET}
	@echo "done"

-include ${DEPS}

${TARGET}: ${OBJECTS}
	${CC} -o ${TARGET} ${OBJECTS} ${CXXFLAGS} ${LDLIBS}

.deps/%.d: ${SRCDIR}/%.cc
	@mkdir -p $(dir $@)
	${CXX} ${CXXFLAGS} -MM $< | sed 's/\(^[^ \.]*\)\.o/${OBJDIR}\/\1.o ${DEPSDIR}\/\1.d/' > $@

.deps/%.d: ${SRCDIR}/%.c
	@mkdir -p $(dir $@)
	${CXX} ${CXXFLAGS} -MM $< | sed 's/\(^[^ \.]*\)\.o/${OBJDIR}\/\1.o ${DEPSDIR}\/\1.d/' > $@

obj/%.o: ${SRCDIR}/%.cc
	@mkdir -p $(dir $@)
	${CXX} -c ${CXXFLAGS} -o $@ $<

obj/%.o: ${SRCDIR}/%.c
	@mkdir -p $(dir $@)
	${CC} -c ${CXXFLAGS} -o $@ $<

clean: clean-target clean-obj

clean-target:
	rm -f ${TARGET}

clean-obj:
	rm -f ${OBJECTS}

clean-depends:
	rm -rf ${DEPSDIR}
