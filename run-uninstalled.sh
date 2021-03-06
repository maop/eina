#!/bin/bash

D="$(dirname -- "$0")"
if [ "${D:0:1}" != "/" ]; then
	D="$PWD/$D"
fi

R="$D/tools/run"
[ -e "$R" ] && rm -rf -- "$R"

# Setup path for libs
export LD_LIBRARY_PATH="$D/lomo/.libs:$D/gel/.libs:$D/eina/ext/.libs:$LD_LIBRARY_PATH"
export DYLD_LIBRARY_PATH="$D/lomo/.libs:$D/gel/.libs:$D/eina/ext/.libs:$DYLD_LIBRARY_PATH"
export GI_TYPELIB_PATH="$D/lomo:$D/gel:$D/eina:$GI_TYPELIB_PATH"

# Resource handling (gel-related)
export EINA_PIXMAP_PATH="$D/pixmaps"
export EINA_LIB_PATH="$R/plugins"

# Eina specific
export EINA_THEME_DIR="$D/icons"
export EINA_DESKTOP_FILE="$D/data/eina.desktop"

# Copy EINA vars to VACA (for testing)
export VACA_PIXMAP_PATH="$EINA_PIXMAP_PATH"
export VACA_LIB_PATH="$EINA_LIB_PATH"
export VACA_THEME_PATH="$EINA_UI_PATH"


[ -e "$R" ] && rm -rf "$R"


if [ ! -z "$(which glib-compile-schemas)" ]; then
	EINA_DATA_DIR="$R/share"
	EINA_SCHEMAS_DIR="$EINA_DATA_DIR/glib-2.0/schemas"
	[ -d "$EINA_SCHEMAS_DIR" ] || mkdir -p "$EINA_SCHEMAS_DIR"

	for schema in $(find "$D/data" "$D/plugins" -name '*gschema.valid' | sed -e 's,.valid$,.xml,')
	do
		echo "[*] Added schema $(basename -- $schema)"
		ln -s "$schema" "$EINA_SCHEMAS_DIR/$(basename -- $schema)"
	done

	export XDG_DATA_DIRS="$EINA_DATA_DIR:$XDG_DATA_DIRS"
	export GSETTINGS_SCHEMA_DIR="$EINA_SCHEMAS_DIR" # Backwards compatibility
	glib-compile-schemas "$EINA_SCHEMAS_DIR"
fi

# DConf
GTK3_DEMO="$(which gtk3-demo)"
if [ ! -z "$GTK3_DEMO" -a -x "$(dirname -- "$GTK3_DEMO")/../libexec/dconf-service" -a -z "$(pidof dconf-service)" ]; then
	$(dirname $(which gtk3-demo))/../libexec/dconf-service &
fi

# Plugins
IFS="
"
for plugindir in $(find "$D/eina" -maxdepth 1 -type d | sort | tail -n +2  )
do
	plugin=$(basename -- "$plugindir")
	[ -f "$plugindir/$plugin.plugin" ] || continue

	mkdir -p "$R/plugins/$plugin"
	for ext in so ui png jpg gif plugin py svg
	do
		for obj in $(find "$plugindir" -name "*.$ext")
		do
			[ "$ext" = "so" ] && echo "[*] Detected plugin $plugin"
			[ "$obj" = "$plugin.py" ] && echo "[*] Detected plugin $plugin"
			ln -s $obj "$R/plugins/$plugin"
		done
	done
	for d in $(find "$plugindir" -maxdepth 1 -type d | tail -n +2)
	do
		ln -s "$d" "$R/plugins/$plugin"
	done
done

# Find real binary
P=${P:="eina"}

if [ -z "$BIN" ]; then
	for i in $D/eina/.libs/eina $D/eina/eina
	do
		echo $i
		if [ -x "$i" ]; then
			BIN="$i"
			break;
		fi
	done
fi
if [ ! -x  "$BIN" ]; then
	echo "Unable to find executable"
	exit 1
fi
echo "[*] Found binary $BIN"

if [ ! -z "$1" ]; then
	case "$1" in
		noop)
			exit 0
			;;

		ltrace)
			shift
			ltrace "$BIN" "$@"
			;;
		strace)
			shift
			strace -f "$BIN" "$@"
			;;
		gdb)
			shift
			gdb --args "$BIN" "$@"
			;;
		*)
			"$BIN" "$@"
			;;
	esac
else
	"$BIN" "$@"
fi

