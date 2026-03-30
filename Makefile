CC = gcc
CFLAGS = -O2 -Wall -Wextra
LDFLAGS = -lraylib -lm
PREFIX = /usr
ICON_SIZES = 48 64 128 256

minesweeper: main.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

icons: minesweeper.svg
	@for s in $(ICON_SIZES); do rsvg-convert minesweeper.svg -w $$s -h $$s -o minesweeper-$$s.png; done

install: minesweeper icons
	install -Dm755 minesweeper $(DESTDIR)$(PREFIX)/bin/minesweeper
	install -Dm644 minesweeper.svg $(DESTDIR)$(PREFIX)/share/icons/hicolor/scalable/apps/minesweeper.svg
	@for s in $(ICON_SIZES); do \
		install -Dm644 minesweeper-$$s.png $(DESTDIR)$(PREFIX)/share/icons/hicolor/$${s}x$${s}/apps/minesweeper.png; \
	done
	install -Dm644 minesweeper.desktop $(DESTDIR)$(PREFIX)/share/applications/minesweeper.desktop
	-gtk-update-icon-cache -f -t $(DESTDIR)$(PREFIX)/share/icons/hicolor 2>/dev/null
	-update-desktop-database $(DESTDIR)$(PREFIX)/share/applications 2>/dev/null

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/minesweeper
	rm -f $(DESTDIR)$(PREFIX)/share/icons/hicolor/scalable/apps/minesweeper.svg
	@for s in $(ICON_SIZES); do \
		rm -f $(DESTDIR)$(PREFIX)/share/icons/hicolor/$${s}x$${s}/apps/minesweeper.png; \
	done
	rm -f $(DESTDIR)$(PREFIX)/share/applications/minesweeper.desktop
	-gtk-update-icon-cache -f -t $(DESTDIR)$(PREFIX)/share/icons/hicolor 2>/dev/null
	-update-desktop-database $(DESTDIR)$(PREFIX)/share/applications 2>/dev/null

clean:
	rm -f minesweeper minesweeper-*.png

.PHONY: clean install uninstall icons
