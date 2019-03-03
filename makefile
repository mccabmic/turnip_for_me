CXX = gcc
CXXFLAGS = -std=c99
CXXFLAGS += -Wall
CXXFLAGS += -pedantic-errors
CXXFLAGS += -g

OBJS = smallsh.o util.o
SRCS = smallsh.c util.c
HEADERS = util.h

smallsh: ${OBJS} ${HEADERS}
	${CXX} ${OBJS} -o smallsh

${OBJS}: ${SRCS}
	${CXX} ${CXXFLAGS} -c $(@:.o=.c)

clean: 
	rm -f ${OBJS}
	rm -f smallsh
