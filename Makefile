################################################################################
# Dahsee makefile
# 2012-08-13
################################################################################

include config.mk


all: man
	@make -C src/

check:
	@make -C test/

clean:
	@make -C src/ clean
	@make -C test/ clean
	@rm -rf "doc/${BIN}.1.gz"

dist: ${BIN}-${VERSION}.tar.gz

${BIN}-${VERSION}.tar.gz: config.mk COPYING data doc makefile README src test
	@echo Creating distribution tarball.
	@mkdir -p "${BIN}-${VERSION}"
	@cp -R $^ "${BIN}-${VERSION}"
	@tar czf "$@" "${BIN}-${VERSION}"
	@rm -rf "${BIN}-${VERSION}"

man: doc/${BIN}.1
	@gzip -c $< > "doc/${BIN}.1.gz"

## TODO: remove echos and @.
install: all
	@echo "PREFIX  = ${PREFIX}"
	@echo Installing executable file to "${DESTDIR}${BINDIR}"
	@mkdir -p "${DESTDIR}${BINDIR}"
	@cp -f "src/${BIN}" "${DESTDIR}${BINDIR}"
	@chmod 755 "${DESTDIR}${BINDIR}/${BIN}"
	@eho Installing man page to "${DESTDIR}${MAN1DIR}"
	@mkdir -p "${DESTDIR}${MAN1DIR}"
	@cp -f "doc/${BIN}.1.gz"  "${DESTDIR}${MAN1DIR}"
	@eho Installing web data to "${DESTDIR}${DATADIR}/${BIN}"
	@mkdir -p "${DESTDIR}${DATADIR}/${BIN}"
	@cp -f data/*  "${DESTDIR}${DATADIR}/${BIN}"

uninstall:
	@echo Removing executable file "${DESTDIR}${BINDIR}/${BIN}"
	@rm -f "${DESTDIR}${BINDIR}/${BIN}"
	@echo Removing man page "${DESTDIR}${MAN1DIR}/${BIN}.1.gz"
	@rm -f "${DESTDIR}${MAN1DIR}/${BIN}.1.gz"
	@echo Removing web data from "${DESTDIR}${DATADIR}/${BIN}"
	@rm -rf "${DESTDIR}${DATADIR}/${BIN}"

.PHONY = all check clean dist install uninstall
