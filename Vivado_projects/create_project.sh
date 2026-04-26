#!/usr/bin/env bash

for arg in "$@"; do
	case "$arg" in
		-h|--help)
			echo -e "Usage:
  ./create_project.sh [options]

Options:
  -n <name>      Project name, will prompt without this
  -h, --help     Prints this menu"
			exit 0
			;;
		-n)
			CAPTURE_NEXT=1
			;;
		esac
		if [[ -v CAPTURE_NEXT ]]; then
			PROJ_NAME=$arg
			CAPTURE_NEXT=
		fi
done

if [ -z "$VIVADO_BIN" ]; then
	VIVADO_BIN=$(command -v vivado 2>/dev/null)
fi
if [ -z "$VIVADO_BIN" ]; then
	VIVADO_BIN=$(find /opt/Xilinx/*/Vivado/bin -maxdepth 1 -type f -name 'vivado' | tail -n1)
fi
if [ -z "$VIVADO_BIN" ]; then
	VIVADO_BIN=$(find /tools/Xilinx/*/Vivado/bin -maxdepth 1 -type f -name 'vivado' | tail -n1)
fi
if [ -z "$VIVADO_BIN" ]; then
	echo "Could not find vivado binary, please try run with 'env VIVADO_BIN=(vivado binary) ./create_project.sh'"
	exit 1
fi

if [ -z "$PROJ_NAME" ]; then
	$VIVADO_BIN -mode batch -source template.tcl
else
	$VIVADO_BIN -mode batch -source template.tcl -tclargs --project_name $PROJ_NAME
fi

rm -f vivado.jou vivado.log vivado_*.log vivado_*.backup.jou
