LANGS = en fr hu it lv pt ru tr uk
LANGMO = $(LANGS:=.mo)
LANGPO = $(LANGS:=.po)

EXTRA_DIST = $(LANGPO)

.po.mo:
	$(MSGFMT) -o $@ $<

BUILT_SOURCES = $(LANGMO)

dist-hook: $(LANGPO)

install-data-local: $(LANGMO)
	for lang in $(LANGS); do \
		$(mkinstalldirs) $(DESTDIR)${libdir}/enigma2/python/Plugins/SystemPlugins/ServiceApp/locale/$$lang/LC_MESSAGES; \
		$(INSTALL_DATA) $$lang.mo $(DESTDIR)${libdir}/enigma2/python/Plugins/SystemPlugins/ServiceApp/locale/$$lang/LC_MESSAGES/ServiceApp.mo; \
	done

uninstall-local:
	for lang in $(LANGS); do \
		$(RM) $(DESTDIR)${libdir}/enigma2/python/Plugins/SystemPlugins/ServiceApp/locale/$$lang/LC_MESSAGES/ServiceApp.mo; \
	done
