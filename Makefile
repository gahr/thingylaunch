# vim: set ts=4 noexpandtab:

REPO=		fossil info | grep ^repository | awk '{print $$2}'
PROG=		thingylaunch
ALL=		${PROG}
SRCS=		bookmark.cpp completion.cpp history.cpp thingylaunch.cpp util.cpp x11_xcb.cpp
OBJS=		${SRCS:.cpp=.o}
JSONS=		${OBJS:.o=.o.json}
XCB_MODULES=xcb xcb-icccm xcb-keysyms
CXXFLAGS=	-std=c++11 -Wall -Werror
CPPFLAGS=	`pkg-config --cflags ${XCB_MODULES}`
LDFLAGS=	`pkg-config --libs ${XCB_MODULES}`

.if "${DEV}"
DEV_FLAGS= -MJ${@:.o=.o.json}
ALL+=		compile_commands.json
compile_commands.json: ${OBJS}
	echo '[' > compile_commands.json
	cat ${JSONS} >> compile_commands.json
	echo ']' >> compile_commands.json

.endif

all: ${ALL}

.cpp.o:
	${CXX} ${CPPFLAGS} ${CXXFLAGS} ${DEV_FLAGS} -c -o $@ $<

${PROG}: ${OBJS}
	${CXX} ${LDFLAGS} -o $@ ${OBJS}

clean:
	rm -f ${PROG} ${OBJS} ${JSONS} compile_commands.json

install: ${PROG}
	install -s -m 555 ${PROG} ${DESTDIR}${PREFIX}/bin/${PROG}

git:
	@if [ -e git-import ]; then \
	    echo "The 'git-import' directory already exists"; \
	    exit 1; \
	fi; \
	git init -b master git-import && cd git-import && \
	fossil export --git --rename-trunk master --repository `${REPO}` | \
	git fast-import && git reset --hard HEAD && \
	git remote add origin git@github.com:gahr/bwget.git && \
	git push -f origin master && \
	cd .. && rm -rf git-import
