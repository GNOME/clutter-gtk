ent_conf = configuration_data()
ent_conf.set('PACKAGE', 'Clutter-GTK')
ent_conf.set('PACKAGE_BUGREPORT', 'https://bugzilla.gnome.org/enter_bug.cgi?product=clutter-gtk')
ent_conf.set('PACKAGE_NAME', 'Clutter-GTK')
ent_conf.set('PACKAGE_STRING', 'clutter-gtk')
ent_conf.set('PACKAGE_TARNAME', '@0@-@1@'.format(meson.project_name(), meson.project_version()))
ent_conf.set('PACKAGE_URL', '')
ent_conf.set('PACKAGE_VERSION', meson.project_version())
configure_file(input: 'gtkdocentities.ent.in', output: 'gtkdocentities.ent', configuration: ent_conf)
