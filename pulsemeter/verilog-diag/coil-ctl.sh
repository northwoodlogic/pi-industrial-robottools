#!/bin/sh

IO_SEL=95
IO_STS=96

INH_STS=79
INH_SET=83
INH_CLR=82

CHIP=gpiochip1

pulse_negedge() {
    gpioset $CHIP $1=0
    gpioget $CHIP $1 > /dev/null
}

initial_setup() {
    gpioget $CHIP $INH_STS > /dev/null
    gpioget $CHIP $INH_CLR > /dev/null
    gpioget $CHIP $INH_SET > /dev/null
    pulse_negedge $INH_CLR
}

inhibit_sts() {
    gpioget $CHIP $INH_STS
}

# make io_sel go to 1, read rack sts input
pswitch_sts() {
    gpioget $CHIP ${IO_SEL} > /dev/null
    gpioget $CHIP ${IO_STS}
}

# make io_sel go to 0, read rack sts input
l24vac_sts() {
    gpioset $CHIP ${IO_SEL}=0
    gpioget $CHIP ${IO_STS}
}

inhibit_set() {
    pulse_negedge $INH_SET
}

inhibit_clr() {
    pulse_negedge $INH_CLR
}

# called as
#    inhibit_test "[0|1]" "message"
inhibit_test() {
    local sts
    sts=$(inhibit_sts)


    if [ "${sts}" != "$1" ] ; then
        echo "${2} fail! sts=${sts}"
        return 1
    fi
    echo "${2} OK! sts=${sts}"
}


case $1 in
	status)
		echo "inhibit status: `inhibit_sts`"
		echo "  24vac_status: `l24vac_sts`"
		echo "pswitch_status: `pswitch_sts`"
		;;
	inhibit)
		echo "Inhibiting coil operation"
		inhibit_set
		;;
	enable)
		echo "Enabling coil operation"
		inhibit_clr
		;;
	*)
		echo ""
		echo "Coil inhibit test program"
		echo "Usage:"
		echo "  coil-ctl.sh [status|inhibit|enable]"
		echo ""
		;;
esac

