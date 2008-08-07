CONFIG ?= makefile.config

-include ${CONFIG}

SOURCES += src/hash.c
SOURCES += src/http.c
SOURCES += src/http_server.c
SOURCES += src/jabber_bind.c
SOURCES += src/log.c
SOURCES += src/main.c
SOURCES += src/socket_monitor.c
SOURCES += src/time.c
SOURCES += src/list.c
SOURCES += src/socket.c

SRCDIR = src
OBJDIR = obj
DEPSDIR = .deps
CFLAGS += -Wall -D_GNU_SOURCE $(shell pkg-config iksemel --cflags)
CXXFLAGS += ${CFLAGS}
LDLIBS += -I${HOME}/.usr/lib -lrt $(shell pkg-config iksemel --libs)
TARGET ?= bosh

CC ?= gcc
CXX ?= g++

OBJECTS = $(patsubst ${SRCDIR}/%.c,${OBJDIR}/%.o,${SOURCES})
DEPS = $(patsubst ${SRCDIR}/%.c,${DEPSDIR}/%.d,${SOURCES})

all: ${TARGET}
	@echo "done"

-include ${DEPS}

${TARGET}: ${OBJECTS}
	${CC} -o ${TARGET} ${OBJECTS} ${CFLAGS} ${LDLIBS}

.deps/%.d: ${SRCDIR}/%.cc
	@mkdir -p $(dir $@)
	${CXX} ${CXXFLAGS} -MM $< | sed 's/\(^[^ \.]*\)\.o/${OBJDIR}\/\1.o ${DEPSDIR}\/\1.d/' > $@

.deps/%.d: ${SRCDIR}/%.c
	@mkdir -p $(dir $@)
	${CC} ${CFLAGS} -MM $< | sed 's/\(^[^ \.]*\)\.o/${OBJDIR}\/\1.o ${DEPSDIR}\/\1.d/' > $@

obj/%.o: ${SRCDIR}/%.cc
	@mkdir -p $(dir $@)
	${CXX} ${CXXFLAGS} -c -o $@ $<

obj/%.o: ${SRCDIR}/%.c
	@mkdir -p $(dir $@)
	${CC} ${CFLAGS} -c -o $@ $<

clean: clean-target clean-obj

clean-target:
	rm -f ${TARGET}

clean-obj:
	rm -f ${OBJECTS}

clean-depends:
	rm -rf ${DEPSDIR}
