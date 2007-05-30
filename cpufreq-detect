#!/bin/sh

CPUINFO=/proc/cpuinfo
IOPORTS=/proc/ioports

[ -f $CPUINFO ] || exit 0

MODEL_NAME=$(grep '^model name' "$CPUINFO" | head -1 | sed -e 's/^.*: //;')
VENDOR_ID=$(grep -E '^vendor_id[^:]+:' "$CPUINFO" | head -1 | sed -e 's/^.*: //;')
CPU_FAMILY=$(sed -e '/^cpu family/ {s/.*: //;p;Q};d' $CPUINFO)

CPUFREQ=

# Two modules for PIII-M depending the chipset.
if [ -f $IOPORTS ] && grep -q 'Intel .*ICH' $IOPORTS ; then
	PIII_CPUFREQ=speedstep-ich
else
	PIII_CPUFREQ=speedstep-smi
fi

case "$VENDOR_ID" in
	
	GenuineIntel*)
		# If the CPU has the est flag, it supports enhanced speedstep and should
		# use the acpi-cpufreq driver (speedstep-centrino is deprecated)
		if grep -q est $CPUINFO; then
			CPUFREQ=acpi-cpufreq;
		elif [ $CPU_FAMILY = 15 ]; then
			# Right. Check if it's a P4 without est.
			# Could be speedstep-ich.
			CPUFREQ=speedstep-ich;
		else
			# So it doesn't have Enhanced Speedstep, and it's not a P4. It could be 
			# a Speedstep PIII, or it may be unsupported. There's no terribly good
			# programmatic way of telling.
			case "$MODEL_NAME" in
				Intel\(R\)\ Pentium\(R\)\ III\ Mobile\ CPU*)
					CPUFREQ=$PIII_CPUFREQ
					;;
				
				# JD: says this works with   cpufreq_userspace
				Mobile\ Intel\(R\)\ Pentium\(R\)\ III\ CPU\ -\ M*)
					CPUFREQ=$PIII_CPUFREQ
					;;
				
				# https://bugzilla.ubuntu.com/show_bug.cgi?id=4262
				# UNCONFIRMED
				Pentium\ III\ \(Coppermine\)*)
					CPUFREQ=$PIII_CPUFREQ
					;;
			esac
		fi
		;;
	
	AuthenticAMD*)
		# Hurrah. This is nice and easy.
		case $CPU_FAMILY in
			5)
				# K6
				CPUFREQ=powernow-k6
				;;
			6)
				# K7
				CPUFREQ=powernow-k7
				;;
			15)
				# K8
				CPUFREQ=powernow-k8
				;;
		esac
		;;
	
	CentaurHauls*)
		# VIA
		if [ $CPU_FAMILY = 6 ]; then
			CPUFREQ=longhaul;
		fi
		;;
	
	GenuineTMx86*)
		# Transmeta
		if grep -q longrun $CPUINFO; then
			CPUFREQ=longrun
		fi
		;;

esac
